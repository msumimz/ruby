#include <cassert>

#include "rbjit/opcodefactory.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/rubyobject.h"

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
  case NODE_LASGN:
    v = buildAssignment(factory, node, useResult);
    break;

  case NODE_LVAR:
    v = buildLocalVariable(factory, node, useResult);
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

  case NODE_WHILE:
    v = buildWhile(factory, node, useResult);
    break;

  case NODE_CALL:
    v = buildCall(factory, node, useResult);
    break;

  default:
    assert(!"node type not implemented");
  }

  return v;
}

Variable*
CfgBuilder::buildAssignment(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_DASGN_CURR || nd_type(node) == NODE_LASGN);

  Variable* rhs = buildNode(factory, node->nd_value, true);
  Variable* lhs = getNamedVariable(factory, node->nd_vid);

  factory->addCopy(lhs, rhs);

  if (useResult) {
    return 0;
  }
  return lhs;
}

Variable*
CfgBuilder::buildLocalVariable(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_LVAR);

  if (!useResult) {
    return 0;
  }

  return getNamedVariable(factory, node->nd_vid);
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
      if (useResult) {
        trueFactory.addCopy(value, trueValue);
        falseFactory.addCopy(value, falseValue);
      }
      trueFactory.addJump(join);
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

Variable*
CfgBuilder::buildWhile(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  //     <while>         <begin-while>
  //  +-----------+      +-----------+
  //  | PREHEADER |      | PREHEADER |
  //  +-----------+      +-----------+
  //        |    +------+      |
  //        |    |      v      v
  //        |    |   +-----------------+
  //        |    |   | BODY            |
  //        |    |   |                 |
  //        |    |   |  if             |
  //        |    |   |     break-----------------+
  //        |    |   |                 |         |
  //        |    |   |  if             |         |
  //        |    |   |     next-----------+      |
  //        |    |   |                 |  |      |
  //        |    |   +-----------------+  |      |
  //        |    |        |               |      |
  //        |    |        |  +------------+      |
  //        |    |        |  |                   |
  //        |    |        v  v                   |
  //        |    |      +------+                 |
  //        |    +------| COND |                 |
  //        +---------->|      |                 |
  //                    +------+                 |
  //                       |                     |
  //                       v                     v
  //                +-------------+          +------+
  //                | PREEXIT     |--------->| EXIT |
  //                |  result=nil |          +------+
  //                +-------------+              |
  //                                             v
  //

  assert(nd_type(node) == NODE_WHILE || nd_type(node) == NODE_UNTIL);

  // Result value
  Variable* value = factory->createTemporary(useResult);

  // Create blocks
  OpcodeFactory preheaderFactory(*factory, 0);
  OpcodeFactory condFactory(*factory, 0);
  OpcodeFactory bodyFactory(*factory, 0);
  OpcodeFactory preexitFactory(*factory, 0);
  OpcodeFactory exitFactory(*factory, 0);

  // Preheader block
  factory->addJump(preheaderFactory.lastBlock());

  if (node->nd_state == 0) {
    // begin-end-while
    preheaderFactory.addJump(bodyFactory.lastBlock());
  }
  else {
    // while
    preheaderFactory.addJump(condFactory.lastBlock());
  }

  // Condition block
  BlockHeader* condBlock = condFactory.lastBlock();
  Variable* cond = buildNode(&condFactory, node->nd_cond, true);
  condFactory.addJumpIf(cond, bodyFactory.lastBlock(), preexitFactory.lastBlock());

  // Preexit block
  Variable* nil = preexitFactory.addImmediate(mri::Object::nilObject(), useResult);
  preexitFactory.addCopy(value, nil);
  preexitFactory.addJump(exitFactory.lastBlock());

  // Body blcok
  exits_.push_back(ExitPoint(&condFactory, &bodyFactory, &exitFactory, value));
  buildNode(&bodyFactory, node->nd_body, false);
  bodyFactory.addJump(condBlock);
  exits_.pop_back();

  *factory = exitFactory;

  return value;
}

Variable*
CfgBuilder::buildCall(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  // Receiver
  Variable* receiver = buildNode(factory, node->nd_recv, true);

  // Arguments
  int argCount = node->nd_args->nd_alen + 1; // includes receiver
  Variable** args = (Variable**)_alloca(argCount * sizeof(Variable*));
  Variable** a = args;
  *a++ = receiver;
  for (RNode* n = node->nd_args; n; n = n->nd_next) {
    *a++ = buildNode(factory, n->nd_head, true);
  }

  // Find a method
  Variable* methodEntry = factory->addLookup(receiver, node->nd_mid);

  // Call a method
  Variable* value = factory->addCall(methodEntry, args, args + argCount, useResult);

  return value;
}

RBJIT_NAMESPACE_END
