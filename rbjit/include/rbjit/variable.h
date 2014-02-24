#pragma once
#include "rbjit/common.h"
#include "ruby.h"

namespace llvm {
class Value;
};

RBJIT_NAMESPACE_BEGIN

class DefInfo;
class TypeInfo;
class Opcode;
class BlockHeader;

class Variable {
public:

  Variable(BlockHeader* defBlock, Opcode* defOpcode, ID name, Variable* original, int index, DefInfo* defInfo)
    : defBlock_(defBlock), defOpcode_(defOpcode), name_(name),
      original_(original == 0 ? this : original), index_(index), defInfo_(defInfo)
  {}

  BlockHeader* defBlock() const { return defBlock_; }
  Opcode* defOpcode() const { return defOpcode_; }
  ID name() const { return name_; }
  Variable* original() const { return original_; }
  int index() const { return index_; }

  void setDefInfo(DefInfo* defInfo) { defInfo_ = defInfo; }
  DefInfo* defInfo() const { return defInfo_; }

  // factory methods
  static Variable* createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name);
  static Variable* createUnnamed(BlockHeader* defBlock, Opcode* defOpcode, int index);
  static Variable* createUnnamedSsa(BlockHeader* defBlock, Opcode* defOpcode, int index);

private:

  // location where this variable is defined
  BlockHeader* defBlock_;
  Opcode* defOpcode_;

  // variable name
  ID name_;

  // original variable when this variable is created by renaming the existing one
  Variable* original_;

  int index_;

  DefInfo* defInfo_;
  TypeInfo* typeInfo_;
};

RBJIT_NAMESPACE_END
