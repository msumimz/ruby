#include "rbjit/rubyobject.h"
#include "rbjit/rubymethod.h"

extern "C" {
#include "ruby.h"
#include "internal.h" // rb_classext_t, rb_subclass_entry_t
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

////////////////////////////////////////////////////////////
// mri::Object

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
// mri::Symbol

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
// mri::Id

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
// mri::SubclassEntry

Class
SubclassEntry::class_() const
{
  return entry_->klass;
}

SubclassEntry
SubclassEntry::next() const
{
  return entry_->next;
}

////////////////////////////////////////////////////////////
// mri::Class

MethodEntry
Class::findMethod(ID methodName) const
{
  return mri::MethodEntry(value(), methodName);
}

MethodEntry
Class::findMethod(const char* methodName) const
{
  return findMethod(mri::Id(methodName));
}

SubclassEntry
Class::subclassEntry() const
{
  rb_classext_t* clsext = ((RClass*)value_)->ptr;
  return SubclassEntry(clsext->subclasses);
}

// Builtin classes

Class Class::objectClass() { return rb_cObject; }
Class Class::classClass() { return rb_cClass; }
Class Class::trueClass() { return rb_cTrueClass; }
Class Class::falseClass() { return rb_cFalseClass; }
Class Class::nilClass() { return rb_cNilClass; }
Class Class::fixnumClass() { return rb_cFixnum; }
Class Class::bignumClass() { return rb_cBignum; }

std::string
Class::debugClassName() const
{
  return std::string(rb_class2name(value_));
}

} // namespace mri

RBJIT_NAMESPACE_END
