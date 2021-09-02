#include "GCNode.hpp"
#include <memory.h>

using namespace RTGC;
namespace RTGC {
    bool TLS_GP = true;

#ifdef _MSC_VER
    const uintptr_t rtgc_malloc_root = (uintptr_t)::malloc(8) & ~0x7FFFFFFF;
#else
    const uintptr_t rtgc_malloc_root = (uintptr_t)::malloc(8);
#endif

	class GarbageProcessor {
		SimpleVector<LinkIterator> _traceStack;
		SimpleVector<GCNode*> _garbages;
	public:
		GarbageProcessor() : _traceStack(255) {}

		void reclaimObject(GCObject* garbage, GCNode* garbageNode);

        void publishInstance(GCObject* obj);

	private:
		void addGarbage(GCObject* garbage);

	};

    GarbageProcessor g_garbageProcessor;

    void* rtgc_malloc(size_t size) {
        int bits = size & 0xF;
        if (bits != 0) {
            size += 16 - bits;
        }
        void* ptr = ::malloc(size);
        if (ptr == nullptr) throw "OutOfMemoryError:E005";
        return ptr;
    }

    void* rtgc_calloc(size_t size) {
        int bits = size & 0xF;
        if (bits != 0) {
            size += 16 - bits;
        }
        void* ptr = ::calloc(size, 1);
        if (ptr == nullptr) throw "OutOfMemoryError:E006";
        return ptr;
    }

    void* rtgc_realloc(void* mem, size_t size) {
        void* ptr = ::realloc(mem, size);
        if (ptr == nullptr) throw "OutOfMemoryError:E007";
        return ptr;
    }

    void rtgc_free(void* mem) {
        ::free(mem);
    }

    uint32_t _pointer2offset(void* ref, void* origin) {
        assert(ref != nullptr);
        assert(((uintptr_t)ref & 0xF) == 0);
        assert((uintptr_t)ref > rtgc_malloc_root);
        uintptr_t offset = ((uintptr_t)ref - rtgc_malloc_root) / 16;
        assert(offset == (uint32_t)offset);
        return (uint32_t)offset;
    }

    void* _offset2Pointer(uint32_t offset, void* origin) {
        assert(offset != 0);
		return (void*)(rtgc_malloc_root + (uintptr_t)offset * 16);
    }

}

LinkIterator::LinkIterator(GCObject* obj) {
    this->_anchor = obj;
    const uint16_t* off = obj->getFieldOffsets();
    if (off == GCArray::_array_offsets) {
        this->_idxField = _array->length();
    }
    else {
        this->_offsets = off;
    }
}

GCObject* LinkIterator::next() {
    union {
        const uint16_t* offsets;
        intptr_t idxField;
    };
    offsets = this->_offsets;

    if (offsets < (void*)(intptr_t)0xFFFFFFFF) {
        while (--idxField >= 0) {
            GCObject* ref = _array->getItemPointer(idxField)->getPointer();//     *pItem++;
            if (ref != nullptr) {
                this->_idxField = idxField;
                return ref;
            }
        }
    }
    else {
        while (true) {
            int offset = *offsets ++;
            if (offset == 0) break;
            OffsetPointer<GCObject>* field = 
                (OffsetPointer<GCObject>*)((uintptr_t)_anchor + offset);
            GCObject* ref = field->getPointer();
            if (ref != nullptr) {
                this->_offsets = offsets;
                return ref;
            }
        }
    }
    return nullptr;
}

void GCRuntime::reclaimGarbage(GCObject* garbage, GCNode* garbageNode) {
    g_garbageProcessor.reclaimObject(garbage, garbageNode);
}

#define PUSH_GARBAGE(garbage) \
    garbage->markDestroyed(); \
    ((void**)garbage)[2] = delete_q; \
    delete_q = garbage;
    

void GarbageProcessor::reclaimObject(GCObject* garbage, GCNode* garbageNode) {

    GCNode* delete_q = nullptr;
    PUSH_GARBAGE(garbage);
    _traceStack.push_back(garbage);

    if (garbageNode != garbage) {
        PUSH_GARBAGE(garbageNode);
    }

    LinkIterator* it = &_traceStack.back();
    while (true) {
        GCObject* link = it->next();
        if (link == nullptr) {
            _traceStack.pop_back();
            if (_traceStack.empty()) break;
            it = &_traceStack.back();
        }
        else if (!link->isDestroyed()) {
            CircuitNode* parent = link->getParentCircuit();
            if (isRoot(parent) || !parent->isDestroyed()) {
                if (!link->removeAllReferrer(it->getContainerObject())
                ||  !link->isGarbage()) {
                    if (!isRoot(parent) // link->inCircuit()
                    &&  parent->removeAllReferrer(it->getContainerObject())
                    &&  parent->isGarbage()) {
                        PUSH_GARBAGE(parent);
                    }
                    else {
                        continue;
                    }
                }
            }
            PUSH_GARBAGE(link);
            _traceStack.push_back(link);
            it = &_traceStack.back();
        }
    }    

    GCNode** ppNode = &_garbages.at(0);
    for (GCNode* node = delete_q; node != nullptr;) {
        GCNode* next = ((GCNode**)node)[2];
        delete node;
        node = next;
    }
    //_garbages.resize(0);
}

void GCObject::publishInstance() {
    g_garbageProcessor.publishInstance(this);
}

void GarbageProcessor::publishInstance(GCObject* obj) {
    if (!obj->markPublished()) return;
    _traceStack.push_back(obj);    
    LinkIterator* it = &_traceStack.back();
    while (true) {
        GCObject* link = it->next();
        if (link == nullptr) {
            _traceStack.pop_back();
            if (_traceStack.empty()) break;
            it = &_traceStack.back();
        }
        else if (link->markPublished()) {
            _traceStack.push_back(link);
            it = &_traceStack.back();
        }
    }    
}


void* GCArray::alloc_internal(int length, int item_size) {
    int size = length * item_size;
    if (size > 200 * 1024) {
        // max-allocation size in 32bit-pointer : 256M bytes.
        void* ptr = rtgc_malloc(200 * 1024);
        void* ptr2 = rtgc_realloc(ptr, size);
        assert(ptr == ptr2);
        memset(ptr2, 0, size);
        return ptr;
    }
    void* ptr = rtgc_calloc(sizeof(GCArray) + item_size * length);
    return ptr;
}








#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
#endif

#define _USE_MMAP 0
#define _ULIMIT 1
void* VirtualMemory::reserve_memory(size_t bytes) {
    void* addr;
#ifdef _MSC_VER
    addr = VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
#elif _USE_MMAP
    addr = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
#else
    addr = malloc(bytes);
#if _ULIMIT    
    addr = realloc(addr, 4096);
#endif
#endif
    return addr;
}

void VirtualMemory::commit_memory(void* addr, size_t offset, size_t bytes) {
#ifdef _MSC_VER
    addr = VirtualAlloc((char*)addr + offset, bytes, MEM_COMMIT, PAGE_READWRITE);
    if (addr != 0) return;
#elif _USE_MMAP
    int res = mprotect(addr, bytes, PROT_READ|PROT_WRITE);
    if (res == 0) return;
#else
#if _ULIMIT    
    void* mem = ::realloc(addr, offset + bytes);
    if (mem == addr) return;
#endif
#endif
    throw "OutOfMemoryError:E009";
}

