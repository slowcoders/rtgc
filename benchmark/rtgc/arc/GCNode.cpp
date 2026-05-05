#include "GCNode.hpp"
#include <memory.h>

namespace RTGC {

    struct TinyChunk {
        int32_t _size;
        int32_t _capacity;
        TinyChunk* _items[2];
    };
    const static bool USE_TINY_MEM_POOL = true;
    const static bool IS_MULTI_LAYER_NODE = false;
    MemoryPool<TinyChunk, 64*1024*1024, -1> gRefListPool;
    MemoryPool<ReversePath, 64*1024*1024, 0> gTrackPool;

};

#define _offset2Object(offset, origin) (GCObject*)_offset2Pointer(offset, origin)
using namespace RTGC;

void ReversePath::init(int initialSize) {
    TinyChunk* chunk = gRefListPool.allocate();;
    *(void**)this = chunk;
    chunk->_capacity = (int)sizeof(TinyChunk) / sizeof(void*) -1;
    chunk->_size = initialSize;
    assert(chunk->_size <= chunk->_capacity);
}

#if GC_DEBUG
int GCRuntime::getTinyChunkCount() {
    return gRefListPool.getAllocationCount();
}

int GCRuntime::getPathInfoCount() {
    return gTrackPool.getAllocationCount();
}
#endif

void GCRuntime::dumpDebugInfos() {
    #if GC_DEBUG
    printf("Circuit: %d, TinyChunk: %d, PathInfo: %d\n", 
        getCircuitCount(), getTinyChunkCount(), getPathInfoCount());
    #endif
}


void* DefaultAllocator::alloc(size_t size) {
    if (USE_TINY_MEM_POOL && size <= sizeof(TinyChunk)) {
        return gRefListPool.allocate();
    }
    else {
        void * mem = ::malloc(size);
        if (mem == nullptr) throw "OutOfMemoryError:E001";
        return mem;
    }
}

void* DefaultAllocator::realloc(void* mem, size_t size) {
    if (USE_TINY_MEM_POOL && gRefListPool.isInside(mem)) {
        if (size <= sizeof(TinyChunk)) {
            return mem;
        }
        void* new_mem = ::malloc(size);
        if (new_mem == nullptr) throw "OutOfMemoryError:E002";
        memcpy(new_mem, mem, sizeof(TinyChunk));
        gRefListPool.delete_((TinyChunk*)mem);
        return new_mem;
    }
    else {
        void* mem2 = ::realloc(mem, size);
        if (mem2 == nullptr) throw "OutOfMemoryError:E003";
        return mem2;
    }
}

void DefaultAllocator::free(void* mem) {
    if (USE_TINY_MEM_POOL && gRefListPool.isInside(mem)) {
        gRefListPool.delete_((TinyChunk*)mem);
    }
    else {
        ::free(mem);
    }
}

GCObject* GCObject::getPrevNodeInTrack() {
    assert(hasReferrer()); 
    GCObject* front;
    if (this->_hasMultiRef) {
        ReversePath* path = getPathInfo();
        front = path->front();
    }
    else {
        front = _offset2Object(_refs, &_refs);
    }
    return front;
}

void GCObject::setPrevNodeInTrack(GCObject* prevNode) {
    assert(hasReferrer()); 
    if (this->_hasMultiRef) {
        ReversePath* path = getPathInfo();
        int idx = path->indexOf(prevNode);
        assert(idx >= 0);
        if (idx != 0) {
            GCObject* tmp = path->at(0);
            path->at(0) = prevNode;
            path->at(idx) = tmp;
        }
    }
    else {
        GCObject* front = _offset2Object(_refs, &_refs);
        assert(front == prevNode);
    }
}

bool GCObject::removeReferrerInTrack(GCObject* prevNode) {
    assert(hasReferrer()); 
    if (this->_hasMultiRef) {
        ReversePath* path = getPathInfo();
        int idx = path->indexOf(prevNode);
        assert(idx >= 0);
        path->removeFast(idx);
        return idx == 0;
    }
    else {
        GCObject* front = _offset2Object(_refs, &_refs);
        assert(front == prevNode);
        this->_refs = 0;
        return true;
    }
}

GCObject* GCNode::getLinearReferrer() {
    assert(_refs != 0);
    if (!_hasMultiRef) {
        GCObject* obj = _offset2Object(_refs, &_refs);
        return obj;
    }
    else {
        return nullptr;
    }
}

GCNode::~GCNode() {
    if (_hasMultiRef && _refs != 0) {
        ReversePath* path = getPathInfo();
        gTrackPool.delete_(path);
    }
}



void GCNode::initIterator(AnchorIterator* iterator) {
    if (!hasReferrer()) {
        iterator->_current = iterator->_end = nullptr;
    }
    else if (!_hasMultiRef) {
        iterator->_temp = _offset2Object(_refs, &_refs);
        iterator->_current = &iterator->_temp;
        iterator->_end = iterator->_current + 1;
    }
    else {
        ReversePath* path = getPathInfo();
        iterator->_current = &path->at(0);
        iterator->_end = iterator->_current + path->size();
    }
}

void GCObject::addReferrer(GCObject* referrer) {
    if (!hasReferrer()) {
        this->_refs = _pointer2offset(referrer, &_refs);
        GCObject* obj = _offset2Object(_refs, &_refs);
        assert(obj == referrer);
    }
    else {
        ReversePath* path;
        if (!_hasMultiRef) {
            path = gTrackPool.allocate();
            path->init(2);
            GCObject* front = _offset2Object(_refs, &_refs);
            path->at(0) = front;
            path->at(1) = referrer;
            _refs = gTrackPool.getIndex(path);
            _hasMultiRef = true;
        }
        else {
            path = getPathInfo();
            path->push_back(referrer);
        }
    }
}

void GCObject::removeReferrer(GCObject* referrer) {
    assert(hasReferrer());

    if (!_hasMultiRef) {
        assert(_refs == _pointer2offset(referrer, &_refs));
        this->_refs = 0;
    }
    else {
        ReversePath* path = getPathInfo();
        int idx = path->indexOf(referrer);
        assert(idx >= 0);
        if (path->size() == 2) {
            GCObject* remained = path->at(1 - idx);
            gTrackPool.delete_(path);
            this->_refs = _pointer2offset(remained, &_refs);
            _hasMultiRef = false;
        }
        else {
            path->removeFast(idx);
        }
    }
}

bool GCObject::removeAllReferrer(GCObject* referrer) {
    ReversePath* path;
    if (!_hasMultiRef) {
        if (_refs != 0 && _refs == _pointer2offset(referrer, &_refs)) {
            this->_refs = 0;
            return true;
        }
    }
    else {
        path = gTrackPool.getPointer(_refs);
        if (path->removeAll(referrer)) {
            if (path->size() <= 1) {
                if (path->size() == 0) {
                    gTrackPool.delete_(path);
                    this->_refs = 0;
                }
                else {
                    GCObject* remained = path->at(0);
                    gTrackPool.delete_(path);
                    this->_refs = _pointer2offset(remained, &_refs);
                }
                this->_hasMultiRef = false;
            }
            return true;
        }
    }
    return false;
}

void CircuitNode::addReferrer(GCObject* referrer) {
    ReversePath* path;
    if (!hasReferrer()) {
        path = gTrackPool.allocate();
        path->init(0);
        _refs = gTrackPool.getIndex(path);
        this->_hasMultiRef = true;
    }
    else {
        path = getPathInfo();
    }
    path->push_back(referrer);
}

void CircuitNode::removeReferrer(GCObject* referrer) {
    assert(hasReferrer());
    ReversePath* path = getPathInfo();
    if (path->size() == 1) {
        assert(path->at(0) == referrer);
        gTrackPool.delete_(path);
        this->_refs = 0;
    }
    else {
        path->remove(referrer);
    }
}

bool CircuitNode::removeAllReferrer(GCObject* referrer) {
    ReversePath* path;
    if (!hasReferrer()) {
        return false;
    }
    path = gTrackPool.getPointer(_refs);
    if (path->removeAll(referrer)) {
        if (path->size() == 0) {
            gTrackPool.delete_(path);
            this->_refs = 0;
        }
        return true;
    }
    return false;
}

//---

ReversePath* GCNode::getPathInfo() {
    assert(_hasMultiRef);
    return gTrackPool.getPointer(_refs);
}

CircuitNode* GCObject::getParentCircuit() {
    int p_id = this->getParentId();
    return p_id == 0 ? nullptr : CircuitNode::getPointer(p_id);
}

void GCObject::setParent(CircuitNode* newParent) {

    CircuitNode* old = this->getParentCircuit();
    if (old == newParent) return;

    this->_parentId = CircuitNode::getIndex(newParent);

    if (!isRoot(old)) {
        if (this->_rootRefCount >= 0) {
            old->decrementRootRefCount();
        }

        for (AnchorIterator ri(this); ri.hasNext(); ) {
            GCObject* obj = ri.next();
            if (!old->contains(obj)) {
                old->removeReferrer(obj);
            }
        }
        
        this->visitLinks([](GCObject* anchor, GCObject* link, void* param)->bool {
            CircuitNode* old = (CircuitNode*)param;
            
            if (old->contains(link)) {
                old->addReferrer(anchor);
            }
            return true;    
        }, old);
    }

    if (!isRoot(newParent)) {
        if (this->_rootRefCount >= 0) {
            newParent->incrementRootRefCount();
        }

        for (AnchorIterator ri(this); ri.hasNext(); ) {
            GCObject* obj = ri.next();
            if (!newParent->contains(obj)) {
                newParent->addReferrer(obj);
            }
        }

        for (AnchorIterator ri(newParent); ri.hasNext(); ) {
            GCObject* obj = ri.next();
            if (this == obj) {
                newParent->removeReferrer(obj);
            }
        }
    }
}



