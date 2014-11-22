#pragma once

#include <vector>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Scope;
class TypeContext;
class Opcode;
class Block;
class OpcodePhi;
class Variable;
class DefInfoMap;
class SourceLocation;
class BlockBuilder;

class OpcodeDemux {
public:

  OpcodeDemux(ControlFlowGraph* cfg, Scope* scope, TypeContext* typeContext)
    : cfg_(cfg), scope_(scope), typeContext_(typeContext),
      phi_(nullptr), envPhi_(nullptr), exitBlock_(nullptr)
  {}

  Block* demultiplex(Block* block, Block::Iterator iter, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise);

  const std::vector<Block*> segments() const { return segments_; }
  Block* exitBlock() const { return exitBlock_; }
  OpcodePhi* phi() const { return phi_; }
  OpcodePhi* envPhi() const { return envPhi_; }

private:

  Variable* generateTypeTestOpcode(BlockBuilder* builder, Variable* selector, mri::Class cls, SourceLocation* loc);

  ControlFlowGraph* cfg_;
  Scope* scope_;
  TypeContext* typeContext_;

  std::vector<Block*> segments_;
  OpcodePhi* phi_;
  OpcodePhi* envPhi_;
  Block* exitBlock_;
};

RBJIT_NAMESPACE_END
