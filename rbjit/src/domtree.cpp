#include <cstdlib>
#include "rbjit/domtree.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

DomTree::DomTree(const ControlFlowGraph* cfg, const std::vector<Block*>& doms)
  : cfg_(cfg), nodes_((Node*)calloc(cfg_->blockCount(), sizeof(Node)))
{
  buildTree(cfg, doms);
}

DomTree::~DomTree()
{
  free(nodes_);
}

DomTree::Node*
DomTree::nodeOf(Block* block) const
{
  return nodes_ + block->index();
}

size_t
DomTree::blockIndexOf(Node* node) const
{
  return node - nodes_;
}

Block*
DomTree::blockOf(Node* node) const
{
  return cfg_->block(blockIndexOf(node));
}

Block*
DomTree::idomOf(Block* block) const
{
  return cfg_->block(blockIndexOf(nodeOf(block)->parent()));
}

void
DomTree::addChild(Block* parent, Block* child)
{
  Node* p = nodeOf(parent);
  Node* c = nodeOf(child);
  c->parent_ = p;
  c->nextSibling_ = p->firstChild_;
  p->firstChild_ = c;
}

void
DomTree::buildTree(const ControlFlowGraph* cfg, const std::vector<Block*>& doms)
{
  for (unsigned i = 0; i < cfg_->blockCount(); ++i) {
    Block* b = cfg->block(i);
    if (!doms[i]) {
      assert(b == cfg->entryBlock());
      continue;
    }
    addChild(doms[i], b);
  }
}

std::string
DomTree::debugPrint() const
{
  std::string result = stringFormat("[DomTree: %Ix]\n", this);

  for (size_t i = 0; i < cfg_->blockCount(); ++i) {
    Node* n = nodes_ + i;
    result += stringFormat("%3d: %Ix(%d) firstChild=%Ix(%d) nextSibling=%Ix(%d)\n",
      i,
      n, n ? blockIndexOf(n) : -1,
      n->firstChild_, n->firstChild_ ? blockIndexOf(n->firstChild_) : -1,
      n->nextSibling_, n->nextSibling_ ? blockIndexOf(n->nextSibling_) : -1);
  }

  return result;
}

RBJIT_NAMESPACE_END
