#include <stdlib.h>

#include <deque>
#include <list>
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <memory.h>

#if defined(_DEBUG) || defined(GC_DEBUG)
  #include "assert.h"
#else
  #undef assert
  #define assert(t) // ignore
#endif

#define PP_MERGE_TOKEN_EX(L, R)	L##R
#define PP_MERGE_TOKEN(L, R)	PP_MERGE_TOKEN_EX(L, R)

#define PP_TO_STRING_EX(T)	    #T
#define PP_TO_STRING(T)			PP_TO_STRING_EX(T)

namespace RTGC {

    template <class T>
    class StdVector : public std::vector<T> {
        typedef std::vector<T> SUPER;
    public:
        // T& back() {
        //     assert(_data->_size > 0);
        //     return _data->_items[_data->_size - 1];
        // }

        // void pop_back() {
        //     assert(_data->_size > 0);
        //     _data->_size --;
        // }

        int indexOf(T v) {
            int idx = SUPER::size();
            T* pObj = &SUPER::at(0) + idx;
            for (; --idx >= 0; ) {
                --pObj;
                if (*pObj == v) break;
            }
            return idx;
        }

        bool contains(T v) {
            return indexOf(v) >= 0;
        }

        bool remove(T v) {
            int idx = indexOf(v);
            if (idx < 0) {
                return false;
            }
            removeFast(idx);
            return true;
        }

        bool removeAll(T v) {
            int idx = SUPER::size();
            T* pObj = &SUPER::at(0) + idx;
            bool found = false;
            for (; --idx >= 0; ) {
                --pObj;
                if (*pObj == v) {
                    found = true;
                    removeFast(idx);
                }
            }
            return found;
        }

        void removeFast(int idx) {
            assert(idx >= 0 && idx < this->size());
            int newSize = this->size() - 1;
            if (idx < newSize) {
                this->at(idx) = this->at(newSize);
            }
            this->resize(newSize);
        }
    };


    struct DefaultAllocator {
        static void* alloc(size_t size);

        static void* realloc(void* mem, size_t size);

        static void free(void* mem);
    };

    enum class DoNotInitialize {
        Flag
    };

    template <class T, class Allocator = DefaultAllocator >
    class SimpleVector {
    public:
        struct Data {
            int _size;
            int _capacity;
            T _items[31];
        };


    protected:
        union {
            Data* _data;
            intptr_t _slot;
        };

        size_t alloc_size(size_t capacity) {
            return sizeof(int) * 2 + sizeof(T) * capacity;
        }

        SimpleVector(DoNotInitialize data) {
            // do nothing;
        }

        void allocate(size_t capacity, int initial_size) {
            _data = (Data*)Allocator::alloc(alloc_size(capacity));
            _data->_capacity = (int)capacity;
            _data->_size = initial_size;
        }

        void deallocate() {
            if ((intptr_t)_data > 0) Allocator::free(_data);
        }

    public:
        SimpleVector(size_t capacity = 8) {
            allocate(capacity, 0);
        }

        SimpleVector(Data* data) {
            _data = data;
        }

        ~SimpleVector() {
            deallocate();
        }

        T front() {
            return this->at(0);
        }

        int size() {
            return _data->_size;
        }

        int getCapacity() {
            return _data->_capacity;
        }

        bool empty() {
            return _data->_size == 0; 
        }

        void push_back(T item) {
            if (_data->_size >= _data->_capacity) {
                int newSize = ((_data->_capacity * 2 + 7) & ~7) - 1;
                _data = (Data*)Allocator::realloc(_data, alloc_size(newSize));
                _data->_capacity = newSize;
            }
            _data->_items[_data->_size++] = item;
        }

        T& back() {
            assert(_data->_size > 0);
            return _data->_items[_data->_size - 1];
        }

        void pop_back() {
            assert(_data->_size > 0);
            _data->_size --;
        }

        T& operator[](size_t __n) {
            return _data->_items[__n];
        }

        T& at(size_t __n) {
            return _data->_items[__n];
        }

        void resize(size_t __n) {
            _data->_size = (int)__n;
        }

        int indexOf(T v) {
            /* find reverse direction */
            int idx = _data->_size;
            T* pObj = _data->_items + idx;
            for (; --idx >= 0; ) {
                --pObj;
                if (*pObj == v) break;
            }
            return idx;
        }

        bool contains(T v) {
            return indexOf(v) >= 0;
        }

        SimpleVector* operator -> () {
            return this;
        }

        bool remove(T v) {
            int idx = indexOf(v);
            if (idx < 0) {
                return false;
            }
            removeFast(idx);
            return true;
        }

        bool removeAll(T v) {
            int idx = _data->_size;
            T* pObj = _data->_items + idx;
            bool found = false;
            for (; --idx >= 0; ) {
                --pObj;
                if (*pObj == v) {
                    found = true;
                    removeFast(idx);
                }
            }
            return found;
        }

        void removeFast(int idx) {
            assert(idx >= 0 && idx < this->size());
            int newSize = this->size() - 1;
            if (idx < newSize) {
                _data->_items[idx] = this->at(newSize);
            }
            this->resize(newSize);
        }
    };


    class GCObject;
    typedef bool (*LinkVisitor)(GCObject* anchor, GCObject* link, void* param);

    template <class T>
    class RefIterator {
    public:
        T** _current;
        T** _end;
        T* _temp;

        bool hasNext() {
            return (_current != _end);
        }

        T* next() {
            assert(hasNext());
            return *_current++;
        }

        T** getLocation() {
            return _current;
        }

        void reset(T** location) {
            _current = location;
        }

    };


    static const intptr_t SINGLE_ITEM_FLAG = (intptr_t)1 << (sizeof(intptr_t) * 8 - 1);

    template <class T>
    class TinyVector : private SimpleVector<T*> {
        typedef SimpleVector<T*> SUPER;

        void removeAt(int idx) {
            if (SUPER::size() == 2) {
                T* remained = SUPER::_data->_items[1 - idx];
                SUPER::deallocate();
                SUPER::_slot = (intptr_t)remained | SINGLE_ITEM_FLAG;
            }
            else {
                SUPER::removeFast(idx);
            }
        }

    public:
        TinyVector() : SUPER(nullptr) {}

        TinyVector(DoNotInitialize data) : SUPER(data) {}

        ~TinyVector() {}

        bool empty() {
            return SUPER::_data == nullptr;
        }

        T* first() {
            T* first;
            if (SUPER::_slot <= 0) {
                first = (T*)(SUPER::_slot & ~SINGLE_ITEM_FLAG);
            }
            else {
                first = SUPER::at(0);
            }
            return first;
        }

        void setFirst(T* first) {
            if (SUPER::_slot > 0) {
                int idx = SUPER::indexOf(first);
                assert(idx >= 0);
                // linear 객체를 가장 첫 referrer 로 설정.
                if (idx > 0) {
                    T* tmp = SUPER::_data->_items[0];
                    SUPER::_data->_items[0] = first;
                    SUPER::_data->_items[idx] = tmp;
                }
            }
            else {
                assert(this->first() == first);
            }
        }

        void add(T* item) {
            assert(item != nullptr);
            if (SUPER::_data == nullptr) {
                SUPER::_slot = (intptr_t)item | SINGLE_ITEM_FLAG;
            }
            else if (SUPER::_slot < 0) {
                T* first = (T*)(SUPER::_slot & ~SINGLE_ITEM_FLAG);
                SUPER::allocate(2, 2);
                SUPER::_data->_items[0] = first;
                SUPER::_data->_items[1] = item;
            }
            else {
                SUPER::push_back(item);
            }
        }


        void remove(T* item) {
            assert(item != nullptr);
            assert(SUPER::_data != nullptr);
            if (SUPER::_slot < 0) {
                assert(SUPER::_slot == ((intptr_t)item | SINGLE_ITEM_FLAG));
                SUPER::_data = nullptr;
            }
            else {
                int idx = SUPER::indexOf(item);
                assert(idx >= 0);
                removeAt(idx);
            }
        }

        bool removeAll(T* item) {
            assert(item != nullptr);
            if (SUPER::_slot <= 0) {
                if (SUPER::_slot == ((intptr_t)item | SINGLE_ITEM_FLAG)) {
                    SUPER::_data = nullptr;
                    return true;
                }
                return false;
            }
            else if (SUPER::removeAll(item)) {
                if (SUPER::size() <= 1) {
                    if (SUPER::size() == 0) {
                        SUPER::deallocate();
                        SUPER::_data = nullptr;
                    }
                    else {
                        T* remained = SUPER::_data->_items[0];
                        SUPER::deallocate();
                        SUPER::_slot = (intptr_t)remained | SINGLE_ITEM_FLAG;
                    }
                }
                return true;
            }
            return false;
        }

        void* getRawData() {
            return SUPER::_data;
        }

        void initIterator(RefIterator<T>* iterator) {
            if (SUPER::_slot == 0) {
                iterator->_current = iterator->_end = nullptr;
            }
            else if (SUPER::_slot < 0) {
                iterator->_temp = first();
                iterator->_current = &iterator->_temp;
                iterator->_end = iterator->_current + 1;
            }
            else {
                iterator->_current = &SUPER::at(0);
                iterator->_end = iterator->_current + SUPER::size();
            }
        }
    };


    struct VirtualMemory {
        static void* reserve_memory(size_t bytes);
        static void commit_memory(void* memory, size_t offset, size_t bytes);
    };

    template <class T, int MAX_BUCKET, int clearOffset>
    class MemoryPool {
        T* _items;
        T* _free;
        int _idxAlloc;
        int _maxAlloc;
        #if GC_DEBUG
        int _cntFree;
        #endif


        T*& NextFree(void* ptr) {
            return *(T**)ptr;
        }

    public:
        MemoryPool() {
            _items = (T*)VirtualMemory::reserve_memory(sizeof(T) * MAX_BUCKET) - 1;
            _free = nullptr;
            _maxAlloc = 1;
            _idxAlloc = 1;
            #if GC_DEBUG
            _cntFree = 0;
            #endif
        }

        #if GC_DEBUG
        int getAllocationCount() {
            return _idxAlloc - _cntFree - 1;
        }
        #endif

        T* allocate() {
            T* ptr;
            if (_free == nullptr) {
                if (_idxAlloc >= _maxAlloc) {
                    VirtualMemory::commit_memory(_items + 1, (_maxAlloc - 1) * sizeof(T), 4096 * sizeof(T));
                    _maxAlloc += 4096;
                }
                ptr = _items + _idxAlloc++;
                return ptr;
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
            assert(idx < _idxAlloc);
            T* ptr = _items + idx;
            return ptr;
        }

        int getIndex(void* ptr) {
            return (int)((T*)ptr - _items);
        }

        bool isInside(void* mem) {
            return mem >= _items && mem < _items + MAX_BUCKET;
        }
    };


}
