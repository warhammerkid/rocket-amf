
#include "loader.h"
#include "ramf_core.h"
#include "ramf_ReadIOHelpers_inline.h"
#include "ramf_AMF3Deserializer.h"
#include <string.h>

#define STATIC static inline

#define bool uint8_t

#define AMF0_NUMBER_MARKER       0x00 // "\000"
#define AMF0_BOOLEAN_MARKER      0x01 // "\001"
#define AMF0_STRING_MARKER       0x02 // "\002"
#define AMF0_OBJECT_MARKER       0x03 // "\003"
#define AMF0_MOVIE_CLIP_MARKER   0x04 // "\004"
#define AMF0_NULL_MARKER         0x05 // "\005"
#define AMF0_UNDEFINED_MARKER    0x06 // "\006"
#define AMF0_REFERENCE_MARKER    0x07 // "\a"
#define AMF0_HASH_MARKER         0x08 // "\b"
#define AMF0_OBJECT_END_MARKER   0x09 // "\t"
#define AMF0_STRICT_ARRAY_MARKER 0x0A // "\n"
#define AMF0_DATE_MARKER         0x0B // "\v"
#define AMF0_LONG_STRING_MARKER  0x0C // "\f"
#define AMF0_UNSUPPORTED_MARKER  0x0D // "\r"
#define AMF0_RECORDSET_MARKER    0x0E // "\016"
#define AMF0_XML_MARKER          0x0F // "\017"
#define AMF0_TYPED_OBJECT_MARKER 0x10 // "\020"
#define AMF0_AMF3_MARKER         0x11 // "\021"
#define READ_TYPE_FROM_IO        0xff // special

VALUE rb_cAMF_C_AMF0Deserializer = Qnil;
static VALUE t_deserialize(VALUE self, VALUE string);
STATIC VALUE rb_deserialize(ramf0_load_context_t* context, uint8_t type);

void Init_ramf_AMF0Deserializer() {
  rb_cAMF_C_AMF0Deserializer = rb_define_class_under(rb_mAMF_C, "AMF0Deserializer", rb_cObject);
  rb_define_method(rb_cAMF_C_AMF0Deserializer, "deserialize", t_deserialize, 1);
}

STATIC double c_read_number(ramf0_load_context_t* context)
{
  return c_read_double(context);
}

STATIC bool c_read_boolean(ramf0_load_context_t* context)
{
  return (c_read_int8(context) != 0);
}

STATIC u_char* c_read_string(ramf0_load_context_t* context, bool is_long, size_t* _len)
{
  uint32_t len = 0;
  if (is_long) {
    len = (uint32_t)c_read_word32_network(context);
  } else {
    len = (uint32_t)c_read_word16_network(context);
  }
  *_len = (size_t)len;
  return read_bytes(context, len);
}

STATIC VALUE rb_read_number(ramf0_load_context_t* context)
{
  return rb_float_new(c_read_number(context));
}

STATIC VALUE rb_read_boolean(ramf0_load_context_t* context)
{
  return (c_read_boolean(context) ? Qtrue : Qfalse);
}

STATIC VALUE rb_read_string(ramf0_load_context_t* context, bool is_long)
{
  size_t len = 0;
  char* buf = (char*)c_read_string(context, is_long, &len);
  return rb_str_new(buf, len);
}

STATIC VALUE rb_read_object(ramf0_load_context_t* context, bool add_to_ref_cache)
{
  VALUE object = rb_hash_new();
  
  if (add_to_ref_cache) {
    rb_ary_push(context->cache, object);
  }
  
  while (1) {
    VALUE key = rb_read_string(context, 0);
    int8_t type = c_read_int8(context);
    if (type == AMF0_OBJECT_END_MARKER) { break; }
    key = rb_str_intern(key);
    rb_hash_aset(object, key, rb_deserialize(context, type));
  }
  
  return object;
}

STATIC VALUE rb_read_reference(ramf0_load_context_t* context)
{
  uint16_t idx = c_read_word16_network(context);
  return rb_ary_entry(context->cache, idx);
}

STATIC VALUE rb_read_hash(ramf0_load_context_t* context)
{
  uint32_t len = c_read_word32_network(context);
  
  VALUE object = Qnil;
  
  VALUE   key = rb_read_string(context, 0);
  int8_t type = c_read_int8(context);
  
  if (type == AMF0_OBJECT_END_MARKER) {
    return rb_ary_new();
  }
  
  // We need to figure out whether this is a real hash, or whether some stupid serializer gave up
  if (rb_str_cmp( rb_big2str(rb_str_to_inum(key, 10, Qfalse),10),  key) == 0) {
    // array
    object = rb_ary_new();
    rb_ary_push(context->cache, object);
    
    rb_ary_store(object, NUM2LONG(rb_str_to_inum(key, 10, Qfalse)), rb_deserialize(context, type));
    while (1) {
      VALUE key = rb_str_to_inum(rb_read_string(context, 0), 10, Qfalse);
      int8_t type = c_read_int8(context);
      if (type == AMF0_OBJECT_END_MARKER) { break; }
      rb_ary_store(object, NUM2LONG(key), rb_deserialize(context, type));
    }
    
    return object;
  } else {
    // hash
    object = rb_hash_new();
    rb_ary_push(context->cache, object);
    
    
    key = rb_str_intern(key);
    rb_hash_aset(object, key, rb_deserialize(context, type));
    while (1) {
      VALUE key = rb_read_string(context, 0);
      int8_t type = c_read_int8(context);
      if (type == AMF0_OBJECT_END_MARKER) { break; }
      key = rb_str_intern(key);
      rb_hash_aset(object, key, rb_deserialize(context, type));
    }
    
    return object;
  }
}

STATIC VALUE rb_read_array(ramf0_load_context_t* context)
{
  uint32_t i=0, len = c_read_word32_network(context);
  
  VALUE object = rb_ary_new();
  rb_ary_push(context->cache, object);
  
  for (;i<len;i++) {
    rb_ary_push(object, rb_deserialize(context, READ_TYPE_FROM_IO));
  }
  
  return object;
}

STATIC VALUE rb_read_date(ramf0_load_context_t* context)
{
  double milliseconds = c_read_double(context);
  uint16_t tz = c_read_word16_network(context);
  time_t seconds = milliseconds/1000.0;
  time_t microseconds = (long)(milliseconds*1000.0) % 1000000;
  return rb_time_new(seconds, microseconds);
}

STATIC VALUE rb_read_typed_object(ramf0_load_context_t* context)
{
  static VALUE rb_amf_cClassMapper         = Qnil;
  static ID    rb_amf_get_ruby_obj_id      = (ID)0;
  static ID    rb_amf_populate_ruby_obj_id = (ID)0;
  if (rb_amf_cClassMapper == Qnil) {
    rb_amf_cClassMapper         = rb_const_get(rb_const_get(rb_cObject, rb_intern("RocketAMF")), rb_intern("ClassMapper"));
    rb_amf_get_ruby_obj_id      = rb_intern("get_ruby_obj");
    rb_amf_populate_ruby_obj_id = rb_intern("populate_ruby_obj");
  }
  
  VALUE klass_name = rb_read_string(context, 0);
  VALUE object     = rb_funcall(
    rb_amf_cClassMapper, rb_amf_get_ruby_obj_id, 1,
    klass_name);
  
  rb_ary_push(context->cache, object);
  
  VALUE props  = rb_read_object(context, 0);
  rb_funcall(
    rb_amf_cClassMapper, rb_amf_populate_ruby_obj_id, 3,
    object, props, rb_hash_new());
  
  return object;
}

STATIC VALUE rb_deserialize(ramf0_load_context_t* context, uint8_t type)
{
  if (type == READ_TYPE_FROM_IO) {
    type = c_read_int8(context);
  }
  
  switch (type) {
  case AMF0_NUMBER_MARKER:
    return rb_read_number(context);
  case AMF0_BOOLEAN_MARKER:
    return rb_read_boolean(context);
  case AMF0_STRING_MARKER:
    return rb_read_string(context, 0);
  case AMF0_OBJECT_MARKER:
    return rb_read_object(context, 1);
  case AMF0_NULL_MARKER:
    return Qnil;
  case AMF0_UNDEFINED_MARKER:
    return Qnil;
  case AMF0_REFERENCE_MARKER:
    return rb_read_reference(context);
  case AMF0_HASH_MARKER:
    return rb_read_hash(context);
  case AMF0_STRICT_ARRAY_MARKER:
    return rb_read_array(context);
  case AMF0_DATE_MARKER:
    return rb_read_date(context);
  case AMF0_LONG_STRING_MARKER:
    return rb_read_string(context, 1);
  case AMF0_UNSUPPORTED_MARKER:
    return Qnil;
  case AMF0_XML_MARKER:
    // return rb_read_xml(context);
    return Qnil;
  case AMF0_TYPED_OBJECT_MARKER:
    return rb_read_typed_object(context);
  case AMF0_AMF3_MARKER:
    return rb_amf3_c_deserialize(&(context->base));
  }
  
  rb_raise(rb_eRuntimeError, "parser error");
}

static VALUE t_deserialize(VALUE self, VALUE string)
{
  ramf0_load_context_t context;
  uint8_t is_stringio = strcmp(rb_class2name(CLASS_OF(string)), "StringIO") == 0 ? 1 : 0;
  if(is_stringio) {
    VALUE str               = rb_funcall(string, rb_intern("string"), 0);
    context.base.buffer     = RSTRING(str)->ptr;
    context.base.cursor     = context.base.buffer + NUM2INT(rb_funcall(string, rb_intern("pos"), 0));
    context.base.buffer_end = context.base.buffer + RSTRING(str)->len;
  } else {
    context.base.buffer     = RSTRING(string)->ptr;
    context.base.cursor     = context.base.buffer;
    context.base.buffer_end = context.base.buffer + RSTRING(string)->len;
  }
  
  context.cache = rb_ary_new();
  
  VALUE return_val = rb_deserialize(&context, READ_TYPE_FROM_IO);
  if(is_stringio) {
    rb_funcall(string, rb_intern("pos="), 1, INT2NUM(context.base.cursor-context.base.buffer));
  }
  return return_val;
} 
