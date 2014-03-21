#pragma once

#include <vector>
#include <string>
#include <utility> // std::move
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class TypeConstraint;

////////////////////////////////////////////////////////////
// TypeList

class TypeList {
public:

  enum Lattice { NONE, ANY, DETERMINED };

  Lattice lattice() const { return lattice_; }
  const std::vector<mri::Class>& list() const { return list_; }

  TypeList(Lattice lattice) : lattice_(lattice)
  {}

  const std::vector<mri::Class>& typeList() const { return list_; }

  void addType(mri::Class type)
  { list_.push_back(type); }

  void join(TypeList* other)
  { list_.insert(list_.end(), other->list_.begin(), other->list_.end()); }

private:

  Lattice lattice_;
  std::vector<mri::Class> list_;

};

////////////////////////////////////////////////////////////
// TypeVistor

class TypeNone;
class TypeAny;
class TypeConstant;
class TypeEnv;
class TypeMethodEntry;
class TypeSameAs;
class TypeExactClass;
class TypeSelection;

class TypeVisitor {
public:

  virtual bool visitType(TypeNone* type) = 0;
  virtual bool visitType(TypeAny* type) = 0;
  virtual bool visitType(TypeConstant* type) = 0;
  virtual bool visitType(TypeEnv* type) = 0;
  virtual bool visitType(TypeMethodEntry* type) = 0;
  virtual bool visitType(TypeSameAs* type) = 0;
  virtual bool visitType(TypeExactClass* type) = 0;
  virtual bool visitType(TypeSelection* type) = 0;

};

////////////////////////////////////////////////////////////
// TypeConstraint

class TypeConstraint {
public:

  TypeConstraint() {}
  virtual ~TypeConstraint() {}

  // Create an object by cloning
  virtual TypeConstraint* clone() const = 0;

  // Test C++ equality
  virtual bool operator==(const TypeConstraint& other) const =  0;

  // Test live equality
  // [NOTE] Don't call this method directly because it doesn't consider the
  // case when the owner of the TypeConstraint is v. Intead, call
  // Variable::isSameValueAs().
  virtual bool isSameValueAs(Variable* v) = 0;

  // Evaluate to the boolean value
  enum Boolean { TRUE_OR_FALSE, ALWAYS_TRUE, ALWAYS_FALSE };
  virtual Boolean evaluatesToBoolean() = 0;

  // Infer a class based on type constraints
  // Returns a null class if its class is not uniquely determined.  This method
  // is a subset of resolve(), thus returning the same class if it is uniquely
  // determined.
  virtual mri::Class evaluateClass() = 0;

  // Resolve type constraints into a list of possible classes.
  // Callers are responsible for releasing TypeList*
  virtual TypeList* resolve() = 0;

  // Visitor
  virtual bool accept(TypeVisitor* visitor) = 0;

  virtual std::string debugPrint() const = 0;

#if 0
  // Implication testers
  virtual bool isImpliedBy(const TypeNone* type) const = 0;
  virtual bool isImpliedBy(const TypeAny* type) const = 0;
  virtual bool isImpliedBy(const TypeConstant* type) const = 0;
  virtual bool isImpliedBy(const TypeEnv* type) const = 0;
  virtual bool isImpliedBy(const TypeMethodEntry* type) const = 0;
  virtual bool isImpliedBy(const TypeSameAs* type) const = 0;
  virtual bool isImpliedBy(const TypeExactClass* type) const = 0;
  virtual bool isImpliedBy(const TypeSelection* type) const = 0;

  virtual bool implies(const TypeConstraint* other) const = 0;
#endif
};

class TypeNone : public TypeConstraint {
public:

  TypeNone() {}
  TypeNone* clone() const { return new TypeNone(); }

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }
};

class TypeAny : public TypeConstraint {
public:

  TypeAny() {}
  TypeAny* clone() const { return new TypeAny(); }

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }
};

class TypeConstant : public TypeConstraint {
public:

  TypeConstant(mri::Object value) : value_(value) {}
  TypeConstant* clone() const { return new TypeConstant(value_); }

  bool operator==(const TypeConstraint& other) const;

  mri::Object value() const { return value_; }

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  mri::Object value_;

};

class TypeEnv : public TypeConstraint {
public:

  TypeEnv() {}
  TypeEnv* clone() const { return new TypeEnv(); }

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

};

class TypeMethodEntry : public TypeConstraint {
public:

  TypeMethodEntry(mri::MethodEntry me) : me_(me) {}
  TypeMethodEntry* clone() const { return new TypeMethodEntry(me_); }

  bool operator==(const TypeConstraint& other) const;

  mri::MethodEntry methodEntry() const { return me_; }

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  mri::MethodEntry me_;

};

class TypeSameAs : public TypeConstraint {
public:

  TypeSameAs(Variable* source) : source_(source) {}
  TypeSameAs* clone() const { return new TypeSameAs(source_); }

  bool operator==(const TypeConstraint& other) const;

  Variable* source() const { return source_; }

  Boolean evaluatesToBoolean();
  bool isSameValueAs(Variable* v);
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  Variable* source_;

};

class TypeExactClass : public TypeConstraint {
public:

  TypeExactClass(mri::Class cls) : cls_(cls) {}
  TypeExactClass* clone() const { return new TypeExactClass(cls_); }

  bool operator==(const TypeConstraint& other) const;

  mri::Class class_() const { return cls_; }

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  mri::Class cls_;

};

class TypeSelection : public TypeConstraint {
public:

  TypeSelection() {}

  TypeSelection(std::vector<TypeConstraint*> types) : types_(std::move(types)) {}

  TypeSelection(TypeConstraint* type1)
  {
    types_.push_back(type1);
  }

  TypeSelection(TypeConstraint* type1, TypeConstraint* type2)
  {
    types_.push_back(type1);
    types_.push_back(type2);
  }

  TypeSelection(TypeConstraint* type1, TypeConstraint* type2, TypeConstraint* type3)
  {
    types_.push_back(type1);
    types_.push_back(type2);
    types_.push_back(type3);
  }

  ~TypeSelection();

  TypeSelection* clone() const;

  void addOption(TypeConstraint* type);

  bool operator==(const TypeConstraint& other) const;

  const std::vector<TypeConstraint*>& types() const { return types_; }

  bool isSameValueAs(Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  // Should not call
  TypeSelection(const TypeSelection&);

  std::vector<TypeConstraint*> types_;

};

RBJIT_NAMESPACE_END
