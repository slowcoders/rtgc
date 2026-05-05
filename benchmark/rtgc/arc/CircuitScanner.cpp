#include "GCNode.hpp" 

namespace RTGC {
	class CircuitScanner;

	static const int NOT_TRACED_TRACK = -1;

	template <bool trace_circuit = false, bool valid_track_only = false>
	class IncomingAnchorIterator : public RefIterator<GCObject> {
	public:
		IncomingAnchorIterator(GCObject* const node, CircuitScanner* scanner);
	};

	template <bool trace_circuit = false, bool valid_track_only = false>
	class OutgoingLinkIterator : public RefIterator<GCObject> {
	public:
		OutgoingLinkIterator(GCObject* const node, CircuitScanner* scanner);
	};

	class CircuitScanner : public ScanVector {
	public:
		CircuitNode* _damagedCircuit;
		ScanVector _circularNodes;
		SimpleVector<int> _brokenTracks;

		CircuitScanner(CircuitNode* circuit) : _circularNodes(256), _brokenTracks(32) {
			this->_damagedCircuit = circuit;
		}

		int reconstructCoreTrack(GCObject* head, GCObject* tail);

		template <bool inTracingBaseTrack = false>
		void reconstructTrackForward(GCObject* tracingNode);

		template <bool inTracingBaseTrack = false>
		void reconstructTrackBackward(GCObject* tracingNode);

		template <Direction direction>
		void replaceTrackIdToEdge(GCObject* node, int newTrackId) {
			int oldTrackId = node->getTrackId();
			while (node->getTrackId() == oldTrackId) {
				node->setTrackId(newTrackId);
				node = node->getSiblingNodeInTrack<direction>();
			}
		}

		template <Direction direction>
		GCObject* getReachableCoreNode(GCObject* node) {
			while (node != nullptr && node->getTrackId() != NOT_TRACED_TRACK) {
				if (!_damagedCircuit->contains(node) 
				||  node->getTrackId() == CORE_TRACK_ID) {
					return node;
				}
				node = node->getSiblingNodeInTrack<direction, true>();
			}
			return nullptr;
		}


		template <Direction direction>
		bool detectPathToCoreTrack(GCObject* node, GCObject* tracingNode) {
			int old_len = _circularNodes.size();
			while (node != tracingNode) {
				if (!_damagedCircuit->contains(node) 
				||  node->getTrackId() == CORE_TRACK_ID) {
					_circularNodes.resize(old_len);
					return true;
				}
				if (node->getTraceState() == TraceState::IN_TRACING) {
					break;
				}
				_circularNodes.push_back(node);
				node = node->getSiblingNodeInTrack<direction>();
			}
			for (int i = _circularNodes.size(); --i >= old_len; ) {
				_circularNodes.at(i)->setTraceState(TraceState::IN_TRACING);
			}
			return false;
		}

		template <Direction direction, bool inTracingBaseTrack = false>
		bool reconstructTrack(GCObject* tracingNode, RefIterator<GCObject>& ri) {
			const Direction reverse = (direction == Direction::FORWARD) ? Direction::BACKWARD : Direction::FORWARD;
			// debug_printf("\ntracing: %s\n", tracingNode->_id);

			for (; ri.hasNext(); ) {
				GCObject* node = ri.next();
				bool hasPathToCoreTrack = detectPathToCoreTrack<reverse>(node, tracingNode);
				if (hasPathToCoreTrack) {
					int newTrackId;
					tracingNode->setSiblingNodeInTrack<reverse>(node);
					if (node->getSiblingNodeInTrack<direction>() == tracingNode) {
						newTrackId = node->getTrackId();
					}
					else if (inTracingBaseTrack && node->getTrackId() == tracingNode->getTrackId()) {
						newTrackId = _damagedCircuit->createTrackId();
					}
					else {
						return true;
					}
					replaceTrackIdToEdge<direction>(tracingNode, newTrackId);
					return true;
				}
			}

			return false;
		}

		template <Direction direction>
		void makeCoreTrack(GCObject* coreNode, GCObject* derivedNode) {
			const Direction reverse = (direction == Direction::FORWARD) ? Direction::BACKWARD : Direction::FORWARD;

			coreNode->setSiblingNodeInTrack<reverse>(derivedNode);
			GCObject* lastNode = coreNode;
			GCObject* node = derivedNode;
			for (;;) {
				node->setSiblingNodeInTrack<direction>(lastNode);
				node->setTrackId(CORE_TRACK_ID);
				if (node == coreNode) break;

				lastNode = node;
				node = node->getSiblingNodeInTrack<reverse>();
			}
		}

		template <Direction direction, class Iterator>
		GCObject* splitCircuit(GCObject* tracingNode) {
			const Direction reverse = (direction == Direction::FORWARD) ? Direction::BACKWARD : Direction::FORWARD;

			for (int i = 0; ++i < _circularNodes.size(); ) {
				GCObject* node = _circularNodes.at(i);
				Iterator r2(node, this);
				if (r2.hasNext()) {
					if (reconstructTrack<direction>(node, *(RefIterator<GCObject>*) & r2)) {
						return nullptr;
					}
				}
			}

			int circuit_id = _damagedCircuit->splitCircuit(&_circularNodes);
			int brokenTrackId = tracingNode->getTrackId();
			GCObject* nextTrace = tracingNode;
			for (;;) {
				nextTrace = nextTrace->getSiblingNodeInTrack<direction>();
				if (nextTrace->getParentId() != circuit_id || nextTrace->getTrackId() != brokenTrackId) break;
			}
			makeCoreTrack<direction>(tracingNode, _circularNodes.at(1));
			_circularNodes.resize(0);
			return nextTrace;
		}
	};
}

using namespace RTGC;

int CircuitScanner::reconstructCoreTrack(GCObject* head, GCObject* tail) {
	int cntDervivedPath = 0;
	int brokenTrackId = _damagedCircuit->peekNextTrackId();

	for (GCObject* tracingNode = head;; tracingNode = tracingNode->getNextNodeInTrack()) {
		assert (tracingNode->getTrackId() == CORE_TRACK_ID);
		assert (_damagedCircuit->contains(tracingNode));

		IncomingAnchorIterator<false, true> ri(tracingNode, this);
		for (; ri.hasNext(); ) {
			cntDervivedPath ++;
			GCObject* node = ri.next();
			GCObject* edge = getReachableCoreNode<Direction::BACKWARD>(node);
			if (edge == nullptr) {
				continue;
			}
			GCObject* next_node = edge->getNextNodeInTrack();
			tracingNode->setPrevNodeInTrack(node);
			node->setNextNodeInTrack(tracingNode);
			for (;;) {
				GCObject* prevNode = node->getPrevNodeInTrack();
				assert (_damagedCircuit->contains(node));
				prevNode->setNextNodeInTrack(node);
				node->setTrackId(CORE_TRACK_ID);
				if (prevNode == edge) break;
				node = prevNode;
			}

			int newTrackId = _damagedCircuit->createTrackId();
			assert(newTrackId == brokenTrackId);
			if (edge != tail) {
				for (;; next_node = next_node->getNextNodeInTrack()) {
					assert (next_node->getTrackId() == CORE_TRACK_ID);
					assert (_damagedCircuit->contains(next_node));
					next_node->setTrackId(brokenTrackId);
					if (next_node == tail) break;
				}
			}
			return brokenTrackId;
		}

		tracingNode->setTrackId(brokenTrackId);
		if (tracingNode == tail) break;
	}

	if (cntDervivedPath == 0) {
		for (GCObject* tracingNode = head;; tracingNode = tracingNode->getNextNodeInTrack()) {
			assert (tracingNode->getTrackId() == NOT_TRACED_TRACK);
			assert (_damagedCircuit->contains(tracingNode));
			tracingNode->setParent_unsafe(0);
			if (tracingNode == tail) break;
		}
	}
	else {
		_damagedCircuit->rescan(tail);
	}
	delete _damagedCircuit;
	return 0;
}

template <bool inTracingBaseTrack>
void CircuitScanner::reconstructTrackBackward(GCObject* tracingNode) {
	int brokenTrackId = tracingNode->getTrackId();
	_brokenTracks.push_back(brokenTrackId);
	while (tracingNode != nullptr && tracingNode->getTrackId() == brokenTrackId) {
		OutgoingLinkIterator<> ri(tracingNode, this);
		if (!ri.hasNext()) {
			GCObject* nextScan = tracingNode->getPrevNodeInTrack();
			tracingNode->setParent(g_rootNode);
			//tracingNode->setTrackId(-1);
			AnchorIterator ri_drived(tracingNode);
			for (; ri_drived.hasNext(); ) {
				GCObject* junior = ri_drived.next(); 
				if (_damagedCircuit->contains(junior) 
				&&  junior->getNextNodeInTrack() == tracingNode
				&&  !_brokenTracks.contains(junior->getTrackId())) {
					assert(junior != nextScan);
					reconstructTrackBackward(junior);
				}
			}
			tracingNode = nextScan;
		}
		else {
			//int old_len = _circularNodes.size();
			tracingNode->setTraceState(TraceState::IN_TRACING);
			_circularNodes.push_back(tracingNode);
			GCObject** p = ri.getLocation();
			if (reconstructTrack<Direction::BACKWARD, inTracingBaseTrack>(tracingNode, ri)) {
				tracingNode = nullptr;
			}
			else {
				tracingNode = splitCircuit<Direction::BACKWARD, OutgoingLinkIterator<true>>(tracingNode);
			}
			for (int x = _circularNodes.size(); --x >= 0; ) {
				_circularNodes.at(x)->setTraceState(TraceState::NOT_TRACED);
			}
			_circularNodes.resize(0);
		}
	}
	_brokenTracks.resize(_brokenTracks.size() - 1);
}

template <bool inTracingBaseTrack>
void CircuitScanner::reconstructTrackForward(GCObject* tracingNode) {
	int brokenTrackId = tracingNode->getTrackId();
	_brokenTracks.push_back(brokenTrackId);
	while (tracingNode != nullptr && tracingNode->getTrackId() == brokenTrackId) {
		IncomingAnchorIterator<> ri(tracingNode, this);
		if (!ri.hasNext()) {
			GCObject* nextScan = tracingNode->getNextNodeInTrack();
			tracingNode->setParent(g_rootNode);
			//tracingNode->setTrackId(-1);
			tracingNode->visitLinks([](GCObject* anchor, GCObject* link, void* param)->bool {
				CircuitScanner* self = (CircuitScanner*)param;
				GCObject* junior = link;
				if (self->_damagedCircuit->contains(junior)
				&&  junior->getPrevNodeInTrack() == anchor
				&&  !self->_brokenTracks.contains(junior->getTrackId())) {
					assert(junior != anchor->getNextNodeInTrack());
					self->reconstructTrackForward(junior);
				}
				return true;
				}, this);
			tracingNode = nextScan;
		}
		else {
			tracingNode->setTraceState(TraceState::IN_TRACING);
			_circularNodes.push_back(tracingNode);
			if (reconstructTrack<Direction::FORWARD, inTracingBaseTrack>(tracingNode, ri)) {
				tracingNode = nullptr;
			}
			else {
				tracingNode = splitCircuit<Direction::FORWARD, IncomingAnchorIterator<true>>(tracingNode);
			}
			for (int x = _circularNodes.size(); --x >= 0; ) {
				_circularNodes.at(x)->setTraceState(TraceState::NOT_TRACED);
			}
			_circularNodes.resize(0);
		}
	}
	_brokenTracks.resize(_brokenTracks.size() - 1);
}

template <bool trace_circuit, bool valid_track_only>
OutgoingLinkIterator<trace_circuit, valid_track_only>::OutgoingLinkIterator(GCObject* const node, CircuitScanner* scanner) {
	ScanVector* list = (ScanVector*)scanner;
	this->_current = &list->at(list->size());
	node->visitLinks([](GCObject* anchor, GCObject* link, void* param) -> bool {
		CircuitScanner* scanner = (CircuitScanner*)param;
		if (link->getParentId() == anchor->getParentId()) {
			if (!valid_track_only || link->getTrackId() != NOT_TRACED_TRACK) {
				if (!trace_circuit || link->getTraceState() != TraceState::IN_TRACING) {
					scanner->push_back(link);
				}
			}
		}
		return true;
	}, scanner);
	this->_end = &list->at(list->size());
}

template <bool trace_circuit, bool valid_track_only>
IncomingAnchorIterator<trace_circuit, valid_track_only>::IncomingAnchorIterator(GCObject* const node, CircuitScanner* list) {
	int parent_id = node->getParentId();
	this->_current = &list->at(list->size());
	for (AnchorIterator ri(node); ri.hasNext(); ) {
		GCObject* anchor = ri.next();
		if (anchor->getParentId() != parent_id) continue;
		if (valid_track_only && anchor->getTrackId() == NOT_TRACED_TRACK) continue;
		if (!trace_circuit || anchor->getTraceState() != TraceState::IN_TRACING) {
			list->push_back(anchor);
		}
	};
	this->_end = &list->at(list->size());
}


void CircuitNode::reconstructDamagedCyclicNode(int track, GCObject* owner, GCObject* erased) {

	assert(owner->getParentId() == this->getId());
	assert(erased->getParentId() == this->getId());

	CircuitScanner scanner(this);
	if (track == CORE_TRACK_ID) {
		track = scanner.reconstructCoreTrack(erased, owner);
		if (track == 0) return;
	}
	if (owner->getTrackId() == track) {
		scanner.reconstructTrackBackward<true>(owner);
	}
	if (erased->getTrackId() == track) {
		scanner.reconstructTrackForward(erased);
	}
}

int CircuitNode::splitCircuit(ScanVector* children) {
	CircuitNode* new_circuit = new CircuitNode();
	int c_id = new_circuit->getId();
	// int parent_id = CircuitNode::getIndex(new_circuit);
	int this_id = this->getId();

	for (int x = children->size(); --x >= 0; ) {
		GCObject* node = children->at(x);
		assert(node->getTraceState() == TraceState::IN_TRACING);
		assert(node->getParentId() == this_id);
		node->setParent_unsafe(c_id);
		if (node->getRootRefCount() > 0) {
			this->decrementRootRefCount();
			new_circuit->incrementRootRefCount();
		}
	}

	int last_track_id = CORE_TRACK_ID;
	for (int x = children->size(); --x >= 0; ) {
		GCObject* node = children->at(x);
		int track_id = node->getTrackId();
		if (track_id > last_track_id) {
			last_track_id = track_id;
		}
		node->setTraceState(TraceState::NOT_TRACED);
		for (AnchorIterator ai(node); ai.hasNext(); ) {
			GCObject* anchor = ai.next();
			int pid = anchor->getParentId();
			if (pid != c_id) {
				new_circuit->addReferrer(anchor);
				if (pid != this_id) {
					this->removeReferrer(anchor);
				}
			}
		}
        node->visitLinks([](GCObject* anchor, GCObject* link, void* param)->bool {
            CircuitNode* old = (CircuitNode*)param;
            
            if (old->contains(link)) {
                old->addReferrer(anchor);
            }
            return true;    
        }, this);

	}
	new_circuit->setLastTrackId_unsafe(last_track_id);
	return c_id;
}
