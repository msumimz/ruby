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
#include "llvm/Transforms/IPO.h"

// for bitcode loader
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/MemoryBuffer.h"

#include "llvm/Support/raw_ostream.h"

#include "rbjit/nativecompiler.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/variable.h"
#include "rbjit/debugprint.h"
#include "rbjit/runtime_methodcall.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/primitivestore.h"
#include "rbjit/typecontext.h"
#include "rbjit/rubyobject.h"

#define NOMINMAX
#include <Windows.h>

RBJIT_NAMESPACE_BEGIN

struct PendingOpcode {
  Variable* variable_;
  BlockHeader* block_;
  Opcode* opcode_;
  llvm::BasicBlock* llvmBlock_;
  llvm::BasicBlock::iterator iter_;

  PendingOpcode(Variable* variable, BlockHeader* block, Opcode* opcode, llvm::BasicBlock* llvmBlock, llvm::BasicBlock::iterator iter)
    : variable_(variable), block_(block), opcode_(opcode), llvmBlock_(llvmBlock), iter_(iter)
  {}
};

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
    module_(loadBitcode()),
    builder_(static_cast<IRBuilder*>(new llvm::IRBuilder<false>(*ctx_))),
    fpm_(new llvm::FunctionPassManager(module_)),
    mpm_(new llvm::PassManager()),
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

  mpm_->add(llvm::createAlwaysInlinerPass());
}

NativeCompiler::~NativeCompiler()
{
  // neccesary? - Not sure who holds ee's ownership
  // delete ee_;
  // delete valueType_;
  delete mpm_;
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
  llvm::Value* value = llvmValues_[v->index()];
  if (!value) {
    RBJIT_DPRINTF(("Block %Ix is blocked by variable %Ix\n", block_, v));
    llvm::BasicBlock* b = builder_->GetInsertBlock();
    llvm::BasicBlock::iterator iter = builder_->GetInsertPoint();
    opcodes_.push_back(new PendingOpcode(v, block_, opcode_, b, iter));
    return nullptr;
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
NativeCompiler::compileMethod(ControlFlowGraph* cfg, TypeContext* typeContext, const char* name)
{
  cfg_ = cfg;
  typeContext_ = typeContext;
  funcName_ = name;
  translateToBitcode();
  return ee_->getPointerToFunction(func_);
}

void
NativeCompiler::translateToBitcode()
{
  // Set up a value vector
  size_t varCount = cfg_->variables()->size();
  llvmValues_.assign(varCount, 0);

  // Define a function object
  std::vector<llvm::Type*> argTypes;
  for (auto i = cfg_->inputs()->cbegin(), end = cfg_->inputs()->cend(); i != end; ++i) {
    argTypes.push_back(valueType_);
  }
  llvm::FunctionType *ft = llvm::FunctionType::get(valueType_, argTypes, false);

  std::string name = getUniqueName(funcName_);
  func_ = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module_);

  // Initialize arguments
  std::vector<Variable*>* inputs = cfg_->inputs();
  int count = 0;
  for (auto i = func_->arg_begin(), end = func_->arg_end(); i != end; ++i, ++count) {
    llvmValues_[(*inputs)[count]->index()] = &*i;
  }

  // Create basic blocks in advance
  size_t blockCount = cfg_->blocks()->size();
  llvmBlocks_.assign(blockCount, 0);
  for (size_t i = 0; i < blockCount; ++i) {
    llvmBlocks_[i] = llvm::BasicBlock::Create(*ctx_, "", func_);
  }

  // Set up a phi list
  phis_.clear();

  // Set up a pending opcode list
  opcodes_.clear();

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

  // Translate each block to bitcode
  translateBlocks();

  // Fill arguments in phi nodes
  for (auto i = phis_.cbegin(), end = phis_.cend(); i != end; ++i) {
    const Phi& phi = *i;
    BlockHeader::Backedge* e = phi.opcode_->block()->backedge();
    for (Variable*const* i = phi.opcode_->rhsBegin(); i < phi.opcode_->rhsEnd(); ++i) {
      assert(e && e->block());
      phi.bitcode_->addIncoming(getValue(*i), llvmBlocks_[e->block()->index()]);
      e = e->next();
    }
  };

#ifdef RBJIT_DEBUG
  llvm::verifyFunction(*func_);
#endif

  // Optimization
  // NOTE: Investigation is necessary about performance hit by applying the
  // module-wide optimization (module pass) to each method complication. The
  // module-wide optimization is required for inlining, and inlining is
  // important to execute primitives efficiently. One of possible alternatives
  // is to build the primitives' IRs on each compile-time instead of loading
  // the precompiled bitcode beforehand.
  mpm_->run(*module_);
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

  // VALUE rbjit_callMethod(rb_method_entry_t* me, int argc, VALUE receiver, ...)
  paramTypes.assign(3, valueType_);
  ft = llvm::FunctionType::get(valueType_, paramTypes, true);
  runtime_[RF_callMethod] = builder_->CreateIntToPtr(
    getInt((size_t)rbjit_callMethod), ft->getPointerTo(0), "");
}

void
NativeCompiler::translateBlocks()
{
  std::vector<BlockHeader*> blocks;
  std::vector<bool> done(cfg_->blocks()->size(), false);

  blocks.push_back(cfg_->entry());

  while (!blocks.empty() || !opcodes_.empty()) {
    if (!blocks.empty()) {
      opcode_ = block_ = blocks.back();
      blocks.pop_back();

      llvm::BasicBlock* bb = llvmBlocks_[block_->index()];
      builder_->SetInsertPoint(bb);
    }
    else {
      assert(!opcodes_.empty());

      PendingOpcode* p = opcodes_.back();
      opcodes_.pop_back();
      PendingOpcode* first = p;
      while (!llvmValues_[p->variable_->index()]) {
        opcodes_.push_front(p);
        p = opcodes_.back();
        opcodes_.pop_back();
        assert(p != first);
      }

      block_ = p->block_;
      opcode_ = p->opcode_;
      builder_->SetInsertPoint(p->llvmBlock_, p->iter_);

      delete p;
    }

    Opcode* footer = block_->footer();
    bool pending = false;
    do {
      opcode_ = opcode_->next();
      if (!opcode_) {
	break;
      }
      if (!opcode_->accept(this)) {
        pending = true;
        break;
      }
    } while (opcode_ != footer);

    if (!pending) {
      BlockHeader* n;
      if ((n = footer->nextAltBlock()) && !done[n->index()]) {
        blocks.push_back(n);
        done[n->index()] = true;
      }
      if ((n = footer->nextBlock()) && !done[n->index()]) {
        blocks.push_back(n);
        done[n->index()] = true;
      }
    }
  }
}

////////////////////////////////////////////////////////////
// Translating opcodes

bool
NativeCompiler::visitOpcode(BlockHeader* block)
{
  RBJIT_UNREACHABLE;
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCopy* op)
{
  llvm::Value* rhs = getValue(op->rhs());
  if (!rhs) {
    return false;
  }

  updateValue(op, rhs);
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
  llvm::Value* cond = getValue(op->cond());
  if (!cond) {
    return false;
  }

  // Represent RTEST in bitcode
  // In ruby/ruby.h:
  // #define RTEST(v) !(((VALUE)(v) & ~Qnil) == 0)
  llvm::Value* rtest = builder_->CreateAnd(cond, getInt(~mri::Object::nilObject()));
  llvm::Value* cmp = builder_->CreateICmpNE(rtest, getInt(0));
  builder_->CreateCondBr(cmp,
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
NativeCompiler::visitOpcode(OpcodeEnv* op)
{
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeLookup* op)
{
  mri::MethodEntry me = op->methodEntry();
  if (!me.isNull()) {
    return getInt((int)me.ptr());
  }

  llvm::Value* rhs = getValue(op->rhs());
  if (!rhs) {
    return false;
  }

  llvm::Value* value = 0;

  // Try compile-time lookup
  TypeLookup* type = dynamic_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lhs()));
  if (type && type->candidates().size() == 1) {
    mri::MethodEntry me = type->candidates()[0];
    if (!me.isNull()) {
      value = getInt((int)me.ptr());
    }
  }

  if (!value) {
    value = builder_->CreateCall2(runtime_[RF_lookupMethod],
      rhs, getInt(op->methodName()), "");
  }

  updateValue(op, value);
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCall* op)
{
  std::vector<llvm::Value*> args(op->rhsCount() + 2);
  int count = 0;

  llvm::Value* lookup = getValue(op->lookup());
  args[count++] = lookup;                      // methoEntry
  args[count++] = getInt(op->rhsCount() - 1);  // argc

  // arguments
   bool complete = true;
   Variable*const* i = op->rhsBegin();
   Variable*const* rhsEnd = op->rhsEnd();
   for (; i < rhsEnd; ++i) {
    llvm::Value* arg = getValue(*i);
    args[count++] = arg;
    if (!arg) {
      complete = false;
    }
  }
  if (!lookup || !complete) {
    return false;
  }

  // call instruction
  llvm::CallInst* value = builder_->CreateCall(runtime_[RF_callMethod], args, "");
  value->setCallingConv(llvm::CallingConv::C);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodePrimitive* op)
{
  int argCount = op->rhsCount();
  std::vector<llvm::Value*> args(argCount);
  bool complete = true;
  for (int i = 0; i < op->rhsCount(); ++i) {
    llvm::Value* arg = getValue(op->rhs(i));
    args[i] = arg;
    if (!arg) {
      complete = false;
    }
  }
  if (!complete) {
    return false;
  }

  llvm::Function* f = module_->getFunction(mri::Id(op->name()).name());
  assert(f);
  llvm::CallInst* value = builder_->CreateCall(f, args);
  value->setCallingConv(llvm::CallingConv::C);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodePhi* op)
{
  if (OpcodeEnv::isEnv(op->lhs())) {
    return true;
  }

  llvm::PHINode* phi = builder_->CreatePHI(valueType_, op->rhsCount());
  updateValue(op, phi);

  // Left-hand side arguments will be filled later, saving the phi node information here
  phis_.push_back(Phi(op, phi));

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeExit* op)
{
  llvm::Value* output = getValue(cfg_->output());
  if (!output) {
    return false;
  }

  builder_->CreateRet(output);
  return true;
}

llvm::Module*
NativeCompiler::loadBitcode()
{
  void* p;
  size_t size;
  PrimitiveStore::instance()->load(&p, &size);

  llvm::MemoryBuffer* mem =
    llvm::MemoryBuffer::getMemBuffer(llvm::StringRef((const char*)p, size), "precompiled methods", false);

  std::string errorMessage;
  llvm::Module* mod = llvm::ParseBitcodeFile(mem, *ctx_, &errorMessage);

  if (!mod) {
    fputs(errorMessage.c_str(), stderr);
    abort();
  }

  delete mem;

  PrimitiveStore::instance()->buildLookupMap(mod);

  return mod;
}

////////////////////////////////////////////////////////////
// debugPrint

std::string
NativeCompiler::debugPrint()
{
  std::string bitcode;
  llvm::raw_string_ostream out(bitcode);
  func_->print(out);

  // skip the first newline
  return out.str().substr(1, std::string::npos);
}

RBJIT_NAMESPACE_END
