#pragma once

#include <vector>
#include <unordered_map>
#include "rbjit/opcode.h"
#include "rbjit/defusechain.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeConstraint;
class BlockHeader;

RBJIT_NAMESPACE_END

namespace std {
  template <>
  class hash<std::pair<rbjit::BlockHeader*, rbjit::BlockHeader*>> {
  public:
    size_t
      operator()(const std::pair<rbjit::BlockHeader*, rbjit::BlockHeader*>& value) const {
        return ((size_t)value.first ^ (size_t)value.second) >> 2;
    }
  };
}

RBJIT_NAMESPACE_BEGIN

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
  void makeEdgeReachable(BlockHeader* from, BlockHeader* to);
  void makeEdgeUnreachable(BlockHeader* from, BlockHeader* to);
  void evaluateExpressionsUsing(Variable* v);

  ControlFlowGraph* cfg_;
  BlockHeader* block_;

  // Working vectors
  std::vector<BlockHeader*> blocks_;
  std::vector<Variable*> variables_;

  // Reachability of blocks and edges
  enum { UNKNOWN = 0, REACHABLE, UNREACHABLE };
  std::vector<char> reachBlocks_;
  std::unordered_map<std::pair<BlockHeader*, BlockHeader*>, char> reachEdges_;

  DefUseChain defUseChain_;
};

RBJIT_NAMESPACE_END