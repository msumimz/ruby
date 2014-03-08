#pragma once
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"
#include "rbjit/methodpropertyset.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;

class MethodInfo {
public:

  MethodInfo() {}

  MethodPropertySet* methodPropertySet() { return &propSet_; }

private:

  MethodPropertySet propSet_;

};

class PrecompiledMethodInfo : public MethodInfo {
public:

  PrecompiledMethodInfo(RNode* node, const char* methodName)
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
