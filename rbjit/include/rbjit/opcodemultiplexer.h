#pragma once

#include <vector>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Scope;
class TypeContext;
class Opcode;
class BlockHeader;
class OpcodePhi;
class Variable;
class OpcodeFactory;

class OpcodeMultiplexer {
public:

  OpcodeMultiplexer(ControlFlowGraph* cfg, Scope* scope, TypeContext* typeContext)
    : cfg_(cfg), scope_(scope), typeContext_(typeContext), phi_(0), envPhi_(0)
  {}

  BlockHeader* multiplex(BlockHeader* block, Opcode* opcode, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise);

  const std::vector<BlockHeader*> segments() const { return segments_; }
  OpcodePhi* phi() const { return phi_; }
  OpcodePhi* envPhi() const { return envPhi_; }

private:

  Variable* generateTypeTestOpcode(OpcodeFactory* factory, Variable* selector, mri::Class cls);

  ControlFlowGraph* cfg_;
  Scope* scope_;
  TypeContext* typeContext_;

  std::vector<BlockHeader*> segments_;
  OpcodePhi* phi_;
  OpcodePhi* envPhi_;

};

RBJIT_NAMESPACE_END
