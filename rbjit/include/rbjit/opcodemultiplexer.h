#pragma once

#include <vector>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Opcode;
class BlockHeader;
class OpcodePhi;
class Variable;
class OpcodeFactory;

class OpcodeMultiplexer {
public:

  OpcodeMultiplexer(ControlFlowGraph* cfg)
    : cfg_(cfg), phi_(0)
  {}

  BlockHeader* multiplex(BlockHeader* block, Opcode* opcode, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise);

  const std::vector<BlockHeader*> segments() const { return segments_; }
  OpcodePhi* phi() const { return phi_; }

private:

  Variable* generateTypeTestOpcode(OpcodeFactory* factory, Variable* selector, mri::Class cls);

  ControlFlowGraph* cfg_;

  std::vector<BlockHeader*> segments_;
  OpcodePhi* phi_;

};

RBJIT_NAMESPACE_END
