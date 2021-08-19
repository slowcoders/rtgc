#ifndef PP_REPLACE_FIELD

#define _POINTER_FIELD(TYPE)                OffsetPointer<TYPE>
#define static_POINTER_FIELD(TYPE)          static TYPE*
#define _POINTER_FIELD_VALUE(NAME)          _##NAME.getPointer()
#define static_POINTER_FIELD_VALUE(NAME)    _##NAME

#define DECL_FIELD_OBJ(SCOPE, TYPE, NAME) \
    private: SCOPE##_POINTER_FIELD(TYPE) _##NAME;  \
    public: SCOPE TYPE::PTR get_##NAME() { return SCOPE##_POINTER_FIELD_VALUE(NAME); }  \
    public: SCOPE void set_##NAME(TYPE* v) { PP_REPLACE_##SCOPE##_FIELD(_##NAME, v); }

#define DECL_FIELD_PRIMITIVE(SCOPE, TYPE, NAME) \
    private: SCOPE TYPE _##NAME;  \
    public: SCOPE TYPE get_##NAME() { return _##NAME; }  \
    public: SCOPE void set_##NAME(TYPE v) { _##NAME = v; }

#define DECL_FIELD_PRIMITIVE_ARRAY(SCOPE, TYPE, NAME) \
    private: SCOPE##_POINTER_FIELD(GCPrimitiveArray<TYPE>) _##NAME;  \
    public: SCOPE RC_PRIMITIVE_ARRAY<TYPE> get_##NAME() { return SCOPE##_POINTER_FIELD_VALUE(NAME); }  \
    public: SCOPE void set_##NAME(GCPrimitiveArray<TYPE>* v) { PP_REPLACE_##SCOPE##_FIELD(_##NAME, v); } \

#define DECL_FIELD_ARRAY(SCOPE, TYPE, NAME) \
    private: SCOPE##_POINTER_FIELD(GCObjectArray<TYPE>) _##NAME;  \
    public: SCOPE TYPE::ARRAY get_##NAME() { return SCOPE##_POINTER_FIELD_VALUE(NAME); }  \
    public: SCOPE void set_##NAME(GCObjectArray<TYPE>* v) { PP_REPLACE_##SCOPE##_FIELD(_##NAME, v); }

#define PP_REPLACE__FIELD(field, v) \
    GCRuntime::replaceMemberVariable(this, (OffsetPointer<GCObject>*)(void*)&field, v)

#define PP_REPLACE_static_FIELD(field, v) \
    GCRuntime::replaceStaticVariable((GCObject**)(void*)&field, v)
#endif

public: typedef RC_PTR<PP_CLASS>    PTR;
public: typedef RC_POINTER_ARRAY<PP_CLASS>  ARRAY;

static uint16_t _fieldMap[];
uint16_t* getFieldOffsets() {
    return _fieldMap;
}

#define PP_FIELD_OBJ(TYPE, NAME) \
    DECL_FIELD_OBJ(, TYPE, NAME)

#define PP_FIELD_PRIMITIVE(TYPE, NAME) \
    DECL_FIELD_PRIMITIVE(, TYPE, NAME)

#define PP_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) \
    DECL_FIELD_PRIMITIVE_ARRAY(, TYPE, NAME)

#define PP_FIELD_ARRAY(TYPE, NAME) \
    DECL_FIELD_ARRAY(, TYPE, NAME)


#define STATIC_FIELD_OBJ(TYPE, NAME) \
    DECL_FIELD_OBJ(static, TYPE, NAME)

#define STATIC_FIELD_PRIMITIVE(TYPE, NAME) \
    DECL_FIELD_PRIMITIVE(static, TYPE, NAME)

#define STATIC_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) \
    DECL_FIELD_PRIMITIVE_ARRAY(static, TYPE, NAME)

#define STATIC_FIELD_ARRAY(TYPE, NAME) \
    DECL_FIELD_ARRAY(static, TYPE, NAME)
