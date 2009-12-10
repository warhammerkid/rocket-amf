
#ifndef __RAMF_READ_IO_HELPERS_INLINE__
#define __RAMF_READ_IO_HELPERS_INLINE__

#include <ruby.h>
#include <stdint.h>
#include "loader.h"

static inline int8_t c_read_int8(void* context)
{
  return (int8_t)read_byte(context);
}

static inline uint8_t c_read_word8(void* context)
{
  return (uint8_t)read_byte(context);
}

static inline double c_read_double(void* context)
{
  union aligned {
    double dval;
    u_char cval[8];
  } d;
  
  // raise un expected end of buffer
  const u_char * cp = read_bytes(context, 8);
  
  if (cp) {
    if (BIG_ENDIAN) {
      d.cval[0] = cp[7]; d.cval[1] = cp[6]; d.cval[2] = cp[5]; d.cval[3] = cp[4];
      d.cval[4] = cp[3]; d.cval[5] = cp[2]; d.cval[6] = cp[1]; d.cval[7] = cp[0];
    } else {
      MEMCPY(d.cval, cp, u_char, 8);
    }
    return d.dval;
  }
}

static inline uint16_t c_read_word16_network(void* context)
{
  union aligned {
    uint16_t ival;
    u_char cval[2];
  } d;
  
  // raise un expected end of buffer
  const u_char * cp = read_bytes(context, 2);
  
  if (cp) {
    if (LITTLE_ENDIAN) {
      d.cval[0] = cp[1]; d.cval[1] = cp[0];
    } else {
      d.cval[0] = cp[0]; d.cval[1] = cp[1];
    }
    return d.ival;
  }
}

static inline int16_t c_read_int16_network(void* context)
{
  union aligned {
    int16_t ival;
    u_char cval[4];
  } d;
  
  // raise un expected end of buffer
  const u_char * cp = read_bytes(context, 4);
  
  if (cp) {
    if (LITTLE_ENDIAN) {
      d.cval[0] = cp[1]; d.cval[1] = cp[0];
    } else {
      d.cval[0] = cp[0]; d.cval[1] = cp[1];
    }
    return d.ival;
  }
}

static inline uint32_t c_read_word32_network(void* context)
{
  union aligned {
    uint32_t ival;
    u_char cval[4];
  } d;
  
  // raise un expected end of buffer
  const u_char * cp = read_bytes(context, 4);
  
  if (cp) {
    if (LITTLE_ENDIAN) {
      d.cval[0] = cp[3]; d.cval[1] = cp[2]; d.cval[2] = cp[1]; d.cval[3] = cp[0];
    } else {
      d.cval[0] = cp[0]; d.cval[1] = cp[1]; d.cval[2] = cp[2]; d.cval[3] = cp[3];
    }
    return d.ival;
  }
}

#endif
