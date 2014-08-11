#pragma once
#include <unordered_map>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"
#include "rbjit/rubyidtable.h"

RBJIT_NAMESPACE_BEGIN

class Scope;

class NamedVariable {
public:

  NamedVariable(ID name, Scope* scope)
    : name_(name), scope_(scope), index_(0)
  {}

  ID name() const { return name_; }
  Scope* scope() const { return scope_; }

  int index() const { return index_; }
  void setIndex(int index) { index_ = index; }

  void addUseFromInnerScope(Scope* scope);
  bool hasUses() const { return !uses_.empty(); }

private:

  ID name_;
  Scope* scope_;
  std::vector<Scope*> uses_;
  int index_;

};

class Scope {
public:

  // Call with node->nd_tbl
  Scope(ID* tbl, Scope* parent);

  NamedVariable* find(ID name) const;

  std::vector<ID> activeVariableList() const;

  // Returns the number of active variables.
  int setIndexes();

private:

  mri::IdTable idTable_;
  std::unordered_map<ID, NamedVariable*> variables_;
  Scope* parent_;

};

RBJIT_NAMESPACE_END
