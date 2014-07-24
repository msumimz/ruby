#pragma once

#include <string>
#include <vector>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

extern "C" {
  typedef struct rb_subclass_entry rb_subclass_entry_t;
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

class Id;
class Class;
class MethodEntry;

////////////////////////////////////////////////////////////
// mri::Object
// Wrapper for the MRI's RObject

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

  // Constant tests
  bool isTrueObject() const { return value_ == trueObject(); }
  bool isFalseObject() const { return value_ == falseObject(); }
  bool isNilObject() const { return value_ == nilObject(); }
  bool isUndefObject() const { return value_ == undefObject(); }

  // Type tests
  // TODO: 64-bit environment
  bool isFixnum() const { return (bool)(value_ & 1); }
  bool isSymbol() const { return (bool)(value_ & 0xf == 0xe); }
  bool isClassOrModule() const;

  // Builtin objects

  static VALUE trueObject();
  static VALUE falseObject();
  static VALUE nilObject();
  static VALUE undefObject();

protected:

  VALUE value_;
};

////////////////////////////////////////////////////////////
// mri::Symbol
// Wrapper for the MRI's Symbol

class Symbol : public Object {
public:

  Symbol(VALUE sym) : Object(sym) {}

  const char* name() const;
  std::string stringName() const;

  Id id() const;

};

////////////////////////////////////////////////////////////
// mri::Id
// Wrapper for the MRI's ID

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

////////////////////////////////////////////////////////////
// mri::String
// Wrapper for the MRI's RString

class String : public Object {
public:

  String(VALUE str) : Object(str) {}
  String(const char* str);

  const char* ToCStr() const;
  std::string toString() const;

};

////////////////////////////////////////////////////////////
// mri::SubclassEntry
// Wrapper for the MRI's rb_subclass_entry_t

// In internal.h
// struct rb_subclass_entry {
//     VALUE klass;
//     rb_subclass_entry_t *next;
// };

class SubclassEntry {
public:

  SubclassEntry() : entry_(nullptr) {}

  SubclassEntry(rb_subclass_entry_t* entry)
    : entry_(entry) {}

  mri::Class class_() const;
  SubclassEntry next() const;

  bool isNull() const { return entry_ == nullptr; }

private:

  rb_subclass_entry_t* entry_;
};

////////////////////////////////////////////////////////////
// mri::Class
// Wrapper for the MRI's RClass

class Class : public Object {
public:

  Class() : Object() {}
  Class(VALUE cls) : Object(cls) {}

  MethodEntry findMethod(ID methodName) const;
  MethodEntry findMethod(const char* methodName) const;

  // Find a constant in this class only
  Object findConstantInThisClass(ID name) const;

  // Find a constant in this class and superclasses.
  Object findConstant(ID name) const;

  bool isRegisteredAsAutoload(ID name) const;

  bool isSubclassOf(mri::Class base) const;

  // Superclass
  Class superclass() const;

  // Subclasses
  SubclassEntry subclassEntry() const;

  template <class F> bool
  traverseSubclassHierarchy(F action)
  {
    for (SubclassEntry entry = subclassEntry(); !entry.isNull(); entry = entry.next()) {
      if (!action(entry.class_())) {
        return false;
      }
      entry.class_().traverseSubclassHierarchy(action);
    }
    return true;
  }

  // Builtin classes
  static Class objectClass();
  static Class classClass();
  static Class trueClass();
  static Class falseClass();
  static Class nilClass();
  static Class fixnumClass();
  static Class bignumClass();

  std::string debugClassName() const;

};

} // namespace mri

RBJIT_NAMESPACE_END
