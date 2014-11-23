#pragma once

#include <vector>
#include <utility> // std::pair
#include "rbjit/rubymethod.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

class PrecompiledMethodInfo;
class ControlFlowGraph;
class TypeContext;
class Opcode;
class OpcodeCall;
class Variable;
class Scope;

class Inliner {
public:

  Inliner(PrecompiledMethodInfo* mi);

  void doInlining();

private:

  bool inlineCallSite(Block* block, Block::Iterator i);
  std::tuple<Variable*, Variable*, Block*> replaceCallWithMethodBody(MethodInfo* methodInfo, Block* entry, OpcodeCall* op, Variable* result, Variable* exitEnv);
  std::tuple<Variable*, Variable*, Block*> insertCall(mri::MethodEntry me, Block* entry, OpcodeCall* op);

  OpcodeCall* duplicateCall(OpcodeCall* op, Variable* lookup, Block* defBlock);

  PrecompiledMethodInfo* mi_;
  ControlFlowGraph* cfg_;
  Scope* scope_;
  TypeContext* typeContext_;

  std::vector<Block*> work_;
  std::vector<char> visited_;
};

RBJIT_NAMESPACE_END
