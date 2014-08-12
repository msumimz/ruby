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

rb_control_frame_t *
vm_push_frame(rb_thread_t *th,
              const rb_iseq_t *iseq,
              VALUE type,
              VALUE self,
              VALUE klass,
              VALUE specval,
              const VALUE *pc,
              VALUE *sp,
              int local_size,
              const rb_method_entry_t *me,
              size_t stack_max);
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

VALUE
rbjit_callCompiledCode(rb_thread_t* th, rb_call_info_t* ci, const VALUE* argv)
{
  return Qnil;
}

VALUE
rbjit_callCompiledCodeWithControlFrame(rb_thread_t* th, rb_control_frame_t* cfp, rb_call_info_t* ci)
{
  return Qnil;
}

void
rbjit_enterMethod(rb_thread_t* th, rb_control_frame_t* cfp, rb_call_info_t* ci)
{
  // ref. vm_insnhelper.c:vm_call_iseq_setup_normal
  //      vm_call_cfunc_with_frame

  int i, local_size;
  VALUE *argv = cfp->sp - ci->argc;
  rb_iseq_t *iseq = ci->me->def->body.iseq;
  VALUE *sp = argv + iseq->arg_size;

  // static inline rb_control_frame_t *
  // vm_push_frame(rb_thread_t *th,
  //               const rb_iseq_t *iseq,
  //               VALUE type,
  //               VALUE self,
  //               VALUE klass,
  //               VALUE specval,
  //               const VALUE *pc,
  //               VALUE *sp,
  //               int local_size,
  //               const rb_method_entry_t *me,
  //               size_t stack_max)
  vm_push_frame(
      th,                                  // th
      iseq,                                // iseq
      VM_FRAME_MAGIC_RBJIT_COMPILED,       // type
      ci->recv,                            // self
      ci->defined_class,                   // klass
      0, //VM_ENVVAL_BLOCK_PTR(ci->blockptr),   // specval
      0,                                   // pc
      sp,                                  // sp
      0,                                   // local_size
      ci->me,                              // me
      iseq->stack_max);                    // stack_max
}

void
rbjit_leaveMethod()
{
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
rbjit_concatArrays(VALUE a1, VALUE a2)
{
  VALUE a = rb_check_convert_type(a2, T_ARRAY, "Array", "to_a");
  if (!NIL_P(a)) {
    rb_ary_concat(a1, a);
  }

  return a1;
}

VALUE
rbjit_pushToArray(VALUE array, VALUE obj)
{
  rb_ary_push(array, obj);
  return array;
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
rbjit_concatStrings(size_t count, ...)
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
