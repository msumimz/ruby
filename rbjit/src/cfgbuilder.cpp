#include <cassert>

#include "rbjit/opcodefactory.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/cfgbuilder.h"

extern "C" {
#include "ruby.h"
#include "node.h"
}

RBJIT_NAMESPACE_BEGIN

Variable*
CfgBuilder::getNamedVariable(OpcodeFactory* factory, mri::ID name)
{
  std::unordered_map<mri::ID, Variable*>::const_iterator i = namedVariables_.find(name);
  if (i != namedVariables_.end()) {
    return i->second;
  }

  Variable* v = factory->createNamedVariable(name);
  namedVariables_[name] = v;

  return v;
}

ControlFlowGraph*
CfgBuilder::buildMethod(const RNode* rootNode)
{
  cfg_ = new ControlFlowGraph;

  OpcodeFactory factory(cfg_);
  factory.createEntryExitBlocks();

  buildProcedureBody(&factory, rootNode, true);

  return cfg_;
}

void
CfgBuilder::buildProcedureBody(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_SCOPE);

  Variable* v = buildNode(factory, node->nd_body, useResult);
  if (factory->continues()) {
    factory->addJumpToReturnBlock(v);
  }
}

Variable*
CfgBuilder::buildNode(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* v;

  if (!factory->continues()) {
    return 0;
  }

  enum node_type type = (enum node_type)nd_type(node);
  switch (type) {
  case NODE_BLOCK:
    for (; node->nd_next; node = node->nd_next) {
      buildNode(factory, node->nd_head, false);
      if (!factory->continues()) {
        return 0;
      }
    }
    v = buildNode(factory, node->nd_head, useResult);
    break;

  case NODE_DASGN_CURR:
    v = buildAssignment(factory, node, useResult);
    break;

  case NODE_LIT:
    v = buildImmediate(factory, node, useResult);
    break;

  case NODE_IF:
    v = buildIf(factory, node, useResult);
    break;

  default:
    assert(!"node type not implemented");
  }

  return v;
}

Variable*
CfgBuilder::buildAssignment(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_DASGN_CURR);

  Variable* rhs = buildNode(factory, node->nd_value, true);
  Variable* lhs = getNamedVariable(factory, node->nd_vid);

  factory->addCopy(lhs, rhs);

  if (useResult) {
    return 0;
  }
  return lhs;
}

Variable*
CfgBuilder::buildImmediate(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  return factory->addImmediate((VALUE)node->nd_lit, useResult);
}

Variable*
CfgBuilder::buildIf(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_IF);

  // condition
  Variable* cond = buildNode(factory, node->nd_cond, true);

  // true block
  OpcodeFactory trueFactory(*factory);
  BlockHeader* trueBlock = trueFactory.addBlockHeader();
  Variable* trueValue = buildNode(&trueFactory, node->nd_body, useResult);

  // false block
  OpcodeFactory falseFactory(*factory);
  BlockHeader* falseBlock = falseFactory.addBlockHeader();
  Variable* falseValue = buildNode(&falseFactory, node->nd_else, useResult);

  // branch
  factory->addJumpIf(cond, trueBlock, falseBlock);

  // join block
  if (trueFactory.continues()) {
    if (falseFactory.continues()) {
      BlockHeader* join = factory->addFreeBlockHeader(factory->lastBlock());
      Variable* value = factory->createTemporary(useResult);
      trueFactory.addCopy(value, trueValue);
      trueFactory.addJump(join);
      falseFactory.addCopy(value, falseValue);
      falseFactory.addJump(join);
      return value;
    }

    *factory = trueFactory;
    return trueValue;
  }
  else {
    if (falseFactory.continues()) {
      *factory = falseFactory;
      return falseValue;
    }
  }

  factory->halt();
  return 0;
}

RBJIT_NAMESPACE_END
