#include <algorithm>
#include <memory> // unique_ptr
#include "rbjit/typeanalyzer.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/variable.h"
#include "rbjit/methodinfo.h"
#include "rbjit/typecontext.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeAnalyzer

TypeAnalyzer::TypeAnalyzer(ControlFlowGraph* cfg)
  : cfg_(cfg),
    reachBlocks_(cfg->blocks()->size(), UNKNOWN),
    defUseChain_(cfg),
    typeContext_(new TypeContext(cfg))
{}

void
TypeAnalyzer::setInputTypeConstraint(int index, const TypeConstraint& type)
{
  typeContext_->updateTypeConstraint((*cfg_->inputs())[index], type);
}

void
TypeAnalyzer::updateTypeConstraint(Variable* v, const TypeConstraint& newType)
{
  assert(cfg_->containsVariable(v));

  if (!v) {
    return;
  }

  if (typeContext_->updateTypeConstraint(v, newType)) {
    if (std::find(variables_.cbegin(), variables_.cend(), v) == variables_.cend()) {
      variables_.push_back(v);
    }
  }
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

TypeContext*
TypeAnalyzer::analyze()
{
  // Initialize type constraints
  for (auto i = cfg_->variables()->cbegin(), end = cfg_->variables()->cend(); i != end; ++i) {
    if (!typeContext_->typeConstraintOf(*i)) {
      typeContext_->setTypeConstraint(*i, TypeAny::create());
    }
  };

  blocks_.push_back(cfg_->entry());

  do {
    BlockHeader* b = blocks_.back();
    blocks_.pop_back();

    b->visitEachOpcode(this);

    while (!variables_.empty()) {
      Variable* v = variables_.back();
      variables_.pop_back();

      evaluateExpressionsUsing(v);
    }

  } while (!blocks_.empty());


  return typeContext_;
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
  updateTypeConstraint(op->lhs(), TypeSameAs(typeContext_, op->rhs()));
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
  TypeConstraint* condType = typeContext_->typeConstraintOf(op->cond());
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
  // If the env is changed between the method entry and the call site, it is
  // impossible to decide actual methods.
  if (!typeContext_->isSameValueAs(op->env(), cfg_->env())) {
    updateTypeConstraint(op->lhs(), TypeLookup()); // empty TypeLookup instance
    return true;
  }

  // The list of the receiver's possible classes
  std::unique_ptr<TypeList> list(typeContext_->typeConstraintOf(op->receiver())->resolve());

  TypeLookup lookup(list->lattice() == TypeList::DETERMINED);
  for (auto i = list->typeList().cbegin(), end = list->typeList().cend(); i != end; ++i) {
    mri::MethodEntry me = (*i).findMethod(op->methodName());
    if (!me.isNull() && !lookup.includes(me)) {
      lookup.addCandidate(me);
    }
  }

  updateTypeConstraint(op->lhs(), lookup);

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeCall* op)
{
  auto lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  int count = lookup->candidates().size();

  if (count == 0) {
    updateTypeConstraint(op->lhs(), TypeAny());
  }

  bool mutator = false;
  TypeSelection sel;
  for (auto i = lookup->candidates().cbegin(), end = lookup->candidates().cend(); i != end; ++i) {
    assert(!i->isNull());
    MethodInfo* mi = i->methodInfo();
    if (!mi && i->methodDefinition().hasAstNode()) {
      mi = PrecompiledMethodInfo::construct(*i);
    }

    if (mi) {
      sel.add(*mi->returnType());
      mutator = mutator || mi->isMutator();
    }
    else {
      // If there is any method without method info, type inference will fail.
      mutator = true;
      sel.clear();
      break;
    }
  }

  if (lookup->isDetermined()) {
    if (sel.types().empty()) {
      updateTypeConstraint(op->lhs(), TypeAny());
    }
    else {
      if (sel.types().size() == 1) {
        updateTypeConstraint(op->lhs(), *sel.types()[0]);
      }
      else {
        updateTypeConstraint(op->lhs(), sel);
      }
    }
  }
  else {
    sel.add(TypeAny());
    updateTypeConstraint(op->lhs(), sel);
  }

  if (mutator) {
    updateTypeConstraint(op->env(), TypeEnv());
  }
  else {
    updateTypeConstraint(op->env(),
      TypeSameAs(typeContext_, op->lookupOpcode()->env()));
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodePrimitive* op)
{
  // TODO: If the primitive is one of Fixnum comparisons or type tests, check
  // if it is used as a condition of JumpIf and re-visit JumpIf.

  if (!op->lhs()) {
    return true;
  }

  if (op->name() == mri::Id("rbjit__typecast_fixnum").id()) {
    updateTypeConstraint(op->lhs(), TypeExactClass(mri::Class::fixnumClass()));
  }
  else if (op->name() == mri::Id("rbjit__typecast_fixnum_bignum").id()) {
    TypeSelection sel;
    sel.add(TypeExactClass(mri::Class::fixnumClass()));
    sel.add(TypeExactClass(mri::Class::bignumClass()));
    updateTypeConstraint(op->lhs(), sel);
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodePhi* op)
{
  TypeSelection types;
  BlockHeader::Backedge* e = block_->backedge();
  for (auto i = op->rhsBegin(), end = op->rhsEnd(); i < end; ++i, e = e->next()) {
    assert(e->block());
    if (typeContext_->typeConstraintOf(*i)) {
      auto r = reachEdges_.find(std::make_pair(e->block(), block_));
      if (r != reachEdges_.end() && r->second == REACHABLE) {
        types.add(*typeContext_->typeConstraintOf(*i));
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
