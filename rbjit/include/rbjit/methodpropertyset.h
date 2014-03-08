#pragma once

#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class MethodPropertySet {
public:

  enum { UNKNOWN = 0, YES = 1, NO = 2 };

  MethodPropertySet()
    : hasDef_(UNKNOWN), hasEval_(UNKNOWN)
  {}

  void setHasDef(int state) { hasDef_ = state; }
  void setHasEval(int state) { hasEval_ = state; }

  bool isMutator() const { return hasDef_ != NO || hasEval_ != NO; }

private:

  int hasDef_ : 2;
  int hasEval_ : 2;

};

RBJIT_NAMESPACE_END
