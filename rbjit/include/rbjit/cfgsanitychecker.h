#pragma once
#include <vector>
#include <string>
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;

class CfgSanityChecker : public OpcodeVisitor {
public:

  CfgSanityChecker(const ControlFlowGraph* cfg);

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

  void check();

  const std::vector<std::string>& errors() const { return errors_; }

private:

  void addBlock(Block* block);
  bool canContinue();
  void addError(const char* format, ...);
  void addError(Opcode* op, const char* format, ...);

  void checkLhs(OpcodeL* op, bool nullable);
  template <class OP> void checkRhs(OP op, int pos, bool nullable);
  template <class OP> void checkRhs(OP op, bool nullable);

  bool checkBlock(Block* block);

  const ControlFlowGraph* cfg_;
  std::vector<bool> visitedVariables_;
  std::vector<bool> visitedBlocks_;

  std::vector<Block*> work_;
  Block* current_;

  std::vector<std::string> errors_;
};

RBJIT_NAMESPACE_END
