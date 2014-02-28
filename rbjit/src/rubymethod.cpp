#include <intrin.h> // suppress warning in ruby.h
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

extern int rb_method_call_status(rb_thread_t *th, const rb_method_entry_t *me, call_type scope, VALUE self);
//#include "vm_eval.c"
}

RBJIT_NAMESPACE_BEGIN

class MethodInfo;

namespace mri {

MethodEntry::MethodEntry(Class cls, Id id)
{
  ::VALUE c;
  rb_method_entry_t* me = rb_method_entry(cls.value(), id.id(), (::VALUE*)&c);
  me_ = me;
}

MethodDefinition
MethodEntry::methodDefinition() const
{
  return ((rb_method_entry_t*)me_)->def;
}

Id
MethodEntry::methodName() const
{
  return ((rb_method_entry_t*)me_)->called_id;
}

bool
MethodEntry::canCall(CallType callType, Object self)
{
  static const int NOEX_OK = NOEX_NOSUPER;

  rb_thread_t *th = GET_THREAD();
  int call_status = rb_method_call_status(th, (const rb_method_entry_t*)me_, (call_type)callType, self.value());
  return call_status == NOEX_OK;
}

////////////////////////////////////////////////////////////
// MethodDefinition

bool
MethodDefinition::hasAstNode() const
{
  return ((rb_method_definition_t*)def_)->type == VM_METHOD_TYPE_ISEQ;
}

RNode*
MethodDefinition::astNode() const
{
  return ((rb_method_definition_t*)def_)->body.iseq->node;
}

int
MethodDefinition::argc() const
{
  return ((rb_method_definition_t*)def_)->body.iseq->argc;
}

void
MethodDefinition::setMethodInfo(MethodInfo* mi)
{
  ((rb_method_definition_t*)def_)->body.iseq->jit_method_info = mi;
}

} // namespace mri

RBJIT_NAMESPACE_END
