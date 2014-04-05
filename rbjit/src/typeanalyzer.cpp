#include <algorithm>
#include <memory> // unique_ptr
#include "rbjit/typeanalyzer.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/variable.h"
#include "rbjit/methodinfo.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeAnalyzer

TypeAnalyzer::TypeAnalyzer(ControlFlowGraph* cfg)
  : cfg_(cfg),
    reachBlocks_(cfg->blocks()->size(), UNKNOWN),
    defUseChain_(cfg)
{}

void
TypeAnalyzer::updateTypeConstraint(Variable* v, const TypeConstraint& newType)
{
  if (!v || *v->typeConstraint() == newType) {
    return;
  }

  TypeConstraint* type = newType.clone();
  v->typeConstraint()->destroy();
  v->setTypeConstraint(type);
  variables_.push_back(v);
}

void
TypeAnalyzer::makeEdgeReachable(BlockHeader* from, BlockHeader* to)
{
  auto c = reachEdges_.find(std::make_pair(from, to));
  if (c == reachEdges_.end()) {
    reachEdges_[std::make_pair(from, to)] = REACHABLE;
  }
  else {
    if (c->second == REACHABLE) {
      // It is unnecessary to visit _to_ block.
      return;
    }
    c->second = REACHABLE;
  }

  blocks_.push_back(to);
}

void
TypeAnalyzer::makeEdgeUnreachable(BlockHeader* from, BlockHeader* to)
{
  reachEdges_[std::make_pair(from, to)] = UNREACHABLE;
}

void
TypeAnalyzer::analyze()
{
  // Initialize type constraints
  for (auto i = cfg_->variables()->cbegin(), end =cfg_->variables()->cend(); i != end; ++i) {
    if ((*i)->typeConstraint()) {
      (*i)->typeConstraint()->destroy();
    }
    (*i)->setTypeConstraint(TypeAny::create());
  };

  blocks_.push_back(cfg_->entry());

  bool changed;
  do {
    changed = false;
    if (!blocks_.empty()) {
      changed = true;
      do {
        BlockHeader* b = blocks_.back();
        blocks_.pop_back();

        b->visitEachOpcode(this);
      } while (!blocks_.empty());
    }

    if (!variables_.empty()) {
      changed = true;
      do {
        Variable* v = variables_.back();
        variables_.pop_back();

        evaluateExpressionsUsing(v);
      } while (!variables_.empty());
    }
  } while (changed);
}

void
TypeAnalyzer::evaluateExpressionsUsing(Variable* v)
{
  const std::vector<std::pair<BlockHeader*, Variable*>>& uses = defUseChain_.uses(v);
  for (auto i = uses.cbegin(), end = uses.cend(); i != end; ++i) {
    block_ = i->first;
    i->second->defOpcode()->accept(this);
  };
}

bool
TypeAnalyzer::visitOpcode(BlockHeader* op)
{
  reachBlocks_[op->index()] = REACHABLE;
  block_ = op;
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeCopy* op)
{
  updateTypeConstraint(op->lhs(), TypeSameAs(op->rhs()));
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeJump* op)
{
  auto* b = op->nextBlock();
  blocks_.push_back(b);;
  reachEdges_[std::make_pair(block_, b)] = REACHABLE;
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeJumpIf* op)
{
  auto* condType = op->cond()->typeConstraint();
  assert(condType);

  switch (condType->evaluatesToBoolean()) {
  case TypeConstraint::ALWAYS_TRUE:
    makeEdgeReachable(block_, op->ifTrue());
    makeEdgeUnreachable(block_, op->ifFalse());
    break;

  case TypeConstraint::ALWAYS_FALSE:
    makeEdgeUnreachable(block_, op->ifTrue());
    makeEdgeReachable(block_, op->ifFalse());
    break;

  case TypeConstraint::TRUE_OR_FALSE:
    makeEdgeReachable(block_, op->ifTrue());
    makeEdgeReachable(block_, op->ifFalse());
    break;

  default:
    RBJIT_UNREACHABLE;
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeImmediate* op)
{
  updateTypeConstraint(op->lhs(), TypeConstant(op->value()));
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeEnv* op)
{
  updateTypeConstraint(op->lhs(), TypeEnv());
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeLookup* op)
{
  // If the 'environment' is changed between the method entry and the call
  // site, it is impossible to decide actual methods.
  if (!op->env()->isSameValueAs(cfg_->env())) {
    updateTypeConstraint(op->lhs(), TypeLookup()); // empty TypeLookup instance
    return true;
  }

  // The list of the receiver's possible classes
  std::unique_ptr<TypeList> list(op->receiver()->typeConstraint()->resolve());

  if (list->lattice() != TypeList::DETERMINED) {
    updateTypeConstraint(op->lhs(), TypeLookup());
    return true;
  }

  TypeLookup lookup;
  auto i = list->typeList().cbegin();
  auto end = list->typeList().cend();
  for (; i != end; ++i) {
    mri::MethodEntry me = (*i).findMethod(op->methodName());
    if (!me.isNull()) {
      lookup.addCandidate(*i, me);
    }
  }

  updateTypeConstraint(op->lhs(), lookup);

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeCall* op)
{
  auto lookup = static_cast<TypeLookup*>(op->lookup()->typeConstraint());
  int count = lookup->candidates().size();

  if (count == 0) {
    updateTypeConstraint(op->lhs(), TypeAny());
  }

  bool mutator = false;
  std::vector<TypeConstraint*> types;
  auto i = lookup->candidates().cbegin();
  auto end = lookup->candidates().cend();
  for (; i != end; ++i) {
    MethodInfo* mi;
    if (!i->methodEntry().isNull() &&
        (mi = i->methodEntry().methodDefinition().methodInfo())) {
      types.push_back(mi->returnType()->clone());
      mutator = mutator || mi->isMutator();
    }
    else {
      // If there is any method without method info, type inference will fail.
      mutator = true;
      types.clear();
      break;
    }
  }

  if (types.empty()) {
    updateTypeConstraint(op->lhs(), TypeAny());
  }
  else {
    updateTypeConstraint(op->lhs(), TypeSelection(std::move(types)));
  }

  if (mutator) {
    updateTypeConstraint(op->env(), TypeEnv());
  }
  else {
    updateTypeConstraint(op->env(),
      TypeSameAs(static_cast<OpcodeLookup*>(op->lookup()->defOpcode())->env()));
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodePrimitive* op)
{
  // TODO: If the primitive is one of Fixnum comparisons or type tests, check
  // if it is used as a condition of JumpIf and re-visit JumpIf.
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodePhi* op)
{
  TypeSelection types;
  auto i = op->rhsBegin();
  auto end = op->rhsEnd();
  BlockHeader::Backedge* e = block_->backedge();
  for (; i < end; ++i, e = e->next()) {
    assert(e->block());
    if ((*i)->typeConstraint()) {
      auto r = reachEdges_.find(std::make_pair(e->block(), block_));
      if (r != reachEdges_.end() && r->second == REACHABLE) {
        types.addOption((*i)->typeConstraint()->clone());
      }
    }
  }

  if (types.types().size() == 1) {
    updateTypeConstraint(op->lhs(), *types.types()[0]);
  }
  else {
    updateTypeConstraint(op->lhs(), types);
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeExit* op)
{
  return true;
}

RBJIT_NAMESPACE_END
