#include <algorithm>
#include "rbjit/scope.h"
#include "rbjit/idstore.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// NamedVariable

void
NamedVariable::addUseInInnerScope(Scope* scope)
{
  if (std::find(uses_.cbegin(), uses_.cend(), scope) == uses_.cend()) {
    uses_.push_back(scope);
  }
}

////////////////////////////////////////////////////////////
// Scope

Scope::Scope(ID* tbl, Scope* parent)
  : idTable_(tbl), parent_(parent)
{
  // Scopes always contain self.
  variables_.insert(std::make_pair(IdStore::get(ID_self), new NamedVariable(IdStore::get(ID_self), this)));

  for (int i = 0; i < idTable_.size(); ++i) {
    ID name = idTable_.idAt(i);
    variables_.insert(std::make_pair(name, new NamedVariable(name, this)));
  }
}

NamedVariable*
Scope::find(ID name) const
{
  auto i = variables_.find(name);
  if (i != variables_.end()) {
    return i->second;
  }
  return nullptr;
}

NamedVariable*
Scope::self() const
{
  return find(IdStore::get(ID_self));
}

std::vector<ID>
Scope::activeVariableList() const
{
  std::vector<ID> result;
  for (int i = 0; i < idTable_.size(); ++i) {
    ID name = idTable_.idAt(i);
    auto pv = variables_.find(name);
    const NamedVariable* v = pv->second;
    assert(v);
    if (v->isUsedInInnerScope()) {
      result.push_back(name);
    }
  }
  return result;
}

int
Scope::setIndexes()
{
  int index = 0;
  for (int i = 0; i < idTable_.size(); ++i) {
    ID name = idTable_.idAt(i);
    NamedVariable* v = variables_[name];
    assert(v);
    if (v->isUsedInInnerScope()) {
      v->setIndex(index++);
    }
  }
  return index;
}

RBJIT_NAMESPACE_END
