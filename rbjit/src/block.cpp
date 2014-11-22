#include <algorithm>
#include "rbjit/block.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

Block*
Block::splitAt(Iterator i, bool deleteOpcode)
{
  // Create a new block
  Block* newBlock = new Block;
  Iterator nextOp = i;
  std::advance(i, 1);
  newBlock->opcodes_.assign(nextOp, opcodes_.end());

  // Update backedges in the following blocks
  Block* b = footer()->nextBlock();
  if (b) {
    b->updateBackedge(this, b);
  }
  b = footer()->nextAltBlock();
  if (b) {
    b->updateBackedge(this, b);
  }

  // Delete the opcodes that are copied into the new block
  opcodes_.erase(i, opcodes_.end());

  return newBlock;
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
