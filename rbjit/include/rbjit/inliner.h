#pragma once

#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeContext;
class BlockHeader;
class Opcode;
class OpcodeCall;
class Variable;

class Inliner {
public:

  Inliner(ControlFlowGraph* cfg, TypeContext* typeContext)
    : cfg_(cfg), typeContext_(typeContext)

  {}

  void doInlining();

private:

  bool inlineCallSite(BlockHeader* block, OpcodeCall* op);
  Variable* replaceCallWithMethodBody(mri::MethodEntry me, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result);

  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

};

RBJIT_NAMESPACE_END
