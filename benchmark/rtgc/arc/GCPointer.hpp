
#include <memory.h>
#include <memory>

#ifdef _MSC_VER
    #define NO_INLINE       __declspec(noinline)
    #define THREAD_LOCAL    __declspec(thread)
#else
    #define NO_INLINE       __attribute__((noinline))
    #define THREAD_LOCAL    thread_local
#endif

namespace RTGC {

class GCObject;
class GCNode;

template <class T> 
class GCObjectArray;

template <class T> 
class GCPrimitiveArray;

uint32_t _pointer2offset(void* ref, void* origin);

void* _offset2Pointer(uint32_t offset, void* origin);

void* rtgc_malloc(size_t size);
void* rtgc_calloc(size_t size);
void* rtgc_realloc(void* mem, size_t size);
void  rtgc_free(void* mem);

#define USE_32BIT_POINTER 1

template <class T>
class OffsetPointer {
	#if USE_32BIT_POINTER
	uint32_t _offset;
	#else
	T* _ptr;
	#endif
public:
	OffsetPointer() {}

	OffsetPointer(std::nullptr_t) {
	#if USE_32BIT_POINTER
		_offset = 0;
	#else
		_ptr = nullptr;
	#endif
	}

	T* getPointer() {
	#if USE_32BIT_POINTER
		return (_offset == 0) ? nullptr : (T*)_offset2Pointer(_offset, this);
	#else
		return _ptr;
	#endif
	}
};


class GCRuntime {
public:
	static void NO_INLINE onReplaceRootVariable(GCObject* assigned, GCObject* erased);

	static void NO_INLINE onAssignRootVariable(GCObject* assigned);

	static void NO_INLINE onEraseRootVariable(GCObject* erased);

	static void NO_INLINE replaceMemberVariable(GCObject* owner, OffsetPointer<GCObject>* pField, GCObject* v);

	static void NO_INLINE replaceStaticVariable(GCObject** pField, GCObject* assigned);

	static void NO_INLINE dumpDebugInfos();

private:
	#if GC_DEBUG
	static int getCircuitCount();
	static int getTinyChunkCount();
	static int getPathInfoCount();
	#endif

	static void onAssignRootVariable_internal(GCObject* assigned);

	static void onEraseRootVariable_internal(GCObject* assigned);

	static void reclaimGarbage(GCObject* garbage, GCNode* garbageNode);

	static void connectReferenceLink(GCObject* assigned, GCObject* owner);

	static void disconnectReferenceLink(GCObject* erased, GCObject* owner);


};

template <class T>
class RETAINED_PTR {
	T* _ptr;
	RETAINED_PTR(T* const ptr) {
		this->_ptr = ptr;
	}
public:
	RETAINED_PTR(RETAINED_PTR<T>&& retained) {
		this->_ptr = retained.clear();
	}

	static RETAINED_PTR make(T* const ptr) {
		return ptr;
	}

	~RETAINED_PTR() {
		if (_ptr != nullptr) {
			GCRuntime::onEraseRootVariable((GCObject*)_ptr);
		}
	}

	T* clear() {
		T* p = _ptr;
		this->_ptr = nullptr;
		return p;
	}

	T* operator -> () const {
		return _ptr;
	}

	operator T* () const {
		return _ptr;
	}
};


template <class T>
class RC_PTR {
protected:
	T* _ptr;
public:
	RC_PTR() { this->_ptr = nullptr; }
	
	RC_PTR(std::nullptr_t) { this->_ptr = nullptr; }

	RC_PTR(const RC_PTR& p) : RC_PTR(p._ptr) {}

	RC_PTR(RETAINED_PTR<T>&& retained) {
		this->_ptr = retained.clear();
	}

	RC_PTR(T* const ptr) {
		this->_ptr = ptr;
		if (ptr != nullptr) {
			GCRuntime::onAssignRootVariable(ptr);
		}
	}

	~RC_PTR() {
		if (_ptr != nullptr) {
			GCRuntime::onEraseRootVariable(_ptr);
		}
	}

	void operator=(const RC_PTR& p) {
		this->operator=(p._ptr);
	}

	void operator = (T* ptr) {
		T* old = this->_ptr;
		if (old != ptr) {
			this->_ptr = ptr;
			GCRuntime::onReplaceRootVariable((GCObject*)ptr, (GCObject*)old);
		}
	}

	T* operator -> () const {
		return _ptr;
	}

	operator T* () const {
		return _ptr;
	}

	bool operator == (T* ptr) const {
		return _ptr == ptr;
	}

	bool operator != (T* ptr) const {
		return _ptr != ptr;
	}

	T* getRawPointer_unsafe() const {
		return _ptr;
	}
};

template <class T>
class RC_POINTER_ARRAY : public RC_PTR<GCObjectArray<T> > {
	typedef RC_PTR<GCObjectArray<T> > SUPER;
public:
	RC_POINTER_ARRAY() : SUPER() {}

	RC_POINTER_ARRAY(GCObjectArray<T>* ptr) : SUPER(ptr) {}

	RC_POINTER_ARRAY(std::nullptr_t) : SUPER() {}

	RC_POINTER_ARRAY(const RC_POINTER_ARRAY& array) : SUPER(array._ptr) {}

	RC_POINTER_ARRAY(RETAINED_PTR<GCObjectArray<T> >&& retained) : SUPER(retained) {}

	~RC_POINTER_ARRAY() {}

	void operator=(GCObjectArray<T>* ptr) {
		SUPER::operator=(ptr);
	}

	RC_PTR<T> operator[](int __n) {
		T* item = this->SUPER::_ptr->getItemPointer(__n)->getPointer();
		return item;
	}

	void _set(int idx, T* item) {
		GCObjectArray<T>* array = SUPER::_ptr;
		GCRuntime::replaceMemberVariable(array, (OffsetPointer<GCObject>*)array->getItemPointer(idx), item);
	}
};

template <class T>
class RC_PRIMITIVE_ARRAY : public RC_PTR<GCPrimitiveArray<T> > {
	typedef RC_PTR< GCPrimitiveArray<T> > SUPER;
public:
	RC_PRIMITIVE_ARRAY() : SUPER() {}

	RC_PRIMITIVE_ARRAY(GCPrimitiveArray<T>* ptr) : SUPER(ptr) {}

	RC_PRIMITIVE_ARRAY(const RC_PRIMITIVE_ARRAY& array) : SUPER(array._ptr) {}

	RC_PRIMITIVE_ARRAY(RETAINED_PTR<GCPrimitiveArray<T> >&& retained) : SUPER(retained) {}

	void operator=(GCPrimitiveArray<T>* ptr) {
		SUPER::operator=(ptr);
	}

	T& operator[](size_t __n) {
		return *(T*)(void*)this->SUPER::_ptr->getItemPointer(__n);
	}
};

}
