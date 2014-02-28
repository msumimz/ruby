#pragma once
#include <string>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class BlockHeader;

class DomTree {
public:

  class Node {
  public:
    Node() {}

    Node* firstChild() { return firstChild_; }
    Node* nextSibling() { return nextSibling_; }

  private:

    friend class DomTree;

    Node* firstChild_;
    Node* nextSibling_;
  };

  DomTree(ControlFlowGraph* cfg);
  ~DomTree();

  Node* nodeOf(BlockHeader* block) const;
  size_t blockIndexOf(Node* node) const;

  std::string debugPrint() const;

private:

  void addChild(BlockHeader* parent, BlockHeader* child);
  void buildTree(ControlFlowGraph* cfg);

  size_t size_;
  Node* nodes_;
};

RBJIT_NAMESPACE_END
