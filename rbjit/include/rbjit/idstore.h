#pragma once

#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

enum PredefinedId {
  ID_self,
  ID_argc,
  ID_argv,
  ID_env,

  ID_rbjit__is_fixnum,

  ID_rbjit__typecast_fixnum,
  ID_rbjit__typecast_fixnum_bignum,

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

