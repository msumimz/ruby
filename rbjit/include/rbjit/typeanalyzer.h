#pragma once

#include <vector>
#include "rbjit/opcode.h"
#include "rbjit/defusechain.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeConstraint;
class BlockHeader;

class TypeAnalyzer : public OpcodeVisitor {
public:

  TypeAnalyzer(ControlFlowGraph* cfg);
  void analyze();

  // Evaluate expressions
  bool visitOpcode(BlockHeader* opcode);
  bool visitOpcode(OpcodeCopy* opcode);
  bool visitOpcode(OpcodeJump* opcode);
  bool visitOpcode(OpcodeJumpIf* opcode);
  bool visitOpcode(OpcodeImmediate* opcode);
  bool visitOpcode(OpcodeEnv* opcode);
  bool visitOpcode(OpcodeLookup* opcode);
  bool visitOpcode(OpcodeCall* opcode);
  bool visitOpcode(OpcodePrimitive* opcode);
  bool visitOpcode(OpcodePhi* opcode);
  bool visitOpcode(OpcodeExit* opcode);

private:

  void updateTypeConstraint(Variable* v, const TypeConstraint& newType);
  void evaluateExpressionsUsing(Variable* v);

  ControlFlowGraph* cfg_;

  // Working vectors
  std::vector<BlockHeader*> blocks_;
  std::vector<Variable*> variables_;

  // Reachability of blocks and edges
  enum { UNKNOWN = 0, REACHABLE, UNREACHABLE };
  std::vector<char> reachBlocks_;
  std::vector<char> reachTrueEdges_;
  std::vector<char> reachFalseEdges_;

  DefUseChain defUseChain_;
};

RBJIT_NAMESPACE_END
