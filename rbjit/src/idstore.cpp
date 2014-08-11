#include "rbjit/rubyobject.h"
#include "rbjit/idstore.h"

RBJIT_NAMESPACE_BEGIN

const char* ID_NAMES[] = {
  "",

  // Keywords
  "<self>",
  "<argc>",
  "<argv>",
  "<env>",

  "eval",
  "instance_eval",
  "instance_exec",
  "__send__",
  "send",
  "public_send",
  "module_exec",
  "class_exec",
  "module_eval",
  "class_eval",
  "load",
  "require",
  "gem",

  "rbjit__test",
  "rbjit__test_not",
  "rbjit__is_fixnum",
  "rbjit__is_true",
  "rbjit__is_false",
  "rbjit__is_nil",
  "rbjit__class_of",
  "rbjit__bitwise_add",
  "rbjit__bitwise_sub",
  "rbjit__bitwise_add_overflow",
  "rbjit__bitwise_sub_overflow",
  "rbjit__bitwise_compare_eq",
  "rbjit__bitwise_compare_ne",
  "rbjit__bitwise_compare_ugt",
  "rbjit__bitwise_compare_uge",
  "rbjit__bitwise_compare_ult",
  "rbjit__bitwise_compare_ule",
  "rbjit__bitwise_compare_sgt",
  "rbjit__bitwise_compare_sge",
  "rbjit__bitwise_compare_slt",
  "rbjit__bitwise_compare_sle",

  "rbjit__convert_to_array",
  "rbjit__concat_arrays",
  "rbjit__push_to_array",

  "rbjit__convert_to_string",
  "rbjit__concat_strings",

  "rbjit__typecast_fixnum",
  "rbjit__typecast_fixnum_bignum",

  // Symbols
  "+",
  "-",
  "*",
  "/",

  0
};

ID IdStore::predefined_[PREDEFINED_ID_COUNT];

void
IdStore::setup()
{
  int i;
  for (i = 0; ID_NAMES[i]; ++i) {
    predefined_[i] = mri::Id(ID_NAMES[i]).id();
  }
  assert(i == PREDEFINED_ID_COUNT);
}

RBJIT_NAMESPACE_END

