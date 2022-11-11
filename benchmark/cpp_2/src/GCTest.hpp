#include <memory.h>
#include <stdint.h>
#include <assert.h>

namespace RTGC {

#define precond     assert
#define postcond    assert

class GCObject;
static const int MEM_BUCKET_SIZE = 64 * 1024;

struct VirtualMemory {
    static void* reserve_memory(size_t bytes);
    static void  commit_memory(void* mem, void* bucket, size_t bytes);
    static void  free(void* mem, size_t bytes);
};


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
        precond(_items + idx < _next);//, "invalid idx: %d (max=%ld)\n", idx, _next - _items);
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



static inline uint32_t _pointer2offset(GCObject* ptr) {
    return (uint32_t)((uintptr_t)ptr >> 3);
}

static inline GCObject* _offset2Pointer(uint32_t ofs) {
    return (GCObject*)((uintptr_t)ofs << 3);
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


class ReferrerList {
    friend class ReverseIterator;    

    static const int MAX_COUNT_IN_CHUNK = 7;
    struct Chunk {
        ShortOOP _items[MAX_COUNT_IN_CHUNK];
        int32_t _last_item_offset;
    };
    static const int CHUNK_MASK = (sizeof(Chunk) - 1);

    Chunk _head;

public:
    void init() {
        _head._last_item_offset = -MAX_COUNT_IN_CHUNK;
    }

    void init(ShortOOP first, GCObject* second) {
        _head._items[0] = first;
        _head._items[1] = second;
        _head._last_item_offset = -(MAX_COUNT_IN_CHUNK - 1);
    }

    ShortOOP* firstItemPtr() {
        return &_head._items[0];
    }

    ShortOOP* lastItemPtr() {
        return &_head._items[MAX_COUNT_IN_CHUNK] + _head._last_item_offset;
    }

    bool empty() {
        return _head._last_item_offset == -(MAX_COUNT_IN_CHUNK+1);
    }

    bool hasSingleItem() {
        return _head._last_item_offset == -(MAX_COUNT_IN_CHUNK);
    }

    bool isTooSmall() {
        uint32_t offset = (uint32_t)_head._last_item_offset + MAX_COUNT_IN_CHUNK + 1;
        return offset < 2;
    }

    bool hasMultiChunk() {
        return (uint32_t)_head._last_item_offset < (uint32_t)(-MAX_COUNT_IN_CHUNK);
    }

    ShortOOP front() {
        return _head._items[0];
    }

    void replaceFirst(ShortOOP first);

    void push_back(ShortOOP item) {
        add(item);
    }

    void add(ShortOOP item);

    bool contains(ShortOOP item) {
        return getItemPtr(item) != NULL;
    }

    // returns removed item pointer (the memory may not accessable);
    const void* remove(ShortOOP item);

    const void* removeMatchedItems(ShortOOP item);

    ShortOOP* getItemPtr(ShortOOP item);

    int approximated_item_count() {
        int count = lastItemPtr() - firstItemPtr();
        if (count < 0) {
            count = -count;
        }
        return count;
    }

    ShortOOP* getLastItemOffsetPtr() {
        return &_head._items[MAX_COUNT_IN_CHUNK];
    }    

    static void validateChunktemPtr(ShortOOP*& ptr) {
        if (((uintptr_t)ptr & CHUNK_MASK) == sizeof(Chunk) - sizeof(int32_t)) {
            ptr = ptr + *(int32_t*)ptr;
        }
    }

    static void initialize() {
        g_chunkPool.initialize();
    }

    static ReferrerList* allocate() {
        return (ReferrerList*)g_chunkPool.allocate();
    }

    static int getIndex(ReferrerList* referrers) {
        return g_chunkPool.getIndex(&referrers->_head);
    }

    static ReferrerList* getPointer(uint32_t idx) {
        return (ReferrerList*)g_chunkPool.getPointer(idx);
    }

    static void delete_(ReferrerList* referrers) {
        g_chunkPool.delete_(&referrers->_head);
    }

    #if GC_DEBUG
    static int getAllocatedItemCount() {
        return g_chunkPool.getAllocatedItemCount();
    }
    #endif

private:
    typedef MemoryPool<Chunk, 64*1024*1024, 1, -1> ChunkPool;
    
    static  ChunkPool g_chunkPool;

    ShortOOP* extend_tail(Chunk* last_chunk);

    Chunk* dealloc_chunk(Chunk* chunk);

    void set_last_item_ptr(ShortOOP* pLast) {
        _head._last_item_offset = pLast - &_head._items[MAX_COUNT_IN_CHUNK];
    }

    void cut_tail_end(ShortOOP* copy_to);
};

template <bool trace_reverse>
class NodeIterator {
protected:    
    ShortOOP* _ptr;
    ShortOOP* _end;

public:
    NodeIterator() {}

    NodeIterator(GCObject* obj);

    void initEmpty() {
        _ptr = _end = NULL;
    }

    void initIterator(ReferrerList* vector) {
        precond(!vector->empty());
        if (!vector->hasMultiChunk()) {
            _ptr = vector->firstItemPtr();
            _end = vector->lastItemPtr() + 1;
            postcond(_ptr != _end);
        } else if (trace_reverse) {
            _ptr = vector->lastItemPtr();
            _end = vector->getLastItemOffsetPtr();
            postcond(_ptr != _end);
        } else {
            _ptr = vector->firstItemPtr();
            _end = _ptr;
        }
    } 

    void initSingleIterator(ShortOOP* temp) {
        _ptr = temp;
        _end = temp + 1;
    }

    bool hasNext() {
        return _ptr != (trace_reverse ? _end : NULL);
    }

    ShortOOP& next() {
        precond(hasNext());
        ShortOOP& oop = *_ptr ++;
        if (_ptr != _end) {
            ReferrerList::validateChunktemPtr(_ptr);
            if (!trace_reverse && _ptr == _end) {
                _ptr = NULL;
            } 
        } else if (!trace_reverse) {
            _ptr = NULL;
        }
        return oop;
    }
};

class ReverseIterator : public NodeIterator<true> {
public:     
    ReverseIterator(ReferrerList* list) {
        initIterator(list);
    }
};

class AnchorIterator : public NodeIterator<false> {
    GCObject* _current;
public:    
    AnchorIterator(GCObject* obj) {
        initialize(obj);
    }

    void initialize(GCObject* obj);

    GCObject* peekPrev() {
        return _current;
    }

    GCObject* next_obj() {
        this->_current = next();
        return this->_current;
    }

    ShortOOP& next() {
        this->_current = *_ptr;
        return NodeIterator<false>::next();
    }
};


}
