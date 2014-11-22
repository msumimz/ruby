#pragma once
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Scope;
class DefInfoMap;
class Block;
class Variable;
class Opcode;
class OpcodeL;

class BlockBuilder {
public:

  BlockBuilder(ControlFlowGraph* cfg, Scope* scope, DefInfoMap* defInfoMap, Block* block);
  BlockBuilder(const BlockBuilder* builder, Block* block);

  Block* block() const { return block_; }
  void setBlock(Block* b) { block_ = b; }

  bool continues() const { return !halted_; }
  void halt() { halted_ = true; }

  Variable* add(ID name, bool belongsToScope, OpcodeL* op);
  Variable* add(OpcodeL* op);
  void add(Opcode* op) { addWithoutLhsAssigned(op); }
  void addWithoutLhsAssigned(Opcode* op);

  Variable* add(bool assignLhs, OpcodeL* op)
  {
    if (assignLhs) {
      return add(op);
    }
    else {
      addWithoutLhsAssigned(op);
      return nullptr;
    }
  }

  void addJump(SourceLocation* loc, Block* dest);
  void addJumpIf(SourceLocation* loc, Variable* cond, Block* ifTrue, Block* ifFalse);

private:

  void updateDefSite(Variable* v, Block* b, Opcode* op);

  ControlFlowGraph* cfg_;
  Scope* scope_;
  DefInfoMap* defInfoMap_;

  Block* block_;

  bool halted_;
};

RBJIT_NAMESPACE_END
