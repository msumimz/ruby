#pragma once

#include <unordered_set>
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class PrecompiledMethodInfo;
class ControlFlowGraph;
class TypeContext;
class BlockHeader;
class Opcode;
class OpcodeCall;
class Variable;

class Inliner {
public:

  Inliner(PrecompiledMethodInfo* mi);

  void doInlining();

private:

  bool inlineCallSite(BlockHeader* block, OpcodeCall* op);
  Variable* replaceCallWithMethodBody(PrecompiledMethodInfo* mi, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result);

  PrecompiledMethodInfo* mi_;
  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

  std::unordered_set<Opcode*> inlined_;

};

RBJIT_NAMESPACE_END
