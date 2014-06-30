#pragma once

#include <vector>
#include <utility> // std::pair

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
  std::pair<Variable*, Variable*> replaceCallWithMethodBody(MethodInfo* methodInfo, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result, Variable* exitEnv);
  std::pair<Variable*, Variable*> insertCall(mri::MethodEntry me, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op);
  void removeOpcodeCall(OpcodeCall* op);

  PrecompiledMethodInfo* mi_;
  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

  std::vector<BlockHeader*> work_;
  std::vector<char> visited_;
};

RBJIT_NAMESPACE_END
