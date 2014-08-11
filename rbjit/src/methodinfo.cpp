#include "rbjit/methodinfo.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/ssatranslator.h"
#include "rbjit/debugprint.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/rubymethod.h"
#include "rbjit/typeanalyzer.h"
#include "rbjit/variable.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/debugprint.h"
#include "rbjit/typecontext.h"
#include "rbjit/recompilationmanager.h"
#include "rbjit/compilationinstance.h"

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// CMethodInfo

CMethodInfo::~CMethodInfo()
{
  if (returnType_) {
    returnType_->destroy();
  }
}

CMethodInfo*
CMethodInfo::construct(mri::Class cls, const char* methodName, bool mutator, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  CMethodInfo* mi = new CMethodInfo(me, mutator, returnType);
  me.setMethodInfo(mi);

  return mi;
}

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

PrecompiledMethodInfo::PrecompiledMethodInfo(mri::MethodEntry me)
: MethodInfo(me), origDef_(),
  compilationInstance_(new CompilationInstance(
    me.class_(),
    me.methodName(), me.methodDefinition().astNode(),
    me.methodDefinition().cref()))
{}

PrecompiledMethodInfo::~PrecompiledMethodInfo()
{
  delete compilationInstance_;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::construct(mri::MethodEntry me)
{
  if (!me.methodDefinition().hasAstNode()) {
    return nullptr;
  }

  if (me.methodInfo()) {
    assert(typeid(*me.methodInfo()) == typeid(PrecompiledMethodInfo));
    return static_cast<PrecompiledMethodInfo*>(me.methodInfo());
  }

  PrecompiledMethodInfo* mi = new PrecompiledMethodInfo(me);
  me.setMethodInfo(mi);

  return mi;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::construct(mri::Class cls, ID methodName)
{
  mri::MethodEntry me(cls, methodName);
  return construct(me);
}

TypeConstraint*
PrecompiledMethodInfo::returnType()
{
  return compilationInstance_->returnType();
}

RNode*
PrecompiledMethodInfo::astNode() const
{
  if (methodEntry().methodDefinition().hasAstNode()) {
    return methodEntry().methodDefinition().astNode();
  }
  assert(!origDef_.isNull());
  return origDef_.astNode();
}

bool
PrecompiledMethodInfo::isMutator()
{
  return compilationInstance_->isMutator();
}

bool
PrecompiledMethodInfo::isJitOnly()
{
  return compilationInstance_->isJitOnly();
}

void
PrecompiledMethodInfo::compile()
{
  void* code = compilationInstance_->generateCode(this);
  assert(code);

  if (origDef_.isNull()) {
    origDef_ = methodEntry().methodDefinition();
    mri::MethodDefinition def(methodEntry().methodName(), code, origDef_.argc());
    methodEntry().setMethodDefinition(def);
  }
}

void
PrecompiledMethodInfo::restoreISeqDefinition()
{
  if (origDef_.isNull()) {
    return;
  }

  methodEntry().methodDefinition().destroy();
  methodEntry().setMethodDefinition(origDef_);
  origDef_.clear();

  compilationInstance_->reset();
}

RBJIT_NAMESPACE_END
