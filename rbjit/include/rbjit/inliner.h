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
class OpcodePhi;
class Variable;

class Inliner {
public:

  Inliner(PrecompiledMethodInfo* mi);

  void doInlining();

private:

  bool inlineCallSite(BlockHeader* block, OpcodeCall* op);
  Variable* replaceCallWithMethodBody(MethodInfo* methodInfo, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result);
  Variable* insertCall(MethodInfo* methodInfo, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op);
  void removeOpcodeCall(OpcodeCall* op);

  PrecompiledMethodInfo* mi_;
  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

  std::unordered_set<Opcode*> inlined_;

  OpcodePhi* phi_;
  OpcodePhi* envPhi_;

};

RBJIT_NAMESPACE_END
