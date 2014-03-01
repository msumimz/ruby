#pragma once
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;

class MethodInfo {
public:

  MethodInfo(RNode* node, const char* methodName)
    : node_(node), methodName_(methodName), cfg_(0), methodBody_(0)
  {}

  void* methodBody() { return methodBody_; }

  void compile();

private:

  RNode* node_;
  const char* methodName_;
  ControlFlowGraph* cfg_;

  void* methodBody_;

};

RBJIT_NAMESPACE_END
