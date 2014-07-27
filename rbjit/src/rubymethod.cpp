#include <intrin.h> // suppress warning in ruby_atomic.h
#include "rbjit/rubymethod.h"

#include "ruby.h"

extern "C" {

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
extern int rb_method_call_status(rb_thread_t *th, const rb_method_entry_t *me, call_type scope, VALUE self);

// defined in vm_method.c
extern void setup_method_cfunc_struct(rb_method_cfunc_t *cfunc, VALUE (*func)(), int argc);
extern void release_method_definition(rb_method_definition_t *def);

extern VALUE (*call_cfunc_invoker_func(int argc))(VALUE (*func)(...), VALUE recv, int argc, const VALUE *);

// defined in vm_insnhelper.c
extern VALUE vm_call0(rb_thread_t* th, VALUE recv, ID id, int argc, const VALUE *argv, const rb_method_entry_t *me, VALUE defined_class);


}

RBJIT_NAMESPACE_BEGIN

namespace mri {

MethodEntry::MethodEntry(VALUE cls, ID id)
{
  VALUE c;
  rb_method_entry_t* me = rb_method_entry(cls, id, &c);

  // Are these conditions always satisfied?
  assert(!me || me->called_id == id);

  me_ = me;
}

MethodInfo*
MethodEntry::methodInfo() const
{
  return static_cast<MethodInfo*>(me_->jit_method_info);
}

void
MethodEntry::setMethodInfo(MethodInfo* mi)
{
  me_->jit_method_info = mi;
}

void setMethodInfo(MethodInfo* mi);

MethodDefinition
MethodEntry::methodDefinition() const
{
  return MethodDefinition(me_->def);
}

void
MethodEntry::setMethodDefinition(mri::MethodDefinition newDef)
{
  me_->def = newDef.ptr();
}

ID
MethodEntry::methodName() const
{
  return me_->called_id;
}

Class
MethodEntry::class_() const
{
  return me_->klass;
}

bool
MethodEntry::canCall(CallType callType, Object self)
{
  static const int NOEX_OK = NOEX_NOSUPER;

  rb_thread_t *th = GET_THREAD();
  int call_status = rb_method_call_status(th, me_, (call_type)callType, self);
  return call_status == NOEX_OK;
}

VALUE
MethodEntry::call(VALUE receiver, ID methodName, int argc, const VALUE* argv, VALUE defClass)
{
  rb_thread_t *th = GET_THREAD();
  VALUE result = vm_call0(th, receiver, methodName, argc, argv, me_, defClass);

  return Object(result);
}

VALUE
MethodEntry::call(VALUE receiver, int argc, const VALUE* argv)
{
  rb_thread_t *th = GET_THREAD();
  VALUE result = vm_call0(th, receiver, me_->called_id, argc, argv, me_, me_->klass);

  return result;
}

////////////////////////////////////////////////////////////
// CRef

CRef::CRef(NODE* cref)
  : cref_(cref)
{}

CRef
CRef::next() const
{
  return cref_->nd_next;
}

Class
CRef::class_() const
{
  return Class(cref_->nd_clss);
}

Object
CRef::findConstant(ID name) const
{
  // Find outer classes/modules
  for (CRef cref = *this; !cref.isNull(); cref = cref.next()) {
    Class cls = cref.class_();
    if (!cls.isNull()) {
      Object value = cref.class_().findConstantInThisClass(name);
      if (!value.isNull()) {
        return value;
      }
    }
  }

  if (!class_().isNull()) {
    Class super = class_().superclass();
    if (!super.isNull()) {
      return findConstant(name);
    }
  }

  return Object();
}

////////////////////////////////////////////////////////////
// MethodDefinition

MethodDefinition::MethodDefinition(ID methodName, void* code, int argc)
{
  // Borrowed to method.h:rb_add_method and other functions

  def_ = ALLOC(rb_method_definition_t);

  def_->type = VM_METHOD_TYPE_CFUNC;
  def_->original_id = methodName;
  def_->alias_count = 0;

  setup_method_cfunc_struct(&def_->body.cfunc, (VALUE (*)())code, argc);
}

void
MethodDefinition::clear()
{
  def_ = nullptr;
}

void
MethodDefinition::destroy()
{
  release_method_definition(def_);
  def_ = nullptr;
}

bool
MethodDefinition::hasAstNode() const
{
  return def_->type == VM_METHOD_TYPE_ISEQ;
}

int
MethodDefinition::methodType() const
{
  return def_->type;
}

RNode*
MethodDefinition::astNode() const
{
  if (hasAstNode()) {
    return def_->body.iseq->node;
  }
  return nullptr;
}

void*
MethodDefinition::iseq() const
{
  assert(hasAstNode());
  return def_->body.iseq;
}

int
MethodDefinition::argc() const
{
  assert(hasAstNode());
  return def_->body.iseq->argc;
}

CRef
MethodDefinition::cref() const
{
  assert(hasAstNode());
  return def_->body.iseq->cref_stack;
}

MethodDefinition::CFunc
MethodDefinition::cFunc() const
{
  assert(hasAstNode());
  return def_->body.cfunc.func;
}

void
MethodDefinition::setCFunc(CFunc func)
{
  def_->body.cfunc.func = func;
}

void
MethodDefinition::setCFuncInvoker(Invoker invoker)
{
  def_->body.cfunc.invoker = invoker;
}

MethodDefinition::Invoker
MethodDefinition::defaultCFuncInvoker()
{
  return call_cfunc_invoker_func(def_->body.cfunc.argc);
}

} // namespace mri

RBJIT_NAMESPACE_END
