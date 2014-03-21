#pragma once

#include <string>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

class Id;
class Class;
class MethodEntry;

class Object {
public:

  Object() : value_(0) {}
  Object(VALUE value) : value_(value) {}
  Object(void* value) : value_((VALUE)value) {}

  VALUE value() const { return value_; }

  operator VALUE() const { return value_; }

  Class class_() const;

  bool isTrue() const { return value_ != falseObject() && value_ != nilObject(); }

  bool isNull() const { return value_ == 0; }

  // Builtin objects
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

class Id {
public:

  Id(ID id) : id_(id) {}

  Id(const char* name);

  const char* name() const;
  std::string stringName() const;

  ID id() const { return id_; }

  operator ID() const { return id_; }

private:

  ID id_;
};

class Class : public Object {
public:

  Class() : Object() {}
  Class(VALUE cls) : Object(cls) {}

  MethodEntry findMethod(ID methodName) const;
  MethodEntry findMethod(const char* methodName) const;

  // Builtin classes
  static Class objectClass();
  static Class classClass() ;
  static Class trueClass() ;
  static Class falseClass() ;
  static Class nilClass() ;
  static Class fixnumClass();
  static Class bignumClass();

};

} // namespace mri

RBJIT_NAMESPACE_END
