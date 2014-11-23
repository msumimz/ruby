#include <algorithm>
#include "rbjit/block.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

// NOTE:
// This method won't delete *op (Opcode*) even when deleteOpcode is true.
// The caller should dispose *op appropriciately.
Block*
Block::splitAt(Iterator op, bool deleteOpcode)
{
  // Create a new block
  Block* newBlock = new Block;
  Iterator nextOp = op;
  std::advance(nextOp, 1);
  newBlock->opcodes_.assign(nextOp, opcodes_.end());

  // Update defBlocks in the opcodes of the new block
  for (auto i = newBlock->begin(), end = newBlock->end(); i != end; ++i) {
    Variable* lhs = (*i)->lhs();
    if (lhs) {
      lhs->setDefBlock(newBlock);
    }
  }

  // Update backedges in the new block
  Block* b = footer()->nextBlock();
  if (b) {
    b->updateBackedge(this, newBlock);
  }
  b = footer()->nextAltBlock();
  if (b) {
    b->updateBackedge(this, newBlock);
  }

  // Delete the opcodes that are copied into the new block
  if (deleteOpcode) {
    opcodes_.erase(op, opcodes_.end());
  }
  else {
    opcodes_.erase(nextOp, opcodes_.end());
  }

  return newBlock;
}

void
Block::addJumpTo(SourceLocation* loc, Block* dest)
{
  addOpcode(new OpcodeJump(loc, dest));
  dest->addBackedge(this);
}

bool
Block::visitEachOpcode(OpcodeVisitor* visitor)
{
  for (auto i = opcodes_.begin(), end = opcodes_.end(); i != end; ++i) {
    if (!(*i)->accept(visitor)) {
      return false;
    }
  }
  return true;
}

RBJIT_NAMESPACE_END
