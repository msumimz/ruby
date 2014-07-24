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

  ID_rbjit__is_fixnum,

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

