#pragma once
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;

class MethodInfo {
public:

  MethodInfo(RNode* node)
    : node_(node), cfg_(0), methodBody_(0)
  {}

  void* methodBody() { return methodBody_; }

  void compile();

private:

  RNode* node_;
  ControlFlowGraph* cfg_;
  void* methodBody_;

};

RBJIT_NAMESPACE_END
