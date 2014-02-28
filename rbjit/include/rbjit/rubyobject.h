#pragma once

#include <string>
#include "rbjit/common.h"

extern "C" {
struct RNode;
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

// placeholders
typedef size_t VALUE;
typedef size_t ID;

class Id;

class Object {
public:

  Object(VALUE value) : value_(value) {}
  Object(void* value) : value_((VALUE)value) {}

  VALUE value() const { return value_; }

  VALUE operator()() const { return value_; }

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

  ID operator()() const { return id_; }
private:

  ID id_;
};

} // namespace mri

RBJIT_NAMESPACE_END
