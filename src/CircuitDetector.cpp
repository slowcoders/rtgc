#include "GCNode.hpp" 
#include <memory.h>

#define USE_ITERATOR_STACK false
#define USE_ANCHOR_VISIOR true
namespace RTGC {

	class CircuitDetector;
	typedef SimpleVector<GCNode*> NodeList;

	MemoryPool<CircuitNode, 64*1024*1024, sizeof(void*)> g_circuitPool;
	THREAD_LOCAL CircuitDetector* pCircuitDetector = nullptr;


	class CircuitDetector {

	public:

		StdVector<GCObject*> _tracingList;
		StdVector<GCNode*> _tracedList;
		int _damagedCircuitId;

		CircuitDetector() {}

		void init(GCNode* from, GCNode* target) {
			assert(_tracedList.size() == 0);
			assert(_tracingList.size() == 0);
		}

		void clear();

		void addTracingList(GCObject* tracingNode, GCNode* refNode);

		template <bool traceDamagedCircleOnly>
		void traceAnchor(GCObject* anchor, GCNode* link);

		template <bool traceDamagedCircleOnly>
		bool detectCircularPath(GCNode* tracingNode);

		static GCNode* getLargestReferrer(GCObject* referrer);

		CircuitNode* addCyclicTrack(GCObject* prevNode, int* circuit_start);
	};

	CircuitDetector g_circuitDetector;
}

using namespace RTGC;

#if GC_DEBUG
int GCRuntime::getCircuitCount() {
	return g_circuitPool.getAllocationCount();
}
#endif

void CircuitDetector::addTracingList(GCObject* tracingNode, GCNode* refNode) {
	refNode->setTraceState(TraceState::IN_TRACING);
	_tracedList.push_back(refNode);
	_tracingList.push_back(tracingNode);
}

GCNode* CircuitDetector::getLargestReferrer(GCObject* referrer) {
	GCNode* parent = referrer->getParentCircuit();
	return (parent != nullptr) ? parent : referrer;
}

template <bool traceDamagedCircleOnly>
void CircuitDetector::traceAnchor(
	GCObject* anchor,
	GCNode* link
) {
	GCNode* referrer_node = traceDamagedCircleOnly ? anchor : getLargestReferrer(anchor);
	if (traceDamagedCircleOnly) {
		anchor->setParent_unsafe(0);
	}
	if (!referrer_node->hasReferrer()) return;
	
	switch (referrer_node->getTraceState()) {
	case TraceState::IN_TRACING: {
		if (!link->inCircuit() || link->getParentId() != anchor->getParentId()) {
			// TODO: delayed circuit creattion to avoid multi-circuit-merge
			int idx_start;
			auto circuit = addCyclicTrack(anchor, &idx_start);
			circuit->setTraceState(TraceState::IN_TRACING);
			_tracingList.resize(idx_start + 1);
			_tracedList.push_back(circuit);
		}
		break;
	}
	case TraceState::NOT_TRACED: {
		int p_id = anchor->getParentId();
		this->addTracingList(anchor, referrer_node);
		size_t trace_depth = _tracingList.size();
		detectCircularPath<traceDamagedCircleOnly>(referrer_node);
		assert(_tracingList.size() <= trace_depth);
		if (_tracingList.size() == trace_depth) {
			getLargestReferrer(anchor)->setTraceState(TraceState::TRACE_FINISHED);
			_tracingList.pop_back();
		}
		break;
	}
	case TraceState::TRACE_FINISHED: default:
		break;
	}
	return;
}

template <bool traceDamagedCircleOnly>
bool CircuitDetector::detectCircularPath(
	GCNode* tracingNode
) {
	GCObject* anchor = tracingNode->getLinearReferrer();
	if (anchor != nullptr) {
		if (!traceDamagedCircleOnly || tracingNode->getParentId() == _damagedCircuitId) {
			traceAnchor<traceDamagedCircleOnly>(anchor, tracingNode);
		}
	}
	else {
		ReversePath* path = tracingNode->getPathInfo();
		GCObject** anchors = &path->at(0);
		for (int len = path->size(); --len >= 0;) {
			anchor = *anchors++;
			if (!traceDamagedCircleOnly || tracingNode->getParentId() == _damagedCircuitId) {
				traceAnchor<traceDamagedCircleOnly>(anchor, tracingNode);
			}
		}
	}
	return true;
}

CircuitNode* CircuitDetector::addCyclicTrack(GCObject* startNode, int* circuit_start) {
	StdVector<GCObject*>* backwardCyclicPath = &_tracingList;
	const int end_of_circuit = (int)backwardCyclicPath->size();
	GCObject* prevNode = startNode;
	GCNode* endNode =  prevNode->getParentCircuit();
	CircuitNode* circuit = nullptr;
	int c_id = -1;
	int trackId;
	if (endNode == nullptr) {
		endNode = prevNode;
	}

	int idx = end_of_circuit;
	bool prevNodeInCoreCircuit = false;
	for (;;) {
		GCObject* node = backwardCyclicPath->at(--idx);
		CircuitNode* c = node->getParentCircuit();
		if (c != nullptr) {
			GCObject* link = prevNode->getLinkInside(c);
			if (circuit == nullptr) {
				circuit = c;
				trackId = circuit->createTrackId();
				c_id = c->getId();
				prevNodeInCoreCircuit = true;
				prevNode->setNextNodeInTrack(link);
			}
			else {
				c->mergeInto(circuit, trackId, prevNode, link, node, nullptr);
				if (prevNodeInCoreCircuit) {
					prevNodeInCoreCircuit = false;
					link->setPrevNodeInTrack(prevNode);
				}
				else {
					prevNode->setNextNodeInTrack(link);
				}
			}
			if (c == endNode) {
				break;
			}
			prevNode = node;
		}
		else {
			if (prevNodeInCoreCircuit) {
				prevNodeInCoreCircuit = false;
			}
			else {
				prevNode->setNextNodeInTrack(node);
			}
			node->setPrevNodeInTrack(prevNode);
			prevNode = node;
		}
		if (prevNode == endNode) {
			break;
		}
	}

	if (circuit == nullptr) {
		circuit = new CircuitNode();
		trackId = circuit->createTrackId();
		c_id = circuit->getId();
	}
	*circuit_start = idx;

	prevNode = startNode;
	for (int i = end_of_circuit; --i >= idx; ) {
		GCObject* node = backwardCyclicPath->at(i);
		if (node->getParentId() != c_id) {
			node->setParent(circuit);
			node->setTrackId(trackId);
		}
	}
	return circuit;
}




void CircuitDetector::clear() {
	if (_tracedList.size() == 0) return;

	GCNode** pItems = &_tracedList.at(0);
	for (int i = (int)_tracedList.size(); --i >= 0; ) {
		GCNode* obj = *pItems ++;
		obj->setTraceState(TraceState::NOT_TRACED);
	}
	_tracedList.resize(0);
	_tracingList.resize(0);
}

GCObject* makeReversePathFromCore(CircuitNode* circuit, GCObject* node) {
	assert(node->getTrackId() > CORE_TRACK_ID);
	assert(circuit->contains(node));
	while (true) {
		node->setTrackId(0);

		GCObject* next = node;
		node = node->getPrevNodeInTrack();
		node->setNextNodeInTrack(next);
		if (node->getTrackId() <= CORE_TRACK_ID
		||  !circuit->contains(node)) {
			break;
		}
	}
	return node;
}

// #######################################################################
void CircuitNode::mergeInto(CircuitNode* coreCircuit, int trackId, GCObject* start, GCObject* enter, GCObject* leave, GCObject* end) {
	int this_id = this->getId();
	GCObject* coreNode;
	if (leave->getTrackId() != CORE_TRACK_ID) {
		coreNode = makeReversePathFromCore(this, leave);
	}
	else {
		coreNode = leave;
	}

	StdVector<GCObject*> coreNodes;
	coreNodes.push_back(coreNode);
	for (GCObject* node = coreNode->getPrevNodeInTrack(); node != coreNode; ) {
		coreNodes.push_back(node);
		node = node->getPrevNodeInTrack();
	}

	GCObject* prev = start;
	for (GCObject* obj = enter;; ) {
		obj->setPrevNodeInTrack(prev);
		obj->setParent(coreCircuit);
		obj->setTrackId(trackId);
		if (obj == leave) break;
		prev = obj;
		obj = prev->getNextNodeInTrack();
	}

#if GC_DEBUG
	for (GCObject* obj = enter;; ) {
		assert(obj->getParentCircuit() == coreCircuit);
		assert(obj->getTrackId() == trackId);
		if (obj == leave) break;
		obj = obj->getNextNodeInTrack();
	}
	for (GCObject* obj = leave; obj != start; ) {
		assert(obj->getParentCircuit() == coreCircuit);
		assert(obj->getTrackId() == trackId);
		obj = obj->getPrevNodeInTrack();
	}
#endif

	this->migrateSubTrack(coreCircuit, leave, start);

	delete this;
}

void CircuitNode::migrateSubTrack(CircuitNode* coreCircuit, GCObject* tail, GCObject* start) {
	for (GCObject* node = tail; node != start; ) {
		for (AnchorIterator ri(node); ri.hasNext(); ) {
			GCObject* anchor = ri.next();
			if (!this->contains(anchor) || anchor->getTrackId() <= CORE_TRACK_ID) {
				continue;
			}
			anchor->setNextNodeInTrack(node);
			makeReversePathFromCore(this, anchor);
		}
		node = node->getPrevNodeInTrack();
	}
	for (GCObject* node = tail; node != start; ) {
		for (LinkIterator ri(node);; ) {
			GCObject* link = ri.next();
			if (link == nullptr) break;

			if (!this->contains(link)) continue;

			int trackId = coreCircuit->createTrackId();
			GCObject* prev = node;
			for (GCObject* obj = link;; ) {
				obj->setPrevNodeInTrack(prev);
				obj->setParent(coreCircuit);
				obj->setTrackId(trackId);
				prev = obj;
				obj = prev->getNextNodeInTrack();
				if (coreCircuit->contains(obj)) break;
			}
			migrateSubTrack(coreCircuit, prev, node);
		}
		node = node->getPrevNodeInTrack();
	}

	#if GC_DEBUG
		// GCObject* head = anchor;
		// for (GCObject* obj = anchor;; ) {
		// 	obj->getParentCircuit() == coreCircuit;
		// 	obj->getTrackId() == trackId;
		// 	head = obj;
		// 	obj = obj->getPrevNodeInTrack();
		// 	if (obj->getTrackId() != trackId) break;
		// }
		// for (GCObject* obj = head; obj != node; ) {
		// 	obj->getParentCircuit() == coreCircuit;
		// 	obj->getTrackId() == trackId;
		// 	obj = obj->getNextNodeInTrack();
		// }
	#endif


	// for (GCObject* node = tail; node != start; ) {
	// 	for (AnchorIterator ri(node); ri.hasNext(); ) {
	// 		GCObject* anchor = ri.next();
	// 		if (this->contains(anchor)) {
	// 			anchor->setNextNodeInTrack(node);
	// 			int trackId = coreCircuit->createTrackId();
	// 			rebuildTrack(coreCircuit, trackId, anchor);
	// 		}
	// 	}
	// 	node = node->getPrevNodeInTrack();
	// }
}

void CircuitNode::rebuildTrack(CircuitNode* coreCircuit, int trackId, GCObject* obj) {
	bool splitTrack = false;
	obj->setParent(coreCircuit);
	obj->setTrackId(trackId);
	for (AnchorIterator ri(obj); ri.hasNext(); ) {
		GCObject* anchor = ri.next();
		if (this->contains(anchor)) {
			anchor->setNextNodeInTrack(obj);
			if (!splitTrack) {
				splitTrack = true;
				obj->setPrevNodeInTrack(anchor);
				rebuildTrack(coreCircuit, trackId, anchor);
			}
			else {
				int trackId = coreCircuit->createTrackId();
				rebuildTrack(coreCircuit, trackId, anchor);
			}
		}
		else if (!splitTrack && coreCircuit->contains(anchor)) {
			splitTrack = true;
			obj->setPrevNodeInTrack(anchor);
		}
	}
}

void CircuitNode::makeShortCutInCircuitNode(GCObject* start, GCObject* end) {
	assert(start->getParentId() == this->getId() && end->getParentId() == this->getId());

	if (start->getNextNodeInTrack() == end) {
		return;
	}

	int track_id = start->getTrackId();
	int track_id_next = end->getTrackId();
	if (track_id == CORE_TRACK_ID || track_id != track_id_next) return;

	bool isForward = false;
	for (GCObject* n = start; n->getTrackId() == track_id; ) {
		n = n->getNextNodeInTrack();
		if (n == end) {
			isForward = true;
			break;
		}
	}

	int trackId = this->createTrackId();

	for (GCObject* s = start->getNextNodeInTrack(); s != end; ) {
		s->setTrackId(trackId);
		s = s->getNextNodeInTrack();
	}
	start->setNextNodeInTrack(end);
	end->setPrevNodeInTrack(start);

}

void CircuitNode::rescan(
	GCObject* node
) {
	g_circuitDetector.init(node, node);
	g_circuitDetector._damagedCircuitId = this->getId();
	g_circuitDetector.traceAnchor<true>(node, nullptr);
	g_circuitDetector.clear();
}

void CircuitNode::detectCircularPathAndConstructCylicNode(
	GCObject* node
) {
	g_circuitDetector.init(node, node);
	#if USE_ANCHOR_VISIOR
	g_circuitDetector.traceAnchor<false>(node, nullptr);
	#else
	g_circuitDetector.detectCircularPath(node);
	#endif
	g_circuitDetector.clear();
}


CircuitNode* CircuitNode::getPointer(int idx) {
	return g_circuitPool.getPointer(idx);
}

int CircuitNode::getIndex(CircuitNode* circuit) {
	return circuit == nullptr ? 0 : g_circuitPool.getIndex(circuit);
}

void CircuitNode::operator delete(void* ptr) {
	g_circuitPool.delete_((CircuitNode*)ptr);
}


void* CircuitNode::operator new (std::size_t size) {
	CircuitNode* circuit = g_circuitPool.allocate();
	return circuit;
}

