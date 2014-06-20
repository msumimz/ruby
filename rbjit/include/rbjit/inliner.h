#pragma once

#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeContext;
class BlockHeader;
class Opcode;
class OpcodeCall;
class PrecompiledMethodInfo;

class Inliner {
public:

  Inliner(ControlFlowGraph* cfg, TypeContext* typeContext)
    : cfg_(cfg), typeContext_(typeContext)

  {}

  void doInlining();

private:

  bool inlineCallSite(BlockHeader* block, OpcodeCall* op);
  void replaceCallWithMethodBody(PrecompiledMethodInfo* mi, BlockHeader* block, OpcodeCall* op);

  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

};

RBJIT_NAMESPACE_END
