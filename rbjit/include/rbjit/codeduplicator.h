#pragma once

#include <vector>
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;
class TypeContext;

class CodeDuplicator : public OpcodeVisitor {
public:

  CodeDuplicator();

  // Incorporate CFG src into CFG dest with type constraint information
  void incorporate(ControlFlowGraph* src, TypeContext* srcTypes, ControlFlowGraph* dest, TypeContext* destTypes);

  // Duplicate a CFG
  ControlFlowGraph* duplicate(ControlFlowGraph* cfg);

  BlockHeader* entry() { return blockOf(src_->entry()); }
  BlockHeader* exit() { return blockOf(src_->exit()); }

  BlockHeader* duplicatedBlockOf(BlockHeader* block) { return blockOf(block); }
  Variable* duplicatedVariableOf(Variable* v) { return variableOf(v); }

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

  BlockHeader* blockOf(BlockHeader* srcBlock);
  Variable* variableOf(Variable* srcVariable);
  void setDefSite(Variable* v);
  void copyRhs(OpcodeVa* dest, OpcodeVa* src);

  void duplicateOpcodes();
  void duplicateTypeContext(TypeContext* srcTypes, TypeContext* destTypes);

  BlockHeader* lastBlock_;
  Opcode* lastOpcode_;

  ControlFlowGraph* src_;
  ControlFlowGraph* dest_;

  size_t blockIndexOffset_;
  size_t variableIndexOffset_;

  bool emitExit_;
};

RBJIT_NAMESPACE_END
