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
class BlockHeader;
class OpcodeFactory;
class MethodInfo;

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

  CfgBuilder() : cfg_(0) {}

  ControlFlowGraph* buildMethod(const RNode* rootNode, ID name);

private:

  Variable* buildNamedVariable(OpcodeFactory* factory, ID name);

  void buildProcedureBody(OpcodeFactory* factory, const RNode* node, bool useResult);
  void buildArguments(OpcodeFactory* factory, const RNode* node);
  Variable* buildNode(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildAssignment(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildLocalVariable(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildImmediate(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildSelf(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildTrue(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildFalse(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildNil(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildArray(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildArrayPush(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildArrayConcat(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildArraySplat(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildRange(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildString(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildStringInterpolation(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildHash(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildAndOr(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildIf(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildWhile(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildReturn(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildCall(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildFuncall(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildConstant(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildRelativeConstant(OpcodeFactory* factory, const RNode* node, bool useResult);
  Variable* buildToplevelConstant(OpcodeFactory* factory, const RNode* node, bool useResult);

  ControlFlowGraph* cfg_;

  std::unordered_map<ID, Variable*> namedVariables_;

  class ExitPoint {
  public:
    ExitPoint(OpcodeFactory* cond, OpcodeFactory* block, OpcodeFactory* exit, Variable* result)
      : cond_(cond), block_(block), exit_(exit), result_(result)
    {}

    OpcodeFactory* cond() const { return cond_; }
    OpcodeFactory* block() const { return block_; }
    OpcodeFactory* exit() const { return exit_; }
    Variable* result() const { return result_; }

  private:

    OpcodeFactory* cond_;
    OpcodeFactory* block_;
    OpcodeFactory* exit_;
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
