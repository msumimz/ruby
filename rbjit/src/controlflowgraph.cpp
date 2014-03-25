#include <cstdarg>
#include <algorithm>
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/domtree.h"
#include "rbjit/variable.h"
#include "rbjit/rubyobject.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/typeconstraint.h"

#ifdef _x64
# define PTRF "% 16Ix"
# define SPCF "                "
#else
# define PTRF "% 8Ix"
# define SPCF "        "
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// ControlFlowGraph

ControlFlowGraph::ControlFlowGraph()
  : opcodeCount_(0), entry_(0), exit_(0),
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
  Variable* v = Variable::copy(defBlock, defOpcode, variables_.size(), source);
  variables_.push_back(v);
  return v;
}

void
ControlFlowGraph::removeVariables(const std::vector<Variable*>* toBeRemoved)
{
  // Zero-clear elements that should be removed
  for (auto i = toBeRemoved->cbegin(), end = toBeRemoved->cend(); i != end; ++i) {
    variables_[(*i)->index()] = nullptr;
    delete (*i);
  };

  // Remove zero-cleared elements
  variables_.erase(std::remove_if(variables_.begin(), variables_.end(),
    [](Variable* v) { return !v; }), variables_.end());

  // Reset indexes
  for (int i = 0; i < variables_.size(); ++i) {
    variables_[i]->setIndex(i);
  }
}

void
ControlFlowGraph::clearDefInfo()
{
  for (auto i = variables_.begin(), end = variables_.end(); i != end; ++i) {
    (*i)->clearDefInfo();
  };
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
  bool visitOpcode(OpcodeEnv* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodePrimitive* op);
  bool visitOpcode(OpcodePhi* op);
  bool visitOpcode(OpcodeExit* op);

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
  const char* opname;
  if (typeid(*op) == typeid(BlockHeader)) {
    opname = "Block";
  }
  else if (typeid(*op) == typeid(OpcodeImmediate)) {
    opname = "Imm";
  }
  else {
    int skip = strlen("const rbjit::Opcode"); // hopefully optimized out
    opname = typeid(*op).name() + skip;
  }

  sprintf(buf_, "  " PTRF " " PTRF " " PTRF " %d:%d ",
    op, op->prev(), op->next(), op->file(), op->line());
  out_ += buf_;
  if (op->lhs()) {
    sprintf(buf_, PTRF " %-7s", op->lhs(), opname);
  }
  else {
    sprintf(buf_, SPCF " %-7s", opname);
  }
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
  out_ += '\n';
  return true;
}

bool
Dumper::visitOpcode(OpcodeCopy* op)
{
  putCommonOutput(op);
  put("%Ix\n", op->rhs());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJump* op)
{
  putCommonOutput(op);
  put("%Ix\n", op->dest());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJumpIf* op)
{
  putCommonOutput(op);
  put("%Ix %Ix %Ix\n",
    op->cond(), op->ifTrue(), op->ifFalse());
  return true;
}

bool
Dumper::visitOpcode(OpcodeImmediate* op)
{
  putCommonOutput(op);
  put("%Ix\n", op->value());
  return true;
}

bool
Dumper::visitOpcode(OpcodeEnv* op)
{
  putCommonOutput(op);
  out_ += '\n';
  return true;
}

bool
Dumper::visitOpcode(OpcodeLookup* op)
{
  putCommonOutput(op);
  put("%Ix '%s' [%Ix]\n",
    op->receiver(), mri::Id(op->methodName()).name(), op->env());
  return true;
}

bool
Dumper::visitOpcode(OpcodeCall* op)
{
  putCommonOutput(op);
  put("%Ix (%d)",
    op->lookup(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  put(" [%Ix]\n", op->env());
  return true;
}

bool
Dumper::visitOpcode(OpcodePrimitive* op)
{
  putCommonOutput(op);
  put("%s (%d)",
    mri::Id(op->name()).name(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  return true;
}

bool
Dumper::visitOpcode(OpcodePhi* op)
{
  putCommonOutput(op);
  put("(%d)", op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  out_ += '\n';
  return true;
}

bool
Dumper::visitOpcode(OpcodeExit* op)
{
  putCommonOutput(op);
  put("\n");
  return true;
}

void
Dumper::dumpCfgInfo(const ControlFlowGraph* cfg)
{
  put("[CFG: %Ix]\nentry=%Ix exit=%Ix opcodeCount=%d output=%Ix env=%Ix\n",
    cfg, cfg->entry(), cfg->exit(), cfg->opcodeCount(), cfg->output(), cfg->env());
}

void
Dumper::dumpBlockHeader(BlockHeader* b)
{
  put("BLOCK %d: %Ix\n", b->index(), b);
  put("depth=%d footer=%Ix backedges=", b->depth(), b->footer());
  for (BlockHeader::Backedge* e = b->backedge(); e; e = e->next()) {
    put("%Ix ", e->block());
  }
  out_ += '\n';
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
  std::string out = "[Variables]\n";
  for (auto i = variables_.cbegin(), end = variables_.cend(); i != end; ++i) {
    out += (*i)->debugPrint();
  };

  return out;
}

std::string
ControlFlowGraph::debugPrintTypeConstraints() const
{
  char buf[256];
  std::string out = "[Type Constraints]\n";
  for (auto i = variables_.cbegin(), end = variables_.cend(); i != end; ++i) {
    Variable* v = *i;
    sprintf(buf, "%Ix: ", v);
    out += buf;
    if (v->typeConstraint()) {
      out += v->typeConstraint()->debugPrint();
    }
    else {
      out += "(null)";
    }
    out += '\n';
  };

  return out;
}

RBJIT_NAMESPACE_END
