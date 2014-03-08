#pragma once

#include <vector>
#include <string>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Opcode;
class BlockHeader;
class Variable;

class OpcodeFactory {
public:

  OpcodeFactory(ControlFlowGraph* cfg);

  // Inherits the internal state of factory and initiates a basic block
  OpcodeFactory(OpcodeFactory& factory, BlockHeader* idom);

  BlockHeader* lastBlock() const { return lastBlock_; }
  Opcode* lastOpcode() const { return lastOpcode_; }

  void setFile(const char* file); // TODO

  void addLine(int n) { line_ += n; }
  void setLine(int line) { line_ = line; }

  void addDepth(int n) { depth_ += n; }
  void setDepth(int depth) { depth_ = depth; }

  BlockHeader* addFreeBlockHeader(BlockHeader* idom);

  Variable* addCopy(Variable* rhs, bool useResult);
  Variable* addCopy(Variable* lhs, Variable* rhs, bool useResult);
  void addJump(BlockHeader* dest = 0);
  void addJumpIf(Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse);
  Variable* addImmediate(VALUE value, bool useResult);
  Variable* addEnv(bool useResult);
  Variable* addLookup(Variable* receiver, ID methodName);
  // args includes a receiver as first argument
  Variable* addCall(Variable* me, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult);
  Variable* addPhi(Variable*const* rhsBegin, Variable*const* rhsEnd, bool useResult);

  void addJumpToReturnBlock(Variable* returnValue);

  void halt() { halted_ = true; }
  bool continues() const { return !halted_; }

  void createEntryExitBlocks();

  Variable* createNamedVariable(ID name);
  Variable* createTemporary(bool useResult);
  BlockHeader* createFreeBlockHeader(BlockHeader* idom);

private:

  ControlFlowGraph* cfg_;

  BlockHeader* lastBlock_;
  Opcode* lastOpcode_;

  int file_;
  int line_;
  int depth_;

  bool halted_;
};

RBJIT_NAMESPACE_END
