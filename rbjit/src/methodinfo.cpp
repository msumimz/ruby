#include "rbjit/methodinfo.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/ssatranslator.h"
#include "rbjit/debugprint.h"
#include "rbjit/ltdominatorfinder.h"

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

extern "C" {
struct RNode;
}

RBJIT_NAMESPACE_BEGIN

void
MethodInfo::compile()
{
  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_);

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());

  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_, methodName_);
}

RBJIT_NAMESPACE_END
