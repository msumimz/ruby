#pragma once
#include <string>
#include "rbjit/definfo.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class Opcode;
class Block;
class TypeConstraint;
class NamedVariable;

class Variable {
public:

  Variable(ID name = 0, NamedVariable* nameRef = nullptr, Variable* original = nullptr, Block* defBlock = nullptr, Opcode* defOpcode = nullptr)
    : index_(-1), name_(name),
      nameRef_(nameRef),
      original_(original == 0 ? this : original),
      defBlock_(defBlock), defOpcode_(defOpcode)
  {}

  Variable* copy(Block* defBlock, Opcode* defOpcode)
  {
    Variable* original = original_ ? original_ : this;
    return new Variable(name_, nameRef_, original, defBlock, defOpcode);
  }

  int index() const { return index_; }

  Block* defBlock() const { return defBlock_; }
  void setDefBlock(Block* block) { defBlock_ = block; }

  Opcode* defOpcode() const { return defOpcode_; }
  void setDefOpcode(Opcode* opcode) { defOpcode_ = opcode; }

  ID name() const { return name_; }
  void setName(ID name) { name_ = name; }

  Variable* original() const { return original_; }

  NamedVariable* nameRef() const { return nameRef_; }

  std::string debugPrint() const;

private:

  friend class ControlFlowGraph;
  friend class CodeDuplicator;

  int index_;

  // Accesses by the ControlFlowGraph class
  void setIndex(int i) { index_ = i; }

  // Variable name
  ID name_;

  // Reference to a named variable in the scope
  NamedVariable* nameRef_;

  // Original variable when this variable is created by renaming the existing one
  Variable* original_;

  // Location where this variable is defined
  Block* defBlock_;
  Opcode* defOpcode_;
};

RBJIT_NAMESPACE_END
