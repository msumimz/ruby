#pragma once
#include <vector>
#include <string>

#include "rbjit/opcode.h"

namespace llvm {
class LLVMContext;
class Module;
class FunctionPassManager;
class Type;
class ExecutionEngine;
class Function;
class BasicBlock;
class Value;
}

RBJIT_NAMESPACE_BEGIN

class IRBuilder;

class ControlFlowGraph;

class NativeCompiler : public OpcodeVisitor {
public:

  NativeCompiler();
  ~NativeCompiler();

  static void setup();
  static NativeCompiler* instance() { return nativeCompiler_; }

  void* compileMethod(ControlFlowGraph* cfg);

  // compile
  bool visitOpcode(BlockHeader* opcode);
  bool visitOpcode(OpcodeCopy* opcode);
  bool visitOpcode(OpcodeJump* opcode);
  bool visitOpcode(OpcodeJumpIf* opcode);
  bool visitOpcode(OpcodeImmediate* opcode);
  bool visitOpcode(OpcodeCall* opcode);
  bool visitOpcode(OpcodePhi* opcode);

private:

  // bit size of VALUE
  static const int VALUE_BITSIZE = sizeof(intptr_t) * CHAR_BIT;

  void translateToBitcode();
  void translateBlocks();

  llvm::LLVMContext* ctx_;
  IRBuilder* builder_;
  llvm::Module* module_;
  llvm::FunctionPassManager* fpm_;
  llvm::Type* valueType_;
  llvm::ExecutionEngine* ee_;
  llvm::Function* func_;
  std::string errorMessage_;

  ControlFlowGraph* cfg_;
  std::vector<llvm::BasicBlock*> llvmBlocks_;
  std::vector<llvm::Value*> llvmValues_;

  enum { WAITING, WORKING, DONE };
  std::vector<int> states_;

  static bool initialized_;
  static NativeCompiler* nativeCompiler_;
};

RBJIT_NAMESPACE_END
