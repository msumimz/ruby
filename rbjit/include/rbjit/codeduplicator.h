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

  Block* entry() { return blockOf(src_->entryBlock()); }
  Block* exit() { return blockOf(src_->exitBlock()); }

  Block* duplicatedBlockOf(Block* block) { return blockOf(block); }
  Variable* duplicatedVariableOf(Variable* v) { return variableOf(v); }

  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeEnv* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodeCodeBlock* op);
  bool visitOpcode(OpcodeConstant* op);
  bool visitOpcode(OpcodePrimitive* op);
  bool visitOpcode(OpcodePhi* op);
  bool visitOpcode(OpcodeExit* op);
  bool visitOpcode(OpcodeArray* op);
  bool visitOpcode(OpcodeRange* op);
  bool visitOpcode(OpcodeString* op);
  bool visitOpcode(OpcodeHash* op);
  bool visitOpcode(OpcodeEnter* op);
  bool visitOpcode(OpcodeLeave* op);
  bool visitOpcode(OpcodeCheckArg* op);

private:

  Block* blockOf(Block* srcBlock);
  Variable* variableOf(Variable* srcVariable);
  void setDefSite(Variable* v, Opcode* defOpcode);
  void copyRhs(OpcodeVa* dest, OpcodeVa* src);

  void duplicateOpcodes();
  void duplicateTypeContext(TypeContext* srcTypes, TypeContext* destTypes);

  ControlFlowGraph* src_;
  ControlFlowGraph* dest_;

  Block* lastBlock_;

  size_t blockIndexOffset_;
  size_t variableIndexOffset_;

  bool emitExit_;
};

RBJIT_NAMESPACE_END
