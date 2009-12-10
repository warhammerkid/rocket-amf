
#include "ruby.h"

VALUE rb_mAMF = Qnil;
VALUE rb_mAMF_C = Qnil;

void Init_ramf_core() {
  rb_mAMF   = rb_define_module("AMF");
  rb_mAMF_C = rb_define_module_under(rb_mAMF, "C");
}
