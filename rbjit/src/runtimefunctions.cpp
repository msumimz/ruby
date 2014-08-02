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

extern "C" {

////////////////////////////////////////////////////////////
// Method call

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

////////////////////////////////////////////////////////////
// Constant lookup

VALUE
rbjit_findConstant(VALUE baseClass, ID name, void* iseq)
{
  // Call MRI's standard constant lookup function.
  // This function involves autoloading and exception raising.
  return vm_get_ev_const(GET_THREAD(), (const rb_iseq_t*)iseq, baseClass, name, 0);
}

////////////////////////////////////////////////////////////
// Array

VALUE
rbjit_createArray(int count, ...)
{
  va_list list;
  va_start(list, count);
  VALUE* firstArg = &(va_arg(list, VALUE));

  return rb_ary_new4(count, firstArg);
}

VALUE
rbjit_convertToArray(VALUE obj)
{
  VALUE array = rb_check_convert_type(obj, T_ARRAY, "Array", "to_a");
  if (NIL_P(array)) {
    return rb_ary_new3(1, obj);
  }
  return rb_ary_dup(array);
}

VALUE
rbjit_concatenateArrays(int count, ...)
{
  va_list list;
  va_start(list, count);
  VALUE result = rb_ary_dup(va_arg(list, VALUE));

  for (int i = 1; i < count; ++i) {
    VALUE obj = va_arg(list, VALUE);
    VALUE a = rb_check_convert_type(obj, T_ARRAY, "Array", "to_a");
    if (!NIL_P(a)) {
      rb_ary_concat(result, a);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////
// Range

VALUE
rbjit_createRange(VALUE low, VALUE high, int exclusive)
{
  return rb_range_new(low, high, exclusive);
}

////////////////////////////////////////////////////////////
// String

VALUE
rbjit_duplicateString(VALUE s)
{
  return rb_str_dup(s);
}

VALUE
rbjit_convertToString(VALUE obj)
{
  return rb_obj_as_string(obj);
}

VALUE
rbjit_concatenateString(int count, ...)
{
  va_list list;
  va_start(list, count);
  VALUE result = rb_str_dup(va_arg(list, VALUE));

  for (int i = 1; i < count; ++i) {
    VALUE s = va_arg(list, VALUE);
    rb_str_append(result, s);
  }

  return result;
}

////////////////////////////////////////////////////////////
// Hash

VALUE
rbjit_createHash(int count, ...)
{
  assert(count % 2 == 0);

  va_list list;
  va_start(list, count);
  VALUE result = rb_hash_new();

  for (int i = 0; i < count; i += 2) {
    VALUE key = va_arg(list, VALUE);
    VALUE value = va_arg(list, VALUE);
    rb_hash_aset(result, key, value);
  }

  return result;
}

}

RBJIT_NAMESPACE_END
