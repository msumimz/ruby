#include <cassert>

#include "rbjit/opcodefactory.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/rubyobject.h"
#include "rbjit/variable.h"
#include "rbjit/methodinfo.h"
#include "rbjit/primitivestore.h"
#include "rbjit/opcode.h"
#include "rbjit/idstore.h"
#include "rbjit/rubytypes.h"
#include "rbjit/rubyidtable.h"
#include "rbjit/scope.h"

#include "ruby.h"
#include "node.h"

extern "C" {
  const char* ruby_node_name(int node);
}

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// UnsupportedSyntaxException

UnsupportedSyntaxException::UnsupportedSyntaxException(std::string what)
  : std::runtime_error(what)
{}

////////////////////////////////////////////////////////////
// CfgBuilder

Variable*
CfgBuilder::buildNamedVariable(OpcodeFactory* factory, ID name)
{
  std::unordered_map<ID, Variable*>::const_iterator i = namedVariables_.find(name);
  if (i != namedVariables_.end()) {
    Variable* v = i->second;
    if (v->defBlock() != factory->lastBlock()) {
      v->setLocal(false);
    }
    return v;
  }

  Variable* v = factory->createNamedVariable(name);
  namedVariables_[name] = v;

  return v;
}

ControlFlowGraph*
CfgBuilder::buildMethod(const RNode* rootNode, ID name)
{
  cfg_ = new ControlFlowGraph;
  scope_ = new Scope(rootNode->nd_tbl, nullptr);
  name_ = name;

  OpcodeFactory factory(cfg_);
  factory.createEntryExitBlocks();
  cfg_->entry()->setDebugName("entry");
  cfg_->exit()->setDebugName("exit");

  try {
    buildArguments(&factory, rootNode);
    buildProcedureBody(&factory, rootNode, true);
  }
  catch (std::exception& e) {
    delete cfg_;
    throw;
  };

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

void
CfgBuilder::buildArguments(OpcodeFactory* factory, const RNode* node)
{
  assert(nd_type(node) == NODE_SCOPE);

  // self
  Variable* self = buildNamedVariable(factory, IdStore::get(ID_self));
  self->setDefOpcode(0);
  self->defInfo()->addDefSite(factory->lastBlock()); // entry block
  cfg_->inputs()->push_back(self);

  RNode* argNode = node->nd_args;
  int requiredArgCount = argNode->nd_ainfo->pre_args_num;
  bool hasOptionalArg = !!argNode->nd_ainfo->opt_args;
  bool hasRestArg = !!argNode->nd_ainfo->rest_arg;

  cfg_->setRequiredArgCount(requiredArgCount);
  cfg_->setHasOptionalArg(hasOptionalArg);
  cfg_->setHasRestArg(hasRestArg);

  if (!hasOptionalArg && !hasRestArg) {
    // Method has the fixed number of arguments
    mri::IdTable idTable(node->nd_tbl);
    for (int i = 0; i < requiredArgCount; ++i) {
      Variable* v = buildNamedVariable(factory, idTable.idAt(i));
      v->setDefOpcode(0);
      v->defInfo()->addDefSite(factory->lastBlock()); // entry block
      cfg_->inputs()->push_back(v);
    }
  }
  else {
    std::string what = stringFormat("Method %s uses vardiac arguments, which is not implemented yet", mri::Id(name_).name());
    throw UnsupportedSyntaxException(what);
#if 0
    // Vardiac arguments
    Variable* argc = buildNamedVariable(factory, IdStore::get(ID_argc));
    Variable* argv = buildNamedVariable(factory, IdStore::get(ID_argv));
    argc->setDefOpcode(0);
    argv->setDefOpcode(0);
    cfg_->inputs()->push_back(argc);
    cfg_->inputs()->push_back(argv);
#endif
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

  case NODE_SELF:
    v = buildSelf(factory, node, useResult);
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

  case NODE_ARRAY:
  case NODE_ZARRAY:
    v = buildArray(factory, node, useResult);
    break;

  case NODE_ARGSPUSH:
    v = buildArrayPush(factory, node, useResult);
    break;

  case NODE_ARGSCAT:
    v = buildArrayConcat(factory, node, useResult);
    break;

  case NODE_SPLAT:
    v = buildArraySplat(factory, node, useResult);
    break;

  case NODE_DOT2:
  case NODE_DOT3:
    v = buildRange(factory, node, useResult);
    break;

  case NODE_STR:
    v = buildString(factory, node, useResult);
    break;

  case NODE_DSTR:
    v = buildStringInterpolation(factory, node, useResult);
    break;

  case NODE_HASH:
    v = buildHash(factory, node, useResult);
    break;

  case NODE_AND:
  case NODE_OR:
    v = buildAndOr(factory, node, useResult);
    break;

  case NODE_IF:
    v = buildIf(factory, node, useResult);
    break;

  case NODE_WHILE:
    v = buildWhile(factory, node, useResult);
    break;

  case NODE_RETURN:
    v = buildReturn(factory, node, useResult);
    break;

  case NODE_CALL:
    v = buildCall(factory, node, useResult);
    break;

  case NODE_FCALL:
  case NODE_VCALL:
    v = buildFuncall(factory, node, useResult);
    break;

  case NODE_CONST:
    v = buildConstant(factory, node, useResult);
    break;

  case NODE_COLON2:
    v = buildRelativeConstant(factory, node, useResult);
    break;

  case NODE_COLON3:
    v = buildToplevelConstant(factory, node, useResult);
    break;

  default:
    std::string what = stringFormat("%s:%d: Node type %s is not implemented yet",
      mri::Id(name_).name(), nd_line(node), ruby_node_name(nd_type(node)));
    throw UnsupportedSyntaxException(what);
  }

  return v;
}

Variable*
CfgBuilder::buildAssignment(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_DASGN_CURR || nd_type(node) == NODE_LASGN);

  Variable* rhs = buildNode(factory, node->nd_value, true);
  Variable* lhs = buildNamedVariable(factory, node->nd_vid);

  Variable* value = factory->addCopy(lhs, rhs, useResult);

  // By copy propagation optimization during SSA translation, the copy opcode
  // will be possibly removed and the variable name will be lost. To keep the
  // variable name, set the name to the temporary.
  rhs->setName(lhs->name());

  return value;
}

Variable*
CfgBuilder::buildLocalVariable(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_LVAR);
  return buildNamedVariable(factory, node->nd_vid);
}

Variable*
CfgBuilder::buildImmediate(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_LIT);
  return factory->addImmediate((VALUE)node->nd_lit, useResult);
}

Variable*
CfgBuilder::buildSelf(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  if (!useResult) {
    return nullptr;
  }

  return buildNamedVariable(factory, mri::Id("<self>"));
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
CfgBuilder::buildArray(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  if (nd_type(node) == NODE_ZARRAY) {
    return factory->addArray(nullptr, nullptr, useResult);
  }

  int count = node->nd_alen;
  Variable** elems = (Variable**)_alloca(count * sizeof(Variable*));
  Variable** e = elems;
  for (const RNode* n = node; n; n = n->nd_next) {
    *e++ = buildNode(factory, n->nd_head, useResult);
  }

  return factory->addArray(elems, elems + count, useResult);
}

Variable*
CfgBuilder::buildArrayPush(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* array = buildNode(factory, node->nd_head, useResult);
  Variable* obj = buildNode(factory, node->nd_body, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = factory->addPrimitive(IdStore::get(ID_rbjit__push_to_array), 2, array, obj);
  }

  return result;
}

Variable*
CfgBuilder::buildArrayConcat(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* a1 = buildNode(factory, node->nd_head, useResult);
  Variable* a2 = buildNode(factory, node->nd_body, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = factory->addPrimitive(IdStore::get(ID_rbjit__concat_arrays), 2, a1, a2);
  }

  return result;
}

Variable*
CfgBuilder::buildArraySplat(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* obj = buildNode(factory, node->nd_head, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = factory->addPrimitive(IdStore::get(ID_rbjit__convert_to_array), 1, obj);
  }

  return result;
}

Variable*
CfgBuilder::buildRange(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* low = buildNode(factory, node->nd_beg, useResult);
  Variable* high = buildNode(factory, node->nd_end, useResult);
  return factory->addRange(low, high, nd_type(node) == NODE_DOT3, useResult);
}

Variable*
CfgBuilder::buildString(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  return factory->addString(node->nd_lit, useResult);
}

Variable*
CfgBuilder::buildStringInterpolation(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  std::vector<Variable*> elems(1, nullptr);

  Variable* v;
  if (useResult && !NIL_P(node->nd_lit)) {
    v = factory->addString(node->nd_lit, true);
    elems.push_back(v);
  }

  for (node = node->nd_next; node; node = node->nd_next) {
    const RNode* n = node->nd_head;
    switch (nd_type(n)) {
    case NODE_STR:
      if (useResult) {
        v = factory->addString(n->nd_lit, true);
        elems.push_back(v);
      }
      break;

    case NODE_EVSTR:
      v = buildNode(factory, n->nd_body, useResult);
      v = factory->addPrimitive(IdStore::get(ID_rbjit__convert_to_string), 1, v);
      if (useResult) {
        elems.push_back(v);
      }
      break;

    default:
      RBJIT_UNREACHABLE;
    }
  }

  if (useResult) {
    Variable* count = factory->addImmediate(elems.size() - 1, true);
    elems[0] = count;
    return factory->addPrimitive(IdStore::get(ID_rbjit__concat_strings), elems, true);
  }
  return nullptr;
}

Variable*
CfgBuilder::buildHash(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  const RNode* n = node->nd_head;
  if (!n) {
    return factory->addHash(nullptr, nullptr, useResult);
  }

  int count = n->nd_alen;
  Variable** elems = (Variable**)_alloca(count * sizeof(Variable*));
  Variable** e = elems;
  for (; n; n = n->nd_next) {
    *e++ = buildNode(factory, n->nd_head, useResult);
  }

  return factory->addHash(elems, elems + count, useResult);
}

Variable*
CfgBuilder::buildIf(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_IF);

  // condition
  Variable* cond = buildNode(factory, node->nd_cond, true);

  // true block
  OpcodeFactory trueFactory(*factory);
  BlockHeader* trueBlock = trueFactory.lastBlock();
  Variable* trueValue;
  if (node->nd_body) {
    trueValue = buildNode(&trueFactory, node->nd_body, useResult);
  }
  else {
    trueValue = trueFactory.addImmediate(mri::Object::nilObject(), useResult);
  }

  // false block
  OpcodeFactory falseFactory(*factory);
  BlockHeader* falseBlock = falseFactory.lastBlock();
  Variable* falseValue;
  if (node->nd_else) {
    falseValue = buildNode(&falseFactory, node->nd_else, useResult);
  }
  else {
    falseValue = falseFactory.addImmediate(mri::Object::nilObject(), useResult);
  }

  // branch
  factory->addJumpIf(cond, trueBlock, falseBlock);

  // join block
  if (trueFactory.continues()) {
    if (falseFactory.continues()) {
      BlockHeader* join = factory->addFreeBlockHeader(0);
      Variable* value = factory->createTemporary(useResult);
      if (useResult) {
        trueFactory.addCopy(value, trueValue, useResult);
        falseFactory.addCopy(value, falseValue, useResult);
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
CfgBuilder::buildAndOr(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_AND || nd_type(node) == NODE_OR);

  // left-side hand value
  Variable* first = buildNode(factory, node->nd_1st, true);

  if (!factory->continues()) {
    return 0;
  }

  // join block
  OpcodeFactory joinFactory(*factory);
  BlockHeader* joinBlock = joinFactory.lastBlock();

  // cushion block to avoid a critical edge
  OpcodeFactory cushionFactory(*factory);
  BlockHeader* cushionBlock = cushionFactory.lastBlock();
  cushionFactory.addJump(joinBlock);

  // right-side hand value
  OpcodeFactory secondFactory(*factory);
  BlockHeader* secondBlock = secondFactory.lastBlock();
  Variable* second = buildNode(&secondFactory, node->nd_2nd, useResult);
  if (useResult) {
    secondFactory.addCopy(first, second, useResult);
  }
  secondFactory.addJump(joinBlock);

  // branch
  if (nd_type(node) == NODE_AND) {
    factory->addJumpIf(first, secondBlock, cushionBlock);
  }
  else {
    factory->addJumpIf(first, cushionBlock, secondBlock);
  }

  *factory = joinFactory;
  return first;
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
  OpcodeFactory preheaderFactory(*factory);
  preheaderFactory.lastBlock()->setDebugName("while_preheader");
  OpcodeFactory condFactory(*factory);
  condFactory.lastBlock()->setDebugName("while_cond");
  OpcodeFactory bodyFactory(*factory);
  bodyFactory.lastBlock()->setDebugName("while_body");
  OpcodeFactory preexitFactory(*factory);
  preexitFactory.lastBlock()->setDebugName("while_preexit");
  OpcodeFactory exitFactory(*factory);
  exitFactory.lastBlock()->setDebugName("while_exit");

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
  if (useResult) {
    preexitFactory.addCopy(value, nil, useResult);
  }
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
CfgBuilder::buildReturn(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  Variable* ret;
  if (node->nd_stts) {
    ret = buildNode(factory, node->nd_stts, true);
  }
  else {
    ret = factory->addImmediate(mri::Object::nilObject(), true);
  }
  factory->addJumpToReturnBlock(ret);

  return ret;
}

Variable*
CfgBuilder::buildCall(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  // Receiver
  Variable* receiver = buildNode(factory, node->nd_recv, true);

  // Arguments
  int argCount = 1;
  if (node->nd_args) {
    argCount = node->nd_args->nd_alen + 1; // includes receiver
  }
  Variable** args = (Variable**)_alloca(argCount * sizeof(Variable*));
  Variable** a = args;
  *a++ = receiver;
  for (RNode* n = node->nd_args; n; n = n->nd_next) {
    *a++ = buildNode(factory, n->nd_head, true);
  }

  // Find a method
  Variable* lookup = factory->addLookup(receiver, node->nd_mid);

  // Call a method
  Variable* value = factory->addCall(lookup, args, args + argCount, useResult);

  return value;
}

Variable*
CfgBuilder::buildFuncall(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_FCALL || nd_type(node) == NODE_VCALL);

  bool isPrimitive = PrimitiveStore::instance()->isPrimitive(node->nd_mid);

  // Argument count
  int argCount = 0;
  if (nd_type(node) == NODE_FCALL) {
    argCount = node->nd_args->nd_alen;
  }
  if (!isPrimitive) {
    argCount += 1; // count up for the implicit receiver
  }

  Variable** args = (Variable**)_alloca(argCount * sizeof(Variable*));
  Variable** a = args;
  if (!isPrimitive) {
    *a++ = buildSelf(factory, 0, true);
  }
  for (RNode* n = node->nd_args; n; n = n->nd_next) {
    *a++ = buildNode(factory, n->nd_head, true);
  }

  Variable* value;
  if (isPrimitive) {
    // Primitive
    value = factory->addPrimitive(node->nd_mid, args, args + argCount, useResult);
  }
  else {
    // Find a method
    Variable* lookup = factory->addLookup(*args, node->nd_mid);

    // Call a method
    value = factory->addCall(lookup, args, args + argCount, useResult);
  }

  return value;
}

Variable*
CfgBuilder::buildConstant(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_CONST);

  return factory->addConstant(node->nd_vid, nullptr, useResult);
}

Variable*
CfgBuilder::buildRelativeConstant(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_COLON2);

  Variable* base = buildNode(factory, node->nd_head, true);
  return factory->addConstant(node->nd_mid, base, useResult);
}

Variable*
CfgBuilder::buildToplevelConstant(OpcodeFactory* factory, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_COLON3);

  return factory->addToplevelConstant(node->nd_mid, useResult);
}

RBJIT_NAMESPACE_END
