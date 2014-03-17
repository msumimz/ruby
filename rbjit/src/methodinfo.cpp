#include "rbjit/methodinfo.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/ssatranslator.h"
#include "rbjit/debugprint.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/rubymethod.h"
#include "rbjit/typeanalyzer.h"

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

void
MethodInfo::addToNativeMethod(mri::Class cls, const char* methodName, unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  me.methodDefinition().setMethodInfo(new MethodInfo(hasDef, hasEval, returnType));
}

void
PrecompiledMethodInfo::compile()
{
  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_, this);

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());

  LTDominatorFinder domFinder(cfg_);
  domFinder.setDominatorsToCfg();

  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());

  cfg_->clearDefInfo();

  TypeAnalyzer ta(cfg_);
  ta.analyze();

  RBJIT_DPRINT(cfg_->debugPrintTypeConstraints());

  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_, methodName_);
}

RBJIT_NAMESPACE_END
