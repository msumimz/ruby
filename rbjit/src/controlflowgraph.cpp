#include <cstdarg>
#include <algorithm>
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/domtree.h"
#include "rbjit/variable.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// ControlFlowGraph

ControlFlowGraph::ControlFlowGraph()
  : hasEvals_(UNKNOWN), hasDefs_(UNKNOWN), hasBindings_(UNKNOWN),
    opcodeCount_(0), entry_(0), exit_(0),
    output_(0), undefined_(0),
    domTree_(0)
{}

DomTree*
ControlFlowGraph::domTree()
{
  if (!domTree_) {
    domTree_ = new DomTree(this);
  }
  return domTree_;
}

Variable*
ControlFlowGraph::copyVariable(BlockHeader* defBlock, Opcode* defOpcode, Variable* source)
{
  Variable* v = source->copy(defBlock, defOpcode, variables_.size(), source);
  variables_.push_back(v);
  return v;
}

void
ControlFlowGraph::removeVariables(const std::vector<Variable*>* toBeRemoved)
{
  // Zero-clear elements to be removed
  std::vector<Variable*>::const_iterator i = toBeRemoved->begin();
  std::vector<Variable*>::const_iterator end = toBeRemoved->end();
  for (; i != end; ++i) {
    delete variables_[(*i)->index()];
    variables_[(*i)->index()] = nullptr;
  }

  // Remove null elements
  variables_.erase(std::remove_if(variables_.begin(), variables_.end(),
    [](Variable* v) { return v == 0; }), variables_.end());

  // Reset indexes
  for (int i = 0; i < variables_.size(); ++i) {
    variables_[i]->setIndex(i);
  }
}

void
ControlFlowGraph::removeOpcodeAfter(Opcode* prev)
{
  Opcode* op = prev->next();
  prev->removeNextOpcode();
  delete op;
}

////////////////////////////////////////////////////////////
// Debugging tool

namespace {

class Dumper : public OpcodeVisitor {
public:

  Dumper() {}

  std::string output() const { return out_; }

  bool visitOpcode(BlockHeader* op);
  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodePhi* op);

  void dumpCfgInfo(const ControlFlowGraph* cfg);
  void dumpBlockHeader(BlockHeader* b);

private:

  void putCommonOutput(Opcode* op);
  void put(const char* format, ...);

  std::string out_;
  char buf_[256];
};

void
Dumper::putCommonOutput(Opcode* op)
{
  int skip = strlen("const rbjit::"); // hopefully optimized out
  sprintf(buf_, "  %Ix %Ix %d:%d %s ",
    op, op->next(), op->file(), op->line(), typeid(*op).name() + skip);
  out_ += buf_;
}

void
Dumper::put(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vsprintf(buf_, format, args);
  out_ += buf_;
}

bool
Dumper::visitOpcode(BlockHeader* op)
{
  putCommonOutput(op);
  put("index=%d depth=%d idom=%Ix footer=%Ix backedges=",
    op->index(), op->depth(), op->idom(), op->footer());
  for (BlockHeader::Backedge* e = op->backedge(); e; e = e->next()) {
    put("%Ix ", e->block());
  }
  out_ += '\n';
  return true;
}

bool
Dumper::visitOpcode(OpcodeCopy* op)
{
  putCommonOutput(op);
  put("lhs=%Ix rhs=%Ix\n", op->lhs(), op->rhs());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJump* op)
{
  putCommonOutput(op);
  put("dest=%Ix\n", op->dest());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJumpIf* op)
{
  putCommonOutput(op);
  put("cond=%Ix ifTrue=%Ix ifFalse=%Ix\n",
    op->cond(), op->ifTrue(), op->ifFalse());
  return true;
}

bool
Dumper::visitOpcode(OpcodeImmediate* op)
{
  putCommonOutput(op);
  put("lhs=%Ix value=%Ix\n", op->lhs(), op->value());
  return true;
}

bool
Dumper::visitOpcode(OpcodeLookup* op)
{
  putCommonOutput(op);
  put("lhs=%Ix receiver=%Ix methodName='%s'\n",
      op->lhs(), op->receiver(), mri::Id(op->methodName()).name());
  return true;
}

bool
Dumper::visitOpcode(OpcodeCall* op)
{
  putCommonOutput(op);
  put("lhs=%Ix methodEntry=%Ix (%d)",
    op->lhs(), op->methodEntry(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  out_ += '\n';
  return true;
}

bool
Dumper::visitOpcode(OpcodePhi* op)
{
  putCommonOutput(op);
  put("lhs=%Ix (%d)", op->lhs(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  out_ += '\n';
  return true;
}

void
Dumper::dumpCfgInfo(const ControlFlowGraph* cfg)
{
  put("[CFG: %Ix]\nentry=%Ix exit=%Ix opcodeCount=%d, output=%Ix\n",
    cfg, cfg->entry(), cfg->exit(), cfg->opcodeCount(), cfg->output());
}

void
Dumper::dumpBlockHeader(BlockHeader* b)
{
  put("BLOCK %d: %Ix\n", b->index(), b);
  b->visitEachOpcode(this);
}

} // anonymous namespace

std::string
ControlFlowGraph::debugPrint() const
{
  Dumper dumper;
  dumper.dumpCfgInfo(this);
  for (std::vector<BlockHeader*>::const_iterator i = blocks_.begin(); i != blocks_.end(); ++i) {
    dumper.dumpBlockHeader(*i);
  }
  return dumper.output();
}

std::string
ControlFlowGraph::debugPrintVariables() const
{
  char buf[256];
  std::string result = "[Variables]\n";
  for (size_t i = 0; i < variables_.size(); ++i) {
    result += variables_[i]->debugPrint();
  }

  return result;
}

RBJIT_NAMESPACE_END
