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

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// MethodInfo

void
MethodInfo::addToNativeMethod(mri::Class cls, const char* methodName, unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  me.methodDefinition().setMethodInfo(new MethodInfo(hasDef, hasEval, returnType));
}

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

PrecompiledMethodInfo*
PrecompiledMethodInfo::addToExistingMethod(mri::Class cls, ID methodName)
{
  mri::MethodEntry me(cls, methodName);
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    return nullptr;
  }

  PrecompiledMethodInfo* mi =
    new PrecompiledMethodInfo(def.astNode(), mri::Id(methodName).name());
  def.setMethodInfo(mi);

  return mi;
}

void
PrecompiledMethodInfo::buildCfg()
{
  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_, this);

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_ASSERT(cfg_->checkSanityAndPrintErrors());

  LTDominatorFinder domFinder(cfg_);
  domFinder.setDominatorsToCfg();

  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

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
  TypeAnalyzer ta(cfg_);
  ta.analyze();
  lock_ = false;

  returnType_ = cfg_->output()->typeConstraint();

  RBJIT_DPRINT(cfg_->debugPrintTypeConstraints());
}

void
PrecompiledMethodInfo::compile()
{
  if (!cfg_) {
    buildCfg();
  }

  analyzeTypes();

  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_, methodName_);
}

TypeConstraint*
PrecompiledMethodInfo::returnType()
{
  if (returnType_) {
    return returnType_;
  }

  if (lock_) {
    return TypeRecursion::create(this);
  }

  analyzeTypes();

  return returnType_;
}

RBJIT_NAMESPACE_END
