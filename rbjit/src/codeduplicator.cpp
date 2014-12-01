#include "rbjit/codeduplicator.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/typecontext.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

CodeDuplicator::CodeDuplicator()
  : lastBlock_(0), src_(0), dest_(0),
    blockIndexOffset_(0), variableIndexOffset_(0),
    emitExit_(false)
{}

Block*
CodeDuplicator::blockOf(Block* srcBlock)
{
  return dest_->block(srcBlock->index() + blockIndexOffset_);
}

Variable*
CodeDuplicator::variableOf(Variable* srcVariable)
{
  if (!srcVariable) {
    return nullptr;
  }
  return dest_->variable(srcVariable->index() + variableIndexOffset_);
}

void
CodeDuplicator::setDefSite(Variable* lhs, Opcode* defOpcode)
{
  if (!lhs) {
    return;
  }
  lhs->defOpcode_ = defOpcode;
}

void
CodeDuplicator::copyRhs(OpcodeVa* dest, OpcodeVa* src)
{
  int count = 0;
  for (auto i = src->begin(), end = src->end(); i < end; ++i) {
    dest->setRhs(count++, variableOf(*i));
  }
}

void
CodeDuplicator::incorporate(ControlFlowGraph* src, TypeContext* srcTypes, ControlFlowGraph* dest, TypeContext* destTypes)
{
  src_ = src;
  dest_ = dest;
  emitExit_ = false;

  duplicateOpcodes();
  duplicateTypeContext(srcTypes, destTypes);
}

ControlFlowGraph*
CodeDuplicator::duplicate(ControlFlowGraph* cfg)
{
  ControlFlowGraph* newCfg = new ControlFlowGraph;

  src_ = cfg;
  dest_ = newCfg;
  emitExit_ = true;

  duplicateOpcodes();

  newCfg->entryBlock_ = entry();
  newCfg->exitBlock_ = exit();
  newCfg->entryEnv_ = variableOf(cfg->entryEnv());
  newCfg->exitEnv_ = variableOf(cfg->exitEnv());
  newCfg->output_ = variableOf(cfg->output());
  newCfg->undefined_ = variableOf(cfg->undefined());

  // Input variables
  newCfg->inputs_.resize(cfg->inputs_.size(), nullptr);
  auto n = &newCfg->inputs_[0];
  for (auto i = cfg->constInputBegin(), end = cfg->constInputEnd(); i != end; ++i) {
    *n++ = variableOf(*i);
  }

  return newCfg;
}

void
CodeDuplicator::duplicateOpcodes()
{
  // Allocate space for new blocks
  blockIndexOffset_ = dest_->blockCount();
  dest_->blocks_.insert(dest_->blocks_.end(), src_->blockCount(), nullptr);

  // Pre-create blocks
  for (auto i = src_->cbegin(), end = src_->cend(); i != end; ++i) {
    Block* b = *i;
    Block* block = new Block();

#ifdef RBJIT_DEBUG
    block->setDebugName(stringFormat("dup_%Ix_%d", src_, b->index()).c_str());
#endif
    block->setIndex(b->index() + blockIndexOffset_);
    dest_->blocks_[b->index() + blockIndexOffset_] = block;
  }

  // Allocate space for new variables
  variableIndexOffset_ = dest_->variableCount();
  dest_->variables_.insert(dest_->variables_.end(), src_->variableCount(), nullptr);

  // Pre-create variables
  for (auto i = src_->variableBegin(), end = src_->variableEnd(); i != end; ++i) {
    Variable* v = *i;
    Block* defBlock = blockOf(v->defBlock());
//    Variable* original = variableOf(v->original());
    Variable* newVar = new Variable(
      v->name(), v->nameRef(), nullptr, defBlock, nullptr);
    newVar->setIndex(v->index() + variableIndexOffset_);
    dest_->variables_[v->index() + variableIndexOffset_] = newVar;
  }

  // Main loop: iterate over the control flow graph and copy blocks
  for (auto i = src_->cbegin(), end = src_->cend(); i != end; ++i) {
    lastBlock_ = blockOf(*i);
    (*i)->visitEachOpcode(this);
//    RBJIT_DPRINT(src_->debugPrintBlock(blockOf(*i)));
  }
}

void
CodeDuplicator::duplicateTypeContext(TypeContext* srcTypes, TypeContext* destTypes)
{
  destTypes->fitSizeToCfg();
  for (auto i = src_->variableBegin(), end = src_->variableEnd(); i != end; ++i) {
    Variable* v = *i;
    Variable* w = variableOf(v);
    TypeConstraint* t = srcTypes->typeConstraintOf(v);
    if (typeid(*t) == typeid(TypeSameAs)) {
      t = TypeSameAs::create(destTypes, w);
    }
    else {
      t = t->clone();
    }
    assert(!destTypes->typeConstraintOf(w));
    destTypes->setTypeConstraint(w, t);
  }
}

bool
CodeDuplicator::visitOpcode(OpcodeCopy* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  Variable* rhs = variableOf(op->rhs());
  RBJIT_ASSUME(rhs);
  OpcodeCopy* newOp = new OpcodeCopy(op->sourceLocation(), lhs, rhs);
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeJump* op)
{
  Block* dest = blockOf(op->nextBlock());
  OpcodeJump* newOp = new OpcodeJump(op->sourceLocation(), dest);

  lastBlock_->addOpcode(newOp);
  dest->addBackedge(lastBlock_);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeJumpIf* op)
{
  Block* ifTrue = blockOf(op->nextBlock());
  Block* ifFalse = blockOf(op->nextAltBlock());
  OpcodeJumpIf* newOp = new OpcodeJumpIf(op->sourceLocation(), variableOf(op->cond()), ifTrue, ifFalse);

  lastBlock_->addOpcode(newOp);
  ifTrue->addBackedge(lastBlock_);
  ifFalse->addBackedge(lastBlock_);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeImmediate* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeImmediate* newOp = new OpcodeImmediate(op->sourceLocation(), lhs, op->value());
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeEnv* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeEnv* newOp = new OpcodeEnv(op->sourceLocation(), lhs);
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeLookup* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeLookup* newOp = new OpcodeLookup(op->sourceLocation(), lhs, variableOf(op->receiver()), op->methodName(), variableOf(op->env()));
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCall* op)
{
  Variable* lhs = variableOf(op->lhs());
  Variable* lookup = variableOf(op->lookup());
  Variable* codeBlock = variableOf(op->codeBlock());
  Variable* outEnv = variableOf(op->outEnv());
  OpcodeCall* newOp = new OpcodeCall(op->sourceLocation(), lhs, lookup, op->rhsCount(), codeBlock, outEnv);
  setDefSite(lhs, newOp);
  setDefSite(codeBlock, newOp);
  setDefSite(outEnv, newOp);
  copyRhs(newOp, op);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCodeBlock* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeCodeBlock* newOp = new OpcodeCodeBlock(op->sourceLocation(), lhs, op->nodeIter());
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeConstant* op)
{
  Variable* lhs = variableOf(op->lhs());
  Variable* base = variableOf(op->base());
  Variable* inEnv = variableOf(op->inEnv());
  Variable* outEnv = variableOf(op->outEnv());
  OpcodeConstant* newOp = new OpcodeConstant(op->sourceLocation(), lhs, base, op->name(), op->toplevel(), inEnv, outEnv);
  setDefSite(lhs, newOp);
  setDefSite(outEnv, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePrimitive* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePrimitive* newOp = new OpcodePrimitive(op->sourceLocation(), lhs, op->name(), op->rhsCount());
  setDefSite(lhs, newOp);
  copyRhs(newOp, op);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePhi* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePhi* newOp = new OpcodePhi(op->sourceLocation(), lhs, op->rhsCount(), lastBlock_);
  setDefSite(lhs, newOp);

  copyRhs(newOp, op);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeExit* op)
{
  // When duplicating for incorporate(), Exit will not be emitted
  if (emitExit_) {
    OpcodeExit* newOp = new OpcodeExit(op->sourceLocation());
    lastBlock_->addOpcode(newOp);
  }


  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeArray* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeArray* newOp = new OpcodeArray(op->sourceLocation(), lhs, op->rhsCount());
  setDefSite(lhs, newOp);
  copyRhs(newOp, op);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeRange* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeRange* newOp = new OpcodeRange(op->sourceLocation(), lhs, variableOf(op->low()), variableOf(op->high()), op->exclusive());
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeString* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeString* newOp = new OpcodeString(op->sourceLocation(), lhs, op->string());
  setDefSite(lhs, newOp);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeHash* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeHash* newOp = new OpcodeHash(op->sourceLocation(), lhs, op->rhsCount());
  setDefSite(lhs, newOp);
  copyRhs(newOp, op);

  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeEnter* op)
{
  OpcodeEnter* newOp = new OpcodeEnter(op->sourceLocation(), op->scope());
  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeLeave* op)
{
  OpcodeLeave* newOp = new OpcodeLeave(op->sourceLocation());
  lastBlock_->addOpcode(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCheckArg* op)
{
  OpcodeCheckArg* newOp = new OpcodeCheckArg(op->sourceLocation());
  lastBlock_->addOpcode(newOp);

  // TODO
//  copyRhs(newOp, op);

  return true;
}


RBJIT_NAMESPACE_END
