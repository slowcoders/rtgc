

uint16_t PP_CLASS::_fieldMap[] = {
    
#define PP_FIELD_OBJ(TYPE, NAME)   (uint16_t)(uintptr_t)&((PP_CLASS*)0)->_##NAME,
#define PP_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) PP_FIELD_OBJ(TYPE, NAME)
#define PP_FIELD_ARRAY(TYPE, NAME) PP_FIELD_OBJ(TYPE, NAME)
#define PP_FIELD_PRIMITIVE(TYPE, NAME)  // ignore

#define STATIC_FIELD_OBJ(TYPE, NAME) // ignore
#define STATIC_FIELD_PRIMITIVE(TYPE, NAME) // ignore
#define STATIC_FIELD_PRIMITIVE_ARRAY(TYPE, NAME) // ignore
#define STATIC_FIELD_ARRAY(TYPE, NAME) // ignore

#define CLOSE_BRACE         0 };

