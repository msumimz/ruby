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
CfgBuilder::getNamedVariable(OpcodeFactory* factory, ID name)
{
  std::unordered_map<ID, Variable*>::const_iterator i = namedVariables_.find(name);
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

  case NODE_TRUE:
    v = buildTrue(factory, node, useResult);
    break;

  case NODE_FALSE:
    v = buildFalse(factory, node, useResult);
    break;

  case NODE_NIL:
    v = buildNil(factory, node, useResult);
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
CfgBuilder::buildTrue(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  return factory->addImmediate(Qtrue, useResult);
}

Variable*
CfgBuilder::buildFalse(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  return factory->addImmediate(Qfalse, useResult);
}

Variable*
CfgBuilder::buildNil(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  return factory->addImmediate(Qnil, useResult);
}

Variable*
CfgBuilder::buildIf(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_IF);

  // condition
  Variable* cond = buildNode(factory, node->nd_cond, true);

  BlockHeader* idom = factory->lastBlock();

  // true block
  OpcodeFactory trueFactory(*factory);
  BlockHeader* trueBlock = trueFactory.addFreeBlockHeader(idom);
  Variable* trueValue = buildNode(&trueFactory, node->nd_body, useResult);

  // false block
  OpcodeFactory falseFactory(*factory);
  BlockHeader* falseBlock = falseFactory.addFreeBlockHeader(idom);
  Variable* falseValue = buildNode(&falseFactory, node->nd_else, useResult);

  // branch
  factory->addJumpIf(cond, trueBlock, falseBlock);

  // join block
  if (trueFactory.continues()) {
    if (falseFactory.continues()) {
      BlockHeader* join = factory->addFreeBlockHeader(idom);
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

  // Both route have been stopped
  factory->halt();
  return 0;
}

RBJIT_NAMESPACE_END
