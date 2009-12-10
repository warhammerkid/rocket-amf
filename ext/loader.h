
#ifndef __RAMF_LOADER__
#define __RAMF_LOADER__

#include <ruby.h>

typedef struct {
  u_char * buffer;
  u_char * cursor;
  u_char * buffer_end;
} ramf_load_context_t;

typedef struct {
  ramf_load_context_t base;
  
  VALUE strings;
  VALUE objects;
  VALUE traits;
} ramf3_load_context_t;

typedef struct {
  ramf_load_context_t base;
  
  VALUE cache;
} ramf0_load_context_t;

static inline int is_eof(void* _context)
{
  ramf_load_context_t * context = (ramf_load_context_t*)_context;
  return (context->buffer_end == context->cursor);
}

static inline u_char peek_byte(void* _context)
{
  ramf_load_context_t * context = (ramf_load_context_t*)_context;
  if (is_eof(context))
    rb_raise(rb_eRuntimeError, "unexpected end of buffer!");
  u_char c = context->cursor[0];
  return c;
}

static inline u_char read_byte(void* _context)
{
  ramf_load_context_t * context = (ramf_load_context_t*)_context;
  if (is_eof(context))
    rb_raise(rb_eRuntimeError, "unexpected end of buffer!");
  u_char c = context->cursor[0];
  context->cursor++;
  return c;
}

static inline u_char* read_bytes(void* _context, size_t len)
{
  ramf_load_context_t * context = (ramf_load_context_t*)_context;
  if ((context->cursor + len) > context->buffer_end)
    rb_raise(rb_eRuntimeError, "unexpected end of buffer!");
  u_char* buffer = context->cursor;
  context->cursor += len;
  return buffer;
}

#endif
