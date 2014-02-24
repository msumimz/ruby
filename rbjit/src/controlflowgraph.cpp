#include <cstdarg>
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

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
  sprintf(buf_, "[%Ix %Ix %d:%d %s ",
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
  put("index=%d depth=%d idom=%Ix footer=%Ix]\n",
    op->index(), op->depth(), op->idom(), op->footer());
  return true;
}

bool
Dumper::visitOpcode(OpcodeCopy* op)
{
  putCommonOutput(op);
  put("lhs=%Ix rhs=%Ix]\n", op->lhs(), op->rhs());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJump* op)
{
  putCommonOutput(op);
  put("dest=%Ix]\n", op->dest());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJumpIf* op)
{
  putCommonOutput(op);
  put("cond=%Ix ifTrue=%Ix ifFalse=%Ix]\n",
    op->cond(), op->ifTrue(), op->ifFalse());
  return true;
}

bool
Dumper::visitOpcode(OpcodeImmediate* op)
{
  putCommonOutput(op);
  put("lhs=%Ix value=%Ix]\n", op->lhs(), op->value());
  return true;
}

bool
Dumper::visitOpcode(OpcodeCall* op)
{
  putCommonOutput(op);
  put("lhs=%Ix methodName=%Ix (%d)",
    op->lhs(), op->methodName(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  out_ += "]\n";
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
  out_ += "]\n";
  return true;
}

void
Dumper::dumpCfgInfo(const ControlFlowGraph* cfg)
{
  put("CFG\nentry=%Ix exit=%Ix opcodeCount=%Ix output=%Ix\n",
    cfg->entry(), cfg->exit(), cfg->opcodeCount(), cfg->output());
}

void
Dumper::dumpBlockHeader(BlockHeader* b)
{
  put("BLOCK %d: %Ix\n", b->index(), b);
  b->visitEachOpcode(this);
}

} // anonymous namespace

std::string
ControlFlowGraph::debugDump() const
{
  Dumper dumper;
  dumper.dumpCfgInfo(this);
  for (std::vector<BlockHeader*>::const_iterator i = blocks_.begin(); i != blocks_.end(); ++i) {
    dumper.dumpBlockHeader(*i);
  }
  return dumper.output();
}

RBJIT_NAMESPACE_END
