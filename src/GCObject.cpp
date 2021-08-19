#include "GCNode.hpp"
#include <memory.h>

using namespace RTGC;

namespace RTGC {
    const static int MAX_CHILD_PER_ONE_WAY_REGION = 64;
    std::atomic<int> g_mv_lock(0);
    static uint64_t array_offsets = 0;
    static uint64_t empty_offsets = 0;
    uint16_t* const GCArray::_array_offsets = (uint16_t*)&array_offsets;
    uint16_t* const GCObject::_empty_offsets = (uint16_t*)&empty_offsets;
}

GCObject* GCObject::getNextNodeInTrack() {
    assert(_nextNodeOffset != 0);
    OffsetPointer<GCObject>* field = 
        (OffsetPointer<GCObject>*)((uintptr_t)this + _nextNodeOffset);
    return field->getPointer();
}

void GCObject::setNextNodeInTrack(GCObject* nextNode) {
    uint16_t* offsets = this->getFieldOffsets();
    if (offsets == GCArray::_array_offsets) {
        GCArray* array = (GCArray*)this;
        OffsetPointer<GCObject>* field = (OffsetPointer<GCObject>*)(array + 1);
        for (int idx = array->_cntItem; --idx >= 0; field++) {
            GCObject* ref = field->getPointer();
            if (ref == nextNode) {
                _nextNodeOffset = idx;
                return;
            }
        }
    }
    else {
        for (int offset; (offset = *offsets) != 0; offsets ++) {
            OffsetPointer<GCObject>* field = 
                (OffsetPointer<GCObject>*)((uintptr_t)this + offset);
            GCObject* ref = field->getPointer();
            if (ref == nextNode) {
                _nextNodeOffset = offset;
                return;
            }
        }
    }
    assert("should not be here" == 0);
}

GCObject* GCObject::getLinkInside(CircuitNode* container) {
    int sibling_id = CircuitNode::getIndex(container);
    uint16_t* offsets = this->getFieldOffsets();
    if (offsets == GCArray::_array_offsets) {
        GCArray* array = (GCArray*)this;
        OffsetPointer<GCObject>* field = (OffsetPointer<GCObject>*)(array + 1);
        for (int idx = array->length(); --idx >= 0; field++) {
            GCObject* ref = field->getPointer();
            if (ref != nullptr && ref->getParentId() == sibling_id) {
                return ref;
            }
        }
    }
    else {
        for (int offset; (offset = *offsets) != 0; offsets ++) {
            OffsetPointer<GCObject>* field = 
                (OffsetPointer<GCObject>*)((uintptr_t)this + offset);
            GCObject* ref = field->getPointer();
            if (ref != nullptr && ref->getParentId() == sibling_id) {
                return ref;
            }
        }
    }
    return nullptr;
}


void GCRuntime::connectReferenceLink(
    GCObject* assigned, 
    GCObject* owner 
) {
    assigned->addReferrer(owner);

    CircuitNode* p;
    if (!isRoot(p = assigned->getParentCircuit())) {
        if (p->contains(owner)) {
    		// p->makeShortCutInCircuitNode(owner, assigned);
            return;
        }
        p->addReferrer(owner);
    }

    if (owner->hasReferrer()) {
        //owner->markCheckCircuit();
        CircuitNode::detectCircularPathAndConstructCylicNode(owner);//Node, assignedNode);
    }
}


void GCRuntime::disconnectReferenceLink(
    GCObject* erased, 
    GCObject* owner 
) {
    GCNode* erasedNode;

    if (erased->inCircuit()) {
        CircuitNode* circuit = erased->getParentCircuit();
        if (!circuit->contains(owner)) {
            erased->removeReferrer(owner);
            circuit->removeReferrer(owner);
            erasedNode = circuit;
        }
        else {
            bool isPrevNodeRemoved = erased->removeReferrerInTrack(owner);
            int trackId;
            if (isPrevNodeRemoved) {
                trackId = erased->getTrackId();
            } 
            else if (owner->getNextNodeInTrack() == erased) {
                trackId = owner->getTrackId();
            }
            else {
                return;
            }
            ((CircuitNode*)circuit)->reconstructDamagedCyclicNode(trackId, owner, erased);
            erasedNode = erased->inCircuit() ? (GCNode*)erased->getParentCircuit() : erased;
        }
    }
    else {
        erased->removeReferrer(owner);
        erasedNode = erased;
    }

    if (erasedNode->isGarbage()) {
        reclaimGarbage(erased, erasedNode);
    }
}

void GCRuntime::onAssignRootVariable_internal(GCObject* assigned) {
    if (assigned->incrementRootRefCount() == 0) {
        GCNode* parent = assigned->getParentCircuit();
        if (parent != nullptr) {
            parent->incrementRootRefCount();
        }
    }
}

void GCRuntime::onAssignRootVariable(GCObject* assigned) {
    if (assigned == nullptr) return;
    bool isPublished = assigned->isPublished();
    if (isPublished) {
        while (g_mv_lock.exchange(1) != 0) { /* do spin. */ }
    }
    onAssignRootVariable_internal(assigned);
    if (isPublished) {
        g_mv_lock = 0;
    }
}

void GCRuntime::onEraseRootVariable_internal(GCObject* erased) {
    if (erased->decrementRootRefCount() < 0) {
        GCNode* parent = erased->getParentCircuit();
        if (erased->shouldCheckCircuit()) {
            CircuitNode::detectCircularPathAndConstructCylicNode(erased);
        }
        if (parent != nullptr) {
            parent->decrementRootRefCount();
            if (parent->isGarbage()) {
                reclaimGarbage(erased, parent);
            }
        }
        else if (erased->isGarbage()) {
            reclaimGarbage(erased, erased);
        }
    }
}

void GCRuntime::onEraseRootVariable(GCObject* erased) {
    if (erased == nullptr) return;

    bool isPublished = erased->isPublished();
    if (isPublished) {
        while (g_mv_lock.exchange(1) != 0) { /* do spin. */ }
    }
    onEraseRootVariable_internal(erased);
    if (isPublished) {
        g_mv_lock = 0;
    }
}

void GCRuntime::replaceStaticVariable(
    GCObject** pField,
    GCObject* assigned 
) {
    if (assigned != nullptr && !assigned->isPublished()) {
        assigned->publishInstance();
    }
    while (g_mv_lock.exchange(1) != 0) { /* do spin. */ }
    GCObject* erased = *pField;
    *pField = assigned;
    if (assigned != erased) {    
        if (assigned != nullptr) {
            onAssignRootVariable_internal(assigned);
        }
        if (erased != nullptr) {
            onEraseRootVariable_internal(erased);
        }
    }
    g_mv_lock = 0;
}

void GCRuntime::onReplaceRootVariable(
    GCObject* assigned, 
    GCObject* erased 
) {
    onAssignRootVariable(assigned);
    onEraseRootVariable(erased);
}

void GCRuntime::replaceMemberVariable(
    GCObject* owner, 
    OffsetPointer<GCObject>* pField,
    GCObject* assigned
) {
    assert(owner != nullptr);
    bool isPublished = owner->isPublished();
    if (isPublished) {
        if (assigned != nullptr && !assigned->isPublished()) {
            assigned->publishInstance();
        }
        while (g_mv_lock.exchange(1) != 0) { /* do spin. */ }
    }
    GCObject* erased = pField->getPointer();
	if (sizeof(*pField) < sizeof(void*)) {
		*(uint32_t*)pField = (assigned == nullptr) ? 0 : _pointer2offset(assigned, 0);
	}
    else {
		*(void**)pField = assigned;
	}
    if (assigned != nullptr && assigned != owner) {
        connectReferenceLink(assigned, owner);
    }
    if (erased != nullptr && erased != owner) {
        disconnectReferenceLink(erased, owner);
    }
    if (isPublished) {
        g_mv_lock = 0;
    }
}

bool GCObject::visitLinks(LinkVisitor visitor, void* callbackParam) {
    int offset;
    for (uint16_t* offsets = this->getFieldOffsets(); (offset = *offsets) != 0; offsets ++) {
        OffsetPointer<GCObject>* field = 
            (OffsetPointer<GCObject>*)((uintptr_t)this + offset);
        GCObject* ref = field->getPointer();
        if (ref != nullptr && ref != this) {
            if (!visitor(this, ref, callbackParam)) return false;
        }
    }
    return true;
}

bool GCArray::visitLinks(LinkVisitor visitor, void* callbackParam) {
    OffsetPointer<GCObject>* field = (OffsetPointer<GCObject>*)(this + 1);
    for (int idx = this->_cntItem; --idx >= 0; field++) {
        GCObject* ref = field->getPointer();
        if (ref != nullptr && ref != this) {
            if (!visitor(this, ref, callbackParam)) return false;
        }
    }
    return true;
}


