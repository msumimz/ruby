#include <cassert>
#include <cstdarg>
#include "rbjit/opcodefactory.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/rubyobject.h"
#include "rbjit/scope.h"

RBJIT_NAMESPACE_BEGIN

OpcodeFactory::OpcodeFactory(ControlFlowGraph* cfg, Scope* scope)
  : cfg_(cfg), scope_(scope), lastBlock_(0), lastOpcode_(0),
    file_(0), line_(0), depth_(0), halted_(false)
{}

OpcodeFactory::OpcodeFactory(ControlFlowGraph* cfg, Scope* scope, BlockHeader* block, Opcode* opcode)
  : cfg_(cfg), scope_(scope), lastBlock_(block), lastOpcode_(opcode),
    file_(opcode->file()), line_(opcode->line()), depth_(block->depth()),
    halted_(false)
{}

OpcodeFactory::OpcodeFactory(OpcodeFactory& factory)
  : cfg_(factory.cfg_), scope_(factory.scope_),
    file_(factory.file_), line_(factory.line_),
    depth_(factory.depth_), halted_(false)
{
  lastBlock_ = createFreeBlockHeader(0);
  lastOpcode_ = lastBlock_;
}

BlockHeader*
OpcodeFactory::createFreeBlockHeader(BlockHeader* idom)
{
  BlockHeader* block = new BlockHeader(file_, line_, 0, 0, cfg_->blocks()->size(), depth_, idom);
  cfg_->blocks_.push_back(block);

  return block;
}

Variable*
OpcodeFactory::createNamedVariable(ID name, bool belongsToScope)
{
  NamedVariable* nameRef = nullptr;
  if (belongsToScope) {
    nameRef = scope_->find(name);
    assert(nameRef);
  }
  return cfg_->createVariableSsa(name, nameRef, lastBlock_, lastOpcode_);
}

Variable*
OpcodeFactory::createTemporary(bool useResult)
{
  if (!useResult) {
    return 0;
  }

  return cfg_->createVariableSsa(0, 0, lastBlock_, lastOpcode_);
}

void
OpcodeFactory::updateDefSite(Variable* v)
{
  if (v) {
    v->updateDefSite(lastBlock_, lastOpcode_);
  }
}

BlockHeader*
OpcodeFactory::addFreeBlockHeader(BlockHeader* idom)
{
  BlockHeader* block = createFreeBlockHeader(idom);
  lastOpcode_ = block;
  lastBlock_ = block;

  return block;
}

void
OpcodeFactory::addBlockHeader()
{
  BlockHeader* block = lastBlock_;
  BlockHeader* newBlock = addFreeBlockHeader(lastBlock_);
  block->updateJumpDestination(newBlock);
}

void
OpcodeFactory::addBlockHeaderAsTrueBlock()
{
  BlockHeader* block = lastBlock_;
  BlockHeader* trueBlock = addFreeBlockHeader(lastBlock_);
  block->updateJumpDestination(trueBlock);
}

void
OpcodeFactory::addBlockHeaderAsFalseBlock()
{
  BlockHeader* block = lastBlock_;
  BlockHeader* falseBlock = addFreeBlockHeader(lastBlock_);
  block->updateJumpAltDestination(falseBlock);
}

Variable*
OpcodeFactory::addCopy(Variable* rhs, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, 0, rhs);
  lastOpcode_ = op;

  Variable* lhs = cfg_->createVariableSsa(rhs->name(), rhs->nameRef(), lastBlock_, lastOpcode_);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCopy(Variable* lhs, Variable* rhs, bool useResult)
{
  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, lhs, rhs);
  lastOpcode_ = op;
  if (lhs) {
    updateDefSite(lhs);
  }

  return useResult ? lhs : 0;
}

void
OpcodeFactory::addJump(BlockHeader* dest)
{
  OpcodeJump* op = new OpcodeJump(file_, line_, lastOpcode_, dest);

  lastBlock_->setFooter(op);

  if (dest) {
    dest->addBackedge(lastBlock_);
    lastOpcode_ = dest;
    lastBlock_ = dest;
  }
  else {
    lastOpcode_ = op;
  }

}

void
OpcodeFactory::addJumpIf(Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse)
{
  OpcodeJumpIf* op = new OpcodeJumpIf(file_, line_, lastOpcode_, cond, ifTrue, ifFalse);

  if (ifTrue) {
    ifTrue->addBackedge(lastBlock_);
  }

  if (ifFalse) {
    ifFalse->addBackedge(lastBlock_);
  }

  lastBlock_->setFooter(op);

  lastOpcode_ = op;
}

Variable*
OpcodeFactory::addImmediate(VALUE value, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeImmediate* op = new OpcodeImmediate(file_, line_, lastOpcode_, 0, value);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addImmediate(Variable* lhs, VALUE value, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeImmediate* op = new OpcodeImmediate(file_, line_, lastOpcode_, lhs, value);
  lastOpcode_ = op;
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addEnv(bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeEnv* op = new OpcodeEnv(file_, line_, lastOpcode_, 0);
  lastOpcode_ = op;

  Variable* lhs = createNamedVariable(OpcodeEnv::envName(), false);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addEnv(Variable* env, bool useResult)
{
  OpcodeEnv* op = new OpcodeEnv(file_, line_, lastOpcode_, env);
  lastOpcode_ = op;

  updateDefSite(env);

  return useResult ? env : 0;
}

Variable*
OpcodeFactory::addLookup(Variable* receiver, ID methodName)
{
  OpcodeLookup* op = new OpcodeLookup(file_, line_, lastOpcode_, 0, receiver, methodName, cfg_->entryEnv());
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addLookup(Variable* receiver, ID methodName, mri::MethodEntry me)
{
  OpcodeLookup* op = new OpcodeLookup(file_, line_, lastOpcode_, 0, receiver, methodName, cfg_->entryEnv(), me);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCall(Variable* lookup, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult)
{
  int n = argsEnd - argsBegin;

  Variable* env = cfg_->entryEnv();
  OpcodeCall* op = new OpcodeCall(file_, line_, lastOpcode_, 0, lookup, n, env);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);
  updateDefSite(lhs);
  updateDefSite(env);

  Variable** v = op->rhsBegin();
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return lhs;
}

Variable*
OpcodeFactory::addDuplicateCall(OpcodeCall* source, Variable* lookup, bool useResult)
{
  Variable*const* argsBegin = source->rhsBegin();
  Variable*const* argsEnd = source->rhsEnd();
  int n = argsEnd - argsBegin;

  OpcodeCall* op = new OpcodeCall(source->file(), source->line(), lastOpcode_, 0, lookup, n, 0);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  Variable* outEnv = createTemporary(true);
  op->setLhs(lhs);
  op->setOutEnv(outEnv);
  updateDefSite(lhs);
  updateDefSite(outEnv);

  Variable** v = op->rhsBegin();
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return lhs;
}

Variable*
OpcodeFactory::addConstant(ID name, Variable* base, bool useResult)
{
  if (!base) {
    // Free constant lookup (CONST) is equivalent to nil::CONST in the ruby script
    base = cfg_->undefined();
  }

  // When useResult is false generate an opcode because constants may cause
  // autoloading as a side effect.
  OpcodeConstant* op = new OpcodeConstant(file_, line_, lastOpcode_, 0, base, name, false, cfg_->entryEnv(), cfg_->entryEnv());
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);
  updateDefSite(lhs);
  updateDefSite(cfg_->entryEnv());

  return lhs;
}

Variable*
OpcodeFactory::addToplevelConstant(ID name, bool useResult)
{
  OpcodeConstant* op = new OpcodeConstant(file_, line_, lastOpcode_, 0, cfg_->undefined(), name, true, cfg_->entryEnv(), cfg_->entryEnv());
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);
  updateDefSite(lhs);
  updateDefSite(cfg_->entryEnv());

  return lhs;
}

Variable*
OpcodeFactory::addPhi(Variable*const* rhsBegin, Variable*const* rhsEnd, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  int n = 1 + (rhsEnd - rhsBegin);

  OpcodePhi* op = new OpcodePhi(file_, line_, lastOpcode_, 0, n, lastBlock_);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* rhs = rhsBegin; rhs < rhsEnd; ++rhs) {
    *v++ = *rhs;
  }

  return lhs;
}

Variable*
OpcodeFactory::addPrimitive(ID name, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult)
{
  int n = argsEnd - argsBegin;
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, 0, name, n);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return lhs;
}

Variable*
OpcodeFactory::addPrimitive(ID name, const std::vector<Variable*>& args, bool useResult)
{
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, 0, name, args.size());
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (auto i = args.cbegin(), end = args.cend(); i != end; ++i) {
    *v++ = *i;
  }

  return lhs;
}

Variable*
OpcodeFactory::addPrimitive(ID name, int argCount, ...)
{
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, 0, name, argCount);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  va_list args;
  va_start(args, argCount);
  for (auto v = op->rhsBegin(), end = op->rhsEnd(); v != end; ++v) {
    *v = va_arg(args, Variable*);
  }
  va_end(args);

  return lhs;
}

Variable*
OpcodeFactory::addPrimitive(Variable* lhs, const char* name, int argCount, ...)
{
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, lhs, mri::Id(name).id(), argCount);
  lastOpcode_ = op;
  updateDefSite(lhs);

  va_list args;
  va_start(args, argCount);
  for (auto v = op->rhsBegin(), end = op->rhsEnd(); v != end; ++v) {
    *v = va_arg(args, Variable*);
  }
  va_end(args);

  return lhs;
}

void
OpcodeFactory::addJumpToReturnBlock(Variable* returnValue)
{
  if (returnValue) {
    OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, 0, returnValue);
    lastOpcode_ = op;
    if (!cfg_->output_) {
      cfg_->output_ = createTemporary(true);
    }
    op->setLhs(cfg_->output_);
    updateDefSite(cfg_->output_);
  }

  addJump(cfg_->exit());

  halted_ = true;
}

Variable*
OpcodeFactory::addCallClone(OpcodeCall* source, Variable* methodEntry)
{
  OpcodeCall* op = source->clone(lastOpcode_, methodEntry);
  lastOpcode_ = op;

  Variable* lhs = 0;
  if (source->lhs()) {
    lhs = cfg_->copyVariable(lastBlock_, op, source->lhs());
  }

  return lhs;
}

Variable*
OpcodeFactory::addArray(Variable*const*elemsBegin, Variable*const* elemsEnd, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  int n = elemsEnd - elemsBegin;

  OpcodeArray* op = new OpcodeArray(file_, line_, lastOpcode_, 0, n);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* elem = elemsBegin; elem < elemsEnd; ++elem) {
    *v++ = *elem;
  }

  return lhs;
}

Variable*
OpcodeFactory::addRange(Variable* low, Variable* high, bool exclusive, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeRange* op = new OpcodeRange(file_, line_, lastOpcode_, 0, low, high, exclusive);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addString(VALUE s, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeString* op = new OpcodeString(file_, line_, lastOpcode_, 0, s);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addHash(Variable*const*elemsBegin, Variable*const* elemsEnd, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  int n = elemsEnd - elemsBegin;
  assert(n % 2 == 0);

  OpcodeHash* op = new OpcodeHash(file_, line_, lastOpcode_, 0, n);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* elem = elemsBegin; elem < elemsEnd; ++elem) {
    *v++ = *elem;
  }

  return lhs;
}

void
OpcodeFactory::createEntryExitBlocks()
{
  ////////////////////////////////////////////////////////////
  // entry block

  BlockHeader* entry = addFreeBlockHeader(0);
  cfg_->entry_ = entry;

  // undefined
  cfg_->undefined_ = addImmediate(mri::Object::nilObject(), true);

  // env
  Variable* env = addEnv(true);
  cfg_->entryEnv_ = cfg_->exitEnv_ = env;

  // scope
  lastOpcode_ = new OpcodeEnter(0, 0, lastOpcode_, scope_);
  entry->setFooter(lastOpcode_);

  ////////////////////////////////////////////////////////////
  // exit block

  BlockHeader* exit = createFreeBlockHeader(0);
  cfg_->exit_ = exit;

  // exit env
  Opcode* op = new OpcodeCopy(0, 0, exit, env, env);
  env->updateDefSite(exit, op);

  // exit
  op = new OpcodeExit(0, 0, op);
  exit->setFooter(op);
}

RBJIT_NAMESPACE_END
