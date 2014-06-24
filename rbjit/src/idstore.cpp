#include "rbjit/rubyobject.h"
#include "rbjit/idstore.h"

RBJIT_NAMESPACE_BEGIN

const char* ID_NAMES[] = {
  "<self>",
  "<argc>",
  "<argv>",
  "<env>",

  "rbjit__typecast_fixnum",
  "rbjit__typecast_fixnum_bignum",

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

