#include "rbjit/rubyobject.h"

#include "ruby.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

////////////////////////////////////////////////////////////
// Symbol

VALUE
Object::trueObject()
{
  return Qtrue;
}

VALUE
Object::falseObject()
{
  return Qfalse;
}

VALUE
Object::nilObject()
{
  return Qnil;
}

VALUE
Object::undefObject()
{
  return Qundef;
}

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

} // namespace mri

RBJIT_NAMESPACE_END
