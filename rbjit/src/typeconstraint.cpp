#include <algorithm>
#include <utility> // std::move
#include <cassert>
#include "rbjit/typeconstraint.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/typecontext.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeNone

bool
TypeNone::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeNone);
}

bool
TypeNone::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeNone::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeNone::evaluateClass()
{
  return mri::Class();
}

bool
TypeNone::isExactClass(mri::Class cls)
{
  return false;
}

bool
TypeNone::isImpossibleToBeClass(mri::Class cls)
{
  return true;
}

TypeList*
TypeNone::resolve()
{
  return new TypeList(TypeList::NONE);
}

std::string
TypeNone::debugPrint() const
{
  return "None";
}

////////////////////////////////////////////////////////////
// TypeAny

bool
TypeAny::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeAny);
}

bool
TypeAny::isSameValueAs(TypeContext* type, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeAny::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeAny::evaluateClass()
{
  return mri::Class();
}

bool
TypeAny::isExactClass(mri::Class cls)
{
  return false;
}

bool
TypeAny::isImpossibleToBeClass(mri::Class cls)
{
  return false;
}

TypeList*
TypeAny::resolve()
{
  return new TypeList(TypeList::ANY);
}

std::string
TypeAny::debugPrint() const
{
  return "Any";
}

////////////////////////////////////////////////////////////
// TypeInteger

bool
TypeInteger::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeInteger) &&
    static_cast<const TypeInteger*>(other)->integer_ == integer_;
}

TypeConstraint::Boolean
TypeInteger::evaluatesToBoolean()
{
  // Not defined because this is not a ruby object
  return TRUE_OR_FALSE;
}

mri::Class
TypeInteger::evaluateClass()
{
  // Not defined because this is not a ruby object
  return mri::Class();
}

bool
TypeInteger::isExactClass(mri::Class cls)
{
  // Always false because this is not a ruby object
  return false;
}

bool
TypeInteger::isImpossibleToBeClass(mri::Class cls)
{
  // Always true because this is not a ruby object
  return true;
}

TypeList*
TypeInteger::resolve()
{
  return new TypeList(TypeList::ANY);
}

std::string
TypeInteger::debugPrint() const
{
  return stringFormat("Integer(%Ix)", integer_);
}

////////////////////////////////////////////////////////////
// TypeConstant

bool
TypeConstant::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeConstant) &&
    static_cast<const TypeConstant*>(other)->value_ == value_;
}

bool
TypeConstant::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return equals(typeContext->typeConstraintOf(v));
}

TypeConstraint::Boolean
TypeConstant::evaluatesToBoolean()
{
  if (mri::Object(value_).isTrue()) {
    return ALWAYS_TRUE;
  }
  return ALWAYS_FALSE;
}

mri::Class
TypeConstant::evaluateClass()
{
  return value_.class_();
}

bool
TypeConstant::isExactClass(mri::Class cls)
{
  return value_.class_() == cls;
}

bool
TypeConstant::isImpossibleToBeClass(mri::Class cls)
{
  return value_.class_() != cls;
}

TypeList*
TypeConstant::resolve()
{
  auto list = new TypeList(TypeList::DETERMINED);
  list->addType(value_.class_());
  return list;
}

std::string
TypeConstant::debugPrint() const
{
  return stringFormat("Constant(%Ix)", value_);
}

////////////////////////////////////////////////////////////
// TypeEnv

bool
TypeEnv::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeEnv);
}

bool
TypeEnv::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return equals(typeContext->typeConstraintOf(v));
}

TypeConstraint::Boolean
TypeEnv::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeEnv::evaluateClass()
{
  // No corresponding Ruby class
  return mri::Class();
}

bool
TypeEnv::isExactClass(mri::Class cls)
{
  return false;
}

bool
TypeEnv::isImpossibleToBeClass(mri::Class cls)
{
  return true;
}

TypeList*
TypeEnv::resolve()
{
  return new TypeList(TypeList::NONE);
}

std::string
TypeEnv::debugPrint() const
{
  return "Env";
}

////////////////////////////////////////////////////////////
// TypeLookup

TypeLookup*
TypeLookup::clone() const
{
  return new TypeLookup(candidates_, determined_);
}

bool
TypeLookup::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeLookup) &&
    static_cast<const TypeLookup*>(other)->candidates_ == candidates_ &&
    static_cast<const TypeLookup*>(other)->determined_ == determined_;
}

bool
TypeLookup::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeLookup::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeLookup::evaluateClass()
{
  // No corresponding Ruby class
  return mri::Class();
}

bool
TypeLookup::isExactClass(mri::Class cls)
{
  return false;
}

bool
TypeLookup::isImpossibleToBeClass(mri::Class cls)
{
  return true;
}

TypeList*
TypeLookup::resolve()
{
  return new TypeList(TypeList::NONE);
}

std::string
TypeLookup::debugPrint() const
{
  std::string out = stringFormat("Lookup[%d, %s]",
    candidates_.size(), isDetermined() ? "true" : "false");

  for (auto i = candidates_.cbegin(), end = candidates_.cend(); i != end; ++i) {
    out += stringFormat(" (%Ix)", i->ptr());
  };
  return out;
}

////////////////////////////////////////////////////////////
// TypeSameAs

TypeSameAs::TypeSameAs(TypeContext* typeContext, Variable* source)
  : typeContext_(typeContext)
{
  for (;;) {
    assert(typeContext->cfg()->containsVariable(source));
    TypeConstraint* t = typeContext->typeConstraintOf(source);
    TypeSameAs* s = dynamic_cast<TypeSameAs*>(t);
    if (!s) {
      break;
    }
    source = s->source_;
  }
  source_ = source;
}

TypeConstraint*
TypeSameAs::independantClone() const
{
  return typeContext_->typeConstraintOf(source_)->independantClone();
}

bool
TypeSameAs::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeSameAs) &&
    static_cast<const TypeSameAs*>(other)->source_ == source_;
}

bool
TypeSameAs::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return source_ == v || typeContext->typeConstraintOf(source_)->isSameValueAs(typeContext, v);
}

TypeConstraint::Boolean
TypeSameAs::evaluatesToBoolean()
{
  return typeContext_->typeConstraintOf(source_)->evaluatesToBoolean();
}

mri::Class
TypeSameAs::evaluateClass()
{
  return typeContext_->typeConstraintOf(source_)->evaluateClass();
}

bool
TypeSameAs::isExactClass(mri::Class cls)
{
  return typeContext_->typeConstraintOf(source_)->isExactClass(cls);
}

bool
TypeSameAs::isImpossibleToBeClass(mri::Class cls)
{
  return typeContext_->typeConstraintOf(source_)->isImpossibleToBeClass(cls);
}

TypeList*
TypeSameAs::resolve()
{
  return typeContext_->typeConstraintOf(source_)->resolve();
}

std::string
TypeSameAs::debugPrint() const
{
  return stringFormat("SameAs(%Ix %Ix)", typeContext_, source_);
}

////////////////////////////////////////////////////////////
// TypeExactClass

bool
TypeExactClass::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeExactClass) &&
    static_cast<const TypeExactClass*>(other)->cls_ == cls_;
}

bool
TypeExactClass::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeExactClass::evaluatesToBoolean()
{
  if (cls_ == mri::Class::falseClass() || cls_ == mri::Class::nilClass()) {
    return ALWAYS_FALSE;
  }
  return ALWAYS_TRUE;
}

mri::Class
TypeExactClass::evaluateClass()
{
  return cls_;
}

bool
TypeExactClass::isExactClass(mri::Class cls)
{
  return cls_ == cls;
}

bool
TypeExactClass::isImpossibleToBeClass(mri::Class cls)
{
  return cls_ != cls;
}

TypeList*
TypeExactClass::resolve()
{
  auto list = new TypeList(TypeList::DETERMINED);
  list->addType(cls_);
  return list;
}

std::string
TypeExactClass::debugPrint() const
{
  return stringFormat("ExactClass(%s)", cls_.debugClassName().c_str());
}

////////////////////////////////////////////////////////////
// TypeClassOrSubclass

bool
TypeClassOrSubclass::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeClassOrSubclass) &&
    static_cast<const TypeClassOrSubclass*>(other)->cls_ == cls_;
}

bool
TypeClassOrSubclass::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeClassOrSubclass::evaluatesToBoolean()
{
  if (cls_ == mri::Class::falseClass() || cls_ == mri::Class::nilClass()) {
    return ALWAYS_FALSE;
  }
  return ALWAYS_TRUE;
}

mri::Class
TypeClassOrSubclass::evaluateClass()
{
  std::unique_ptr<TypeList> list(resolve());
  if (list->isDetermined() && list->size() == 1) {
    mri::Class cls = list->typeList()[0];
    return cls;
  }
  return mri::Class();
}

bool
TypeClassOrSubclass::isExactClass(mri::Class cls)
{
  mri::SubclassEntry entry = cls_.subclassEntry();
  if (!entry.isNull()) {
    // If the class has any subclass, we can't determine the exact class.
    return false;
  }

  return cls_ == cls;
}

bool
TypeClassOrSubclass::isImpossibleToBeClass(mri::Class cls)
{
  return !cls.isSubclassOf(cls_);
}

TypeList*
TypeClassOrSubclass::resolve()
{
  auto list = new TypeList(TypeList::DETERMINED);
  list->addType(cls_);
  resolveInternal(cls_, list);

  return list;
}

bool
TypeClassOrSubclass::resolveInternal(mri::Class cls, TypeList* list)
{
  for (mri::SubclassEntry entry = cls.subclassEntry(); !entry.isNull(); entry = entry.next()) {
    if (!list->includes(entry.class_())) {
      list->addType(entry.class_());
    }
    if (list->size() >= MAX_CANDIDATE_COUNT) {
      list->setLattice(TypeList::ANY);
      return false;
    }
    if (!resolveInternal(entry.class_(), list)) {
      return false;
    }
  }
  return true;
}

std::string
TypeClassOrSubclass::debugPrint() const
{
  return stringFormat("ClassOrSubclass(%s)", cls_.debugClassName().c_str());
}

////////////////////////////////////////////////////////////
// TypeSelection

TypeSelection::TypeSelection()
{}


TypeSelection::TypeSelection(std::vector<TypeConstraint*> types)
  : types_(std::move(types))
{}

TypeSelection*
TypeSelection::create()
{
  return new TypeSelection();
}

TypeSelection*
TypeSelection::create(TypeConstraint* type1)
{
  TypeSelection* sel = new TypeSelection();
  sel->types_.push_back(type1);
  return sel;
}

TypeSelection*
TypeSelection::create(TypeConstraint* type1, TypeConstraint* type2)
{
  TypeSelection* sel = new TypeSelection();
  sel->types_.push_back(type1);
  sel->types_.push_back(type2);
  return sel;
}

TypeSelection*
TypeSelection::create(TypeConstraint* type1, TypeConstraint* type2, TypeConstraint* type3)
{
  TypeSelection* sel = new TypeSelection();
  sel->types_.push_back(type1);
  sel->types_.push_back(type2);
  sel->types_.push_back(type3);
  return sel;
}

TypeSelection*
TypeSelection::clone() const
{
  std::vector<TypeConstraint*> types(types_.size());

  auto p = types.begin();
  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    *p++ = (*i)->clone();
  }
  assert(p == types.end());

  return new TypeSelection(std::move(types));
}

TypeSelection*
TypeSelection::independantClone() const
{
  std::vector<TypeConstraint*> types(types_.size());

  auto p = types.begin();
  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    // TODO: check duplicates
    *p++ = (*i)->independantClone();
  }
  assert(p == types.end());

  return new TypeSelection(std::move(types));
}

TypeSelection::~TypeSelection()
{
  clear();
}

void
TypeSelection::add(const TypeConstraint& type)
{
  if (typeid(type) == typeid(TypeSelection)) {
    // For TypeSelection, add each element in it
    const TypeSelection& sel = static_cast<const TypeSelection&>(type);
    for (auto i = sel.types().cbegin(), end = sel.types().cend(); i != end; ++i) {
      add(**i);
    }
    return;
  }

  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    if ((*i)->equals(&type)) {
      return;
    }
  }

  types_.push_back(type.clone());
}

void
TypeSelection::clear()
{
  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    (*i)->destroy();
  }
  types_.clear();
}

bool
TypeSelection::equals(const TypeConstraint* other) const
{
  if (typeid(*other) != typeid(TypeSelection)) {
    return types_.size() == 1 && types_[0]->equals(other);
  }

  if (types_.size() != static_cast<const TypeSelection*>(other)->types_.size()) {
    return false;
  }

  std::vector<TypeConstraint*> o = static_cast<const TypeSelection*>(other)->types_;

  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    bool matched = false;
    for (auto j = o.begin(), jend = o.end(); j != jend; ++j) {
      if (*j && (*i)->equals(*j)) {
        matched = true;
        *j = 0;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }

  return true;
}

bool
TypeSelection::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  return false;
}

TypeConstraint::Boolean
TypeSelection::evaluatesToBoolean()
{
  int trueCount = 0;
  int falseCount = 0;
  auto i = types_.cbegin();
  auto end = types_.cend();
  for (; i != end; ++i) {
    switch ((*i)->evaluatesToBoolean()) {
    case ALWAYS_TRUE:
      if (falseCount > 0) {
        return TRUE_OR_FALSE;
      }
      ++trueCount;
      break;

    case ALWAYS_FALSE:
      if (trueCount > 0) {
        return TRUE_OR_FALSE;
      }
      ++falseCount;
      break;

    case TRUE_OR_FALSE:
      return TRUE_OR_FALSE;

    default:
      RBJIT_UNREACHABLE;
    }
  }

  return trueCount > 0 ? ALWAYS_TRUE : ALWAYS_FALSE;
}

mri::Class
TypeSelection::evaluateClass()
{
  std::unique_ptr<TypeList> list(resolve());
  if (list->typeList().empty()) {
    return mri::Class();
  }

  auto i = list->typeList().cbegin();
  auto end = list->typeList().cend();
  mri::Class cls = *i++;
  for (; i != end; ++i) {
    if (*i != cls) {
      return mri::Class();
    }
  }

  return cls;
}

bool
TypeSelection::isExactClass(mri::Class cls)
{
  return evaluateClass() == cls;
}

bool
TypeSelection::isImpossibleToBeClass(mri::Class cls)
{
  std::unique_ptr<TypeList> list(resolve());
  return list->isDetermined() && !list->includes(cls);
}

TypeList*
TypeSelection::resolve()
{
  if (types_.empty()) {
    return new TypeList(TypeList::NONE);
  }

  std::unique_ptr<TypeList> type(types_[0]->resolve());

  for (size_t i = 1; i < types_.size(); ++i) {

    if (type->lattice() == TypeList::ANY) {
      return type.release();
    }

    std::unique_ptr<TypeList> otherType(types_[i]->resolve());
    switch (type->lattice()) {
    case TypeList::NONE:
      type = std::move(otherType);
      break;

    case TypeList::DETERMINED:
      switch (otherType->lattice()) {
      case TypeList::NONE:
        break;

      case TypeList::ANY:
        type = std::move(otherType);
        break;

      case TypeList::DETERMINED:
        type->join(otherType.get());
        break;

      default:
        RBJIT_UNREACHABLE;
      }
      break;

    default:
      RBJIT_UNREACHABLE;
    }
  }

  return type.release();
}

std::string
TypeSelection::debugPrint() const
{
  std::string out = stringFormat("Selection[%d]", types_.size());

  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    out += ' ';
    out += (*i)->debugPrint();
  };
  return out;
}

////////////////////////////////////////////////////////////
// TypeRecursion

std::unordered_map<MethodInfo*, TypeRecursion*> TypeRecursion::cache_;

TypeRecursion*
TypeRecursion::create(MethodInfo* mi)
{
  TypeRecursion* t = cache_[mi];
  if (t) {
    return t;
  }

  t = new TypeRecursion(mi);
  cache_[mi] = t;

  return t;
}

bool
TypeRecursion::equals(const TypeConstraint* other) const
{
  return typeid(other) == typeid(TypeRecursion) &&
    static_cast<const TypeRecursion*>(other)->mi_ == mi_;
}

bool
TypeRecursion::isSameValueAs(TypeContext* typeContext, Variable* v)
{
  // Comparison by pointer
  return this == typeContext->typeConstraintOf(v);
}

TypeConstraint::Boolean
TypeRecursion::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeRecursion::evaluateClass()
{
  return mri::Class();
}

bool
TypeRecursion::isExactClass(mri::Class cls)
{
  return false;
}

bool
TypeRecursion::isImpossibleToBeClass(mri::Class cls)
{
  return false;
}

TypeList*
TypeRecursion::resolve()
{
  // Resolved to an empty type list
  return new TypeList(TypeList::DETERMINED);
}

std::string
TypeRecursion::debugPrint() const
{
  return stringFormat("TypeRecursion(%Ix)", mi_);
}

RBJIT_NAMESPACE_END
