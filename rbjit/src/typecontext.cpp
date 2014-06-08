#include "rbjit/typecontext.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

TypeContext::TypeContext(ControlFlowGraph* cfg)
  : cfg_(cfg), typeContext_(cfg->variables()->size(), 0)
{}

void
TypeContext::addInputTypeConstraint(const TypeConstraint* type)
{
  inputTypes_.push_back(type->clone());
}

void
TypeContext::setTypeConstraint(Variable* v, TypeConstraint* type)
{
  typeContext_[v->index()] = type;
}

bool
TypeContext::updateTypeConstraint(Variable* v, const TypeConstraint& type)
{
  int i = v->index();
  TypeConstraint* t = typeContext_[i];
  if (t) {
    if (*t == type) {
      return false;
    }
    t->destroy();
  }
  typeContext_[i] = type.clone();

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
  char buf[256];
  std::string out = "[Type Constraints]\n";
  for (auto i = cfg_->variables()->cbegin(), end = cfg_->variables()->cend(); i != end; ++i) {
    Variable* v = *i;
    sprintf(buf, "%Ix: ", v);
    out += buf;
    if (typeContext_[v->index()]) {
      out += typeContext_[v->index()]->debugPrint();
    }
    else {
      out += "(null)";
    }
    out += '\n';
  };

  return out;
}
RBJIT_NAMESPACE_END
