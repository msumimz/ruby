#pragma once

#include <vector>
#include <string>
#include <utility> // std::move
#include <unordered_map>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class TypeConstraint;
class MethodInfo;
class TypeContext;

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
class TypeInteger;
class TypeConstant;
class TypeEnv;
class TypeLookup;
class TypeSameAs;
class TypeExactClass;
class TypeClassOrSubclass;
class TypeSelection;
class TypeRecursion;

class TypeVisitor {
public:

  virtual bool visitType(TypeNone* type) = 0;
  virtual bool visitType(TypeAny* type) = 0;
  virtual bool visitType(TypeInteger* type) = 0;
  virtual bool visitType(TypeConstant* type) = 0;
  virtual bool visitType(TypeEnv* type) = 0;
  virtual bool visitType(TypeLookup* type) = 0;
  virtual bool visitType(TypeSameAs* type) = 0;
  virtual bool visitType(TypeExactClass* type) = 0;
  virtual bool visitType(TypeClassOrSubclass* type) = 0;
  virtual bool visitType(TypeSelection* type) = 0;
  virtual bool visitType(TypeRecursion* type) = 0;

};

////////////////////////////////////////////////////////////
// TypeConstraint

class TypeConstraint {
public:

  TypeConstraint() {}
  virtual ~TypeConstraint() {}

  // Create an object by cloning
  virtual TypeConstraint* clone() const = 0;

  // Destroy this
  virtual void destroy() { delete this; }

  // Test C++ equality
  virtual bool operator==(const TypeConstraint& other) const =  0;

  // Test live equality
  // [NOTE] Don't call this method directly because it doesn't consider the
  // case when the owner of the TypeConstraint is v. Intead, call
  // TypeContext::isSameValueAs().
  virtual bool isSameValueAs(TypeContext* typeContext, Variable* v) = 0;

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
  virtual bool isImpliedBy(const TypeInteger* type) const = 0;
  virtual bool isImpliedBy(const TypeConstant* type) const = 0;
  virtual bool isImpliedBy(const TypeEnv* type) const = 0;
  virtual bool isImpliedBy(const TypeMethodEntry* type) const = 0;
  virtual bool isImpliedBy(const TypeSameAs* type) const = 0;
  virtual bool isImpliedBy(const TypeExactClass* type) const = 0;
  virtual bool isImpliedBy(const TypeSelection* type) const = 0;

  virtual bool implies(const TypeConstraint* other) const = 0;
#endif

protected:

  void* operator new(size_t s) { return ::operator new(s); }
  void operator delete(void* p) { ::delete p; }
};

class TypeNone : public TypeConstraint {
public:

  TypeNone() {}
  static TypeNone* create() { static TypeNone none; return &none; }
  TypeNone* clone() const { return create(); }
  void destroy() {}

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
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
  static TypeAny* create() { static TypeAny any; return &any; }
  TypeAny* clone() const { return create(); }
  void destroy() {}

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }
};

// Just an integer, not a Fixnum; Internal use only.
// Used for an argument count in variable-length argument methods.
class TypeInteger : public TypeConstraint {
public:

  TypeInteger(intptr_t integer) : integer_(integer) {}
  TypeInteger* clone() const { return new TypeInteger(integer_); }

  bool operator==(const TypeConstraint& other) const;

  intptr_t integer() const { return integer_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  intptr_t integer_;
};

class TypeConstant : public TypeConstraint {
public:

  TypeConstant(mri::Object value) : value_(value) {}
  TypeConstant* clone() const { return new TypeConstant(value_); }

  bool operator==(const TypeConstraint& other) const;

  mri::Object value() const { return value_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
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
  static TypeEnv* create() { static TypeEnv env; return &env; }
  TypeEnv* clone() const { return create(); }
  void destroy() {}

  bool operator==(const TypeConstraint& other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

};

class TypeLookup : public TypeConstraint {
public:

  // Possible method entites at the call site
  class Candidate {
  public:
    Candidate(mri::Class cls, mri::MethodEntry me) : cls_(cls), me_(me) {}
    mri::Class class_() const { return cls_; }
    mri::MethodEntry methodEntry() const { return me_; }
    bool operator==(const Candidate& other) const
    { return cls_ == other.cls_ && me_ == other.me_;  }
  private:
    mri::Class cls_;
    mri::MethodEntry me_;
  };


  TypeLookup() {}
  TypeLookup(std::vector<Candidate> candidates)
    : candidates_(std::move(candidates)) {}
  TypeLookup* clone() const;

  bool operator==(const TypeConstraint& other) const;

  void addCandidate(mri::Class cls, mri::MethodEntry me)
  {
    candidates_.push_back(Candidate(cls, me));
  }

  std::vector<Candidate>& candidates() { return candidates_; }
  const std::vector<Candidate>& candidates() const { return candidates_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  std::vector<Candidate> candidates_;

};

class TypeSameAs : public TypeConstraint {
public:

  TypeSameAs(TypeContext* typeContext, Variable* source)
    : typeContext_(typeContext), source_(source) {}
  TypeSameAs* clone() const { return new TypeSameAs(typeContext_, source_); }

  bool operator==(const TypeConstraint& other) const;

  Variable* source() const { return source_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  TypeContext* typeContext_;
  Variable* source_;

};

class TypeExactClass : public TypeConstraint {
public:

  TypeExactClass(mri::Class cls) : cls_(cls) {}
  static TypeExactClass* create(mri::Class cls) { return new TypeExactClass(cls); }
  TypeExactClass* clone() const { return create(cls_); }

  bool operator==(const TypeConstraint& other) const;

  mri::Class class_() const { return cls_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }
//  bool implies(const TypeConstraint* other) const { return other->isImpliedBy(this); }

private:

  mri::Class cls_;

};

class TypeClassOrSubclass : public TypeConstraint {
public:

  TypeClassOrSubclass(mri::Class cls) : cls_(cls) {}
  static TypeClassOrSubclass* create(mri::Class cls) { return new TypeClassOrSubclass(cls); }
  TypeClassOrSubclass* clone() const { return create(cls_); }

  bool operator==(const TypeConstraint& other) const;

  mri::Class class_() const { return cls_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
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

  TypeSelection();
  TypeSelection(std::vector<TypeConstraint*> types);

  TypeSelection(TypeConstraint* type1);
  TypeSelection(TypeConstraint* type1, TypeConstraint* type2);
  TypeSelection(TypeConstraint* type1, TypeConstraint* type2, TypeConstraint* type3);

  ~TypeSelection();

  static TypeSelection* create() { return new TypeSelection(); }
  static TypeSelection* create(std::vector<TypeConstraint*> types) { return new TypeSelection(types); }
  static TypeSelection* create(TypeConstraint* type1) { return new TypeSelection(type1); }
  static TypeSelection* create(TypeConstraint* type1, TypeConstraint* type2) { return new TypeSelection(type1, type2); }
  static TypeSelection* create(TypeConstraint* type1, TypeConstraint* type2, TypeConstraint* type3) { return new TypeSelection(type1, type2, type3); }
  TypeSelection* clone() const;

  void addOption(TypeConstraint* type);

  bool operator==(const TypeConstraint& other) const;

  const std::vector<TypeConstraint*>& types() const { return types_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
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

class TypeRecursion : public TypeConstraint {
public:

  TypeRecursion(MethodInfo* mi) : mi_(mi) {}
  static TypeRecursion* create(MethodInfo* mi);
  TypeRecursion* clone() const { return create(mi_); }

  bool operator==(const TypeConstraint& other) const;

  MethodInfo* methodInfo() const { return mi_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  static std::unordered_map<MethodInfo*, TypeRecursion*> cache_;

  MethodInfo* mi_;

};

RBJIT_NAMESPACE_END
