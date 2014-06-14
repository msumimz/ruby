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

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// MethodInfo

MethodInfo::~MethodInfo()
{
  if (returnType_) {
    returnType_->destroy();
  }
}

void
MethodInfo::addToNativeMethod(mri::Class cls, const char* methodName, unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  me.methodDefinition().setMethodInfo(new MethodInfo(cls, hasDef, hasEval, returnType));
}

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

PrecompiledMethodInfo*
PrecompiledMethodInfo::addToExistingMethod(mri::MethodEntry me)
{
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    return nullptr;
  }

  PrecompiledMethodInfo* mi =
    new PrecompiledMethodInfo(me.class_(), def.astNode(), mri::Id(me.methodName()).name());
  def.setMethodInfo(mi);

  return mi;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::addToExistingMethod(mri::Class cls, ID methodName)
{
  mri::MethodEntry me(cls, methodName);
  return addToExistingMethod(me);
}

void
PrecompiledMethodInfo::buildCfg()
{
  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_, this);

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());

  LTDominatorFinder domFinder(cfg_);
  domFinder.setDominatorsToCfg();

  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

  cfg_->clearDefInfo();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
}

void
PrecompiledMethodInfo::analyzeTypes()
{
  if (lock_) {
    return;
  }

  if (!cfg_) {
    buildCfg();
  }

  lock_ = true;

  if (typeContext_) {
    delete typeContext_;
  }

  TypeAnalyzer ta(cfg_);

  // Set self's type
  ta.setInputTypeConstraint(0, TypeClassOrSubclass(cls_));
  typeContext_ = ta.analyze();

  lock_ = false;

  if (returnType_) {
    returnType_->destroy();
  }
  returnType_ = typeContext_->typeConstraintOf(cfg_->output())->independantClone();

  RBJIT_DPRINT(debugPrintBanner());
  RBJIT_DPRINT(typeContext_->debugPrint());
}

void
PrecompiledMethodInfo::compile()
{
  RBJIT_DPRINT(debugPrintBanner());

  if (!cfg_) {
    buildCfg();
  }

  analyzeTypes();

  RBJIT_DPRINT(debugPrintBanner());
  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_, typeContext_, methodName_);
}

ControlFlowGraph*
PrecompiledMethodInfo::cfg()
{
  if (!cfg_) {
    buildCfg();
  }
  return cfg_;
}

TypeConstraint*
PrecompiledMethodInfo::returnType()
{
  if (returnType_) {
    return returnType_;
  }

  if (lock_) {
    returnType_ = TypeRecursion::create(this);
    return returnType_;
  }

  analyzeTypes();

  return returnType_;
}

////////////////////////////////////////////////////////////
// debugPrintBanner

std::string
PrecompiledMethodInfo::debugPrintBanner() const
{
  return stringFormat(
    "============================================================\n"
    "Method: '%s' (%Ix)\n"
    "============================================================\n",
    methodName_, this);
}

RBJIT_NAMESPACE_END
