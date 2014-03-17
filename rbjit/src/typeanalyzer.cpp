#include <algorithm>
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
    reachTrueEdges_(cfg->blocks()->size(), UNKNOWN),
    reachFalseEdges_(cfg->blocks()->size(), UNKNOWN),
    defUseChain_(cfg)
{}

void
TypeAnalyzer::updateTypeConstraint(Variable* v, const TypeConstraint& newType)
{
  if (*v->typeConstraint() == newType) {
    return;
  }

  TypeConstraint* type = newType.clone();
  delete v->typeConstraint();
  v->setTypeConstraint(type);
  variables_.push_back(v);
}

void
TypeAnalyzer::analyze()
{
  // Initialize type constraints
  std::for_each(cfg_->variables()->cbegin(), cfg_->variables()->cend(), [](Variable* v) {
    delete v->typeConstraint();
    v->setTypeConstraint(new TypeAny());
  });

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
  const std::vector<Variable*>& uses = defUseChain_.uses(v);
  std::for_each(uses.cbegin(), uses.cend(), [this](Variable* u) {
    u->defOpcode()->accept(this);
  });
}

bool
TypeAnalyzer::visitOpcode(BlockHeader* op)
{
  reachBlocks_[op->index()] = REACHABLE;
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
  reachTrueEdges_[b->index()] = REACHABLE;
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeJumpIf* op)
{
  auto* condType = op->cond()->typeConstraint();
  assert(condType);

  switch (condType->evaluatesToBoolean()) {
  case TypeConstraint::ALWAYS_TRUE:
    blocks_.push_back(op->ifTrue());
    reachTrueEdges_[op->ifTrue()->index()] = REACHABLE;
    reachFalseEdges_[op->ifFalse()->index()] = UNREACHABLE;
    break;

  case TypeConstraint::ALWAYS_FALSE:
    blocks_.push_back(op->ifFalse());
    reachTrueEdges_[op->ifTrue()->index()] = UNREACHABLE;
    reachFalseEdges_[op->ifFalse()->index()] = REACHABLE;
    break;

  case TypeConstraint::TRUE_OR_FALSE:
    blocks_.push_back(op->ifTrue());
    blocks_.push_back(op->ifFalse());
    reachTrueEdges_[op->ifTrue()->index()] = REACHABLE;
    reachFalseEdges_[op->ifFalse()->index()] = REACHABLE;
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
  TypeMethodEntry* type;
  mri::Class cls = op->receiver()->typeConstraint()->evaluateClass();

  mri::MethodEntry me;
  if (!cls.isNull() && op->env()->isSameValueAs(cfg_->env())) {
    me = cls.findMethod(op->methodName());
  }
  updateTypeConstraint(op->lhs(), TypeMethodEntry(me));

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeCall* op)
{
  mri::MethodEntry me =
    static_cast<TypeMethodEntry*>(op->methodEntry()->typeConstraint())->methodEntry();

  bool mutator = true;
  TypeConstraint* type = 0;
  if (!me.isNull()) {
    MethodInfo* mi = me.methodDefinition().methodInfo();
    if (mi) {
      type = mi->returnType();
      mutator = mi->isMutator();
    }
  }

  if (type) {
    updateTypeConstraint(op->lhs(), *type);
  }
  else {
    updateTypeConstraint(op->lhs(), TypeAny());
  }

  if (mutator) {
    updateTypeConstraint(op->env(), TypeEnv());
  }
  else {
    updateTypeConstraint(op->env(),
      TypeSameAs(static_cast<OpcodeLookup*>(op->methodEntry()->defOpcode())->env()));
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
  // TODO: Consider reachTrueEdges and reachFalseEdges
  auto types = TypeSelection();
  auto i = op->rhsBegin();
  auto end = op->rhsEnd();
  for (; i < end; ++i) {
    types.addOption((*i)->typeConstraint());
  }
  updateTypeConstraint(op->lhs(), types);

  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeExit* op)
{
  return true;
}

RBJIT_NAMESPACE_END
