#pragma once

#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

enum PredefinedId {
  ID_NULL,

  // Keywords
  ID_self,
  ID_argc,
  ID_argv,
  ID_env,

  ID_eval,
  ID_instance_eval,
  ID_instance_exec,
  ID___send__,
  ID_send,
  ID_public_send,
  ID_module_exec,
  ID_class_exec,
  ID_module_eval,
  ID_class_eval,
  ID_load,
  ID_require,
  ID_gem,

  // Primitives
  ID_rbjit__test,
  ID_rbjit__test_not,
  ID_rbjit__is_fixnum,
  ID_rbjit__bitwise_add,
  ID_rbjit__bitwise_sub,
  ID_rbjit__bitwise_add_overflow,
  ID_rbjit__bitwise_sub_overflow,
  ID_rbjit__bitwise_compare_eq,
  ID_rbjit__bitwise_compare_ne,
  ID_rbjit__bitwise_compare_ugt,
  ID_rbjit__bitwise_compare_uge,
  ID_rbjit__bitwise_compare_ult,
  ID_rbjit__bitwise_compare_ule,
  ID_rbjit__bitwise_compare_sgt,
  ID_rbjit__bitwise_compare_sge,
  ID_rbjit__bitwise_compare_slt,
  ID_rbjit__bitwise_compare_sle,

  // Array literal helpers
  ID_rbjit__convert_to_array,
  ID_rbjit__concat_arrays,
  ID_rbjit__push_to_array,

  // String literal helpers
  ID_rbjit__convert_to_string,
  ID_rbjit__concat_strings,

  ID_rbjit__typecast_fixnum,
  ID_rbjit__typecast_fixnum_bignum,

  // Symbols
  IDS_plus,
  IDS_minus,
  IDS_asterisk,
  IDS_slash,

  PREDEFINED_ID_COUNT
};

class IdStore {
public:

  static void setup();

  static ID get(PredefinedId pre) { return predefined_[pre]; }

private:

  static ID predefined_[PREDEFINED_ID_COUNT];
};

RBJIT_NAMESPACE_END

