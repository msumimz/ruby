#include <algorithm>
#include <utility> // std::move
#include <cassert>
#include "rbjit/typeconstraint.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/typecontext.h"

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
  // Each TypeEnv is regarded as different from the other.
  return false;
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
  return new TypeLookup(candidates_);
}

bool
TypeLookup::equals(const TypeConstraint* other) const
{
  return typeid(*other) == typeid(TypeLookup) &&
    static_cast<const TypeLookup*>(other)->candidates_ == candidates_;
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
    out += stringFormat(" (%Ix)", i->methodEntry());
  };
  return out;
}

////////////////////////////////////////////////////////////
// TypeSameAs

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

TypeList*
TypeSameAs::resolve()
{
  return typeContext_->typeConstraintOf(source_)->resolve();
}

std::string
TypeSameAs::debugPrint() const
{
  return stringFormat("SameAs(%Ix, %Ix)", typeContext_, source_);
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
  return stringFormat("ExactClass(%Ix)", cls_);
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
  TypeList* list = resolve();
  if (list->size() == 1) {
    mri::Class cls = list->typeList()[0];
    delete list;
    return cls;
  }
  return mri::Class();
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
  return stringFormat("ClassOrSubclass(%Ix)", cls_);
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

  std::vector<TypeConstraint*> o = static_cast<const TypeSelection*>(other)->types_;

  if (types_.size() != o.size()) {
    return false;
  }

  for (auto i = types_.cbegin(), end = types_.cend(); i != end; ++i) {
    for (auto j = o.begin(), jend = o.end(); j != jend; ++j) {
      if (!((*i)->equals(*j))) {
        return false;
      }
      *j = 0;
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
