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
class NamedVariable;

class Variable {
public:

  // constructor indirectly called by factory methods
  Variable(BlockHeader* defBlock, Opcode* defOpcode, ID name, Variable* original, NamedVariable* nameRef, int index, DefInfo* defInfo);

  // factory methods
//  static Variable* createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name, NamedVariable* nameRef);
  static Variable* createNamed(BlockHeader* defBlock, Opcode* defOpcode, int index, ID name);
  static Variable* createUnnamed(BlockHeader* defBlock, Opcode* defOpcode, int index);
  static Variable* createUnnamedSsa(BlockHeader* defBlock, Opcode* defOpcode, int index);
  static Variable* copy(BlockHeader* defBlock, Opcode* defOpcode, int index, Variable* v);

  ~Variable() { delete defInfo_; }

  BlockHeader* defBlock() const { return defBlock_; }
  void setDefBlock(BlockHeader* block) { defBlock_ = block; }

  Opcode* defOpcode() const { return defOpcode_; }
  void setDefOpcode(Opcode* opcode) { defOpcode_ = opcode; }

  ID name() const { return name_; }
  void setName(ID name) { name_ = name; }

  Variable* original() const { return original_; }

  NamedVariable* nameRef() const { return nameRef_; }

  bool local() const { return defInfo_->isLocal(); }
  void setLocal(bool local) { defInfo_->setLocal(local); }

  int index() const { return index_; }
  void setIndex(int i) { index_ = i; }

  // DefInfo
  DefInfo* defInfo() const { return defInfo_; }
  void setDefInfo(DefInfo* defInfo) { defInfo_ = defInfo; }
  int defCount() const { return defInfo_ ? defInfo_->defCount() : 0; }

  void updateDefSite(BlockHeader* defBlock, Opcode* defOpcode);

  void clearDefInfo();

  std::string debugPrint() const;

private:

  // location where this variable is defined
  BlockHeader* defBlock_;
  Opcode* defOpcode_;

  // variable name
  ID name_;

  // original variable when this variable is created by renaming the existing one
  Variable* original_;

  // Reference to a named variable in the scope
  NamedVariable* nameRef_;

  int index_;

  DefInfo* defInfo_;

};

RBJIT_NAMESPACE_END
