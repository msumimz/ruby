#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

#include "ruby.h"
extern "C" {
#include "vm_core.h" // GET_THREAD
#include "constant.h" // rb_public_const_get

// Defined in vm_insnhelper.c (originally defined as static inline)
VALUE
vm_get_ev_const(rb_thread_t *th, const rb_iseq_t *iseq,
                VALUE orig_klass, ID id, int is_defined);
}

RBJIT_NAMESPACE_BEGIN

rb_method_entry_t*
rbjit_lookupMethod(VALUE receiver, ID methodName)
{
  VALUE cls = CLASS_OF(receiver);
  mri::MethodEntry me(cls, methodName);

  // TODO: call method_missing
  assert(!me.isNull());

  return me.ptr();
}

VALUE
rbjit_callMethod(rb_method_entry_t* me, int argc, VALUE receiver, ...)
{
  va_list list;

  va_start(list, receiver);
  VALUE* argv = (VALUE*)_alloca(argc * sizeof(VALUE));
  for (int i = 0; i < argc; ++i) {
    argv[i] = va_arg(list, VALUE);
  }

  mri::Object result = mri::MethodEntry(me).call(receiver, argc, argv);
  return result;
}

VALUE
rbjit_findConstant(VALUE baseClass, ID name, void* iseq)
{
  // Call MRI's standard constant lookup function.
  // This function involves autoloading and exception raising.
  return vm_get_ev_const(GET_THREAD(), (const rb_iseq_t*)iseq, baseClass, name, 0);
}

RBJIT_NAMESPACE_END
