#include "ruby/ruby.h"
#include "rbjit/common.h"

extern "C" {
void Init_rbjit();
}

static VALUE
debugbreak(int argc, VALUE *argv, VALUE obj)
{
  __asm { int 3 }
  return Qnil;
}

void
Init_rbjit()
{
  rb_define_method(rb_cObject, "debugbreak", (VALUE (*)(...))debugbreak, 0);
}

