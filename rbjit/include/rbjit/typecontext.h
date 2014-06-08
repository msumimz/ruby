#pragma once

#include <vector>
#include <string>
#include "variable.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeConstraint;

class TypeContext {
public:

  TypeContext(ControlFlowGraph* cfg);

  void addInputTypeConstraint(const TypeConstraint* type);

  // Simply set the type constraint
  void setTypeConstraint(Variable* v, TypeConstraint* type);
  // Update the existing type constraint value
  bool updateTypeConstraint(Variable* v, const TypeConstraint& type);

  TypeConstraint* typeConstraintOf(Variable* v) const
  { return typeContext_[v->index()]; }

  bool isSameValueAs(Variable* v1, Variable* v2);

  std::string debugPrint() const;

private:

  ControlFlowGraph* cfg_;
  std::vector<TypeConstraint*> inputTypes_;
  std::vector<TypeConstraint*> typeContext_;

};

RBJIT_NAMESPACE_END
