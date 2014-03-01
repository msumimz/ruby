#include <intrin.h> // suppress warning in ruby_atomic.h
#include "rbjit/rubymethod.h"

extern "C" {
#include "ruby.h"
#include "method.h" // rb_method_entry_t, rb_method_definition_t
#include "vm_core.h" // rb_iseq_t, rb_thread_t, GET_THREAD

// copied from vm_eval.c:26
typedef enum call_type {
    CALL_PUBLIC,
    CALL_FCALL,
    CALL_VCALL,
    CALL_TYPE_MAX
} call_type;

// defined in vm_eval.c
extern int
rb_method_call_status(rb_thread_t *th, const rb_method_entry_t *me, call_type scope, VALUE self);

// defined in vm_insnhelper.c
extern VALUE
vm_call0(rb_thread_t* th, VALUE recv, ID id, int argc, const VALUE *argv,
         const rb_method_entry_t *me, VALUE defined_class);
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

MethodEntry::MethodEntry(Class cls, Id id)
{
  ::VALUE c;
  rb_method_entry_t* me = rb_method_entry(cls, id, (VALUE*)&c);
  me_ = me;
}

MethodDefinition
MethodEntry::methodDefinition() const
{
  return me_->def;
}

Id
MethodEntry::methodName() const
{
  return me_->called_id;
}

bool
MethodEntry::canCall(CallType callType, Object self)
{
  static const int NOEX_OK = NOEX_NOSUPER;

  rb_thread_t *th = GET_THREAD();
  int call_status = rb_method_call_status(th, me_, (call_type)callType, self);
  return call_status == NOEX_OK;
}

Object
MethodEntry::call(Object receiver, Id methodName, Id id, int argc, const Object* argv, Class defClass)
{
  rb_thread_t *th = GET_THREAD();
  VALUE result = vm_call0(th, receiver, id, argc, (const VALUE*)argv, me_, defClass);

  return Object(result);
}

////////////////////////////////////////////////////////////
// MethodDefinition

bool
MethodDefinition::hasAstNode() const
{
  return def_->type == VM_METHOD_TYPE_ISEQ;
}

RNode*
MethodDefinition::astNode() const
{
  return def_->body.iseq->node;
}

int
MethodDefinition::argc() const
{
  return def_->body.iseq->argc;
}

void
MethodDefinition::setMethodInfo(MethodInfo* mi)
{
  def_->body.iseq->jit_method_info = mi;
}

} // namespace mri

RBJIT_NAMESPACE_END
