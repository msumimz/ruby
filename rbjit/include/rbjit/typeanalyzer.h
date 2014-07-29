#pragma once

#include <vector>
#include <unordered_map>
#include <tuple>
#include "rbjit/opcode.h"
#include "rbjit/defusechain.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class TypeConstraint;
class BlockHeader;
class TypeContext;

RBJIT_NAMESPACE_END

namespace std {
  template <>
  class hash<std::pair<rbjit::BlockHeader*, rbjit::BlockHeader*>> {
  public:
    size_t
    operator()(const std::pair<rbjit::BlockHeader*, rbjit::BlockHeader*>& value) const
    {
      return (reinterpret_cast<size_t>(value.first) ^ reinterpret_cast<size_t>(value.second)) >> 2;
    }
  };
}

RBJIT_NAMESPACE_BEGIN

class TypeAnalyzer : public OpcodeVisitor {
public:

  TypeAnalyzer(ControlFlowGraph* cfg, mri::Class holderClass, mri::CRef cref);

  void setInputTypeConstraint(int index, const TypeConstraint& type);

  std::tuple<TypeContext*, bool, bool> analyze();

  // Evaluate expressions
  bool visitOpcode(BlockHeader* op);
  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeEnv* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodeConstant* op);
  bool visitOpcode(OpcodePrimitive* op);
  bool visitOpcode(OpcodePhi* op);
  bool visitOpcode(OpcodeExit* op);
  bool visitOpcode(OpcodeArray* op);
  bool visitOpcode(OpcodeRange* op);
  bool visitOpcode(OpcodeString* op);
  bool visitOpcode(OpcodeHash* op);

protected:

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

  TypeContext* typeContext_;
  bool mutator_;
  bool jitOnly_;

  mri::Class holderClass_;
  mri::CRef cref_;
};

RBJIT_NAMESPACE_END
