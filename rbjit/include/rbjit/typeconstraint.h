#pragma once

#include <vector>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class Variable;

class TypeList {
};

class TypeConstraint {
public:

  static TypeConstraint* create(Variable* v);

  virtual TypeList* resolve() = 0;
  virtual TypeList* resolveInProgress(TypeConstraint* requester) = 0;
};

class TypeConstant : public TypeConstraint {
public:

  TypeConstant(mri::Object value)
    : value_(value)
  {}

private:

  mri::Object value_;

};

class TypeSameAs : public TypeConstraint {
public:

  TypeSameAs(Variable* source)
    : source_(source)
  {}

private:

  Variable* source_;

};

class TypeSelection : public TypeConstraint {
public:

  TypeSelection(TypeConstraint* right, TypeConstraint* left)
    : right_(right), left_(left)
  {}

private:

  TypeConstraint* right_;
  TypeConstraint* left_;

};

class TypeMethodEntry : public TypeConstraint {
public:

};

class TypePhi : public TypeConstraint {
public:
  
private:

  std::vector<TypeConstraint*> constraints_;
};

RBJIT_NAMESPACE_END
