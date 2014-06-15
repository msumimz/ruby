#include "rbjit/codeduplicator.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/typecontext.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

CodeDuplicator::CodeDuplicator(ControlFlowGraph* src, TypeContext* srcTypes, ControlFlowGraph* dest, TypeContext* destTypes)
  : src_(src), srcTypes_(srcTypes), dest_(dest), destTypes_(destTypes)
{}

BlockHeader*
CodeDuplicator::blockOf(BlockHeader* srcBlock)
{
  return (*dest_->blocks())[srcBlock->index() + blockIndexOffset_];
}

Variable*
CodeDuplicator::variableOf(Variable* srcVariable)
{
  return (*dest_->variables())[srcVariable->index() + variableIndexOffset_];
}

void
CodeDuplicator::setDefSite(Variable* lhs)
{
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
CodeDuplicator::duplicateCfg()
{
  // Allocate space for new blocks
  std::vector<BlockHeader*>* blocks = dest_->blocks();
  blockIndexOffset_ = blocks->size();
  blocks->resize(blockIndexOffset_ + src_->variables()->size(), 0);

  // Pre-create blocks
  for (auto i = src_->blocks()->cbegin(), end = src_->blocks()->cend(); i != end; ++i) {
    BlockHeader* b = *i;
    BlockHeader* block = new BlockHeader(b->file(), b->line(), 0, 0,
      b->index() + blockIndexOffset_, b->depth(), 0);
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
  variables->resize(variableIndexOffset_ + src_->variables()->size(), 0);

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
    RBJIT_DPRINT(src_->debugPrintBlock(blockOf(*i)));
  }

  // Duplicate type constraints
  destTypes_->fitSizeToCfg();
  for (auto i = src_->variables()->cbegin(), end = src_->variables()->cend(); i != end; ++i) {
    Variable* v = *i;
    Variable* w = variableOf(v);
    TypeConstraint* t = srcTypes_->typeConstraintOf(v);
    if (typeid(*t) == typeid(TypeSameAs)) {
      t = TypeSameAs::create(destTypes_, w);
    }
    else {
      t = t->clone();
    }
    assert(!destTypes_->typeConstraintOf(w));
    destTypes_->setTypeConstraint(w, t);
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
  OpcodeCopy* newOp = new OpcodeCopy(op->file(), op->line(), lastOpcode_, lhs, op->rhs());
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
  OpcodeLookup* newOp = new OpcodeLookup(op->file(), op->line(), lastOpcode_, lhs, op->receiver(), op->methodName(), variableOf(op->env()));
  lastOpcode_ = newOp;
  setDefSite(lhs);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCall* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeCall* newOp = new OpcodeCall(op->file(), op->line(), lastOpcode_, lhs, variableOf(op->lookup()), op->rhsSize(), variableOf(op->env()));
  lastOpcode_ = newOp;
  setDefSite(lhs);

  copyRhs(newOp, op);

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
  // Emit nothing, because it is more useful when the duplicated code will be inlined.
  return true;
}

RBJIT_NAMESPACE_END
