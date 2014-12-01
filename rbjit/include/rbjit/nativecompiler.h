#pragma once
#include <vector>
#include <deque>
#include <string>
#include <unordered_set>

#include "rbjit/opcode.h"
#include "rbjit/block.h"

namespace llvm {
class LLVMContext;
class Module;
class FunctionPassManager;
class PassManager;
class Type;
class ExecutionEngine;
class Function;
class BasicBlock;
class Value;
class PHINode;
}

RBJIT_NAMESPACE_BEGIN

class IRBuilder;

class PrecompiledMethodInfo;
class ControlFlowGraph;
class TypeContext;

struct PendingOpcode;

class NativeCompiler : public OpcodeVisitor {
public:

  NativeCompiler();
  ~NativeCompiler();

  static void setup();
  static NativeCompiler* instance() { return nativeCompiler_; }

  void* compileMethod(PrecompiledMethodInfo* mi);

  std::string debugPrint();

  // Compile opcode
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

  // bit size of the MRI's VALUE type
  static const int VALUE_BITSIZE = sizeof(size_t) * CHAR_BIT;

  // Helper methods

  std::string getUniqueName(const char* baseName) const;
  llvm::Type* getValueType() const;
  llvm::Value* getInt(size_t value) const;
  llvm::Value* getValue(Variable* v);
  void updateValue(OpcodeL* op, llvm::Value* value);
  void setBogusValue(Variable* v);

  void translateToBitcode();
  void translateBlocks();

  void declareRuntimeFunction(int index, size_t func, int argCount, bool isVarArg);
  void declareRuntimeFunctions();

  bool prepareArguments(std::vector<llvm::Value*>& args, OpcodeVa* op);
  llvm::Value* emitCall(llvm::Value* f, const std::vector<llvm::Value*>& args);

  llvm::Module* loadBitcode();

  // Shared with every compilation session

  llvm::LLVMContext* ctx_;
  IRBuilder* builder_;
  llvm::Module* module_;
  llvm::FunctionPassManager* fpm_;
  llvm::PassManager* mpm_;
  llvm::Type* valueType_;
  llvm::ExecutionEngine* ee_;
  std::string errorMessage_;

  std::unordered_set<std::string> nameList_;

  // Updated with each compilation session

  PrecompiledMethodInfo* mi_;
  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

  llvm::Function* func_;
  std::vector<llvm::BasicBlock*> llvmBlocks_;
  std::vector<llvm::Value*> llvmValues_;
  Block* block_;
  Block::Iterator opcode_;
  std::deque<PendingOpcode*> opcodes_;

  class Phi {
  public:
    Phi(OpcodePhi* opcode, llvm::PHINode* bitcode)
      : opcode_(opcode), bitcode_(bitcode)
    {}
    OpcodePhi* opcode_;
    llvm::PHINode* bitcode_;
  };
  std::vector<Phi> phis_;

  // Runtime functions
  enum {
    RF_lookupMethod,
    RF_callMethod,
    RF_findConstant,
    RF_createArray,
    RF_createRange,
    RF_duplicateString,
    RF_createHash,
    RF_enterMethod,
    RF_leaveMethod,
    RUNTIME_FUNCTION_COUNT
  };
  llvm::Value* runtime_[RUNTIME_FUNCTION_COUNT];

  // Singleton
  static bool initialized_;
  static NativeCompiler* nativeCompiler_;
};

RBJIT_NAMESPACE_END
