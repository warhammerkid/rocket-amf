
#include "ruby.h"
#include "ramf_core.h"
#include "ramf_AMF0Deserializer.h"
#include "ramf_AMF3Deserializer.h"

void Init_ramf_ext() {
  Init_ramf_core();
  Init_ramf_AMF0Deserializer();
  Init_ramf_AMF3Deserializer();
}
