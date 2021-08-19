#define PP_FIELD_OBJ(TYPE, NAME) // ignore
#define PP_FIELD_PRIMITIVE(TYPE, NAME) // ignore
#define PP_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) // ignore
#define PP_FIELD_ARRAY(TYPE, NAME) // ignore

#define STATIC_FIELD_OBJ(TYPE, NAME) \
    TYPE* PP_CLASS::_##NAME = nullptr;

#define STATIC_FIELD_PRIMITIVE(TYPE, NAME) \
    TYPE PP_CLASS::_##NAME = 0;

#define STATIC_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) \
    GCPrimitiveArray<TYPE>* PP_CLASS::_##NAME = nullptr;

#define STATIC_FIELD_ARRAY(TYPE, NAME) \
    GCObjectArray<TYPE>* PP_CLASS::_##NAME = nullptr;
