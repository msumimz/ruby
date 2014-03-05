#pragma once

#include <string>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

class Id;

class Object {
public:

  Object(VALUE value) : value_(value) {}
  Object(void* value) : value_((VALUE)value) {}

  VALUE value() const { return value_; }

  operator VALUE() const { return value_; }

  static VALUE trueObject();
  static VALUE falseObject();
  static VALUE nilObject();
  static VALUE undefObject();

protected:

  VALUE value_;
};

class Symbol : public Object {
public:

  Symbol(VALUE sym) : Object(sym) {}

  const char* name() const;
  std::string stringName() const;

  Id id() const;

};

class Class : public Object {
public:

  Class(VALUE cls) : Object(cls) {}
};

class Id {
public:

  Id(ID id) : id_(id) {}

  const char* name() const;
  std::string stringName() const;

  ID id() const { return id_; }

  operator ID() const { return id_; }

private:

  ID id_;
};

} // namespace mri

RBJIT_NAMESPACE_END
