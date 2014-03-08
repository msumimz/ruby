#pragma once
#include <unordered_set>
#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;
class DomTree;
class Opcode;
class OpcodeL;

class SsaTranslator {
public:

  SsaTranslator(ControlFlowGraph* cfg, bool doCopyFolding);
  ~SsaTranslator();

  void translate();

  std::string debugPrintDf() const;

private:

  void computeDf();

  void insertPhiFunctions();
  void insertPhiFunctionsForSingleDefSite(int blockIndex, Variable* v);
  void insertSinglePhiFunction(BlockHeader* block, Variable* v);

  void renameVariables();
  void renameVariablesForSingleBlock(BlockHeader* b);
  void renameVariablesInLhs(BlockHeader* block, OpcodeL* op, Variable* lhs);
  void renameEnv(BlockHeader* block, Opcode* op);
  void renameVariablesInRhs(Opcode* op);
  void renameRhsOfPhiFunctions(BlockHeader* parent, BlockHeader* b);

  ControlFlowGraph* cfg_;
  bool doCopyFolding_;
  DomTree* domTree_;

  // Dominance frontier
  std::vector<std::vector<bool> > df_;

  // Working vectors for phi insertion
  std::vector<Variable*> phiInserted_;
  std::vector<Variable*> processed_;

  // Working vectors for variable renaming
  std::vector<Variable*> folded_;
  std::vector<std::vector<Variable*> > renameStack_;
};

RBJIT_NAMESPACE_END
