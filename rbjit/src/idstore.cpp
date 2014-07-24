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

  "rbjit__is_fixnum",

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

