#include "rbjit/blockbuilder.h"
#include "rbjit/block.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/scope.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

BlockBuilder::BlockBuilder(ControlFlowGraph* cfg, Scope* scope, DefInfoMap* defInfoMap, Block* block)
  : cfg_(cfg), scope_(scope), defInfoMap_(defInfoMap), block_(block), halted_(false)
{}

BlockBuilder::BlockBuilder(const BlockBuilder* builder, Block* block)
  : cfg_(builder->cfg_), scope_(builder->scope_), defInfoMap_(builder->defInfoMap_), block_(block), halted_(false)
{}

void
BlockBuilder::updateDefSite(Variable* v, Block* b, Opcode* op)
{
  if (defInfoMap_) {
    defInfoMap_->updateDefSite(v, b, op);
  }
  else {
    v->setDefBlock(b);
    v->setDefOpcode(op);
  }
}

Variable*
BlockBuilder::add(ID name, bool belongsToScope, OpcodeL* op)
{
  NamedVariable* nameRef = nullptr;
  if (belongsToScope) {
    nameRef = scope_->find(name);
    assert(nameRef);
  }

  Variable* v = cfg_->createVariable(name, nameRef);
  op->setLhs(v);
  block_->addOpcode(op);
  updateDefSite(v, block_, op);

  return v;
}

Variable*
BlockBuilder::add(OpcodeL* op)
{
  Variable* v = op->lhs();
  if (!v) {
    v = cfg_->createVariable();
    op->setLhs(v);
  }
  block_->addOpcode(op);
  updateDefSite(v, block_, op);

  return v;
}

void
BlockBuilder::addWithoutLhsAssigned(Opcode* op)
{
  block_->addOpcode(op);
}

void
BlockBuilder::addJump(SourceLocation* loc, Block* dest)
{
  addWithoutLhsAssigned(new OpcodeJump(loc, dest));
  dest->addBackedge(block_);
}

void
BlockBuilder::addJumpIf(SourceLocation* loc, Variable* cond, Block* ifTrue, Block* ifFalse)
{
  addWithoutLhsAssigned(new OpcodeJumpIf(loc, cond, ifTrue, ifFalse));
  ifTrue->addBackedge(block_);
  ifFalse->addBackedge(block_);
}

RBJIT_NAMESPACE_END
