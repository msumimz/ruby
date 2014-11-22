#include <cstdarg>
#include <string>
#include "rbjit/blockdebugprinter.h"
#include "rbjit/block.h"

#ifdef _x64
# define PTRF "% 16Ix"
# define SPCF "                "
#else
# define PTRF "% 8Ix"
# define SPCF "        "
#endif

RBJIT_NAMESPACE_BEGIN

void
BlockDebugPrinter::putCommonOutput(Opcode* op)
{
  std::string opname = op->shortTypeName();

  out_ += stringFormat(PTRF " " PTRF " ", op, op->sourceLocation());
  if (op->lhs()) {
    out_ += stringFormat(PTRF " %-7s", op->lhs(), opname.c_str());
  }
  else {
    out_ += stringFormat(SPCF " %-7s", opname.c_str());
  }
}

void
BlockDebugPrinter::put(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  out_ += stringFormatVarargs(format, args);
}

std::string
BlockDebugPrinter::print(Block* b)
{
  out_.clear();

  put("BLOCK %d: %Ix (%s)\n", b->index(), b, b->debugName());
  put("backedges=");
  for (auto i = b->backedgeBegin(), end = b->backedgeEnd(); i != end; ++i) {
    put("%Ix ", *i);
  }
  out_ += '\n';

  for (auto i = b->begin(), end = b->end(); i != end; ++i) {
    putCommonOutput(*i);
    bool ok = (*i)->accept(this);
    out_ += '\n';
    if (!ok) {
      break;
    }
  }

  return out_;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeCopy* op)
{
  put("%Ix", op->rhs());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeJump* op)
{
  put("%Ix", op->nextBlock());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeJumpIf* op)
{
  put("%Ix %Ix %Ix",
    op->cond(), op->nextBlock(), op->nextAltBlock());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeImmediate* op)
{
  put("%Ix", op->value());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeEnv* op)
{
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeLookup* op)
{
  put("%Ix '%s' [%Ix]",
    op->receiver(), mri::Id(op->methodName()).name(), op->env());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeCall* op)
{
  put("%Ix (%d)",
    op->lookup(), op->rhsCount());
  for (Variable*const* i = op->begin(); i < op->end(); ++i) {
    put(" %Ix", *i);
  }
  put(" [%Ix]", op->outEnv());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeConstant* op)
{
  put("%Ix '%s' %s[%Ix <- %Ix]",
    op->base(), mri::Id(op->name()).name(),
    op->toplevel() ? "toplevel " : "",
    op->outEnv(), op->inEnv());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodePrimitive* op)
{
  put("%s (%d)",
    mri::Id(op->name()).name(), op->rhsCount());
  for (Variable*const* i = op->begin(); i < op->end(); ++i) {
    put(" %Ix", *i);
  }
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodePhi* op)
{
  put("(%d)", op->rhsCount());
  for (Variable*const* i = op->begin(); i < op->end(); ++i) {
    put(" %Ix", *i);
  }
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeExit* op)
{
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeArray* op)
{
  put("(%d)", op->rhsCount());
  for (Variable*const* i = op->begin(); i < op->end(); ++i) {
    put(" %Ix", *i);
  }
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeRange* op)
{
  put("%Ix %Ix %s",
    op->low(), op->high(), op->exclusive() ? "excl" : "incl");
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeString* op)
{
  put("'%s'", mri::String(op->string()).toCStr());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeHash* op)
{
  put("(%d)", op->rhsCount());
  for (Variable*const* i = op->begin(); i < op->end() - 1; i += 2) {
    put(" %Ix => %Ix", *i, *(i + 1));
  }
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeEnter* op)
{
  put("%Ix", op->scope());
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeLeave* op)
{
  return true;
}

bool
BlockDebugPrinter::visitOpcode(OpcodeCheckArg* op)
{
  // TODO
  return true;
}

std::string
BlockDebugPrinter::printAsDot(Block* b)
{
  out_.clear();

  // block
  put("BLOCK%d [label=\"BLOCK %d (%s)\\l", b->index(), b->index(), b->debugName());

  for (auto i = b->begin(), end = b->end(); i != end; ++i) {
    Opcode* op = *i;
    if (op->lhs()) {
      out_ += stringFormat(PTRF " " PTRF " ", op, op->lhs());
    }
    else {
      out_ += stringFormat(PTRF " " SPCF " ", op);
    }
    out_ += op->shortTypeName();
    out_ += ' ';
    op->accept(this);
    out_ += "\\l";
  }

  put("\"]\n");

  // edges
  OpcodeTerminator* footer = b->footer();
  if (typeid(*footer) == typeid(OpcodeJump)) {
    put("BLOCK%d -> BLOCK%d\n", b->index(), b->nextBlock()->index());
  }
  else if (typeid(*footer) == typeid(OpcodeJumpIf)) {
    put("BLOCK%d -> BLOCK%d [label=\"true\"]\n", b->index(), b->nextBlock()->index());
    put("BLOCK%d -> BLOCK%d [label=\"false\"]\n", b->index(), b->nextAltBlock()->index());
  }

  return out_;
}

RBJIT_NAMESPACE_END
