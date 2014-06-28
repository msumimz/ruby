#include "rbjit/typecontext.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

TypeContext::TypeContext(ControlFlowGraph* cfg)
  : cfg_(cfg), types_(cfg->variables()->size(), 0)
{}

TypeContext::~TypeContext()
{
  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    (*i)->destroy();
  }
}

void
TypeContext::fitSizeToCfg()
{
  types_.resize(cfg_->variables()->size(), 0);
}

void
TypeContext::setTypeConstraint(Variable* v, TypeConstraint* type)
{
  types_[v->index()] = type;
}

bool
TypeContext::updateTypeConstraint(Variable* v, const TypeConstraint& type)
{
  int i = v->index();
  TypeConstraint* t = types_[i];
  if (t) {
    if (t->equals(&type)) {
      return false;
    }
  }
  types_[i] = type.clone();
  if (t) {
    t->destroy();
  }

  // Test runtime type
  assert(dynamic_cast<TypeSameAs*>(types_[i]) || !dynamic_cast<TypeSameAs*>(types_[i]));

  return true;
}

void
TypeContext::addNewTypeConstraint(Variable* v, TypeConstraint* type)
{
  if (v->index() >= types_.size()) {
    fitSizeToCfg();
  }

  assert(!typeConstraintOf(v));

  setTypeConstraint(v, type);
}

bool
TypeContext::isSameValueAs(Variable* v1, Variable* v2)
{
  if (v1 == v2) {
    return true;
  }

  TypeSameAs* sa;
  while (sa = dynamic_cast<TypeSameAs*>(typeConstraintOf(v1))) {
    assert(sa->typeContext() == this);
    v1 = sa->source();
    if (v1 == v2) {
      return true;
    }
  }

  while (sa = dynamic_cast<TypeSameAs*>(typeConstraintOf(v2))) {
    assert(sa->typeContext() == this);
    v2 = sa->source();
    if (v1 == v2) {
      return true;
    }
  }

  return false;
}

std::string
TypeContext::debugPrint() const
{
  std::string out = "[Type Constraints]\n";
  for (auto i = cfg_->variables()->cbegin(), end = cfg_->variables()->cend(); i != end; ++i) {
    Variable* v = *i;
    out += stringFormat("%Ix: ", v);
    if (types_[v->index()]) {
      out += types_[v->index()]->debugPrint();
      if (std::find(cfg_->inputs()->cbegin(), cfg_->inputs()->cend(), v) != cfg_->inputs()->cend()) {
        out += "  +";
      }
      if (cfg_->output() == v) {
        out += "  *";
      }
    }
    else {
      out += "(null)";
    }
    out += '\n';
  };

  return out;
}

RBJIT_NAMESPACE_END
