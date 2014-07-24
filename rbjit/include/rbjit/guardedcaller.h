#pragma once

#include "rbjit/rubyobject.h"

extern "C" {
VALUE rb_rescue(VALUE(*)(...),VALUE,VALUE(*)(...),VALUE);
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

template<class F>
class GuardedCaller {
public:

  GuardedCaller(F callBody) : callBody_(callBody) {}

  mri::Object call()
  { return mri::Object(rb_rescue((VALUE(*)(...))callFunc, (VALUE)this, (VALUE(*)(...))rescueFunc, (VALUE)this)); }

private:

  static VALUE callFunc(GuardedCaller* self)
  { return self->callBody_(); }

  // If an exception occurs, simply return 0.
  static VALUE rescueFunc(GuardedCaller* self)
  { return 0; }

  F callBody_;
};

template <class F>
inline mri::Object
guardedCall(F& body)
{
  GuardedCaller<F> caller(body);
  return caller.call();
}

}

RBJIT_NAMESPACE_END
