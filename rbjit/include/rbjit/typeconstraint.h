#pragma once

#include <vector>
#include <string>
#include <utility> // std::move
#include <algorithm> // std::find
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

  TypeList(Lattice lattice) : lattice_(lattice) {}

  Lattice lattice() const { return lattice_; }
  void setLattice(Lattice l) { lattice_ = l; }

  const std::vector<mri::Class>& typeList() const { return list_; }

  size_t size() const { return list_.size(); }

  bool includes(mri::Class cls)
  { return std::find(list_.cbegin(), list_.cend(), cls) != list_.cend(); }

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

  enum { MAX_CANDIDATE_COUNT = 3 };

  TypeConstraint() {}
  virtual ~TypeConstraint() {}

  // Create an object by cloning
  virtual TypeConstraint* clone() const = 0;

  // Clone as an 'independant' type constraint, or a type constraint that does
  // not refer to any variables or constraints.  Currently, the only instance
  // of a 'dependant' type constraint is TypeSameAs.
  virtual TypeConstraint* independantClone() const = 0;

  // Destroy this
  // Should be overwriten by classes having unusual life cycles
  virtual void destroy() { delete this; }

  // Test C++ equality
  virtual bool equals(const TypeConstraint* other) const =  0;

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

protected:

  void* operator new(size_t s) { return ::operator new(s); }
  void operator delete(void* p) { ::delete p; }

private:

  bool operator==(const TypeConstraint&) const;

};

class TypeNone : public TypeConstraint {
public:

  TypeNone() {}
  static TypeNone* create() { static TypeNone none; return &none; }
  TypeNone* clone() const { return create(); }
  TypeNone* independantClone() const { return clone(); }
  void destroy() {}

  bool equals(const TypeConstraint* other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

};

class TypeAny : public TypeConstraint {
public:

  TypeAny() {}
  static TypeAny* create() { static TypeAny any; return &any; }
  TypeAny* clone() const { return create(); }
  TypeAny* independantClone() const { return clone(); }
  void destroy() {}

  bool equals(const TypeConstraint* other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

};

// Just an integer, not a Fixnum; Internal use only.
// Used for an argument count in variable-length argument methods.
class TypeInteger : public TypeConstraint {
public:

  TypeInteger(intptr_t integer) : integer_(integer) {}
  TypeInteger* clone() const { return new TypeInteger(integer_); }
  TypeInteger* independantClone() const { return clone(); }

  bool equals(const TypeConstraint* other) const;

  intptr_t integer() const { return integer_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  intptr_t integer_;

};

class TypeConstant : public TypeConstraint {
public:

  TypeConstant(mri::Object value) : value_(value) {}
  TypeConstant* clone() const { return new TypeConstant(value_); }
  TypeConstant* independantClone() const { return clone(); }

  bool equals(const TypeConstraint* other) const;

  mri::Object value() const { return value_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  mri::Object value_;

};

class TypeEnv : public TypeConstraint {
public:

  TypeEnv() {}
  static TypeEnv* create() { static TypeEnv env; return &env; }
  TypeEnv* clone() const { return create(); }
  TypeEnv* independantClone() const { return clone(); }
  void destroy() {}

  bool equals(const TypeConstraint* other) const;

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

};

class TypeLookup : public TypeConstraint {
public:

  TypeLookup() : determined_(false) {}

  TypeLookup(bool determined) : determined_(determined) {}

  TypeLookup(const std::vector<mri::MethodEntry>& candidates, bool determined)
    : candidates_(candidates), determined_(determined)
  {}

  TypeLookup* clone() const;
  TypeLookup* independantClone() const { return clone(); }

  bool equals(const TypeConstraint* other) const;

  bool includes(mri::MethodEntry me)
  { return std::find(candidates_.cbegin(), candidates_.cend(), me) != candidates_.cend(); }

  void addCandidate(mri::MethodEntry me)
  { candidates_.push_back(me); }

  std::vector<mri::MethodEntry>& candidates() { return candidates_; }
  const std::vector<mri::MethodEntry>& candidates() const { return candidates_; }

  bool isDetermined() const { return determined_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  std::vector<mri::MethodEntry> candidates_;
  bool determined_;

};

class TypeSameAs : public TypeConstraint {
public:

  TypeSameAs(TypeContext* typeContext, Variable* source)
    : typeContext_(typeContext), source_(source) {}
  TypeSameAs* clone() const { return new TypeSameAs(typeContext_, source_); }
  TypeConstraint* independantClone() const;

  bool equals(const TypeConstraint* other) const;

  TypeContext* typeContext() const { return typeContext_; }
  Variable* source() const { return source_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  TypeContext* typeContext_;
  Variable* source_;

};

class TypeExactClass : public TypeConstraint {
public:

  TypeExactClass(mri::Class cls) : cls_(cls) {}
  static TypeExactClass* create(mri::Class cls) { return new TypeExactClass(cls); }
  TypeExactClass* clone() const { return create(cls_); }
  TypeExactClass* independantClone() const { return clone(); }

  bool equals(const TypeConstraint* other) const;

  mri::Class class_() const { return cls_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  mri::Class cls_;

};

class TypeClassOrSubclass : public TypeConstraint {
public:

  TypeClassOrSubclass(mri::Class cls) : cls_(cls) {}
  static TypeClassOrSubclass* create(mri::Class cls) { return new TypeClassOrSubclass(cls); }
  TypeClassOrSubclass* clone() const { return create(cls_); }
  TypeClassOrSubclass* independantClone() const { return clone(); }

  bool equals(const TypeConstraint* other) const;

  mri::Class class_() const { return cls_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  static bool resolveInternal(mri::Class cls, TypeList* list);

  mri::Class cls_;

};

class TypeSelection : public TypeConstraint {
public:

  TypeSelection();

  ~TypeSelection();

  // The create() methods take the ownership of the arguments
  static TypeSelection* create();
  static TypeSelection* create(TypeConstraint* type1);
  static TypeSelection* create(TypeConstraint* type1, TypeConstraint* type2);
  static TypeSelection* create(TypeConstraint* type1, TypeConstraint* type2, TypeConstraint* type3);

  TypeSelection* clone() const;
  TypeSelection* independantClone() const { return clone(); }

  // The add method adds the clone of the argument
  void add(const TypeConstraint& type);
  void clear();

  bool equals(const TypeConstraint* other) const;

  const std::vector<TypeConstraint*>& types() const { return types_; }

  bool isSameValueAs(TypeContext* typeContext, Variable* v);
  Boolean evaluatesToBoolean();
  mri::Class evaluateClass();
  TypeList* resolve();
  std::string debugPrint() const;

  bool accept(TypeVisitor* visitor) { return visitor->visitType(this); }

private:

  // Should not call
  TypeSelection(const TypeSelection&);

  // Called inside clone()
  TypeSelection(std::vector<TypeConstraint*> types);

  std::vector<TypeConstraint*> types_;

};

class TypeRecursion : public TypeConstraint {
public:

  TypeRecursion(MethodInfo* mi) : mi_(mi) {}
  static TypeRecursion* create(MethodInfo* mi);
  TypeRecursion* clone() const { return create(mi_); }
  TypeRecursion* independantClone() const { return clone(); }
  void destroy() {}

  bool equals(const TypeConstraint* other) const;

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
