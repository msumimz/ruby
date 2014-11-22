#pragma once
#include <string>
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

class Block;

class BlockDebugPrinter : public OpcodeVisitor {
public:

  BlockDebugPrinter() {}

  std::string print(Block* b);
  std::string printAsDot(Block* b);

  std::string output() const { return out_; }

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

private:

//  void printBlockHeader(BlockHeader* b);
//  void printAsDot(const ControlFlowGraph* cfg);

  void putCommonOutput(Opcode* op);
  void put(const char* format, ...);

  void printBlockHeaderAsDot(Block* b);

  std::string out_;
};

RBJIT_NAMESPACE_END