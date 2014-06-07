#pragma once

#include <vector>
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;

class CodeDuplicator : public OpcodeVisitor {
public:

  CodeDuplicator(ControlFlowGraph* source, ControlFlowGraph* dest);

  void duplicateCfg();

  BlockHeader* entry() { return blockOf(source_->entry()); }
  BlockHeader* exit() { return blockOf(source_->exit()); }

  bool visitOpcode(BlockHeader* op);
  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeEnv* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodePrimitive* op);
  bool visitOpcode(OpcodePhi* op);
  bool visitOpcode(OpcodeExit* op);

private:

  BlockHeader* blockOf(BlockHeader* sourceBlock);
  Variable* variableOf(Variable* sourceVariable);
  void setDefInfo(Variable* lhs, Opcode* op);
  void copyRhs(OpcodeVa* dest, OpcodeVa* source);

  BlockHeader* lastBlock_;
  Opcode* lastOpcode_;

  ControlFlowGraph* source_;
  ControlFlowGraph* dest_;

  std::vector<BlockHeader*>* destBlocks_;
  std::vector<Variable*>* destVariables_;

  size_t blockIndexOffset_;
  size_t variableIndexOffset_;
};

RBJIT_NAMESPACE_END
