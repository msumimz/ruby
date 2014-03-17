#include "rbjit/rubyobject.h"
#include "rbjit/rubymethod.h"

#include "ruby.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

////////////////////////////////////////////////////////////
// Object

Class
Object::class_() const
{
  return Class(rb_class_of(value_));
}

// Builtin objects

VALUE Object::trueObject() { return Qtrue; }
VALUE Object::falseObject() { return Qfalse; }
VALUE Object::nilObject() { return Qnil; }
VALUE Object::undefObject() { return Qundef; }

////////////////////////////////////////////////////////////
// Symbol

const char*
Symbol::name() const
{
  return rb_id2name(SYM2ID((VALUE)value_));
}

std::string
Symbol::stringName() const
{
  return std::string(name());
}

Id
Symbol::id() const
{
  return Id((ID)SYM2ID((VALUE)value_));
}

////////////////////////////////////////////////////////////
// Id

Id::Id(const char* name)
  : id_(rb_intern(name))
{}

const char*
Id::name() const
{
  return rb_id2name((ID)id_);
}

std::string
Id::stringName() const
{
  return std::string(name());
}

////////////////////////////////////////////////////////////
// Class

MethodEntry
Class::findMethod(ID methodName)
{
  return mri::MethodEntry(value(), methodName);
}

MethodEntry
Class::findMethod(const char* methodName)
{
  return findMethod(mri::Id(methodName));
}

// Builtin classes

Class Class::objectClass() { return rb_cObject; }
Class Class::classClass() { return rb_cClass; }
Class Class::trueClass() { return rb_cTrueClass; }
Class Class::falseClass() { return rb_cFalseClass; }
Class Class::nilClass() { return rb_cNilClass; }
Class Class::fixnumClass() { return rb_cFixnum; }
Class Class::bignumClass() { return rb_cBignum; }

} // namespace mri

RBJIT_NAMESPACE_END
