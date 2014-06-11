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
    if (*t == type) {
      return false;
    }
    t->destroy();
  }
  types_[i] = type.clone();

  return true;
}

bool
TypeContext::isSameValueAs(Variable* v1, Variable* v2)
{
  return v1 == v2 || typeConstraintOf(v1)->isSameValueAs(this, v2);
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
    }
    else {
      out += "(null)";
    }
    out += '\n';
  };

  return out;
}

RBJIT_NAMESPACE_END
