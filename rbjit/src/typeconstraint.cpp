#include <algorithm>
#include "rbjit/typeconstraint.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeNone

bool
TypeNone::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeNone);
}

bool
TypeNone::isSameValueAs(Variable* v)
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
TypeAny::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeAny);
}

bool
TypeAny::isSameValueAs(Variable* v)
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
// TypeConstant

bool
TypeConstant::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeConstant) &&
    static_cast<const TypeConstant&>(other).value_ == value_;
}

bool
TypeConstant::isSameValueAs(Variable* v)
{
  return this == v->typeConstraint();
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
  char buf[256];
  sprintf(buf, "Constant(%Ix)", value_);
  return buf;
}

////////////////////////////////////////////////////////////
// TypeEnv

bool
TypeEnv::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeEnv);
}

bool
TypeEnv::isSameValueAs(Variable* v)
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
// TypeMethodLookup

bool
TypeMethodEntry::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeMethodEntry) &&
    static_cast<const TypeMethodEntry&>(other).me_ == me_;
}

bool
TypeMethodEntry::isSameValueAs(Variable* v)
{
  // The null method entry indicates 'any' method, so that it should be
  // regarded as different from the other ones.
  return !methodEntry().isNull() && this == v->typeConstraint();
}

TypeConstraint::Boolean
TypeMethodEntry::evaluatesToBoolean()
{
  return TRUE_OR_FALSE;
}

mri::Class
TypeMethodEntry::evaluateClass()
{
  // No corresponding Ruby class
  return mri::Class();
}

TypeList*
TypeMethodEntry::resolve()
{
  return new TypeList(TypeList::NONE);
}

std::string
TypeMethodEntry::debugPrint() const
{
  char buf[256];
  sprintf(buf, "MethodEntry(%Ix)", me_.methodEntry());
  return buf;
}

////////////////////////////////////////////////////////////
// TypeSameAs

bool
TypeSameAs::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeSameAs) &&
    static_cast<const TypeSameAs&>(other).source_ == source_;
}

bool
TypeSameAs::isSameValueAs(Variable* v)
{
  return source_ == v || isSameValueAs(source_);
}

TypeConstraint::Boolean
TypeSameAs::evaluatesToBoolean()
{
  return source_->typeConstraint()->evaluatesToBoolean();
}

mri::Class
TypeSameAs::evaluateClass()
{
  return source_->typeConstraint()->evaluateClass();
}

TypeList*
TypeSameAs::resolve()
{
  return source_->typeConstraint()->resolve();
}

std::string
TypeSameAs::debugPrint() const
{
  char buf[256];
  sprintf(buf, "SameAs(%Ix)", source_);
  return buf;
}

////////////////////////////////////////////////////////////
// TypeExactClass

bool
TypeExactClass::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeExactClass) &&
    static_cast<const TypeExactClass&>(other).cls_ == cls_;
}

bool
TypeExactClass::isSameValueAs(Variable* v)
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
  char buf[256];
  sprintf(buf, "ExactClass(%Ix)", cls_);
  return buf;
}

////////////////////////////////////////////////////////////
// TypeSelection

void
TypeSelection::addOption(TypeConstraint* type)
{
  types_.push_back(type);
}

bool
TypeSelection::operator==(const TypeConstraint& other) const
{
  return typeid(other) == typeid(TypeSelection) &&
    static_cast<const TypeSelection&>(other).types_ == types_;
}

bool
TypeSelection::isSameValueAs(Variable* v)
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
  TypeList* list = resolve();
  if (list->typeList().empty()) {
    delete list;
    return mri::Class();
  }

  auto i = list->typeList().cbegin();
  auto end = list->typeList().cend();
  mri::Class cls = *i++;
  for (; i != end; ++i) {
    if (*i != cls) {
      delete list;
      return mri::Class();
    }
  }
  delete list;

  return cls;
}

TypeList*
TypeSelection::resolve()
{
  if (types_.empty()) {
    return new TypeList(TypeList::NONE);
  }

  TypeList* type = types_[0]->resolve();

  for (size_t i = 1; i < types_.size(); ++i) {

    if (type->lattice() == TypeList::ANY) {
      return type;
    }

    TypeList* otherType = types_[i]->resolve();
    switch (type->lattice()) {
    case TypeList::NONE:
      delete type;
      type = otherType;
      break;

    case TypeList::DETERMINED:
      switch (otherType->lattice()) {
      case TypeList::NONE:
        break;

      case TypeList::ANY:
        delete type;
        type = otherType;
        break;

      case TypeList::DETERMINED:
        type->join(otherType);
        delete otherType;
        break;

      default:
        RBJIT_UNREACHABLE;
      }
      break;

    default:
      RBJIT_UNREACHABLE;
    }
  }

  return type;
}

std::string
TypeSelection::debugPrint() const
{
  char buf[256];
  sprintf(buf, "Selection (%d)", types_.size());
  std::string out = buf;

  std::for_each(types_.cbegin(), types_.cend(), [&](const TypeConstraint* type) {
    out += ' ';
    out += type->debugPrint();
  });
  return out;
}

RBJIT_NAMESPACE_END
