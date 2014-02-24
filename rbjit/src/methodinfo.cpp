#include "rbjit/methodinfo.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/controlflowgraph.h"

extern "C" {
struct RNode;
}

RBJIT_NAMESPACE_BEGIN

void
MethodInfo::compile()
{
  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_);
#ifdef RBJIT_NDEBUG
  fprintf(stderr, cfg_->debugDump().c_str());
#endif

  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_);
}

RBJIT_NAMESPACE_END
