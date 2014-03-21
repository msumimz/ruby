#include <cassert>
#include "rbjit/variable.h"
#include "rbjit/definfo.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

Variable::Variable(BlockHeader* defBlock, Opcode* defOpcode, ID name, Variable* original, int index, DefInfo* defInfo)
  : defBlock_(defBlock), defOpcode_(defOpcode), name_(name),
    original_(original == 0 ? this : original), local_(true),
    index_(index), defInfo_(defInfo), typeConstraint_(0)
{}

Variable*
Variable::createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name)
{
  return new Variable(defBlock, defOpcode, name, 0, index, new DefInfo());
}

Variable*
Variable::createUnnamed(BlockHeader* defBlock, Opcode* defOpcode, int index)
{
  return new Variable(defBlock, defOpcode, 0, 0, index, 0);
}

Variable*
Variable::createUnnamedSsa(BlockHeader* defBlock, Opcode* defOpcode, int index)
{
  return new Variable(defBlock, defOpcode, 0, 0, index, new DefInfo());
}

Variable*
Variable::copy(BlockHeader* defBlock, Opcode* defOpcode, int index, Variable* v)
{
  return new Variable(defBlock, defOpcode, v->name(), 0, index, 0);
}

void
Variable::clearDefInfo()
{
  if (defInfo_) {
    delete defInfo_;
    defInfo_ = 0;
  }
}

bool
Variable::isSameValueAs(Variable* v)
{
  assert(typeConstraint_);
  return this == v || typeConstraint_->isSameValueAs(v);
}

extern "C" {
#include "ruby.h"
}

std::string
Variable::debugPrint() const
{
  char buf[256];
  std::string result;

  sprintf(buf, "%d:%Ix: '%s' defBlock=%Ix defOpcode=%Ix local=%d ",
    index_, this, mri::Id(name_).name(), defBlock_, defOpcode_, local_);
  result = buf;

  if (defInfo_) {
    sprintf(buf, "defCount=%d defSites=", defInfo_->defCount());
    result += buf;
    for (const DefSite* s = defInfo_->defSite(); s; s = s->next()) {
      sprintf(buf, "%Ix ", s->defBlock());
      result += buf;
    }
  }
  else {
    result += "defInfo=(null)";
  }

  result += '\n';

  return result;
}

RBJIT_NAMESPACE_END
