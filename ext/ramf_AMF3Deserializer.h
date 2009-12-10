
#include "loader.h"

extern VALUE rb_cAMF_C_AMF3Deserializer;

void Init_ramf_AMF3Deserializer();
VALUE rb_amf3_c_deserialize(ramf_load_context_t* loader);
