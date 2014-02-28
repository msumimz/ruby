#include "rbjit/variable.h"
#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

Variable::Variable(BlockHeader* defBlock, Opcode* defOpcode, mri::ID name, Variable* original, int index, DefInfo* defInfo)
  : defBlock_(defBlock), defOpcode_(defOpcode), name_(name),
    original_(original == 0 ? this : original), index_(index), defInfo_(defInfo)
{}

Variable*
Variable::createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, mri::ID name)
{
  return new Variable(defBlock, defOpcode, name, 0, index, new DefInfo(defBlock));
}

Variable*
Variable::createUnnamed(BlockHeader* defBlock, Opcode* defOpcode, int index)
{
  return new Variable(defBlock, defOpcode, 0, 0, index, 0);
}

Variable*
Variable::createUnnamedSsa(BlockHeader* defBlock, Opcode* defOpcode, int index)
{
  return new Variable(defBlock, defOpcode, 0, 0, index, new DefInfo(defBlock));
}

Variable*
Variable::copy(BlockHeader* defBlock, Opcode* defOpcode, int index, Variable* v)
{
  return new Variable(defBlock, defOpcode, v->name(), v, index, 0);
}

extern "C" {
#include "ruby.h"
}

std::string
Variable::debugPrint() const
{
  char buf[256];
  std::string result;

  sprintf(buf, "%Ix defBlock=%Ix defOpcode=%Ix name=%s original=%Ix index=%d ",
    this, defBlock_, defOpcode_, rb_id2name((ID)name_), original_, index_);
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
