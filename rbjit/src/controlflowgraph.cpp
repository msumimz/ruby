#include <cstdarg>
#include <algorithm>
#include <iterator> // std::distance
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/domtree.h"
#include "rbjit/variable.h"
#include "rbjit/rubyobject.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/debugprint.h"
#include "rbjit/cfgsanitychecker.h"
#include "rbjit/ssachecker.h"
#include "rbjit/blockdebugprinter.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// ControlFlowGraph

ControlFlowGraph::~ControlFlowGraph()
{
  // Delete cfg
  for (auto i = blocks_.begin(), end = blocks_.end(); i != end; ++i) {
    Block* block = *i;
    for (auto j = block->begin(), jend = block->end(); j != jend; ++j) {
      Opcode* op = *j;
      delete op;
    }
  }

  // Delete variables
  for (auto i = variables_.begin(), end = variables_.end(); i != end; ++i) {
    delete *i;
  }
}

bool
ControlFlowGraph::checkSanity() const
{
  CfgSanityChecker checker(this);
  checker.check();

  return checker.errors().empty();
}

bool
ControlFlowGraph::checkSanityAndPrintErrors() const
{
  CfgSanityChecker checker(this);
  checker.check();

  for (auto i = checker.errors().cbegin(), end = checker.errors().cend(); i != end; ++i) {
    fprintf(stderr, "%s\n", i->c_str());
  }

  return checker.errors().empty();
}

bool
ControlFlowGraph::checkSsaAndPrintErrors() const
{
  SsaChecker checker(this);
  checker.check();

  for (auto i = checker.errors().cbegin(), end = checker.errors().cend(); i != end; ++i) {
    fprintf(stderr, "SSA Checker: %s\n", i->c_str());
  }

  return checker.errors().empty();
}

////////////////////////////////////////////////////////////
// Debugging tool: printing cfg

std::string
ControlFlowGraph::debugPrint() const
{
  std::string out = stringFormat("[CFG: %Ix]\nentry=%Ix exit=%Ix output=%Ix entryEnv=%Ix exitEnv=%Ix\n",
    this, entryBlock(), exitBlock(), output(), entryEnv(), exitEnv());

  BlockDebugPrinter printer;
  for (auto i = begin(), e = end(); i != e; ++i) {
    out += printer.print(*i);
  }
  return out;
}

std::string
ControlFlowGraph::debugPrintDotHeader() const
{
  static int count = 0;
  return stringFormat("[Dot: %d %Ix]\n", count++, this);
}

std::string
ControlFlowGraph::debugPrintAsDot() const
{
  std::string out;
  BlockDebugPrinter printer;
  for (auto i = begin(), e = end(); i != e; ++i) {
    out += printer.printAsDot(*i);
  }
  return out;
}

std::string
ControlFlowGraph::debugPrintBlock(Block* block) const
{
  std::string out = stringFormat("[Block %Ix (CFG: %Ix)]\n", block, this);
  BlockDebugPrinter printer;
  out += printer.print(block);
  return out;
}

std::string
ControlFlowGraph::debugPrintVariables() const
{
  std::string out = "[Inputs]\n";
  for (auto i = constInputBegin(), end = constInputEnd(); i != end; ++i) {
    out += (*i)->debugPrint();
  };

  out += "[Variables]\n";
  for (auto i = constVariableBegin(), end = constVariableEnd(); i != end; ++i) {
    out += (*i)->debugPrint();
  };

  return out;
}

RBJIT_NAMESPACE_END
