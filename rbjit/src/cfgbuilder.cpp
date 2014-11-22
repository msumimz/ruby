#include <cassert>

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
#include "rbjit/block.h"
#include "rbjit/blockbuilder.h"

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
// Helper methods

Variable*
CfgBuilder::createNamedVariable(BlockBuilder* builder, ID name)
{
  auto i = namedVariables_.find(name);
  if (i != namedVariables_.end()) {
    return i->second;
  }

  NamedVariable* nv = scope_->find(name);
  assert(nv);

  Variable* v  = new Variable(name, nv);
  cfg_->addVariable(v);
  namedVariables_.insert(std::make_pair(name, v));

  return v;
}

void
CfgBuilder::buildJumpToReturnBlock(BlockBuilder* builder, Variable* returnValue)
{
  if (returnValue) {
    Variable* output = builder->add(new OpcodeCopy(loc_, cfg_->output(), returnValue));
    cfg_->setOutput(output);
  }

  builder->addJump(loc_, cfg_->exitBlock());

  builder->halt();
}

////////////////////////////////////////////////////////////
// Builder methods

ControlFlowGraph*
CfgBuilder::buildMethod(const RNode* rootNode, ID name)
{
  cfg_ = new ControlFlowGraph;
  scope_ = new Scope(rootNode->nd_tbl, nullptr);
  name_ = name;
  defInfoMap_ = new DefInfoMap;

  // Start the entry block
  Block* entry = new Block;
  entry->setDebugName("entry");
  BlockBuilder builder(cfg_, scope_, defInfoMap_, entry);
  cfg_->setEntryBlock(entry);

  // undefined
  Variable* undefined = builder.add(new OpcodeImmediate(nullptr, nullptr, mri::Object::nilObject()));
  cfg_->setUndefined(undefined);

  // env
  Variable* env = builder.add(new OpcodeEnv(loc_, nullptr));
  env->setName(IdStore::get(ID_env));
  cfg_->setEntryEnv(env);
  cfg_->setExitEnv(env);

  // scope
  builder.add(new OpcodeEnter(nullptr, scope_));

  // Create the exit block
  Block* exit = new Block;
  exit->setDebugName("exit");
  BlockBuilder exitBuilder(&builder, exit);
  cfg_->setExitBlock(exit);

  // Exit env
  exitBuilder.add(new OpcodeCopy(loc_, env, env));

  // Exit
  exitBuilder.add(new OpcodeExit(loc_));
  try {
    buildArguments(&builder, rootNode);
    buildProcedureBody(&builder, rootNode, true);
  }
  catch (std::exception& e) {
    delete cfg_;
    delete scope_;
    delete defInfoMap_;
    throw;
  };

  return cfg_;
}

void
CfgBuilder::buildProcedureBody(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_SCOPE);

  Variable* v = buildNode(builder, node->nd_body, useResult);
  if (builder->continues()) {
    buildJumpToReturnBlock(builder, v);
  }
}

void
CfgBuilder::buildArguments(BlockBuilder* builder, const RNode* node)
{
  assert(nd_type(node) == NODE_SCOPE);

  // self
  Variable* self = createNamedVariable(builder, IdStore::get(ID_self));
  defInfoMap_->updateDefSite(self, cfg_->entryBlock(), nullptr);
  cfg_->addInput(self);

  RNode* argNode = node->nd_args;

  int requiredArgCount = argNode->nd_ainfo->pre_args_num;
  bool hasOptionalArg = !!argNode->nd_ainfo->opt_args;
  bool hasRestArg = !!argNode->nd_ainfo->rest_arg;
/*
  cfg_->setRequiredArgCount(requiredArgCount);
  cfg_->setHasOptionalArg(hasOptionalArg);
  cfg_->setHasRestArg(hasRestArg);
*/
  if (!hasOptionalArg && !hasRestArg) {
    // Method has the fixed number of arguments
    mri::IdTable idTable(node->nd_tbl);
    for (int i = 0; i < requiredArgCount; ++i) {
      Variable* v = createNamedVariable(builder, idTable.idAt(i));
      defInfoMap_->updateDefSite(v, cfg_->entryBlock(), nullptr);
      cfg_->addInput(v);
    }
  }
  else {
    std::string what = stringFormat("Method %s uses vardiac arguments, which is not implemented yet", mri::Id(name_).name());
    throw UnsupportedSyntaxException(what);
  }
}

Variable*
CfgBuilder::buildNode(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* v;

  if (!builder->continues()) {
    return nullptr;
  }

  enum node_type type = (enum node_type)nd_type(node);
  switch (type) {
  case NODE_BLOCK:
    for (; node->nd_next; node = node->nd_next) {
      buildNode(builder, node->nd_head, false);
      if (!builder->continues()) {
        return nullptr;
      }
    }
    v = buildNode(builder, node->nd_head, useResult);
    break;

  case NODE_DASGN_CURR:
  case NODE_LASGN:
    v = buildAssignment(builder, node, useResult);
    break;

  case NODE_LVAR:
    v = buildLocalVariable(builder, node, useResult);
    break;

  case NODE_LIT:
    v = buildImmediate(builder, node, useResult);
    break;

  case NODE_SELF:
    v = buildSelf(builder, node, useResult);
    break;

  case NODE_TRUE:
    v = buildTrue(builder, node, useResult);
    break;

  case NODE_FALSE:
    v = buildFalse(builder, node, useResult);
    break;

  case NODE_NIL:
    v = buildNil(builder, node, useResult);
    break;

  case NODE_ARRAY:
  case NODE_ZARRAY:
    v = buildArray(builder, node, useResult);
    break;

  case NODE_ARGSPUSH:
    v = buildArrayPush(builder, node, useResult);
    break;

  case NODE_ARGSCAT:
    v = buildArrayConcat(builder, node, useResult);
    break;

  case NODE_SPLAT:
    v = buildArraySplat(builder, node, useResult);
    break;

  case NODE_DOT2:
  case NODE_DOT3:
    v = buildRange(builder, node, useResult);
    break;

  case NODE_STR:
    v = buildString(builder, node, useResult);
    break;

  case NODE_DSTR:
    v = buildStringInterpolation(builder, node, useResult);
    break;

  case NODE_HASH:
    v = buildHash(builder, node, useResult);
    break;

  case NODE_AND:
  case NODE_OR:
    v = buildAndOr(builder, node, useResult);
    break;

  case NODE_IF:
    v = buildIf(builder, node, useResult);
    break;

  case NODE_WHILE:
    v = buildWhile(builder, node, useResult);
    break;

  case NODE_RETURN:
    v = buildReturn(builder, node, useResult);
    break;

  case NODE_CALL:
    v = buildCall(builder, node, useResult);
    break;

  case NODE_FCALL:
  case NODE_VCALL:
    v = buildFuncall(builder, node, useResult);
    break;

  case NODE_CONST:
    v = buildConstant(builder, node, useResult);
    break;

  case NODE_COLON2:
    v = buildRelativeConstant(builder, node, useResult);
    break;

  case NODE_COLON3:
    v = buildToplevelConstant(builder, node, useResult);
    break;

  default:
    std::string what = stringFormat("%s:%d: Node type %s is not implemented yet",
      mri::Id(name_).name(), nd_line(node), ruby_node_name(nd_type(node)));
    throw UnsupportedSyntaxException(what);
  }

  return v;
}

Variable*
CfgBuilder::buildAssignment(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_DASGN_CURR || nd_type(node) == NODE_LASGN);

  Variable* rhs = buildNode(builder, node->nd_value, true);
  Variable* lhs = createNamedVariable(builder, node->nd_vid);

  builder->add(new OpcodeCopy(loc_, lhs, rhs));

  return lhs;
}

Variable*
CfgBuilder::buildLocalVariable(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_LVAR);
  return createNamedVariable(builder, node->nd_vid);
}

Variable*
CfgBuilder::buildImmediate(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_LIT);
  if (!useResult) {
    return nullptr;
  }

  return builder->add(new OpcodeImmediate(loc_, nullptr, (VALUE)node->nd_lit));
}

Variable*
CfgBuilder::buildSelf(BlockBuilder* builder, const RNode* node, bool useResult)
{
  if (!useResult) {
    return nullptr;
  }

  return createNamedVariable(builder, IdStore::get(ID_self));
}

Variable*
CfgBuilder::buildTrue(BlockBuilder* builder, const RNode* node, bool useResult)
{
  if (!useResult) {
    return nullptr;
  }

  return builder->add(new OpcodeImmediate(loc_, nullptr, Qtrue));
}

Variable*
CfgBuilder::buildFalse(BlockBuilder* builder, const RNode* node, bool useResult)
{
  if (!useResult) { return nullptr; }

  return builder->add(new OpcodeImmediate(loc_, nullptr, Qfalse));
}

Variable*
CfgBuilder::buildNil(BlockBuilder* builder, const RNode* node, bool useResult)
{
  if (!useResult) { return nullptr; }

  return builder->add(new OpcodeImmediate(loc_, nullptr, Qnil));
}

Variable*
CfgBuilder::buildArray(BlockBuilder* builder, const RNode* node, bool useResult)
{
  if (nd_type(node) == NODE_ZARRAY) {
    if (!useResult) { return nullptr; }
    return builder->add(new OpcodeArray(loc_, nullptr, 0));
  }

  if (!useResult) {
    for (const RNode* n = node; n; n = n->nd_next) {
      buildNode(builder, n->nd_head, false);
    }
  }

  int count = node->nd_alen;
  OpcodeArray* array = new OpcodeArray(loc_, nullptr, count);
  Variable** e = array->begin();
  for (const RNode* n = node; n; n = n->nd_next) {
    *e++ = buildNode(builder, n->nd_head, true);
  }
  return builder->add(array);
}

Variable*
CfgBuilder::buildArrayPush(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* array = buildNode(builder, node->nd_head, useResult);
  Variable* obj = buildNode(builder, node->nd_body, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = builder->add(OpcodePrimitive::create(loc_, nullptr, IdStore::get(ID_rbjit__push_to_array), 2, array, obj));
  }

  return result;
}

Variable*
CfgBuilder::buildArrayConcat(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* a1 = buildNode(builder, node->nd_head, useResult);
  Variable* a2 = buildNode(builder, node->nd_body, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = builder->add(OpcodePrimitive::create(loc_, nullptr, IdStore::get(ID_rbjit__concat_arrays), 2, a1, a2));
  }

  return result;
}

Variable*
CfgBuilder::buildArraySplat(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* obj = buildNode(builder, node->nd_head, useResult);

  Variable* result = nullptr;
  if (useResult) {
    result = builder->add(OpcodePrimitive::create(loc_, nullptr, IdStore::get(ID_rbjit__convert_to_array), 1, obj));
  }

  return result;
}

Variable*
CfgBuilder::buildRange(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* low = buildNode(builder, node->nd_beg, useResult);
  Variable* high = buildNode(builder, node->nd_end, useResult);
  return builder->add(new OpcodeRange(loc_, nullptr, low, high, nd_type(node) == NODE_DOT3));
}

Variable*
CfgBuilder::buildString(BlockBuilder* builder, const RNode* node, bool useResult)
{
  return builder->add(new OpcodeString(loc_, nullptr, node->nd_lit));
}

Variable*
CfgBuilder::buildStringInterpolation(BlockBuilder* builder, const RNode* node, bool useResult)
{
  std::vector<Variable*> elems(1, nullptr);

  Variable* v;
  if (useResult && !NIL_P(node->nd_lit)) {
    v = builder->add(new OpcodeString(loc_, nullptr, node->nd_lit));
    elems.push_back(v);
  }

  for (node = node->nd_next; node; node = node->nd_next) {
    const RNode* n = node->nd_head;
    switch (nd_type(n)) {
    case NODE_STR:
      if (useResult) {
        v = builder->add(new OpcodeString(loc_, nullptr, n->nd_lit));
        elems.push_back(v);
      }
      break;

    case NODE_EVSTR:
      v = buildNode(builder, n->nd_body, useResult);
      v = builder->add(OpcodePrimitive::create(loc_, nullptr, IdStore::get(ID_rbjit__convert_to_string), 1, v));
      if (useResult) {
        elems.push_back(v);
      }
      break;

    default:
      RBJIT_UNREACHABLE;
    }
  }

  if (useResult) {
    Variable* count = builder->add(new OpcodeImmediate(loc_, nullptr, elems.size() - 1));
    elems[0] = count;
    return builder->add(OpcodePrimitive::create(loc_, nullptr, IdStore::get(ID_rbjit__concat_strings), &*elems.begin(), &*elems.end()));
  }
  return nullptr;
}

Variable*
CfgBuilder::buildHash(BlockBuilder* builder, const RNode* node, bool useResult)
{
  const RNode* n = node->nd_head;
  if (!n) {
    return builder->add(new OpcodeHash(loc_, nullptr, 0));
  }

  if (!useResult) {
    for (; n; n = n->nd_next) {
      buildNode(builder, n->nd_head, false);
      return nullptr;
    }
  }

  int count = n->nd_alen;
  OpcodeHash* hash = new OpcodeHash(loc_, nullptr, count);
  Variable** e = hash->begin();
  for (; n; n = n->nd_next) {
    *e++ = buildNode(builder, n->nd_head, true);
  }

  return builder->add(hash);
}

Variable*
CfgBuilder::buildIf(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_IF);

  // condition
  Variable* cond = buildNode(builder, node->nd_cond, true);

  // true block
  Block* trueBlock = new Block;
  BlockBuilder trueBuilder(builder, trueBlock);
  Variable* trueValue = nullptr;
  if (node->nd_body) {
    trueValue = buildNode(&trueBuilder, node->nd_body, useResult);
  }
  else {
    if (useResult) {
      trueValue = trueBuilder.add(new OpcodeImmediate(loc_, nullptr, mri::Object::nilObject()));
    }
  }

  // false block
  Block* falseBlock = new Block;
  BlockBuilder falseBuilder(builder, falseBlock);
  Variable* falseValue = nullptr;
  if (node->nd_else) {
    falseValue = buildNode(&falseBuilder, node->nd_else, useResult);
  }
  else {
    if (useResult) {
      falseValue = falseBuilder.add(new OpcodeImmediate(loc_, nullptr, mri::Object::nilObject()));
    }
  }

  // branch
  builder->add(new OpcodeJumpIf(loc_, cond, trueBlock, falseBlock));

  // join block
  if (trueBuilder.continues()) {
    if (falseBuilder.continues()) {
      Block* join = new Block;
      builder->setBlock(join);
      Variable* value = nullptr;
      if (useResult) {
        value = new Variable;
        trueBuilder.add(new OpcodeCopy(loc_, value, trueValue));
        falseBuilder.add(new OpcodeCopy(loc_, value, falseValue));
      }
      trueBuilder.add(new OpcodeJump(loc_, join));
      falseBuilder.add(new OpcodeJump(loc_, join));
      return value;
    }

    builder->setBlock(trueBuilder.block());
    return trueValue;
  }
  else {
    if (falseBuilder.continues()) {
      builder->setBlock(falseBuilder.block());
      return falseValue;
    }
  }

  // Both route have been stopped
  builder->halt();
  return nullptr;
}

Variable*
CfgBuilder::buildAndOr(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_AND || nd_type(node) == NODE_OR);

  // left-side hand value
  Variable* first = buildNode(builder, node->nd_1st, true);

  if (!builder->continues()) {
    return nullptr;
  }

  // join block
  Block* joinBlock = new Block;
  BlockBuilder joinBuilder(builder, joinBlock);

  // cushion block to avoid a critical edge
  Block* cushionBlock = new Block;
  BlockBuilder cushionBuilder(builder, cushionBlock);
  cushionBuilder.addJump(loc_, joinBlock);

  // right-side hand value
  Block* secondBlock = new Block;
  BlockBuilder secondBuilder(builder, secondBlock);
  Variable* second = buildNode(&secondBuilder, node->nd_2nd, useResult);
  if (useResult) {
    secondBuilder.add(new OpcodeCopy(loc_, first, second));
  }
  secondBuilder.addJump(loc_, joinBlock);

  // branch
  if (nd_type(node) == NODE_AND) {
    builder->addJumpIf(loc_, first, secondBlock, cushionBlock);
  }
  else {
    builder->addJumpIf(loc_, first, cushionBlock, secondBlock);
  }

  builder->setBlock(joinBuilder.block());
  return first;
}

Variable*
CfgBuilder::buildWhile(BlockBuilder* builder, const RNode* node, bool useResult)
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
  Variable* value = useResult ? new Variable : nullptr;

  // Create blocks
  BlockBuilder preheaderBuilder(builder, new Block("while_preheader"));
  BlockBuilder condBuilder(builder, new Block("while_cond"));
  BlockBuilder bodyBuilder(builder, new Block("while_body"));
  BlockBuilder preexitBuilder(builder, new Block("while_preexit"));
  BlockBuilder exitBuilder(builder, new Block("while_exit"));

  // Preheader block
  builder->addJump(loc_, preheaderBuilder.block());

  if (node->nd_state == 0) {
    // begin-end-while
    preheaderBuilder.addJump(loc_, bodyBuilder.block());
  }
  else {
    // while
    preheaderBuilder.addJump(loc_, condBuilder.block());
  }

  // Condition block
  Block* condBlock = condBuilder.block();
  Variable* cond = buildNode(&condBuilder, node->nd_cond, true);
  condBuilder.addJumpIf(loc_, cond, bodyBuilder.block(), preexitBuilder.block());

  // Preexit block
  if (useResult) {
    Variable* nil = preexitBuilder.add(new OpcodeImmediate(loc_, nullptr, mri::Object::nilObject()));
    preexitBuilder.add(new OpcodeCopy(loc_, value, nil));
  }
  preexitBuilder.addJump(loc_, exitBuilder.block());

  // Body blcok
  exits_.push_back(ExitPoint(&condBuilder, &bodyBuilder, &exitBuilder, value));
  buildNode(&bodyBuilder, node->nd_body, false);
  bodyBuilder.addJump(loc_, condBlock);
  exits_.pop_back();

  builder->setBlock(exitBuilder.block());

  return value;
}

Variable*
CfgBuilder::buildReturn(BlockBuilder* builder, const RNode* node, bool useResult)
{
  Variable* ret;
  if (node->nd_stts) {
    ret = buildNode(builder, node->nd_stts, true);
  }
  else {
    ret = builder->add(new OpcodeImmediate(loc_, nullptr, mri::Object::nilObject()));
  }
  buildJumpToReturnBlock(builder, ret);

  return ret;
}

Variable*
CfgBuilder::buildCall(BlockBuilder* builder, const RNode* node, bool useResult)
{
  // Count the number of arguments
  int argCount = 1;
  if (node->nd_args) {
    argCount = node->nd_args->nd_alen + 1; // includes receiver
  }

  Variable* receiver = buildNode(builder, node->nd_recv, true);

  // Create a Lookup opcode
  OpcodeLookup* lookupOp = new OpcodeLookup(loc_, nullptr, receiver, node->nd_mid, cfg_->entryEnv());

  // Create a Call opcode
  OpcodeCall* op = new OpcodeCall(loc_, nullptr, nullptr, argCount, cfg_->exitEnv());
  defInfoMap_->updateDefSite(cfg_->exitEnv(), builder->block(), op);

  // Arguments
  Variable** args = op->begin();
  *args++ = receiver; // Receiver
  for (RNode* n = node->nd_args; n; n = n->nd_next) {
    *args++ = buildNode(builder, n->nd_head, true);
  }

  Variable* lookup = builder->add(lookupOp);
  op->setLookup(lookup);
  return builder->add(useResult, op);
}

Variable*
CfgBuilder::buildFuncall(BlockBuilder* builder, const RNode* node, bool useResult)
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

  // Create an appropriate opcode
  if (isPrimitive) {
    // Primitive
    OpcodePrimitive* op = new OpcodePrimitive(loc_, nullptr, node->nd_mid, argCount);
    Variable** args = op->begin();
    for (RNode* n = node->nd_args; n; n = n->nd_next) {
      *args++ = buildNode(builder, n->nd_head, true);
    }
    return builder->add(useResult, op);
  }

  // Call a method

  Variable* receiver = buildSelf(builder, 0, true);
  OpcodeLookup* lookupOp = new OpcodeLookup(loc_, nullptr, receiver, node->nd_mid, cfg_->entryEnv());
  OpcodeCall* op = new OpcodeCall(loc_, nullptr, nullptr, argCount, cfg_->exitEnv());
  defInfoMap_->updateDefSite(cfg_->exitEnv(), builder->block(), op);

  // Arguments
  Variable** args = op->begin();
  *args++ = receiver;
  for (RNode* n = node->nd_args; n; n = n->nd_next) {
    *args++ = buildNode(builder, n->nd_head, true);
  }

  Variable* lookup = builder->add(lookupOp);
  op->setLookup(lookup);
  return builder->add(useResult, op);
}

Variable*
CfgBuilder::buildConstant(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_CONST);

  OpcodeConstant* op = new OpcodeConstant(loc_, nullptr, cfg_->undefined(), node->nd_vid, false, cfg_->entryEnv(), cfg_->exitEnv());
  defInfoMap_->updateDefSite(cfg_->exitEnv(), builder->block(), op);

  return builder->add(useResult, op);
}

Variable*
CfgBuilder::buildRelativeConstant(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_COLON2);

  Variable* base = buildNode(builder, node->nd_head, true);
  OpcodeConstant* op = new OpcodeConstant(loc_, nullptr, base, node->nd_mid, false, cfg_->entryEnv(), cfg_->exitEnv());
  defInfoMap_->updateDefSite(cfg_->exitEnv(), builder->block(), op);

  return builder->add(useResult, op);
}

Variable*
CfgBuilder::buildToplevelConstant(BlockBuilder* builder, const RNode* node, bool useResult)
{
  assert(nd_type(node) == NODE_COLON3);

  OpcodeConstant* op = new OpcodeConstant(loc_, nullptr, cfg_->undefined(), node->nd_mid, true, cfg_->entryEnv(), cfg_->exitEnv());
  defInfoMap_->updateDefSite(cfg_->exitEnv(), builder->block(), op);

  return  builder->add(useResult, op);
}

RBJIT_NAMESPACE_END
