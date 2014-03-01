#pragma once

#include "rubymethod.h"

RBJIT_NAMESPACE_BEGIN

rb_method_entry_t* rbjit_lookupMethod(VALUE receiver, ID methodName);
VALUE rbjit_callMethod(rb_method_entry_t* me, VALUE receiver, int argc, ...);

RBJIT_NAMESPACE_END
