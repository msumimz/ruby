#include "rbjit/codeduplicator.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

CodeDuplicator::CodeDuplicator(ControlFlowGraph* source, ControlFlowGraph* dest)
  : source_(source), dest_(dest)
{}

BlockHeader*
CodeDuplicator::blockOf(BlockHeader* sourceBlock)
{
  return (*destBlocks_)[sourceBlock->index() + blockIndexOffset_];
}

Variable*
CodeDuplicator::variableOf(Variable* sourceVariable)
{
  return (*destVariables_)[sourceVariable->index() + variableIndexOffset_];
}

void
CodeDuplicator::setDefInfo(Variable* lhs, Opcode* op)
{
  lhs->setDefOpcode(op);
  lhs->defInfo()->addDefSite(lastBlock_);
}

void
CodeDuplicator::copyRhs(OpcodeVa* dest, OpcodeVa* source)
{
  int count = 0;
  for (auto i = source->rhsBegin(), end = source->rhsEnd(); i < end; ++i) {
    dest->setRhs(count++, variableOf(*i));
  }
}

void
CodeDuplicator::duplicateCfg()
{
  // Allocate space for new blocks
  std::vector<BlockHeader*>* blocks = dest_->blocks();
  blockIndexOffset_ = blocks->size();
  blocks->resize(blockIndexOffset_ + source_->variables()->size(), 0);

  // Pre-create blocks
  for (auto i = source_->blocks()->cbegin(), end = source_->blocks()->cend(); i != end; ++i) {
    BlockHeader* b = *i;
    BlockHeader* block = new BlockHeader(b->file(), b->line(), 0, 0,
      b->index() + blockIndexOffset_, b->depth(), 0);
    (*blocks)[b->index() + blockIndexOffset_] = block;
  }

  // Set the idom of each block
  for (int i = 0, end = source_->blocks()->size(); i < end; ++i) {
    BlockHeader* idom = blockOf((*source_->blocks())[i]->idom());
    (*blocks)[i + blockIndexOffset_]->setIdom(idom);
  }

  // Allocate space for new variables
  std::vector<Variable*>* variables = dest_->variables();
  variableIndexOffset_ = variables->size();
  variables->resize(variableIndexOffset_ + source_->variables()->size(), 0);

  // Pre-create variables
  for (auto i = source_->variables()->cbegin(), end = source_->variables()->cend(); i != end; ++i) {
    Variable* v = *i;
    BlockHeader* defBlock = blockOf(v->defBlock());
    Variable* newVar = Variable::copy(defBlock, 0, v->index() + variableIndexOffset_, v);
    (*variables)[v->index() + variableIndexOffset_] = newVar;
  }

  // Main loop: iterate over the control flow graph and copy blocks
  for (auto i = source_->blocks()->cbegin(), end = source_->blocks()->cend(); i != end; ++i) {
    (*i)->visitEachOpcode(this);
  }

  // Set opcode count
  dest_->setOpcodeCount(dest_->opcodeCount() + source_->opcodeCount());
}

bool
CodeDuplicator::visitOpcode(BlockHeader* op)
{
  lastOpcode_ = lastBlock_ = op;
  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCopy* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSERT(lhs);
  OpcodeCopy* newOp = new OpcodeCopy(op->file(), op->line(), lastOpcode_, lhs, op->rhs());
  setDefInfo(lhs, newOp);
  lastOpcode_ = newOp;

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
  RBJIT_ASSERT(lhs);
  OpcodeImmediate* newOp = new OpcodeImmediate(op->file(), op->line(), lastOpcode_, lhs, op->value());

  setDefInfo(lhs, newOp);
  lastOpcode_ = newOp;

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeEnv* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSERT(lhs);
  OpcodeEnv* newOp = new OpcodeEnv(op->file(), op->line(), lastOpcode_, lhs);

  setDefInfo(lhs, op);
  lastOpcode_ = newOp;

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeLookup* op)
{
  Variable* lhs = variableOf(op->lhs());
  RBJIT_ASSERT(lhs);
  OpcodeLookup* newOp = new OpcodeLookup(op->file(), op->line(), lastOpcode_, lhs, op->receiver(), op->methodName(), variableOf(op->env()));

  setDefInfo(lhs, op);
  lastOpcode_ = newOp;

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeCall* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodeCall* newOp = new OpcodeCall(op->file(), op->line(), lastOpcode_, lhs, variableOf(op->lookup()), op->rhsSize(), variableOf(op->env()));

  setDefInfo(lhs, newOp);
  lastOpcode_ = newOp;

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePrimitive* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePrimitive* newOp = new OpcodePrimitive(op->file(), op->line(), lastOpcode_, lhs, op->name(), op->rhsSize());

  setDefInfo(lhs, newOp);
  lastOpcode_ = newOp;

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodePhi* op)
{
  Variable* lhs = variableOf(op->lhs());
  OpcodePhi* newOp = new OpcodePhi(op->file(), op->line(), lastOpcode_, lhs, op->rhsSize(), lastBlock_);

  setDefInfo(lhs, newOp);
  lastOpcode_ = newOp;

  copyRhs(newOp, op);

  return true;
}

bool
CodeDuplicator::visitOpcode(OpcodeExit* op)
{
  // Emit nothing, because it is more useful when duplicated code will be inlined.
  return true;
}

RBJIT_NAMESPACE_END