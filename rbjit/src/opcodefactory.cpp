#include <cassert>
#include <cstdarg>
#include "rbjit/opcodefactory.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

OpcodeFactory::OpcodeFactory(ControlFlowGraph* cfg)
  : cfg_(cfg), lastBlock_(0), lastOpcode_(0),
    file_(0), line_(0), depth_(0), halted_(false)
{}

OpcodeFactory::OpcodeFactory(ControlFlowGraph* cfg, BlockHeader* block, Opcode* opcode)
  : cfg_(cfg), lastBlock_(block), lastOpcode_(opcode),
    file_(opcode->file()), line_(opcode->line()), depth_(block->depth()),
    halted_(false)
{}

OpcodeFactory::OpcodeFactory(OpcodeFactory& factory, int)
  : cfg_(factory.cfg_), file_(factory.file_), line_(factory.line_),
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
OpcodeFactory::createNamedVariable(ID name)
{
  Variable* v = Variable::createNamed(lastBlock_, lastOpcode_, cfg_->variables_.size(), name);
  cfg_->variables_.push_back(v);
  return v;
}

Variable*
OpcodeFactory::createTemporary(bool useResult)
{
  if (!useResult) {
    return 0;
  }

  Variable* v = Variable::createUnnamedSsa(lastBlock_, lastOpcode_, cfg_->variables_.size());
  cfg_->variables_.push_back(v);
  return v;
}

void
OpcodeFactory::updateDefSite(Variable* v)
{
  // Before SSA translastion, Variable::defBlock_ and Variable::defOpcode_ are
  // not significant.  Thus update defSite_ only.
  v->defInfo()->addDefSite(lastBlock_);
}

BlockHeader*
OpcodeFactory::addFreeBlockHeader(BlockHeader* idom)
{
  BlockHeader* block = createFreeBlockHeader(idom);
  lastOpcode_ = block;
  lastBlock_ = block;

  return block;
}

Variable*
OpcodeFactory::addCopy(Variable* rhs, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, 0, rhs);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCopy(Variable* lhs, Variable* rhs, bool useResult)
{
  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, lhs, rhs);
  updateDefSite(lhs);

  lastOpcode_ = op;

  return useResult ? lhs : 0;
}

void
OpcodeFactory::addJump(BlockHeader* dest)
{
  if (!dest) {
    dest = createFreeBlockHeader(lastBlock_);
  }

  OpcodeJump* op = new OpcodeJump(file_, line_, lastOpcode_, dest);

  dest->addBackedge(lastBlock_);

  lastBlock_->setFooter(op);

  lastOpcode_ = dest;
  lastBlock_ = dest;
}

void
OpcodeFactory::addJumpIf(Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse)
{
  BlockHeader* nextBlock = ifTrue;

  if (!ifFalse) {
    ifFalse = createFreeBlockHeader(lastBlock_);
    nextBlock = ifFalse;
  }

  if (!ifTrue) {
    ifTrue = createFreeBlockHeader(lastBlock_);
    nextBlock = ifTrue;
  }

  OpcodeJumpIf* op = new OpcodeJumpIf(file_, line_, lastOpcode_, cond, ifTrue, ifFalse);

  ifTrue->addBackedge(lastBlock_);
  ifFalse->addBackedge(lastBlock_);

  lastBlock_->setFooter(op);

  lastBlock_ = nextBlock;
  lastOpcode_ = nextBlock;
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
OpcodeFactory::addEnv(bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeEnv* op = new OpcodeEnv(file_, line_, lastOpcode_, 0);
  lastOpcode_ = op;

  Variable* lhs = createNamedVariable(OpcodeEnv::envName());
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addLookup(Variable* receiver, ID methodName)
{
  OpcodeLookup* op = new OpcodeLookup(file_, line_, lastOpcode_, 0, receiver, methodName, cfg_->env());
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCall(Variable* methodEntry, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult)
{
  int n = argsEnd - argsBegin;

  Variable* env = cfg_->env();
  OpcodeCall* op = new OpcodeCall(file_, line_, lastOpcode_, 0, methodEntry, n, env);
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
  if (!useResult) {
    // Currently primitives have no side effects.
    return 0;
  }

  int n = argsEnd - argsBegin;
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, 0, name, n);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return lhs;
}

Variable*
OpcodeFactory::addPrimitive(const char* name, int argCount, ...)
{
  OpcodePrimitive* op = new OpcodePrimitive(file_, line_, lastOpcode_, 0, mri::Id(name).id(), argCount);
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);
  updateDefSite(lhs);

  va_list args;
  va_start(args, name);
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
    lhs = Variable::copy(lastBlock_, op, cfg_->variables()->size(), source->lhs());
    cfg_->variables()->push_back(lhs);
  }

  return lhs;
}

void
OpcodeFactory::createEntryExitBlocks()
{
  // entry block
  cfg_->entry_ = addFreeBlockHeader(0);
  cfg_->undefined_ = addImmediate(mri::Object::nilObject(), true);
  cfg_->env_ = addEnv(true);

  // exit block
  BlockHeader* exit = createFreeBlockHeader(0);
  OpcodeExit* op = new OpcodeExit(0, 0, exit);
  exit->setFooter(op);
  cfg_->exit_ = exit;
}

RBJIT_NAMESPACE_END
