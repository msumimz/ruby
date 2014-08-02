#pragma once

#include "rubymethod.h"

RBJIT_NAMESPACE_BEGIN

extern "C" {

// All argumments and return values should have the same size as the platform's
// natural word size, because NativeCompiler doesn't respect their types and
// treats them as integers.

rb_method_entry_t* rbjit_lookupMethod(VALUE receiver, ID methodName);
VALUE rbjit_callMethod(rb_method_entry_t* me, size_t argc, VALUE receiver, ...);

VALUE rbjit_findConstant(VALUE baseClass, ID name, void* iseq);

VALUE rbjit_createArray(size_t count, ...);
VALUE rbjit_convertToArray(VALUE obj);
VALUE rbjit_concatenateArrays(VALUE a1, VALUE a2);
VALUE rbjit_pushToArray(VALUE array, VALUE obj);

VALUE rbjit_createRange(VALUE low, VALUE high, size_t exclusive);

VALUE rbjit_duplicateString(VALUE s);
VALUE rbjit_convertToString(VALUE obj);
VALUE rbjit_concatenateStrings(size_t count, ...);

VALUE rbjit_createHash(size_t count, ...);

}

RBJIT_NAMESPACE_END
