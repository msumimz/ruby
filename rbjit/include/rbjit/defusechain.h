#pragma once

#include <vector>
#include <string>
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;

class DefUseChain : public OpcodeVisitor {
public:

  DefUseChain(ControlFlowGraph* cfg);

  std::vector<std::pair<Block*, Variable*>>& uses(int index)
  { return uses_[index]; }
  const std::vector<std::pair<Block*, Variable*>>& uses(int index) const
  { return uses_[index]; }

  std::vector<std::pair<Block*, Variable*>>& uses(Variable* v);
  const std::vector<std::pair<Block*, Variable*>>& uses(Variable* v) const;

  bool isCondition(int index) const { return conditions_[index]; }
  bool isCondition(Variable* v) const;

  // visitors
  bool visitOpcode(Block* op);
  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeEnv* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
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

  std::string debugPrint() const;

private:

  void build();
  bool visitOpcodeVa(Variable* lhs, OpcodeVa* op);
  void addDefUseChain(Variable* def, Variable* use);

  ControlFlowGraph* cfg_;
  std::vector<std::vector<std::pair<Block*, Variable*>>> uses_;
  std::vector<bool> conditions_;
  Block* block_;
};

RBJIT_NAMESPACE_END
