#pragma once
#include <string>
#include "rbjit/definfo.h"
#include "rbjit/rubytypes.h"

namespace llvm {
class Value;
};

RBJIT_NAMESPACE_BEGIN

class Opcode;
class BlockHeader;
class TypeConstraint;

class Variable {
public:

  // factory methods
  static Variable* createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name);
  static Variable* createUnnamed(BlockHeader* defBlock, Opcode* defOpcode, int index);
  static Variable* createUnnamedSsa(BlockHeader* defBlock, Opcode* defOpcode, int index);
  static Variable* copy(BlockHeader* defBlock, Opcode* defOpcode, int index, Variable* v);

  ~Variable() { delete defInfo_; }

  BlockHeader* defBlock() const { return defBlock_; }
  Opcode* defOpcode() const { return defOpcode_; }
  ID name() const { return name_; }
  void setName(ID name) { name_ = name; }
  Variable* original() const { return original_; }
  bool local() const { return local_; }
  void setLocal(bool local) { local_ = local; }
  int index() const { return index_; }
  void setIndex(int i) { index_ = i; }

  // DefInfo
  DefInfo* defInfo() const { return defInfo_; }
  void setDefInfo(DefInfo* defInfo) { defInfo_ = defInfo; }
  int defCount() const { return defInfo_ ? defInfo_->defCount() : 0; }
  void clearDefInfo();

  // TypeConstraint
  TypeConstraint* typeConstraint() const { return typeConstraint_; }
  void setTypeConstraint(TypeConstraint* type) { typeConstraint_ = type; }
  bool isSameValueAs(Variable* v);

  std::string debugPrint() const;

private:

  // constructor indirectly called through factory methods
  Variable(BlockHeader* defBlock, Opcode* defOpcode, ID name, Variable* original, int index, DefInfo* defInfo);

  // location where this variable is defined
  BlockHeader* defBlock_;
  Opcode* defOpcode_;

  // variable name
  ID name_;

  // original variable when this variable is created by renaming the existing one
  Variable* original_;

  // True if every def and use is located in the same block
  bool local_;

  int index_;

  DefInfo* defInfo_;
  TypeConstraint* typeConstraint_;

};

RBJIT_NAMESPACE_END
