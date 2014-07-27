#include <algorithm>
#include <memory> // unique_ptr
#include "rbjit/typeanalyzer.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/variable.h"
#include "rbjit/methodinfo.h"
#include "rbjit/typecontext.h"
#include "rbjit/idstore.h"
#include "rbjit/mutatortester.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeAnalyzer

TypeAnalyzer::TypeAnalyzer(ControlFlowGraph* cfg, mri::Class holderClass, mri::CRef cref)
  : cfg_(cfg),
    reachBlocks_(cfg->blocks()->size(), UNKNOWN),
    defUseChain_(cfg),
    typeContext_(new TypeContext(cfg)),
    mutator_(false), jitOnly_(false),
    holderClass_(holderClass),
    cref_(cref)
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

  RBJIT_DPRINTF(("Update type: %Ix to %s", v, newType.debugPrint().c_str()));
  if (!v) {
    RBJIT_DPRINT("\n");
    return;
  }

  if (typeContext_->updateTypeConstraint(v, newType)) {
    RBJIT_DPRINT(" *");
    if (std::find(variables_.cbegin(), variables_.cend(), v) == variables_.cend()) {
      variables_.push_back(v);
    }
  }
  RBJIT_DPRINT("\n");
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

std::tuple<TypeContext*, bool, bool>
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
    do {
      BlockHeader* b = blocks_.back();
      blocks_.pop_back();

      b->visitEachOpcode(this);
    } while (!blocks_.empty());

    while (!variables_.empty()) {
      Variable* v = variables_.back();
      variables_.pop_back();

      evaluateExpressionsUsing(v);
    }

  } while (!blocks_.empty());

  return std::make_tuple(typeContext_, mutator_, jitOnly_);
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
  updateTypeConstraint(op->lhs(), *typeContext_->typeConstraintOf(op->rhs()));
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
  if (!typeContext_->isSameValueAs(op->env(), cfg_->entryEnv())) {
    updateTypeConstraint(op->lhs(), TypeLookup()); // empty TypeLookup instance
    return true;
  }

  // The list of the receiver's possible classes
  std::unique_ptr<TypeList> list(typeContext_->typeConstraintOf(op->receiver())->resolve());

  if (list->lattice() != TypeList::DETERMINED &&
      (op->methodName() == IdStore::get(IDS_plus) ||
       op->methodName() == IdStore::get(IDS_minus) ||
       op->methodName() == IdStore::get(IDS_asterisk) ||
       op->methodName() == IdStore::get(IDS_slash))) {
    list->addType(mri::Class::fixnumClass());
  }

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
  else {
    TypeSelection sel;
    for (auto i = lookup->candidates().cbegin(), end = lookup->candidates().cend(); i != end; ++i) {
      assert(!i->isNull());
      MethodInfo* mi = i->methodInfo();
      if (!mi && i->methodDefinition().hasAstNode()) {
        mi = PrecompiledMethodInfo::construct(*i);
      }

      if (mi) {
        sel.add(*mi->returnType());
        mutator_ = mutator_ || mi->isMutator();
      }
      else {
        mutator_ = mutator_ || MutatorTester::instance()->isMutator(*i);
        sel.clear();
        break;
      }
    }

    if (op->lhs()) {
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
    }
  }

  if (mutator_) {
    updateTypeConstraint(op->outEnv(), TypeEnv());
  }
  else {
    updateTypeConstraint(op->outEnv(),
      TypeSameAs(typeContext_, op->lookupOpcode()->env()));
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeConstant* op)
{
  if (!typeContext_->isSameValueAs(op->inEnv(), cfg_->entryEnv())) {
    updateTypeConstraint(op->lhs(), TypeAny());
    updateTypeConstraint(op->outEnv(), TypeEnv());
    return true;
  }

  if (op->toplevel()) {
    // Toplevel constant lookup (:CONST)
    mri::Object value = mri::Class::objectClass().findConstant(op->name());
    if (!value.isNull()) {
      updateTypeConstraint(op->lhs(), TypeConstant(value));
    }
    else {
      if (mri::Class::objectClass().isRegisteredAsAutoload(op->name())) {
        mutator_ = true;
        updateTypeConstraint(op->lhs(), TypeAny());
      }
      else {
        updateTypeConstraint(op->lhs(), TypeNone());
      }
    }
    return true;
  }

  std::vector<mri::Class> baseClasses;
  std::pair<std::vector<mri::Object>, bool> list = typeContext_->typeConstraintOf(op->base())->resolveToValues();
  std::vector<mri::Object>& consts = list.first;
  bool determined = list.second;

  if (determined && consts.size() == 1 && consts[0].isNilObject()) {
    // Free constant lookup (CONST)
    mri::Object value = cref_.findConstant(op->name());
    if (value.isUndefObject()) {
      // Registered as an autoloaded constant
      mutator_ = true;
      updateTypeConstraint(op->lhs(), TypeAny());
    }
    else if (value.isNull()) {
      // The constant was not found
      updateTypeConstraint(op->lhs(), TypeNone());
    }
    else {
      // Found
      updateTypeConstraint(op->lhs(), TypeConstant(value));
    }
    return true;
  }

  // Relative constant lookup (base::CONST)
  for (auto i = consts.cbegin(), end = consts.cend(); i != end; ++i) {
    mri::Object obj = *i;
    if (obj.isClassOrModule()) {
      baseClasses.push_back(mri::Class(obj.value()));
    }
    else {
      // TODO
      // if a base object is not a class or module, then TypeError will be
      // raised on runtime. When we implement begin-rescue caluses, don't
      // forget to add control edges from constants to rescue clauses.
    }
  }

  if (baseClasses.empty()) {
    if (determined) {
      // This case would cause an exception on runtime, or autoloading
      updateTypeConstraint(op->lhs(), TypeNone());
    }
    else {
      updateTypeConstraint(op->lhs(), TypeAny());
      mutator_ = true;
    }
  }
  else {
    TypeSelection sel;
    for (auto i = baseClasses.cbegin(), end = baseClasses.cend(); i != end; ++i) {
      mri::Class base = *i;
      mri::Object value = base.findConstant(op->name());
      if (!value.isNull()) {
        sel.add(TypeConstant(value));
      }
      else {
        determined = false;
        if (base.isRegisteredAsAutoload(op->name())) {
          sel.add(TypeAny());
          mutator_ = true;
        }
      }
    }

    if (!determined) {
      sel.add(TypeAny());
    }

    if (op->lhs()) {
      if (sel.types().empty()) {
        updateTypeConstraint(op->lhs(), TypeNone());
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
  }

  if (mutator_) {
    updateTypeConstraint(op->outEnv(), TypeEnv());
  }
  else {
    updateTypeConstraint(op->outEnv(), TypeSameAs(typeContext_, op->inEnv()));
  }

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodePrimitive* op)
{
  // TODO: If the primitive is one of Fixnum comparisons or type tests, check
  // if it is used as a condition of JumpIf and re-visit JumpIf.

  jitOnly_ = true;

  if (!op->lhs()) {
    return true;
  }

  ID name = op->name();
  if (name == IdStore::get(ID_rbjit__is_fixnum)) {
    TypeConstraint* type = typeContext_->typeConstraintOf(op->rhs());
    if (type->isExactClass(mri::Class::fixnumClass())) {
      updateTypeConstraint(op->lhs(), TypeConstant(mri::Object::trueObject()));
    }
    else if (type->isImpossibleToBeClass(mri::Class::fixnumClass())) {
      updateTypeConstraint(op->lhs(), TypeConstant(mri::Object::falseObject()));
    }
    else {
      TypeSelection sel;
      sel.add(TypeConstant(mri::Object::trueObject()));
      sel.add(TypeConstant(mri::Object::falseObject()));
      updateTypeConstraint(op->lhs(), sel);
    }
  }
  else if (name == IdStore::get(ID_rbjit__typecast_fixnum)) {
    updateTypeConstraint(op->lhs(), TypeExactClass(mri::Class::fixnumClass()));
  }
  else if (name == IdStore::get(ID_rbjit__typecast_fixnum_bignum)) {
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
        TypeConstraint* t = typeContext_->typeConstraintOf(*i);
        if (typeid(*t) == typeid(TypeEnv)) {
          types.add(TypeSameAs(typeContext_, *i));
        }
        else {
          types.add(*t->clone());
        }
      }
    }
  }

  if (types.types().empty()) {
    updateTypeConstraint(op->lhs(), TypeAny());
  }
  else if (types.types().size() == 1) {
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
