#include <cassert>

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#ifdef RBJIT_DEBUG
#include "llvm/Support/raw_ostream.h"
#endif

#include "rbjit/nativecompiler.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/variable.h"
#include "rbjit/debugprint.h"
#include "rbjit/runtime_methodcall.h"

#include "ruby.h"

RBJIT_NAMESPACE_BEGIN

class IRBuilder: public llvm::IRBuilder<false> {};

bool NativeCompiler::initialized_ = false;
NativeCompiler* NativeCompiler::nativeCompiler_ = 0;

void
NativeCompiler::setup()
{
  // thread-unsafe
  if (!initialized_) {
    initialized_ = true;
    llvm::InitializeNativeTarget();
    nativeCompiler_ = new NativeCompiler();
  }
}

NativeCompiler::NativeCompiler()
  : ctx_(new llvm::LLVMContext()),
    module_(new llvm::Module("precompiled method", *ctx_)),
    builder_(static_cast<IRBuilder*>(new llvm::IRBuilder<false>(*ctx_))),
    fpm_(new llvm::FunctionPassManager(module_)),
    valueType_(getValueType())
{
  assert(initialized_);

  // Setting EngineKind to JIT makes sure to prevent LVMInterpreter.lib from
  // being linked unneccesarily.
  ee_ = llvm::EngineBuilder(module_)
    .setEngineKind(llvm::EngineKind::JIT)
    .setErrorStr(&errorMessage_)
    .create();
  assert(ee_);

  // copied from the Kaleidoscope example

  // target lays out data structures.
  fpm_->add(new llvm::DataLayout(*ee_->getDataLayout()));
  // Provide basic AliasAnalysis support for GVN.
  fpm_->add(llvm::createBasicAliasAnalysisPass());
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  fpm_->add(llvm::createInstructionCombiningPass());
  // Reassociate expressions.
  fpm_->add(llvm::createReassociatePass());
  // Eliminate Common SubExpressions.
  fpm_->add(llvm::createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  fpm_->add(llvm::createCFGSimplificationPass());

  fpm_->doInitialization();
}

NativeCompiler::~NativeCompiler()
{
  // neccesary? - Not sure who holds ee's ownership
  // delete ee_;
  // delete valueType_;
  delete fpm_;
  delete builder_;
  delete module_;
  delete ctx_;
}

////////////////////////////////////////////////////////////
// Helper methods

llvm::Type*
NativeCompiler::getValueType() const
{
  return llvm::Type::getIntNTy(*ctx_, VALUE_BITSIZE);
}

std::string
NativeCompiler::getUniqueName(const char* baseName) const
{
  std::string n = baseName;
  int count = 1;
  while (nameList_.find(n) != nameList_.end()) {
    n = baseName;
    n += '_';
    n += count;
  }

  return n;
}

llvm::Value*
NativeCompiler::getInt(size_t value) const
{
  return llvm::ConstantInt::get(*ctx_, llvm::APInt(VALUE_BITSIZE, value));
}

llvm::Value*
NativeCompiler::getValue(Variable* v)
{
  int i = v->index();
  llvm::Value* value;
  while ((value = llvmValues_[i]) == 0) {
    translateBlocks();
  }
  return value;
}

void
NativeCompiler::updateValue(OpcodeL* op, llvm::Value* value)
{
  assert(llvmValues_[op->lhs()->index()] == 0);
  llvmValues_[op->lhs()->index()] = value;
}

////////////////////////////////////////////////////////////
// Translating blocks

void*
NativeCompiler::compileMethod(ControlFlowGraph* cfg, const char* name)
{
  cfg_ = cfg;
  funcName_ = name;
  translateToBitcode();
  return ee_->getPointerToFunction(func_);
}

void
NativeCompiler::translateToBitcode()
{
  std::vector<llvm::Type*> argTypes;
  llvm::FunctionType *ft = llvm::FunctionType::get(valueType_, argTypes, false);

  std::string name = getUniqueName(funcName_);
  func_ = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module_);

  // Create basic blocks in advance
  size_t blockCount = cfg_->blocks()->size();
  llvmBlocks_.assign(blockCount, 0);
  for (size_t i = 0; i < blockCount; ++i) {
    llvmBlocks_[i] = llvm::BasicBlock::Create(*ctx_, "", func_);
  }

  // Value vector
  size_t varCount = cfg_->variables()->size();
  llvmValues_.assign(varCount, 0);

  // States list
  states_.assign(blockCount, WAITING);

#ifdef RBJIT_DEBUG
  // Add debugtrap
  if (IsDebuggerPresent()) {
    // declare void @llvm.debugtrap() nounwind
    llvm::Function* debugtrapFunc = llvm::Intrinsic::getDeclaration(module_, llvm::Intrinsic::debugtrap, llvm::None);
    llvm::BasicBlock* bb = llvmBlocks_[cfg_->entry()->index()];
    builder_->SetInsertPoint(bb);
    builder_->CreateCall(debugtrapFunc);
  }
#endif

  // Declare runtime functions in the entry block
  llvm::BasicBlock* bb = llvmBlocks_[cfg_->entry()->index()];
  builder_->SetInsertPoint(bb);
  declareRuntimeFunctions();

  // Suspend translating the exit block
  BlockHeader* exitBlock = cfg_->exit();
  states_[exitBlock->index()] = WORKING;

  // Translate each block to bitcode
  translateBlocks();

  // Create the exit block
  // The exit block is always empty and its bitcode should consist of a single ret
  visitOpcode(exitBlock);
  builder_->CreateRet(llvmValues_[cfg_->output()->index()]);
  states_[exitBlock->index()] = DONE;

#ifdef RBJIT_DEBUG
  std::string bitcode;
  llvm::raw_string_ostream out(bitcode);
  func_->print(out);
  RBJIT_DPRINTLN(out.str());
  llvm::verifyFunction(*func_);
#endif

  // Optimize
  fpm_->run(*func_);
}

void
NativeCompiler::declareRuntimeFunctions()
{
  // rb_method_entry_t* rbjit_lookupMethod(VALUE receiver, ID methodName)
  std::vector<llvm::Type*> paramTypes(2, valueType_);
  llvm::FunctionType* ft = llvm::FunctionType::get(valueType_, paramTypes, false);
  runtime_[RF_lookupMethod] = builder_->CreateIntToPtr(
    getInt((size_t)rbjit_lookupMethod), ft->getPointerTo(0), "");

  // VALUE rbjit_callMethod(rb_method_entry_t* me, VALUE receiver, int argc, ...)
  paramTypes.assign(3, valueType_);
  ft = llvm::FunctionType::get(valueType_, paramTypes, true);
  runtime_[RF_callMethod] = builder_->CreateIntToPtr(
    getInt((size_t)rbjit_callMethod), ft->getPointerTo(0), "");
}

void
NativeCompiler::translateBlocks()
{
  size_t blockCount = cfg_->blocks()->size();
  for (size_t i = 0; i < blockCount; ++i) {
    if (states_[i] == WAITING) {
      states_[i] = WORKING;
      BlockHeader* block = (*cfg_->blocks())[i];
      block->visitEachOpcode(this);
      states_[i] = DONE;
    }
  }
}

////////////////////////////////////////////////////////////
// Translating opcodes

bool
NativeCompiler::visitOpcode(BlockHeader* block)
{
  llvm::BasicBlock* bb = llvmBlocks_[block->index()];
  builder_->SetInsertPoint(bb);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCopy* op)
{
  updateValue(op, getValue(op->rhs()));
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeJump* op)
{
  llvm::BasicBlock* dest = llvmBlocks_[op->dest()->index()];
  builder_->CreateBr(dest);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeJumpIf* op)
{
  // In ruby/ruby.h
  // #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
  llvm::Value* rtest = builder_->CreateAnd(getValue(op->cond()), getInt(~Qnil));
  llvm::Value* cond = builder_->CreateICmpNE(rtest, getInt(0));
  builder_->CreateCondBr(cond,
    llvmBlocks_[op->ifTrue()->index()],
    llvmBlocks_[op->ifFalse()->index()]);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeImmediate* op)
{
  updateValue(op, getInt(op->value()));
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeLookup* op)
{
  llvm::Value* value = builder_->CreateCall2(runtime_[RF_lookupMethod],
    getValue(op->rhs()), getInt(op->methodName()), "");
  updateValue(op, value);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCall* op)
{
  std::vector<llvm::Value*> args(op->rhsCount() + 2);
  int count = 0;
  Variable*const* i = op->rhsBegin();
  Variable*const* rhsEnd = op->rhsEnd();

  args[count++] = getValue(op->methodEntry()); // methoEntry
  args[count++] = getValue(*i++);              // receiver
  args[count++] = getInt(op->rhsCount() - 1);  // argc

  // arguments
  for (; i < rhsEnd; ++i) {
    args[count++] = getValue(*i);
  }

  // call instruction
  llvm::CallInst* value = builder_->CreateCall(runtime_[RF_callMethod], args, "");
  value->setCallingConv(llvm::CallingConv::C);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodePhi* op)
{
  llvm::PHINode* phi = builder_->CreatePHI(valueType_, op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    phi->addIncoming(getValue(*i), llvmBlocks_[(*i)->defBlock()->index()]);
  }
  updateValue(op, phi);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeExit* op)
{
  return true;
}

RBJIT_NAMESPACE_END
