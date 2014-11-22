#include <cassert>
#include "rbjit/variable.h"
#include "rbjit/definfo.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

std::string
Variable::debugPrint() const
{
  std::string result = stringFormat("%d:%Ix: '%s' (%Ix %Ix) orig=%Ix nameRef=%Ix ",
    index_, this, mri::Id(name_).name(), defBlock_, defOpcode_, original_, nameRef_);

#if 0
  if (defInfo_) {
    result += stringFormat("defCount=%d local=%d defSites=", defInfo_->defCount(), defInfo_->isLocal());
    for (const DefSite* s = defInfo_->defSite(); s; s = s->next()) {
      result += stringFormat("%Ix ", s->defBlock());
    }
  }
  else {
    result += "defInfo=(null)";
  }
#endif

  result += '\n';

  return result;
}

RBJIT_NAMESPACE_END
