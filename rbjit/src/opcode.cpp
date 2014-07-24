#include <typeinfo>
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/rubyobject.h"
#include "rbjit/idstore.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// Opcode

const char*
Opcode::typeName() const
{
  if (typeid(*this) == typeid(BlockHeader)) {
    return "BlockHeader";
  }
  else {
    const int skip = strlen("const rbjit::Opcode"); // hopefully expaneded to constant at compile time
    return typeid(*this).name() + skip;
  }
}

const char*
Opcode::shortTypeName() const
{
  if (typeid(*this) == typeid(BlockHeader)) {
    return "Block";
  }
  else if (typeid(*this) == typeid(OpcodeImmediate)) {
    return "Imm";
  }
  else if (typeid(*this) == typeid(OpcodeConstant)) {
    return "Const";
  }
  else if (typeid(*this) == typeid(OpcodePrimitive)) {
    return "Prim";
  }
  else {
    return typeName();
  }
}

////////////////////////////////////////////////////////////
// Call

OpcodeLookup*
OpcodeCall::lookupOpcode() const
{
  assert(typeid(*lookup_->defOpcode()) == typeid(OpcodeLookup));
  return static_cast<OpcodeLookup*>(lookup_->defOpcode());
}

OpcodeCall*
OpcodeCall::clone(Opcode* prev, Variable* methodEntry) const
{
  OpcodeCall* op = new OpcodeCall(file(), line(), prev, lhs(), methodEntry, rhsCount(), env_);
  for (int i = 0; i <= rhsCount(); ++i) {
    op->setRhs(i, rhs(i));
  }
  return op;
}

////////////////////////////////////////////////////////////
// BlockHeader

BlockHeader::~BlockHeader()
{
   Backedge* e = backedge_.next_;
   while (e) {
    Backedge* next = e->next_;
    delete e;
    e = next;
  }
}

void
BlockHeader::updateJumpDestination(BlockHeader* dest)
{
  assert(!footer_ || typeid(*footer_) == typeid(OpcodeJump) || typeid(*footer_) == typeid(OpcodeJumpIf));

  if (footer_ && nextBlock()) {
    nextBlock()->removeBackedge(this);
    dest->addBackedge(this);
  }

  static_cast<OpcodeJump*>(footer_)->setNextBlock(dest);
}

void
BlockHeader::updateJumpAltDestination(BlockHeader* dest)
{
  assert(!footer_ || typeid(*footer_) == typeid(OpcodeJumpIf));

  if (footer_ && nextAltBlock()) {
    nextAltBlock()->removeBackedge(this);
    dest->addBackedge(this);
  }

  static_cast<OpcodeJumpIf*>(footer_)->setNextAltBlock(dest);
}

bool
BlockHeader::containsOpcode(const Opcode* op)
{
  Opcode* o = this;
  Opcode* footer = footer_;
  do {
    if (op == o) {
      return true;
    }
    o = o->next();
  } while (o && o != footer);

  return false;
}

void
BlockHeader::addBackedge(BlockHeader* block)
{
  if (backedge_.block_ == 0) {
    backedge_.block_ = block;
  }
  else {
    Backedge* e = new Backedge(block, backedge_.next_);
    backedge_.next_ = e;
  }
}

void
BlockHeader::removeBackedge(BlockHeader* block)
{
  Backedge* e = &backedge_;
  if (e->block_ == block) {
    e->block_ = 0;
    return;
  }

  for (;;) {
    Backedge* prev = e;
    e = e->next_;
    if (!e) {
      break;
    }
    if (e->block_ == block) {
      prev->next_ = e->next_;
      delete e;
      return;
    }
  }

  RBJIT_UNREACHABLE;
}

void
BlockHeader::updateBackedge(BlockHeader* oldBlock, BlockHeader* newBlock)
{
  Backedge* e = &backedge_;
  assert(e->block_);

  do {
    if (e->block_ == oldBlock) {
      e->block_ = newBlock;
      return;
    }
    e = e->next_;
  } while (e);

  RBJIT_UNREACHABLE;
}

int
BlockHeader::backedgeSize() const
{
  if (backedge_.block_ == 0) {
    return 0;
  }

  int count = 0;
  const Backedge* e = &backedge_;
  do {
    ++count;
    e = e->next_;
  } while (e);
  return count;
}

const BlockHeader::Backedge*
BlockHeader::backedgeAt(int n) const
{
  const Backedge* e = &backedge_;
  for (; n > 0; --n) {
    e = e->next_;
  }
  return e;
}

bool
BlockHeader::visitEachOpcode(OpcodeVisitor* visitor)
{
  Opcode* op = this;
  bool result;
  do {
    result = op->accept(visitor);
    if (!result || op == footer_) {
      break;
    }
    op = op->next();
  } while (op);

  return result;
}

////////////////////////////////////////////////////////////
// Env

ID OpcodeEnv::envName_;

ID
OpcodeEnv::envName()
{
  if (!envName_) {
    envName_ = IdStore::get(ID_env);
  }
  return envName_;
}

bool
OpcodeEnv::isEnv(Variable* v)
{
  return v->name() == envName();
}

RBJIT_NAMESPACE_END
