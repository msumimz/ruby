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
#include "rbjit/runtimefunctions.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/primitivestore.h"
#include "rbjit/primitive.h"
#include "rbjit/typecontext.h"
#include "rbjit/rubyobject.h"
#include "rbjit/recompilationmanager.h"
#include "rbjit/methodinfo.h"
#include "rbjit/compilationinstance.h"

#define NOMINMAX
#include <windows.h>

RBJIT_NAMESPACE_BEGIN

struct PendingOpcode {
  Variable* variable_;
  Block* block_;
  Block::Iterator opcode_;
  llvm::BasicBlock* llvmBlock_;
  llvm::BasicBlock::iterator iter_;

  PendingOpcode(Variable* variable, Block* block, Block::Iterator opcode, llvm::BasicBlock* llvmBlock, llvm::BasicBlock::iterator iter)
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

void
NativeCompiler::setBogusValue(Variable* v)
{
  assert(llvmValues_[v->index()] == 0);
  llvmValues_[v->index()] = reinterpret_cast<llvm::Value*>(1);
}

////////////////////////////////////////////////////////////
// Translating blocks

void*
NativeCompiler::compileMethod(PrecompiledMethodInfo* mi)
{
  mi_ = mi;
  cfg_ = mi->compilationInstance()->cfg();
  typeContext_ = mi->compilationInstance()->typeContext();
  translateToBitcode();

  RecompilationManager::instance()->addCalleeCallerRelation(mi->methodName(), mi);

  return ee_->getPointerToFunction(func_);
}

void
NativeCompiler::translateToBitcode()
{
  // Set up a value vector
  size_t varCount = cfg_->variableCount();
  llvmValues_.assign(varCount, 0);

  // Define a function object
  std::vector<llvm::Type*> argTypes;
  for (auto i = cfg_->constInputBegin(), end = cfg_->constInputEnd(); i != end; ++i) {
    argTypes.push_back(valueType_);
  }
  llvm::FunctionType *ft = llvm::FunctionType::get(valueType_, argTypes, false);

  std::string name = getUniqueName(mri::Id(mi_->methodName()).name());
  func_ = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module_);

  // Initialize arguments
  int count = 0;
  for (auto i = func_->arg_begin(), end = func_->arg_end(); i != end; ++i, ++count) {
    llvmValues_[cfg_->input(count)->index()] = &*i;
  }

  // Create basic blocks in advance
  size_t blockCount = cfg_->blockCount();
  llvmBlocks_.assign(blockCount, 0);
  for (size_t i = 0; i < blockCount; ++i) {
#ifdef RBJIT_DEBUG
    std::string name = stringFormat("block%d", i);
    llvmBlocks_[i] = llvm::BasicBlock::Create(*ctx_, name.c_str(), func_);
#else
    llvmBlocks_[i] = llvm::BasicBlock::Create(*ctx_, "", func_);
#endif
  }

  // Set up a phi list
  phis_.clear();

  // Set up a pending opcode list
  opcodes_.clear();

#ifdef RBJIT_DEBUG
  // Add debugtrap
  if (false) { // IsDebuggerPresent()) {
    // declare void @llvm.debugtrap() nounwind
    llvm::Function* debugtrapFunc = llvm::Intrinsic::getDeclaration(module_, llvm::Intrinsic::debugtrap, llvm::None);
    llvm::BasicBlock* bb = llvmBlocks_[cfg_->entryBlock()->index()];
    builder_->SetInsertPoint(bb);
    builder_->CreateCall(debugtrapFunc);
  }
#endif

  // Declare runtime functions in the entry block
  llvm::BasicBlock* bb = llvmBlocks_[cfg_->entryBlock()->index()];
  builder_->SetInsertPoint(bb);
  declareRuntimeFunctions();

  // Translate each block to bitcode
  translateBlocks();

  // Fill arguments in phi nodes
  for (auto i = phis_.cbegin(), end = phis_.cend(); i != end; ++i) {
    const Phi& phi = *i;
    auto e = phi.opcode_->block()->backedgeBegin();
    for (Variable*const* i = phi.opcode_->begin(); i < phi.opcode_->end(); ++i, ++e) {
      assert(*e);
      phi.bitcode_->addIncoming(getValue(*i), llvmBlocks_[(*e)->index()]);
    }
  }

#ifdef RBJIT_DEBUG
  RBJIT_DPRINT(debugPrint());
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
NativeCompiler::declareRuntimeFunction(int index, size_t func, int argCount, bool isVarArg)
{
  std::vector<llvm::Type*> paramTypes(argCount, valueType_);
  llvm::FunctionType* ft = llvm::FunctionType::get(valueType_, paramTypes, isVarArg);
  runtime_[index] = builder_->CreateIntToPtr(getInt(func), ft->getPointerTo(0), "");
}

void
NativeCompiler::declareRuntimeFunctions()
{
  declareRuntimeFunction(RF_lookupMethod, (size_t)rbjit_lookupMethod, 2, false);
  declareRuntimeFunction(RF_callMethod, (size_t)rbjit_callMethod, 3, true);
  declareRuntimeFunction(RF_findConstant, (size_t)rbjit_findConstant, 3, false);
  declareRuntimeFunction(RF_createArray, (size_t)rbjit_createArray, 1, true);
  declareRuntimeFunction(RF_createRange, (size_t)rbjit_createRange, 3, false);
  declareRuntimeFunction(RF_duplicateString, (size_t)rbjit_duplicateString, 1, false);
  declareRuntimeFunction(RF_createHash, (size_t)rbjit_createHash, 1, true);
  declareRuntimeFunction(RF_enterMethod, (size_t)rbjit_enterMethod, 3, true);
  declareRuntimeFunction(RF_leaveMethod, (size_t)rbjit_leaveMethod, 0, true);
}

void
NativeCompiler::translateBlocks()
{
  std::vector<Block*> blocks;
  std::vector<bool> done(cfg_->blockCount(), false);

  blocks.push_back(cfg_->entryBlock());

  while (!blocks.empty() || !opcodes_.empty()) {
    if (!blocks.empty()) {
      block_ = blocks.back();
      blocks.pop_back();
      opcode_ = block_->begin();

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

    bool pending = false;
    for (; opcode_ != block_->end(); ++opcode_) {
      RBJIT_DPRINTF(("compiling block %Ix opcode %Ix\n", block_, opcode_));
      if (!(*opcode_)->accept(this)) {
	pending = true;
	break;
      }
      if (opcode_ == block_->end()) {
	break;
      }
    }

    if (!pending) {
      Block* n;
      if ((n = block_->nextAltBlock()) && !done[n->index()]) {
        blocks.push_back(n);
        done[n->index()] = true;
      }
      if ((n = block_->nextBlock()) && !done[n->index()]) {
        blocks.push_back(n);
        done[n->index()] = true;
      }
    }
  }
}

////////////////////////////////////////////////////////////
// Translating opcodes

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
  llvm::BasicBlock* dest = llvmBlocks_[op->nextBlock()->index()];
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
    llvmBlocks_[op->nextBlock()->index()],
    llvmBlocks_[op->nextAltBlock()->index()]);
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
  setBogusValue(op->lhs());
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeLookup* op)
{
  mri::MethodEntry me = op->methodEntry();
  if (!me.isNull()) {
    updateValue(op, getInt((int)me.ptr()));
    return true;
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
      RecompilationManager::instance()->addCalleeCallerRelation(mi_->methodName(), mi_);
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
  // The env will produce no code anyway
  setBogusValue(op->outEnv());

  std::vector<llvm::Value*> args(op->argCount() + 2); // +2 means methodEntry and argc
  int count = 0;

  llvm::Value* lookup = getValue(op->lookup());
  args[count++] = lookup;                      // methoEntry
  args[count++] = getInt(op->argCount() - 1);  // argc

  // arguments
   bool complete = true;
   for (auto i = op->constArgBegin(), end = op->constArgEnd(); i != end; ++i) {
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

  if (op->lhs()) {
    updateValue(op, value);
  }

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCodeBlock* op)
{
  // code blocks are processed at OpcodeCall
  setBogusValue(op->lhs());
  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeConstant* op)
{
  setBogusValue(op->outEnv());

  Variable* lhs = op->lhs();
  if (lhs) {
    TypeConstraint* type = typeContext_->typeConstraintOf(lhs);
    if (typeid(*type) == typeid(TypeConstant)) {
      updateValue(op, getInt(type->resolveToValues().first[0]));
      RecompilationManager::instance()->addConstantReferrer(op->name(), mi_);
      return true;
    }
  }
  else {
    if (typeContext_->isSameValueAs(op->inEnv(), op->outEnv())) {
      // Emit nothing if autoloading is not defined
      RecompilationManager::instance()->addConstantReferrer(op->name(), mi_);
      return true;
    }
  }

  std::vector<llvm::Value*> args(3);
  args[0] = getValue(op->base());
  args[1] = getInt(op->name());
  args[2] = getInt(reinterpret_cast<size_t>(mi_->methodEntry().methodDefinition().iseq()));
  llvm::CallInst* value = builder_->CreateCall(runtime_[RF_findConstant], args, "");
  value->setCallingConv(llvm::CallingConv::C);

  if (lhs) {
    updateValue(op, value);
  }

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

  Primitive* prim = Primitive::find(op->name());
  llvm::Value* value;
  if (prim) {
    value = prim->emit(builder_, args);
  }
  else {
    llvm::Function* f = module_->getFunction(mri::Id(op->name()).name());
    assert(f);
    llvm::CallInst* call = builder_->CreateCall(f, args);
    call->setCallingConv(llvm::CallingConv::C);
    value = call;
  }
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodePhi* op)
{
  if (OpcodeEnv::isEnv(op->lhs())) {
    setBogusValue(op->lhs());
    return true;
  }

  llvm::PHINode* phi = builder_->CreatePHI(valueType_, op->rhsCount());
  updateValue(op, phi);

  // Because the values of left-hand side arguments will be filled later, we
  // should save the phi node information here
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

bool
NativeCompiler::prepareArguments(std::vector<llvm::Value*>& args, OpcodeVa* op)
{
  int argCount = op->rhsCount();
  bool complete = true;
  for (int i = 0; i < op->rhsCount(); ++i) {
    llvm::Value* arg = getValue(op->rhs(i));
    args.push_back(arg);
    if (!arg) {
      complete = false;
    }
  }
  return complete;
}

llvm::Value*
NativeCompiler::emitCall(llvm::Value* f, const std::vector<llvm::Value*>& args)
{
  llvm::CallInst* value = builder_->CreateCall(f, args);
  value->setCallingConv(llvm::CallingConv::C);
  return value;
}

bool
NativeCompiler::visitOpcode(OpcodeArray* op)
{
  std::vector<llvm::Value*> args(1, getInt(op->rhsCount()));
  bool complete = prepareArguments(args, op);
  if (!complete) {
    return false;
  }

  llvm::Value* value = emitCall(runtime_[RF_createArray], args);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeRange* op)
{
  std::vector<llvm::Value*> args(3);
  args[0] = getValue(op->low());
  args[1] = getValue(op->high());
  if (!args[0] || !args[1]) {
    return false;
  }
  args[2] = getInt(op->exclusive());

  llvm::Value* value = emitCall(runtime_[RF_createRange], args);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeString* op)
{
  std::vector<llvm::Value*> args(1);
  args[0] = getInt(op->string());

  llvm::Value* value = emitCall(runtime_[RF_duplicateString], args);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeHash* op)
{
  std::vector<llvm::Value*> args(1, getInt(op->rhsCount()));
  bool complete = prepareArguments(args, op);
  if (!complete) {
    return false;
  }

  llvm::Value* value = emitCall(runtime_[RF_createHash], args);
  updateValue(op, value);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeEnter* op)
{
//  std::vector<llvm::Value*> args(3);
//  args[0] = getInt(reinterpret_cast<size_t>(op->thread()));
//  args[1] = getInt(reinterpret_cast<size_t>(op->controlFramePointer()));
//  args[2] = getInt(reinterpret_cast<size_t>(op->callInfo()));
//
//  emitCall(runtime_[RF_enterMethod], args);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeLeave* op)
{
//  std::vector<llvm::Value*> args;
//  llvm::Value* value = emitCall(runtime_[RF_leaveMethod], args);

  return true;
}

bool
NativeCompiler::visitOpcode(OpcodeCheckArg* op)
{
  // TODO
  return true;
}

////////////////////////////////////////////////////////////
// Bitcode loader

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
