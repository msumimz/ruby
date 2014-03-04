#include <cstdlib>
#include "rbjit/domtree.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

DomTree::DomTree(ControlFlowGraph* cfg, std::vector<BlockHeader*>* idoms)
  : size_(cfg->blocks()->size()), nodes_((Node*)calloc(size_, sizeof(Node)))
{
  buildTree(cfg, idoms);
}

DomTree::~DomTree()
{
  free(nodes_);
}

DomTree::Node*
DomTree::nodeOf(BlockHeader* block) const
{
  return nodes_ + block->index();
}

size_t
DomTree::blockIndexOf(Node* node) const
{
  return node - nodes_;
}

void
DomTree::addChild(BlockHeader* parent, BlockHeader* child)
{
  Node* p = nodeOf(parent);
  Node* c = nodeOf(child);
  c->nextSibling_ = p->firstChild_;
  p->firstChild_ = c;
}

void
DomTree::buildTree(ControlFlowGraph* cfg, std::vector<BlockHeader*>* idoms)
{
  for (unsigned i = 0; i < size_; ++i) {
    BlockHeader* b = (*cfg->blocks())[i];
    if (b == cfg->entry()) {
      continue;
    }
    addChild((*idoms)[b->index()], b);
  }
}

std::string
DomTree::debugPrint() const
{
  char buf[256];
  sprintf(buf, "[DomTree: %Ix]\n", this);
  std::string result(buf);

  for (size_t i = 0; i < size_; ++i) {
    Node* n = nodes_ + i;
    sprintf(buf, "%3d: %Ix(%d) firstChild=%Ix(%d) nextSibling=%Ix(%d)\n",
      i, n, blockIndexOf(n), n->firstChild_, blockIndexOf(n->firstChild_),
      n->nextSibling_, blockIndexOf(n->nextSibling_));
    result += buf;
  }

  return result;
}

RBJIT_NAMESPACE_END
