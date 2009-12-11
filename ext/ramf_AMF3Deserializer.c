
#include "loader.h"
#include "ramf_core.h"
#include "ramf_ReadIOHelpers_inline.h"

// AMF3 Type Markers
#define AMF3_UNDEFINED_MARKER  0x00 //"\000"
#define AMF3_NULL_MARKER       0x01 //"\001"
#define AMF3_FALSE_MARKER      0x02 //"\002"
#define AMF3_TRUE_MARKER       0x03 //"\003"
#define AMF3_INTEGER_MARKER    0x04 //"\004"
#define AMF3_DOUBLE_MARKER     0x05 //"\005"
#define AMF3_STRING_MARKER     0x06 //"\006"
#define AMF3_XML_DOC_MARKER    0x07 //"\a"
#define AMF3_DATE_MARKER       0x08 //"\b"
#define AMF3_ARRAY_MARKER      0x09 //"\t"
#define AMF3_OBJECT_MARKER     0x0A //"\n"
#define AMF3_XML_MARKER        0x0B //"\v"
#define AMF3_BYTE_ARRAY_MARKER 0x0C //"\f"
#define READ_TYPE_FROM_IO      0xff // special

// Other AMF3 Markers
#define AMF3_EMPTY_STRING         0x01
#define AMF3_ANONYMOUS_OBJECT     0x01
#define AMF3_DYNAMIC_OBJECT       0x0B
#define AMF3_CLOSE_DYNAMIC_OBJECT 0x01
#define AMF3_CLOSE_DYNAMIC_ARRAY  0x01
  
#define MAX_AMF3_INTEGER  268435455
#define MIN_AMF3_INTEGER -268435456

VALUE rb_cAMF_C_AMF3Deserializer = Qnil;
static VALUE t_deserialize(VALUE self, VALUE string);
static inline VALUE rb_deserialize(ramf3_load_context_t* context, uint8_t type);

void Init_ramf_AMF3Deserializer() {
  rb_cAMF_C_AMF3Deserializer = rb_define_class_under(rb_mAMF_C, "AMF3Deserializer", rb_cObject);
  rb_define_method(rb_cAMF_C_AMF3Deserializer, "deserialize", t_deserialize, 1);
}

static inline int32_t c_read_integer(ramf3_load_context_t * context)
{
  uint32_t  i = 0;
  u_char    c = 0;
  char      b = 0;
  
  for (; b<4; b++) {
    c = read_byte(context);
    
    if (b < 3) {
      i = i << 7;
      i = i  | (c & 0x7F);
      if (!(c & 0x80)) break;
    } else {
      i = i << 8;
      i = i  | c;
    }
  }
  
  if (i > MAX_AMF3_INTEGER) {
    i -= (1 << 29);
  }
  
  return i;
}

static inline VALUE rb_read_integer(ramf3_load_context_t* context)
{
  return INT2NUM(c_read_integer(context));
}

static inline VALUE rb_read_number(ramf3_load_context_t* context)
{
  return rb_float_new(c_read_double(context));
}

static inline VALUE rb_read_string(ramf3_load_context_t * context)
{
  int32_t type = c_read_integer(context);
  
  if ((type & 0x01) == 0) {
    int32_t ref = (type >> 1);
    return rb_ary_entry(context->strings, ref);
  } else {
    int32_t len = (type >> 1);
    char * buf = (char*)read_bytes(context, len);
    VALUE object = rb_str_new(buf, len);
    if(len > 0) rb_ary_push(context->strings, object);
    return object;
  }
}

static inline VALUE rb_read_array(ramf3_load_context_t * context)
{
  int32_t type = c_read_integer(context);
  
  if ((type & 0x01) == 0) {
    int32_t ref = (type >> 1);
    return rb_ary_entry(context->objects, ref);
  }
  
  int32_t len = (type >> 1);
  if ((len >= 0) && (peek_byte(context) == 0x01)) {
    // array
    read_byte(context); // ignore
    
    VALUE object = rb_ary_new();
    rb_ary_push(context->objects, object);
    int32_t i = 0;
    for (;i<len;i++) {
      rb_ary_push(object, rb_deserialize(context, READ_TYPE_FROM_IO));
    }
    
    return object;
  } else {
    // hash
    VALUE object = rb_hash_new();
    rb_ary_push(context->objects, object);
    VALUE key, value;
    while (1) {
      key = rb_read_string(context);
      if (RSTRING(key)->len == 0) { break; }
      value = rb_deserialize(context, READ_TYPE_FROM_IO);
      rb_hash_aset(object, key, value);
    }
    
    return object;
  }
}

static inline VALUE rb_read_object(ramf3_load_context_t * context)
{
  int32_t i,type = c_read_integer(context);
  
  if ((type & 0x01) == 0) {
    int32_t ref = (type >> 1);
    return rb_ary_entry(context->objects, ref);
  }
  
  VALUE class_definition   = Qnil;
  VALUE class_name         = Qnil;
  uint8_t externalizable   = 0;
  uint8_t dynamic          = 0;
  int32_t attribute_count  = 0;
  VALUE   class_attributes = Qnil;
  
  int32_t class_type = type >> 1;
  if ((class_type & 0x01) == 0) {
    int32_t class_ref = (type >> 1);
    class_definition  = rb_ary_entry(context->traits, class_ref);
    
    externalizable   = rb_hash_aget(class_definition, rb_str_new2("class_name"));
    class_attributes = rb_hash_aget(class_definition, rb_str_new2("members"));
    externalizable   = rb_hash_aget(class_definition, rb_str_new2("externalizable")) == Qtrue;
    dynamic          = rb_hash_aget(class_definition, rb_str_new2("dynamic"))        == Qtrue;
    attribute_count  = RARRAY(class_attributes)->len;
  } else {
    class_definition = rb_hash_new();
    class_name       = rb_read_string(context);
    externalizable   = ((class_type & 0x02) != 0);
    dynamic          = ((class_type & 0x04) != 0);
    attribute_count  = (class_type >> 3);
    class_attributes = rb_ary_new();
    
    for(i=0; i< attribute_count; i++) {
      rb_ary_push(class_attributes, rb_read_string(context));
    }
    
    rb_hash_aset(class_definition, rb_str_new2("class_name"),    class_name);
    rb_hash_aset(class_definition, rb_str_new2("members"),       class_attributes);
    rb_hash_aset(class_definition, rb_str_new2("externalizable"),(externalizable ? Qtrue : Qfalse));
    rb_hash_aset(class_definition, rb_str_new2("dynamic"),       (dynamic        ? Qtrue : Qfalse));
    rb_ary_push(context->traits, class_definition);
  }
  
  
  static VALUE rb_amf_cClassMapper         = Qnil;
  static ID    rb_amf_get_ruby_obj_id      = (ID)0;
  static ID    rb_amf_populate_ruby_obj_id = (ID)0;
  static ID    rb_amf_externalized_data_id = (ID)0;
  if (rb_amf_cClassMapper == Qnil) {
    rb_amf_cClassMapper         = rb_const_get(rb_const_get(rb_cObject, rb_intern("RocketAMF")), rb_intern("ClassMapper"));
    rb_amf_get_ruby_obj_id      = rb_intern("get_ruby_obj");
    rb_amf_populate_ruby_obj_id = rb_intern("populate_ruby_obj");
    rb_amf_externalized_data_id = rb_intern("externalized_data=");
  }
  
  VALUE object     = rb_funcall(
    rb_amf_cClassMapper, rb_amf_get_ruby_obj_id, 1,
    class_name);
  rb_ary_push(context->objects, object);
  
  if (externalizable) {
    rb_funcall(
      object, rb_amf_externalized_data_id, 1,
      rb_deserialize(context, READ_TYPE_FROM_IO));
  } else {
    VALUE props         = rb_hash_new();
    VALUE dynamic_props = Qnil;
    
    VALUE key   = Qnil;
    for (i=0; i<RARRAY(class_attributes)->len; i++) {
      key = rb_ary_entry(class_attributes, i);
      rb_hash_aset(props, key, rb_deserialize(context, READ_TYPE_FROM_IO));
    }
    
    if (dynamic) {
      dynamic_props = rb_hash_new();
      while (peek_byte(context) != 0x01) {
        key = rb_read_string(context);
        rb_hash_aset(dynamic_props, key, rb_deserialize(context, READ_TYPE_FROM_IO));
      }
      read_byte(context);
    }
    
    rb_funcall(
      rb_amf_cClassMapper, rb_amf_populate_ruby_obj_id, 3,
      object, props, dynamic_props);
  }
  
  return object;
}

static inline VALUE rb_read_date(ramf3_load_context_t * context)
{
  int32_t type = c_read_integer(context);
  
  if ((type & 0x01) == 0) {
    int32_t ref = (type >> 1);
    return rb_ary_entry(context->objects, ref);
  }
  
  double milliseconds = c_read_double(context);
  
  time_t seconds      = (milliseconds / 1000.0);
  time_t microseconds = ((milliseconds / 1000.0) - (double)seconds) * (1000.0 * 1000.0);
  
  VALUE object = rb_time_new(seconds, microseconds);
  rb_ary_push(context->objects, object);
  
  return object;
}

static inline VALUE rb_deserialize(ramf3_load_context_t* context, uint8_t type)
{
  if (type == READ_TYPE_FROM_IO) {
    type = c_read_int8(context);
  }
  
  switch (type) {
  case AMF3_UNDEFINED_MARKER:
    return Qnil;
  case AMF3_NULL_MARKER:
    return Qnil;
  case AMF3_FALSE_MARKER:
    return Qfalse;
  case AMF3_TRUE_MARKER:
    return Qtrue;
  case AMF3_INTEGER_MARKER:
    return rb_read_integer(context);
  case AMF3_DOUBLE_MARKER:
    return rb_read_number(context);
  case AMF3_STRING_MARKER:
    return rb_read_string(context);
  case AMF3_XML_DOC_MARKER:
    // return rb_read_xml_string;
    return Qnil;
  case AMF3_DATE_MARKER:
    return rb_read_date(context);
  case AMF3_ARRAY_MARKER:
    return rb_read_array(context);
  case AMF3_OBJECT_MARKER:
    return rb_read_object(context);
  case AMF3_XML_MARKER:
    // return rb_read_amf3_xml;
    return Qnil;
  case AMF3_BYTE_ARRAY_MARKER:
    // return rb_read_amf3_byte_array;
    return Qnil;
  }
  
  rb_raise(rb_eRuntimeError, "parser error");
}

static VALUE t_deserialize(VALUE self, VALUE string)
{
  ramf3_load_context_t context;
  context.base.buffer     = (u_char*)RSTRING(string)->ptr;
  context.base.cursor     = context.base.buffer;
  context.base.buffer_end = context.base.buffer + RSTRING(string)->len;
  
  context.strings = rb_ary_new();
  context.objects = rb_ary_new();
  context.traits  = rb_ary_new();
  
  return rb_deserialize(&context, READ_TYPE_FROM_IO);
} 

VALUE rb_amf3_c_deserialize(ramf_load_context_t* loader)
{
  ramf3_load_context_t context;
  context.base.buffer     = loader->buffer;
  context.base.cursor     = loader->cursor;
  context.base.buffer_end = loader->buffer_end;
  
  context.strings = rb_ary_new();
  context.objects = rb_ary_new();
  context.traits  = rb_ary_new();
  
  VALUE object = rb_deserialize(&context, READ_TYPE_FROM_IO);
  
  loader->buffer     = context.base.buffer;
  loader->cursor     = context.base.cursor;
  loader->buffer_end = context.base.buffer_end;
  
  return object;
} 
