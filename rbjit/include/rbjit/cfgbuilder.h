#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class Opcode;
class Block;
class BlockBuilder;
class MethodInfo;
class Scope;
class DefInfoMap;
class SourceLocation;

////////////////////////////////////////////////////////////
// UnsupportedSyntaxException

class UnsupportedSyntaxException : public std::runtime_error {
public:

  UnsupportedSyntaxException(std::string what);

};

////////////////////////////////////////////////////////////
// CfgBuilder

class CfgBuilder {
public:

  CfgBuilder() : cfg_(nullptr), scope_(nullptr), loc_(nullptr) {}

  ControlFlowGraph* buildMethod(const RNode* rootNode, ID name);
  DefInfoMap* defInfoMap() const { return defInfoMap_; }

private:

  Variable* createNamedVariable(BlockBuilder* builder, ID name);
  void buildJumpToReturnBlock(BlockBuilder* builder, Variable* returnValue);

  void buildProcedureBody(BlockBuilder* builder, const RNode* node, bool useResult);
  void buildArguments(BlockBuilder* builder, const RNode* node);
  Variable* buildNode(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildAssignment(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildLocalVariable(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildImmediate(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildSelf(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildTrue(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildFalse(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildNil(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildArray(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildArrayPush(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildArrayConcat(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildArraySplat(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildRange(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildString(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildStringInterpolation(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildHash(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildAndOr(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildIf(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildWhile(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildReturn(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildCall(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildFuncall(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildCallWithBlock(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildConstant(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildRelativeConstant(BlockBuilder* builder, const RNode* node, bool useResult);
  Variable* buildToplevelConstant(BlockBuilder* builder, const RNode* node, bool useResult);

  ControlFlowGraph* cfg_;
  Scope* scope_;
  DefInfoMap* defInfoMap_;
  SourceLocation* loc_;

  std::unordered_map<ID, Variable*> namedVariables_;

  class ExitPoint {
  public:
    ExitPoint(BlockBuilder* cond, BlockBuilder* block, BlockBuilder* exit, Variable* result)
      : cond_(cond), block_(block), exit_(exit), result_(result)
    {}

    BlockBuilder* cond() const { return cond_; }
    BlockBuilder* block() const { return block_; }
    BlockBuilder* exit() const { return exit_; }
    Variable* result() const { return result_; }

  private:

    BlockBuilder* cond_;
    BlockBuilder* block_;
    BlockBuilder* exit_;
    Variable* result_;
  };

  std::vector<ExitPoint> exits_;

  // For diagnostic messages
  ID name_;

  /*
  std::vector<OpcodeFactory*> rescueBlocks_;
  std::vector<const AstNode*> ensureNodes_;
  std::vector<OpcodeFactory*> retryBlocks_;

  OpcodeFactory exitBlock_;

  int* dfsOrder_;
  */
};

RBJIT_NAMESPACE_END
