#pragma once

#include <vector>
#include <string>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Opcode;
class OpcodeCall;
class BlockHeader;
class Variable;

class OpcodeFactory {
public:

  OpcodeFactory(ControlFlowGraph* cfg);
  OpcodeFactory(ControlFlowGraph* cfg, BlockHeader* block, Opcode* opcode);

  // Inherits the internal state of the factory and initiates a basic block
  // (the second parameter is meaningless)
  OpcodeFactory(OpcodeFactory& factory, int);

  BlockHeader* lastBlock() const { return lastBlock_; }
  Opcode* lastOpcode() const { return lastOpcode_; }

  void setFile(const char* file); // TODO

  void addLine(int n) { line_ += n; }
  void setLine(int line) { line_ = line; }

  void addDepth(int n) { depth_ += n; }
  void setDepth(int depth) { depth_ = depth; }

  BlockHeader* addFreeBlockHeader(BlockHeader* idom);
  void addBlockHeader();
  void addBlockHeaderAsTrueBlock();
  void addBlockHeaderAsFalseBlock();

  Variable* addCopy(Variable* rhs, bool useResult);
  Variable* addCopy(Variable* lhs, Variable* rhs, bool useResult);
  void addJump(BlockHeader* dest = 0);
  void addJumpIf(Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse);
  Variable* addImmediate(VALUE value, bool useResult);
  Variable* addImmediate(Variable* lhs, VALUE value, bool useResult);
  Variable* addEnv(bool useResult);
  Variable* addEnv(Variable* env, bool useResult);
  Variable* addLookup(Variable* receiver, ID methodName);
  Variable* addLookup(Variable* receiver, ID methodName, mri::MethodEntry me);
  // args includes a receiver as first argument
  Variable* addCall(Variable* me, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult);
  Variable* addDuplicateCall(OpcodeCall* source, Variable* lookup, bool useResult);
  Variable* addConstant(ID name, Variable* base, bool useResult);
  Variable* addToplevelConstant(ID name, bool useResult);
  Variable* addPhi(Variable*const* rhsBegin, Variable*const* rhsEnd, bool useResult);
  Variable* addPhi(Variable* lhs, int rhsCount, bool useResult);
  Variable* addPrimitive(ID name, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult);
  Variable* addPrimitive(const char* name, int argCount, ...);
  Variable* addPrimitive(Variable* lhs, const char* name, int argCount, ...);

  void addJumpToReturnBlock(Variable* returnValue);

  Variable* addCallClone(OpcodeCall* source, Variable* methodEntry);

  void halt() { halted_ = true; }
  bool continues() const { return !halted_; }

  void createEntryExitBlocks();

  Variable* createNamedVariable(ID name);
  Variable* createTemporary(bool useResult);
  BlockHeader* createFreeBlockHeader(BlockHeader* idom);

private:

  void updateDefSite(Variable* v);

  ControlFlowGraph* cfg_;

  BlockHeader* lastBlock_;
  Opcode* lastOpcode_;

  int file_;
  int line_;
  int depth_;

  bool halted_;
};

RBJIT_NAMESPACE_END
