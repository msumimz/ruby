#pragma once
#include <string>
#include <vector>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Block;

class DomTree {
public:

  class Node {
  public:
    Node() {}

    Node* parent() { return parent_; }
    Node* firstChild() { return firstChild_; }
    Node* nextSibling() { return nextSibling_; }

  private:

    friend class DomTree;

    Node* parent_;
    Node* firstChild_;
    Node* nextSibling_;
  };

  DomTree(const ControlFlowGraph* cfg, const std::vector<Block*>& doms);
  ~DomTree();

  Node* nodeOf(Block* block) const;
  size_t blockIndexOf(Node* node) const;
  Block* blockOf(Node* node) const;
  Block* idomOf(Block* block) const;

  std::string debugPrint() const;

private:

  void addChild(Block* parent, Block* child);
  void buildTree(const ControlFlowGraph* cfg, const std::vector<Block*>& doms);

  const ControlFlowGraph* cfg_;
  Node* nodes_;
};

RBJIT_NAMESPACE_END
