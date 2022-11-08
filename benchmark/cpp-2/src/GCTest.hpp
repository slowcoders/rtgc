#include <memory.h>
#include <stdint.h>
#include <assert.h>

namespace RTGC {

#define precond     assert
#define postcond    assert

class GCObject;

static inline uint32_t _pointer2offset(GCObject* ptr) {
    return (uint32_t)((uintptr_t)ptr >> 3);
}

static inline GCObject* _offset2Pointer(uint32_t ofs) {
    return (GCObject*)(ofs << 3);
}

class ShortOOP {
    uint32_t _ofs;
public:
    ShortOOP(GCObject* ptr) {
        precond(ptr != NULL);
        _ofs = _pointer2offset(ptr);
        postcond(_ofs != 0);
    }

    operator GCObject* () const {
        return (GCObject*)_offset2Pointer(_ofs);
    }

    GCObject* operator -> () const {
        return (GCObject*)_offset2Pointer(_ofs);
    }
};

static const int MAX_COUNT_IN_CHUNK = 7;

class NodeChunk {
public:
	ShortOOP _items[MAX_COUNT_IN_CHUNK];
	int32_t _next_offset;
    NodeChunk() {}
};

static const int CHUNK_MASK = (sizeof(NodeChunk) - 1);


class ReferrerList : private NodeChunk {

    static ShortOOP* allocate_tail(NodeChunk* last_chunk) {
        NodeChunk* tail = new NodeChunk();
        tail->_next_offset = last_chunk->_items - &tail->_items[MAX_COUNT_IN_CHUNK];
        return &tail->_items[MAX_COUNT_IN_CHUNK-1];
    }

    NodeChunk* get_prev_chunk(NodeChunk* chunk) {
        NodeChunk* prev = (NodeChunk*)(&chunk->_items[MAX_COUNT_IN_CHUNK] + chunk->_next_offset);
        precond(chunk == this || ((uintptr_t)prev & CHUNK_MASK) == 0);
        return prev;
    }

    void set_last_item_ptr(ShortOOP* pLast) {
        this->_next_offset = pLast - &this->_items[MAX_COUNT_IN_CHUNK];
    }

    ShortOOP cut_last_tail() {
        ShortOOP* pLast = lastItemPtr();
        ShortOOP last_v = *pLast;
        NodeChunk* last_chunk = (NodeChunk*)((uintptr_t)pLast & ~CHUNK_MASK);
        if (last_chunk == this) {
            _next_offset --;
        } else if (pLast - last_chunk->_items > MAX_COUNT_IN_CHUNK - 1) {
            last_chunk = get_prev_chunk(last_chunk);
            if (last_chunk == this) {
                set_last_item_ptr(&this->_items[MAX_COUNT_IN_CHUNK - 6]);
            } else {
                set_last_item_ptr(&last_chunk->_items[0]);
            }
        } else {
            _next_offset ++;
        }
        return last_v;
    }

public:
    ReferrerList() {

    }

    ShortOOP* firstItemPtr() {
        return &_items[0];
    }

    ShortOOP* lastItemPtr() {
        return &_items[MAX_COUNT_IN_CHUNK] + _next_offset;
    }

    bool empty() {
        return _next_offset == -MAX_COUNT_IN_CHUNK;
    }

    bool hasSingleItem() {
        return _next_offset == -(MAX_COUNT_IN_CHUNK-1);
    }

    bool hasSingleChunk() {
        return _next_offset < 0 && _next_offset > -MAX_COUNT_IN_CHUNK
    }

    ShortOOP first() {
        return _items[0];
    }

    void setFirst(ShortOOP first) {
        _items[0] = first;
    }

    void add(ShortOOP item) {
        ShortOOP* pLast = lastItemPtr();
        NodeChunk* last_chunk = (NodeChunk*)((uintptr_t)pLast & ~CHUNK_MASK);
        if (last_chunk == this) {
            if (pLast == &this->_items[MAX_COUNT_IN_CHUNK - 1]) {
                pLast = allocate_tail(last_chunk);
                *pLast = item;
            } else {
                pLast[+1] = item;
                this->_next_offset ++;
            }
        } else {
            if (((uintptr_t)pLast % sizeof(NodeChunk)) == 0) {
                pLast = allocate_tail(last_chunk);
                set_last_item_ptr(pLast);
            } else {
                pLast --;
                this->_next_offset --;
            }
            *pLast = item;
        }
    }

    // return true: first item removed;
    bool remove(ShortOOP item);
     {
        NodeIterator iter<false>(this);
        for (ShortOOP* ptr; (ptr = iter.next()) != NULL;) {
            if (*ptr == item) {
                *ptr = cut_last_tail();
                return ptr == this->_items;
            }
        }
        return false;
    }

    int approximated_count() {
        int count = last_item_adr() - this->_items[-1];
        if (count < 0) {
            return -count;
        }
    }
};


template <bool from_tail>
class NodeIterator {
    static ShortOOP g_end_of_node;
public:
    GCObject* _prev;
    ShortOOP* _current;
    ShortOOP* _end;

    void initEmpty() {
        _current = NULL;
    }

    void initVectorIterator(ReferrerList* vector) {
        if (vector->hasSingleChunk()) {
            _current = vector->firstItemPtr();
            _end = vector->lastItemPtr() + 1;
        }
        else {
            if (from_tail) {
                _current = vector->lastItemPtr();
            } else {
                _current = vector->firstItemPtr();
            }
            _end = _current;
        }
    } 

    void initSingleIterator(ShortOOP* temp) {
        _current = temp;
        _end = temp + 1;
    }

    bool hasNext() {
        return (_current != NULL);
    }

    GCObject* peekPrev() {
        return _prev;
    }

    GCObject* next_obj() {
        this->_prev = next();
        return this->_prev;
    }

    ShortOOP& next() {
        precond(hasNext());
        ShortOOP& oop = *_current++;
        if (_current == _end) {
            _current = NULL;
        }
        else if (((uintptr_t)_current & CHUNK_MASK) == sizeof(NodeChunk) - sizeof(int32_t)) {
            int offset = *(int32_t*)_current;
            _current = _current + offset;
            if (_current == _end) {
                _current = NULL;
            }
        }
        return oop;
    }
};


inline bool ReferrerList::remove(ShortOOP item) {
    NodeIterator<false> iter;
    iter.initVectorIterator(this);
    
    while (iter.hasNext()) {
        ShortOOP& ptr = iter.next();
        if (ptr == item) {
            ptr = cut_last_tail();
            return &ptr == this->_items;
        }
    }
    return false;
}



template <class T, size_t MAX_BUCKET, int indexOffset, int clearOffset>
class MemoryPool {
    T* _items;
    T* _free;
    T* _next;
    void* _end;
    #if GC_DEBUG
    int _cntFree;
    #endif


    T*& NextFree(void* ptr) {
        return *(T**)ptr;
    }

public:
    void initialize() {
        _end = _next = (T*)VirtualMemory::reserve_memory(MAX_BUCKET*MEM_BUCKET_SIZE);
        _items = _next - indexOffset;
        _free = nullptr;
        #if GC_DEBUG
        _cntFree = 0;
        #endif
    }

    #if GC_DEBUG
    int getAllocatedItemCount() {
        return _next - _items - _cntFree - indexOffset;
    }
    #endif

    T* allocate() {
        T* ptr;
        if (_free == nullptr) {
            ptr = _next ++;
            if (_next >= _end) {
                VirtualMemory::commit_memory(_items, _end, MEM_BUCKET_SIZE);
                _end = (char*)_end + MEM_BUCKET_SIZE;
            }
        }
        else {
            ptr = _free;
            #if GC_DEBUG
            _cntFree --;
            #endif
            _free = NextFree(ptr);
            if (clearOffset >= 0) {
                memset((char*)ptr + clearOffset, 0, sizeof(T) - clearOffset);
            }
        }
        return ptr;
    }

    void delete_(T* ptr) {
        ptr->~T();
        #if GC_DEBUG
        _cntFree ++;
        #endif
        NextFree(ptr) = _free;
        _free = ptr;
    }

    T* getPointer(int idx) {
        assert(_items + idx < _next, "invalid idx: %d (max=%ld)\n", idx, _next - _items);
        T* ptr = _items + idx;
        return ptr;
    }

    int size() {
        return _next - _items;
    }

    int getIndex(void* ptr) {
        return (int)((T*)ptr - _items);
    }

    bool isInside(void* mem) {
        return mem >= _items && mem < _items + MAX_BUCKET;
    }
};


}
