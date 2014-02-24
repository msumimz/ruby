#include "rbjit/variable.h"
#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

Variable*
Variable::createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name)
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

RBJIT_NAMESPACE_END
