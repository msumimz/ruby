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

  std::vector<Variable*>& uses(int index) { return uses_[index]; }
  const std::vector<Variable*>& uses(int index) const { return uses_[index]; }

  std::vector<Variable*>& uses(Variable* v);
  const std::vector<Variable*>& uses(Variable* v) const;

  bool isCondition(int index) const { return conditions_[index]; }
  bool isCondition(Variable* v) const;

  // visitors
  bool visitOpcodeVa(OpcodeVa* op);
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

  std::string debugPrint() const;

private:

  void build();

  ControlFlowGraph* cfg_;
  std::vector<std::vector<Variable*> > uses_;
  std::vector<bool> conditions_;
};

RBJIT_NAMESPACE_END
