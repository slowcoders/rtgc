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
    void initEmpty() {
        _head._last_item_offset = -MAX_COUNT_IN_CHUNK;
    }

    void init(ShortOOP first, GCObject* second) {
        _head._items[0] = first;
        _head._items[1] = second;
        _head._last_item_offset = -(MAX_COUNT_IN_CHUNK - 1);
    }

    const ShortOOP* firstItemPtr() {
        return &_head._items[0];
    }

    const ShortOOP* lastItemPtr() {
        return &_head._items[MAX_COUNT_IN_CHUNK] + _head._last_item_offset;
    }

    bool empty() {
        return approximated_item_count() == 0;
    }

    bool hasSingleItem() {
        return approximated_item_count() == 1;
    }

    bool isTooSmall() {
        return approximated_item_count() <= 1;
    }

    bool hasMultiChunk() {
        return approximated_item_count() > MAX_COUNT_IN_CHUNK;
    }

    ShortOOP front() {
        precond(!empty());
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

    const ShortOOP* getItemPtr(ShortOOP item);

    uint32_t approximated_item_count() {
        uint32_t size = _head._last_item_offset + (MAX_COUNT_IN_CHUNK+1);
        return size;
    }

    const ShortOOP* getLastItemOffsetPtr() {
        return &_head._items[MAX_COUNT_IN_CHUNK];
    }    

    static bool isEndOfChunk(const ShortOOP* ptr) {
        return ((uintptr_t)ptr & CHUNK_MASK) == (sizeof(Chunk) - sizeof(ShortOOP));
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

    static int getAllocatedItemCount() {
        return g_chunkPool.getAllocatedItemCount();
    }

private:
    typedef MemoryPool<Chunk, 64*1024*1024, 1, -1> ChunkPool;
    
    static  ChunkPool g_chunkPool;

    const ShortOOP* extend_tail(Chunk* last_chunk);

    Chunk* dealloc_chunk(Chunk* chunk);

    void set_last_item_ptr(const ShortOOP* pLast) {
        _head._last_item_offset = pLast - &_head._items[MAX_COUNT_IN_CHUNK];
    }

    void cut_tail_end(ShortOOP* copy_to);
};

template <bool trace_reverse>
class NodeIterator {
protected:    
    const ShortOOP* _ptr;
    const ShortOOP* _end;

public:
    void initEmpty() {
        _ptr = _end = NULL;
    }

    void initSingleIterator(const ShortOOP* temp) {
        _ptr = temp;
        _end = temp + 1;
    }

    bool hasNext() {
        return _ptr != (trace_reverse ? _end : NULL);
    }

    const ShortOOP* getAndNext() {
        precond(hasNext());
        const ShortOOP* oop = _ptr ++;
        precond((GCObject*)*oop != NULL);
        if (_ptr != _end) {
            if (ReferrerList::isEndOfChunk(_ptr)) {
                _ptr = _ptr + *(int32_t*)_ptr;
            }
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
        precond(!list->empty());
        if (!list->hasMultiChunk()) {
            _ptr = list->firstItemPtr();
            _end = list->lastItemPtr() + 1;
        } else {
            _ptr = list->lastItemPtr();
            _end = list->getLastItemOffsetPtr();
        }
        postcond(_ptr != _end);
    }     
};

class AnchorIterator : public NodeIterator<false> {
    GCObject* _current;
public:    
    AnchorIterator(GCObject* obj) {
        initialize(obj);
    }

    void initialize(GCObject* obj);

    void initIterator(ReferrerList* list) {
        if (!list->hasMultiChunk()) {
            if (list->empty()) {
                _ptr = _end = NULL;
            } else {
                _ptr = list->firstItemPtr();
                _end = list->lastItemPtr() + 1;
            }
        } else {
            _ptr = list->firstItemPtr();
            _end = _ptr;
        }
    } 

    GCObject* peekPrev() {
        return _current;
    }

    GCObject* next() {
        this->_current = *getAndNext();
        return this->_current;
    }

    const ShortOOP* getAndNext() {
        this->_current = *_ptr;
        return NodeIterator<false>::getAndNext();
    }
};

}
