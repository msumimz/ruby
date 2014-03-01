#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

extern "C" {
#include "ruby.h"
}

RBJIT_NAMESPACE_BEGIN

rb_method_entry_t*
rbjit_lookupMethod(VALUE receiver, ID methodName)
{
  VALUE cls = CLASS_OF(receiver);
  mri::MethodEntry me(cls, methodName);

  return me.methodEntry();
}

VALUE
rbjit_callMethod(rb_method_entry_t* me, VALUE receiver, int argc, ...)
{
  va_list list;

  va_start(list, argc);
  VALUE* argv = (VALUE*)_alloca(argc * sizeof(VALUE));
  for (int i = 0; i < argc; ++i) {
    argv[i] = va_arg(list, VALUE);
  }

  mri::Object result = mri::MethodEntry(me).call(receiver, argc, argv);
  return result;
}

RBJIT_NAMESPACE_END
