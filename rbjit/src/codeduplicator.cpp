#include "rbjit/codeduplicator.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/typecontext.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

CodeDuplicator::CodeDuplicator()
  : lastBlock_(0), lastOpcode_(0), src_(0), dest_(0),
    blockIndexOffset_(0), variableIndexOffset_(0),
    emitExit_(false)
{}

BlockHeader*
CodeDuplicator::blockOf(BlockHeader* srcBlock)
{
  return (*dest_->blocks())[srcBlock->index() + blockIndexOffset_];
}

Variable*
CodeDuplicator::variableOf(Variable* srcVariable)
{
  if (!srcVariable) {
    return nullptr;
  }
  return (*dest_->variables())[srcVariable->index() + variableIndexOffset_];
}

void
CodeDuplicator::setDefSite(Variable* lhs)
{
  if (!lhs) {
    return;
  }
  lhs->updateDefSite(lastBlock_, lastOpcode_);
}

void
CodeDuplicator::copyRhs(OpcodeVa* dest, OpcodeVa* src)
{
  int count = 0;
  for (auto i = src->rhsBegin(), end = src->rhsEnd(); i < end; ++i) {
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

  newCfg->entry_ = entry();
  newCfg->exit_ = exit();
  newCfg->entryEnv_ = variableOf(cfg->entryEnv());
  newCfg->exitEnv_ = variableOf(cfg->exitEnv());
  newCfg->output_ = variableOf(cfg->output());
  newCfg->undefined_ = variableOf(cfg->undefined());

  // Input variables
  newCfg->inputs_.resize(cfg->inputs_.size(), nullptr);
  auto n = &newCfg->inputs_[0];
  for (auto i = cfg->inputs()->cbegin(), end = cfg->inputs()->cend(); i != end; ++i) {
    *n++ = variableOf(*i);
  }

  newCfg->requiredArgCount_ = cfg->requiredArgCount_;
  newCfg->hasOptionalArg_ = cfg->hasOptionalArg_;
  newCfg->hasRestArg_ = cfg->hasRestArg_;

  return newCfg;
}

void
CodeDuplicator::duplicateOpcodes()
{
  // Allocate space for new blocks
  std::vector<BlockHeader*>* blocks = dest_->blocks();
  blockIndexOffset_ = blocks->size();
  blocks->insert(blocks->end(), src_->blocks()->size(), nullptr);

  // Pre-create blocks
  for (auto i = src_->blocks()->cbegin(), end = src_->blocks()->cend(); i != end; ++i) {
    BlockHeader* b = *i;
    BlockHeader* block = new BlockHeader(b->file(), b->line(), 0, 0,
      b->index() + blockIndexOffset_, b->depth(), 0);
#ifdef RBJIT_DEBUG
    block->setDebugName(stringFormat("dup_%Ix_%d", src_, b->index()).c_str());
#endif
    (*blocks)[b->index() + blockIndexOffset_] = block;
  }

  // Set the idom of each block
  for (int i = 0, end = src_->blocks()->size(); i < end; ++i) {
    BlockHeader* idom = (*src_->blocks())[i]->idom();
    if (idom) {
      (*blocks)[i + blockIndexOffset_]->setIdom(blockOf(idom));
    }
  }

  // Allocate space for new variables
  std::vector<Variable*>* variables = dest_->variables();
  variableIndexOffset_ = variables->size();
  variables->insert(variables->end(), src_->variables()->size(), nullptr);

  // Pre-create variables
  for (auto i = src_->variables()->cbegin(), end = src_->variables()->cend(); i != end; ++i) {
    Variable* v = *i;
    BlockHeader* defBlock = blockOf(v->defBlock());
    Variable* newVar = Variable::copy(defBlock, 0, v->index() + variableIndexOffset_, v);
    (*variables)[v->index() + variableIndexOffset_] = newVar;
  }

  // Main loop: iterate over the control flow graph and copy blocks
  for (auto i = src_->blocks()->cbegin(), end = src_->blocks()->cend(); i != end; ++i) {
    (*i)->visitEachOpcode(this);
//    RBJIT_DPRINT(src_->debugPrintBlock(blockOf(*i)));
  }
}

void
CodeDuplicator::duplicateTypeContext(TypeContext* srcTypes, TypeContext* destTypes)
{
  destTypes->fitSizeToCfg();
  for (auto i = src_->variables()->cbegin(), end = src_->variables()->cend(); i != end; ++i) {
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
CodeDuplicator::visitOpcode(BlockHeader* op)
{
  lastOpcode_ = lastBlock_ = blockOf(op);
  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCopy* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  Variable* rhs = variableOf(op->rhs());
  RBJIT_ASSUME(rhs);
  OpcodeCopy* newOp = new OpcodeCopy(op->file(), op->line(), lastOpcode_, lhs, rhs);
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeJump* op)
{
  BlockHeader* dest = blockOf(op->nextBlock());
  OpcodeJump* newOp = new OpcodeJump(op->file(), op->line(), lastOpcode_, dest);

  dest->addBackedge(lastBlock_);
  lastBlock_->setFooter(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeJumpIf* op)
{
  BlockHeader* ifTrue = blockOf(op->nextBlock());
  BlockHeader* ifFalse = blockOf(op->nextAltBlock());
  OpcodeJumpIf* newOp = new OpcodeJumpIf(op->file(), op->line(), lastOpcode_, variableOf(op->cond()), ifTrue, ifFalse);

  ifTrue->addBackedge(lastBlock_);
  ifFalse->addBackedge(lastBlock_);
  lastBlock_->setFooter(newOp);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeImmediate* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeImmediate* newOp = new OpcodeImmediate(op->file(), op->line(), lastOpcode_, lhs, op->value());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeEnv* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeEnv* newOp = new OpcodeEnv(op->file(), op->line(), lastOpcode_, lhs);
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeLookup* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSUME(lhs);
  OpcodeLookup* newOp = new OpcodeLookup(op->file(), op->line(), lastOpcode_, lhs, variableOf(op->receiver()), op->methodName(), variableOf(op->env()));
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCall* op)
{
  Variable* lhs = variableOf(op->lhs());
  Variable* lookup = variableOf(op->lookup());
  Variable* outEnv = variableOf(op->outEnv());
  OpcodeCall* newOp = new OpcodeCall(op->file(), op->line(), lastOpcode_, lhs, lookup, op->rhsSize(), outEnv);
  lastOpcode_ = newOp;
  setDefSite(lhs);
  setDefSite(outEnv);

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeConstant* op)
{
  Variable* lhs = variableOf(op->lhs());
  Variable* base = variableOf(op->base());
  Variable* inEnv = variableOf(op->inEnv());
  Variable* outEnv = variableOf(op->outEnv());
  OpcodeConstant* newOp = new OpcodeConstant(op->file(), op->line(), lastOpcode_, lhs, base, op->name(), op->toplevel(), inEnv, outEnv);
  lastOpcode_ = newOp;
  setDefSite(lhs);
  setDefSite(outEnv);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePrimitive* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePrimitive* newOp = new OpcodePrimitive(op->file(), op->line(), lastOpcode_, lhs, op->name(), op->rhsSize());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePhi* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePhi* newOp = new OpcodePhi(op->file(), op->line(), lastOpcode_, lhs, op->rhsSize(), lastBlock_);
  lastOpcode_ = newOp;
  setDefSite(lhs);

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeExit* op)
{
  // When duplicating for incorporate(), Exit will not be emitted
  if (emitExit_) {
    OpcodeExit* newOp = new OpcodeExit(op->file(), op->line(), lastOpcode_);
    lastOpcode_ = newOp;
  }

  lastBlock_->setFooter(lastOpcode_);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeArray* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeArray* newOp = new OpcodeArray(op->file(), op->line(), lastOpcode_, lhs, op->rhsSize());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeRange* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeRange* newOp = new OpcodeRange(op->file(), op->line(), lastOpcode_, lhs, variableOf(op->low()), variableOf(op->high()), op->exclusive());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeString* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeString* newOp = new OpcodeString(op->file(), op->line(), lastOpcode_, lhs, op->string());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeHash* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeHash* newOp = new OpcodeHash(op->file(), op->line(), lastOpcode_, lhs, op->rhsSize());
  lastOpcode_ = newOp;
  setDefSite(lhs);

  copyRhs(newOp, op);

  return true;
}


RBJIT_NAMESPACE_END
