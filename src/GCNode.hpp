#include "GCUtils.hpp"
#include "GCPointer.hpp"
#include <atomic>

#if GC_DEBUG
#define debug_printf		printf
#else
#define debug_printf(...)	// do nothing
#endif


#define RTGC_NEW(TYPE, ARGS) RETAINED_PTR<TYPE>::make((new TYPE())->init$ ARGS)

namespace RTGC {

enum class TraceState : int {
	NOT_TRACED,
	IN_TRACING,
	TRACE_FINISHED,
};

enum class Direction : int {
	FORWARD,
	BACKWARD
};

enum class NodeType : int {
	Object,
	Circuit,
	Destroyed,
};


class GCNode;
class GCObject;
class CompositeNode;
class OneWayNode;
class CircuitNode;
class AnchorIterator;


static const int FLAG_PUBLISHED = 0x08;
static const int FLAG_CHECK_CIRCUIT = 0x04;
static const int CORE_TRACK_ID = 1;
static CircuitNode* const g_rootNode = nullptr;

class ReversePath : public SimpleVector<GCObject*> {
public:    
	void init(int initialSize);
};

class GCNode {
	friend class GCRuntime;
	friend class GCObject;
	friend class GCArray;
	friend class GarbageProcessor;
	friend class CircuitNode;
	friend class Anchor_Iterator;
	friend class CircuitDetector;

	uint32_t _refs;
	struct {
		int32_t _rootRefCount: 25;
		uint32_t _traceState: 2;
		uint32_t _isPublished: 1;
		uint32_t _shouldCheckCitcuit: 1;
		uint32_t _hasMultiRef: 1;
		uint32_t _nodeType: 2;
	};

	struct {
		int64_t _parentId: 22;
		int64_t _nextNodeOffset: 22;
		int64_t _trackId: 20;
	};

public:
#ifdef GC_DEBUG
	const char * _id;
#endif

	GCNode(NodeType type) {

		*((int64_t*)this) = 0;
		_nodeType = (int)type;
	} //: _referrers(DoNotInitialize::Flag) {}

	virtual ~GCNode();

	NodeType getNodeType() {
		return (NodeType)_nodeType;
	}

	bool isCircuit() {
		return getNodeType() == NodeType::Circuit;
	}

	bool isComposite() {
		return getNodeType() != NodeType::Object;
	}

	CircuitNode* toCircuitNode() {
		return getNodeType() == NodeType::Circuit ? (CircuitNode*)this: nullptr;
	}

	bool isGarbage() {
		return this->_rootRefCount < 0 && !this->hasReferrer(); // this->_externalReferrers->empty();
	}

	bool isPublished() {
		return this->_isPublished;
	}

	bool markPublished() {
		if (!this->isPublished()) {
			this->_isPublished = true;
			return true;
		}
		return false;
	}

	bool shouldCheckCircuit() {
		return this->_shouldCheckCitcuit;
	}

	bool markCheckCircuit() {
		if (!shouldCheckCircuit()) {
			this->_shouldCheckCitcuit = true;
			return true;
		}
		return false;
	}

	TraceState getTraceState() {
		return (TraceState)_traceState;
	}

	void setTraceState(TraceState state) {
		_traceState = (int)state;
	}


	bool inCircuit() {
		return _parentId != 0;
	}


	bool hasReferrer() {
		return this->_refs != 0;
	}

	GCObject* getLinearReferrer();

	int getRootRefCount() {
		return this->_rootRefCount;
	}

	int incrementRootRefCount() {
		return ++this->_rootRefCount;
	}

	int decrementRootRefCount() {
		return --this->_rootRefCount;
	}

	void initIterator(AnchorIterator* iterator);

	int getParentId() {
		return _parentId;
	}

protected:
	ReversePath* getPathInfo();

	void markDestroyed() {
		this->_nodeType = (int)NodeType::Destroyed;
	}

	bool isDestroyed() {
		return this->_nodeType == (int)NodeType::Destroyed;;
	}

};


class CompositeNode : public GCNode {
protected:
	int _lastTrackId;
	int _circuit_id;

public:
	CompositeNode(NodeType nodeType) : GCNode(nodeType) {
		decrementRootRefCount();
	}

	int getId() {
		return _circuit_id;
	}

	bool contains(GCObject* obj) {
		return ((GCNode*)obj)->getParentId() == _circuit_id;
	}
};

typedef SimpleVector<GCObject*> ScanVector;

class CircuitNode : public CompositeNode {

	friend class CircuitScanner;

	void setLastTrackId_unsafe(int id) {
		this->_lastTrackId = id;
	}

	int splitCircuit(ScanVector* children);

public:

	CircuitNode() : CompositeNode(NodeType::Circuit) {
		_circuit_id = getIndex(this);
		_hasMultiRef = true;
#ifdef GC_DEBUG
		this->_id = "Circuit";
#endif
	}

	CircuitNode* toCircuitNode() {
		return this;
	}

	bool isCyclic() { return true; }


	int createTrackId() {
		return (_lastTrackId += CORE_TRACK_ID);
	}

	int peekNextTrackId() {
		return (_lastTrackId + CORE_TRACK_ID);
	}

	void mergeInto(CircuitNode* coreCircuit, int trackId, GCObject* start, GCObject* enter, GCObject* leave, GCObject* end);

	void migrateSubTrack(CircuitNode* coreCircuit, GCObject* tail, GCObject* start);

	void rebuildTrack(CircuitNode* coreCircuit, int trackId, GCObject* tail);

	static int getIndex(CircuitNode* circuit);

	static CircuitNode* getPointer(int idx);

	void* operator new (std::size_t size);

	void operator delete(void* ptr);

	void addReferrer(GCObject* referrer);

	void removeReferrer(GCObject* referrer);

	bool removeAllReferrer(GCObject* referrer);

	void rescan(GCObject* node);

	static void detectCircularPathAndConstructCylicNode(GCObject* node);

	void reconstructDamagedCyclicNode(int track, GCObject* referrer, GCObject* referent);

	void makeShortCutInCircuitNode(GCObject* start, GCObject* end);
};


class GCObject : public GCNode {
	static uint16_t* const _empty_offsets;
public:
	typedef RC_PTR<GCObject> PTR;
	typedef RC_POINTER_ARRAY<GCObject> ARRAY;

	GCObject(NodeType nodeType = NodeType::Object) : GCNode(nodeType) {
	}

	GCObject* init$() {
		return this;
	}

	CircuitNode* getParentCircuit();

	void setParent(CircuitNode* parent);


	int getParentId() {
		return _parentId;
	}

	int getTrackId() {
		return this->_trackId;
	}

	void setTrackId(int trackId) {
		this->_trackId = trackId;
	}
	//  {
	// 	this->_pathType = PathType::InCircuit();
	// 	this->_trackId = trackId;
	// }


	GCObject* getPrevNodeInTrack();

	void setPrevNodeInTrack(GCObject* prevNode);

	bool removeReferrerInTrack(GCObject* prevNode);

	template <Direction direction, bool mustHasSibling = true>
	GCObject* getSiblingNodeInTrack() {
		return direction == Direction::FORWARD
			? getNextNodeInTrack()
			: (mustHasSibling || this->hasReferrer()) ? getPrevNodeInTrack() : nullptr;
	}

	GCObject* getNextNodeInTrack();

	template <Direction direction>
	void setSiblingNodeInTrack(GCObject* sibling) {
		if (direction == Direction::FORWARD) {
			setNextNodeInTrack(sibling);
		}
		else {
			setPrevNodeInTrack(sibling);
		}
	}

	void* operator new (size_t size) {
		void* ptr = rtgc_calloc(size);
		return ptr;
	}

	void setParent_unsafe(int parentId) {
		this->_parentId = parentId;
	}



	GCObject* getLinkInside(CircuitNode* container);

	virtual uint16_t* getFieldOffsets() {
		return _empty_offsets;
	};

	void setNextNodeInTrack(GCObject* nextNode);

	virtual ~GCObject() {}

	bool visitLinks(LinkVisitor visitor, void* callbackParam);

	void publishInstance();
	
	void addReferrer(GCObject* referrer);

	void removeReferrer(GCObject* referrer);

	bool removeAllReferrer(GCObject* referrer);

};


class GCArray : public GCObject {
	friend GCObject;
protected:
	int _cntItem;

	GCArray(int cntItem) {
		_cntItem = cntItem;
	}



	static void* alloc_internal(int length, int item_size);

	void* operator new (std::size_t size, void* ptr) {
		return ptr;
	}

	bool visitLinks(LinkVisitor visitor, void* callbackParam);

	void setNextNodeInTrack(GCObject* nextNode);

public:
	static uint16_t* const _array_offsets;

	int length() {
		return _cntItem;
	}

};

template <class T>
class GCObjectArray : public GCArray {
	OffsetPointer<T> _items[32];
public:
	GCObjectArray(int cntItem) : GCArray(cntItem) {
#ifdef GC_DEBUG
		this->_id = "Object-Array";
#endif
	}

	virtual uint16_t* getFieldOffsets() {
		return _array_offsets;
	}

	OffsetPointer<T>* getItemPointer(size_t index) {
		if (index >= _cntItem) throw "illegal index";
		return _items + index;
	}

	static RETAINED_PTR<GCObjectArray> alloc(int length) {
		void* ptr = alloc_internal(length, sizeof(OffsetPointer<T>));
		GCObjectArray* array = new(ptr)GCObjectArray(length);
		return RETAINED_PTR<GCObjectArray>::make(array);
	}

};

template <class T>
class GCPrimitiveArray : public GCArray {
protected:
	T _items[32];

	GCPrimitiveArray(int cntItem) : GCArray(cntItem) {
#ifdef GC_DEBUG
		this->_id = "Primitive-Array";
#endif
	}

	bool visitLinks(LinkVisitor visitor, void* callbackParam) {
		return true;
	}

public:

	T* getItemPointer(size_t index) {
		if (index >= _cntItem) throw "illegal index";
		return _items + index;
	}

	static RETAINED_PTR<GCPrimitiveArray> alloc(int length) {
		void* ptr = alloc_internal(length, sizeof(T));
		GCPrimitiveArray* array = new(ptr)GCPrimitiveArray(length);
		return RETAINED_PTR<GCPrimitiveArray>::make(array);
	}
};


class AnchorIterator : public RefIterator<GCObject> {
public:
	AnchorIterator(GCNode* const node) {
		node->initIterator(this);
	}
};

class LinkIterator {
	union {
		GCObject* _anchor;
		GCObjectArray<GCObject>* _array;
	};
	union {
		const uint16_t* _offsets;
		intptr_t _idxField;
	};

public:
	LinkIterator(GCObject* obj);

	GCObject* next();

	GCObject* getContainerObject() { 
		return _anchor;
	}
};


static inline bool isRoot(GCNode* node) { return node == nullptr; }

static inline bool isRoot(int id) { return id == 0; }

static inline bool contains(int parentId, GCObject* obj) {
	return obj->getParentId() == parentId;
}

}