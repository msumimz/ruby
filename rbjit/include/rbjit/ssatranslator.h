#pragma once
#include <unordered_set>
#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class DefInfoMap;
class Variable;
class DomTree;
class Opcode;
class OpcodeL;

class SsaTranslator {
public:

  SsaTranslator(ControlFlowGraph* cfg, DefInfoMap* defInfoMap, DomTree* domTree, bool doCopyFolding);
  ~SsaTranslator();

  void translate();

  std::string debugPrintDf() const;

private:

  void computeDf();

  void insertPhiFunctions();
  void insertPhiFunctionsForSingleDefSite(int blockIndex, Variable* v);
  void insertSinglePhiFunction(Block* block, Variable* v);

  void renameVariables();
  void renameVariablesForSingleBlock(Block* b);
  void renameVariablesInLhs(Block* block, OpcodeL* op, Variable* lhs);
  void renameEnvInLhs(Block* block, Opcode* op, Variable* env);
  void renameVariablesInRhs(Opcode* op);
  void renameRhsOfPhiFunctions(Block* parent, Block* b);

  ControlFlowGraph* cfg_;
  DefInfoMap* defInfoMap_;
  DomTree* domTree_;
  bool doCopyFolding_;

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
