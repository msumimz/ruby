#pragma once

#include "rubymethod.h"

RBJIT_NAMESPACE_BEGIN

rb_method_entry_t* rbjit_lookupMethod(VALUE receiver, ID methodName);
VALUE rbjit_callMethod(rb_method_entry_t* me, int argc, VALUE receiver, ...);
VALUE rbjit_findConstant(VALUE baseClass, ID name, void* iseq);

RBJIT_NAMESPACE_END
