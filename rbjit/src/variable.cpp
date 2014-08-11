#include <cassert>
#include "rbjit/variable.h"
#include "rbjit/definfo.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

Variable::Variable(BlockHeader* defBlock, Opcode* defOpcode, ID name, Variable* original, NamedVariable* nameRef, int index, DefInfo* defInfo)
  : defBlock_(defBlock), defOpcode_(defOpcode), name_(name),
    original_(original == 0 ? this : original),
    nameRef_(nameRef),
    index_(index), defInfo_(defInfo)
{}

Variable*
Variable::copy(BlockHeader* defBlock, Opcode* defOpcode, int index)
{
  return new Variable(defBlock, defOpcode, name(), original(), nameRef(), index, 0);
}

void
Variable::updateDefSite(BlockHeader* defBlock, Opcode* defOpcode)
{
  defBlock_ = defBlock;
  defOpcode_ = defOpcode;
  if (defInfo_) {
    defInfo_->addDefSite(defBlock);
  }
}

void
Variable::clearDefInfo()
{
  if (defInfo_) {
    delete defInfo_;
    defInfo_ = 0;
  }
}

extern "C" {
#include "ruby.h"
}

std::string
Variable::debugPrint() const
{
  std::string result = stringFormat("%d:%Ix: '%s' (%Ix %Ix) orig=%Ix nameRef=%Ix ",
    index_, this, mri::Id(name_).name(), defBlock_, defOpcode_, original_, nameRef_);

  if (defInfo_) {
    result += stringFormat("defCount=%d local=%d defSites=", defInfo_->defCount(), defInfo_->isLocal());
    for (const DefSite* s = defInfo_->defSite(); s; s = s->next()) {
      result += stringFormat("%Ix ", s->defBlock());
    }
  }
  else {
    result += "defInfo=(null)";
  }

  result += '\n';

  return result;
}

RBJIT_NAMESPACE_END
