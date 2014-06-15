#pragma once

#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeContext;
class BlockHeader;
class Opcode;
class OpcodeCall;

class Inliner {
public:

  Inliner(ControlFlowGraph* cfg, TypeContext* typeContext)
    : cfg_(cfg), typeContext_(typeContext)

  {}

  void doInlining();

private:

  Opcode* inlineCallSite(OpcodeCall* op);

  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;
  BlockHeader* block_;

};

RBJIT_NAMESPACE_END
