#include <typeinfo>
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

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

RBJIT_NAMESPACE_END
