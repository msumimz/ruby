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
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#ifndef _NDEBUG
#include "llvm/Support/raw_ostream.h"
#endif

#include "rbjit/nativecompiler.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/variable.h"

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
    valueType_(llvm::Type::getIntNTy(*ctx_, VALUE_BITSIZE))
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

void*
NativeCompiler::compileMethod(ControlFlowGraph* cfg)
{
  cfg_ = cfg;
  translateToBitcode();
  return ee_->getPointerToFunction(func_);
}

void
NativeCompiler::translateToBitcode()
{
  std::vector<llvm::Type*> argTypes;
  llvm::FunctionType *ft = llvm::FunctionType::get(valueType_, argTypes, false);

  func_ = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "f", module_);

  // create basic blocks
  size_t blockCount = cfg_->blocks()->size();
  llvmBlocks_.resize(blockCount, 0);
  for (size_t i = 0; i < blockCount; ++i) {
    llvmBlocks_[i] = llvm::BasicBlock::Create(*ctx_, "", func_);
  }

  // states list
  states_.resize(blockCount, WAITING);

  // prepare value vector
  size_t varCount = cfg_->variables()->size();
  llvmValues_.resize(varCount, 0);

  // suspend translating the exit block
  BlockHeader* exitBlock = cfg_->exit();
  states_[exitBlock->index()] = WORKING;

  // translate each block to bitcode
  translateBlocks();

  // create exit block
  // exit block is always empty and its bitcode should consist of a single ret
  visitOpcode(exitBlock);
  builder_->CreateRet(llvmValues_[cfg_->output()->index()]);
  states_[exitBlock->index()] = DONE;

#ifdef RBJIT_DEBUG
  std::string bitcode;
  llvm::raw_string_ostream out(bitcode);
  func_->print(out);
  fprintf(stderr, "%s\n", out.str().c_str());
  llvm::verifyFunction(*func_);
#endif

  // optimize
  fpm_->run(*func_);
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
  while (llvmValues_[op->rhs()->index()] == 0) {
    translateBlocks();
  }
  llvmValues_[op->lhs()->index()] = llvmValues_[op->rhs()->index()];
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
  // RTEST
  llvm::Value* cond = builder_->CreateAnd(llvmValues_[op->cond()->index()], llvm::ConstantInt::get(*ctx_, llvm::APInt(VALUE_BITSIZE, ~Qnil)));
  builder_->CreateCondBr(cond,
    llvmBlocks_[op->ifTrue()->index()],
    llvmBlocks_[op->ifFalse()->index()]);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeImmediate* op)
{
  llvm::Value* value = llvm::ConstantInt::get(*ctx_, llvm::APInt(VALUE_BITSIZE, op->value()));
  llvmValues_[op->lhs()->index()] = value;
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCall* op)
{
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodePhi* op)
{
  return true;
}

RBJIT_NAMESPACE_END
