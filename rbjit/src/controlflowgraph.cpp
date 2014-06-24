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
  : entry_(0), exit_(0),
    requiredArgCount_(0), hasOptionalArg_(false), hasRestArg_(false),
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
ControlFlowGraph::createVariable(ID name, BlockHeader* defBlock, Opcode* defOpcode)
{
  Variable* v = new Variable(defBlock, defOpcode, name, 0, variables_.size(), 0);
  variables_.push_back(v);
  return v;
}

Variable*
ControlFlowGraph::createVariableSsa(ID name, BlockHeader* defBlock, Opcode* defOpcode)
{
  Variable* v = new Variable(defBlock, defOpcode, name, 0, variables_.size(), new DefInfo());
  variables_.push_back(v);
  return v;
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
  op->unlink();
  delete op;
}

void
ControlFlowGraph::removeOpcode(Opcode* op)
{
  op->unlink();
  delete op;
}

BlockHeader*
ControlFlowGraph::splitBlock(BlockHeader* block, Opcode* op, bool discardOpcode, bool addJump)
{
  op->unlink();

  BlockHeader* latter = new BlockHeader(op->file(), op->line(), 0, 0, blocks_.size(), block->depth(), 0);
  latter->linkToNext(op->next());
  latter->setFooter(block->footer());
  blocks_.push_back(latter);

  if (addJump) {
    OpcodeJump* jump;
    if (discardOpcode) {
      jump = new OpcodeJump(op->file(), op->line(), op->prev(), 0);
    }
    else {
      jump = new OpcodeJump(op->file(), op->line(), op, 0);
    }
    block->setFooter(jump);
  }
  else {
    block->setFooter(op);
  }

  // Update the defBlocks of the variables in the latter block
  Opcode* o = latter;
  Opcode* footer = latter->footer();
  do {
    OpcodeL* opl = dynamic_cast<OpcodeL*>(o);
    if (opl) {
      opl->lhs()->setDefBlock(latter);
      OpcodeCall* opc = dynamic_cast<OpcodeCall*>(o);
      if (opc) {
        opc->env()->setDefBlock(latter);
      }
    }
    o = o->next();
  } while (o && o != footer);

  // Update backedges
  OpcodeJump* jump = dynamic_cast<OpcodeJump*>(footer);
  if (jump) {
    latter->nextBlock()->updateBackedge(block, latter);
  }
  else {
    OpcodeJumpIf* jumpIf = dynamic_cast<OpcodeJumpIf*>(footer);
    if (jumpIf) {
      latter->nextBlock()->updateBackedge(block, latter);
      latter->nextAltBlock()->updateBackedge(block, latter);
    }
  }

  return latter;
}

BlockHeader*
ControlFlowGraph::insertEmptyBlockAfter(BlockHeader* block)
{
  OpcodeJump* footer = static_cast<OpcodeJump*>(block->footer());
  assert(typeid(*footer) == typeid(OpcodeJump));

  BlockHeader* newBlock = new BlockHeader(footer->file(), footer->line(), footer, block, blocks_.size(), block->depth(), block);
  newBlock->setFooter(newBlock);
  blocks_.push_back(newBlock);

  block->updateJumpDestination(newBlock);

  return newBlock;
}

////////////////////////////////////////////////////////////
// Debugging tool: sanity checker

namespace {

class SanityChecker : public OpcodeVisitor {
public:

  SanityChecker(const ControlFlowGraph* cfg);

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

  void check();

  const std::vector<std::string>& errors() const { return errors_; }

private:

  void addBlock(BlockHeader* block);
  bool canContinue();
  void addError(const char* format, ...);
  void addError(Opcode* op, const char* format, ...);

  void checkLhs(OpcodeL* op, bool nullable);
  template <class OP> void checkRhs(OP op, bool nullable);

  const ControlFlowGraph* cfg_;
  std::vector<bool> visitedVariables_;
  std::vector<bool> visitedBlocks_;

  std::vector<BlockHeader*> work_;
  BlockHeader* current_;

  std::vector<std::string> errors_;
};

SanityChecker::SanityChecker(const ControlFlowGraph* cfg)
  : cfg_(cfg),
    visitedVariables_(cfg->variables()->size(), false),
    visitedBlocks_(cfg->blocks()->size(), false),
    current_(0)
{}

void
SanityChecker::addBlock(BlockHeader* block)
{
  if (!visitedBlocks_[block->index()]) {
    work_.push_back(block);
  }
}

bool
SanityChecker::canContinue()
{
  if (errors_.size() >= 10) {
    errors_.push_back(std::string("Too many inconsistencies. Aborted."));
    return false;
  }
  return true;
}

void
SanityChecker::addError(const char* format, ...)
{
  // header
  std::string error;
  if (current_) {
    error = stringFormat("Block %d(%Ix): ", current_->index(), current_);
  }

  va_list args;
  va_start(args, format);
  error += stringFormatVarargs(format, args);

  errors_.push_back(error);
}

void
SanityChecker::addError(Opcode* op, const char* format, ...)
{
  char buf[256];

  // header
  std::string error = stringFormat("Block %d(%Ix):%s: ", current_->index(), current_, op->typeName());

  va_list args;
  va_start(args, format);
  error += stringFormatVarargs(format, args);

  errors_.push_back(error);
}

void
SanityChecker::checkLhs(OpcodeL* op, bool nullable)
{
  if (!op->lhs()) {
    if (!nullable) {
      addError(op, "lhs is null");
    }
  }
  else if (!cfg_->containsVariable(op->lhs())) {
    addError(op, "lhs variable does not belong to the cfg");
  }
}

template <class OP> void
SanityChecker::checkRhs(OP op, bool nullable)
{
  for (auto i = op->rhsBegin(), end = op->rhsEnd(); i < end; ++i) {
    if (!*i) {
      if (!nullable) {
        addError(op, "rhs variable at %d is null", i - op->rhsBegin());
      }
    }
    else if (!cfg_->containsVariable(*i)) {
      addError(op, "rhs variable %d does not belong to the cfg");
    }
  }
}

bool
SanityChecker::visitOpcode(BlockHeader* op)
{
  visitedBlocks_[op->index()] =  true;

  if (op->depth() < 0) {
    addError(op, "depth is %d, a negative number", op->depth());
  }
  if (op->idom() && !cfg_->containsBlock(op->idom())) {
    addError(op, "idom does not belong to the cfg");
  }

  Opcode* footer = op->footer();
  if (!op->footer()) {
    addError(op, "footer is null", op);
  }
  else if (!footer->isTerminator()) {
    addError(op, "footer is not a terminator");
  }
  else {
    Opcode* o;
    for (o = op->next(); o && !o->isTerminator(); o = o->next())
      ;
    if (!o) {
      addError(op, "footer is null");
    }
    else if (o != footer) {
      addError(op, "footer %Ix is different from the actual footer %Ix", footer, o);
    }
  }

  // Check backedge consistency

  if (op->backedge()->block()) {
    for (const BlockHeader::Backedge* edge = op->backedge(); edge; edge = edge->next()) {
      BlockHeader* block = edge->block();
      if (!edge->block()) {
        addError(op, "Backedge %Ix's block is null");
      }
      else if (!cfg_->containsBlock(edge->block())) {
        addError(op, "Backedge %Ix does not belong to the cfg");
      }
      else if (block->footer()->nextBlock() != op && block->footer()->nextAltBlock() != op) {
        addError(op, "Backedge %Ix refers to the block %d(%Ix), which has no edges to this block", edge, block->index(), block);
      }
    }
  }

  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeCopy* op)
{
  checkLhs(op, false);
  checkRhs(op, false);
  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeJump* op)
{
  if (!op->nextBlock()) {
    addError(op, "next block is null");
  }
  else if (!cfg_->containsBlock(op->nextBlock())) {
    addError(op, "next block does not belong to the cfg");
  }
  else {
    addBlock(op->nextBlock());
  }

  if (op->nextAltBlock()) {
    addError(op, "next alt block is defined");
  }

  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeJumpIf* op)
{
  if (!op->nextBlock()) {
    addError(op, "true block is null");
  }
  else if (!cfg_->containsBlock(op->nextBlock())) {
    addError(op, "true block does not belong to the cfg");
  }
  else {
    addBlock(op->nextBlock());
  }

  if (!op->nextAltBlock()) {
    addError(op, "false block is null");
  }
  else if (!cfg_->containsBlock(op->nextAltBlock())) {
    addError(op, "false block does not belong to the cfg");
  }
  else {
    addBlock(op->nextAltBlock());
  }

  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeImmediate* op)
{
  checkLhs(op, false);
  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeEnv* op)
{
  checkLhs(op, false);
  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeLookup* op)
{
  checkLhs(op, false);
  checkRhs(op, false);
  return canContinue();
}

bool
SanityChecker::visitOpcode(OpcodeCall* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
SanityChecker::visitOpcode(OpcodePrimitive* op)
{
  checkLhs(op, true);
  return true;
}

bool
SanityChecker::visitOpcode(OpcodePhi* op)
{
  checkLhs(op, false);
  checkRhs(op, false);
  return true;
}

bool
SanityChecker::visitOpcode(OpcodeExit* op)
{
  return true;
}

void
SanityChecker::check()
{
  // Check BlockHeader consistency

  int index = 0;
  for (auto i = cfg_->blocks()->cbegin(), end = cfg_->blocks()->cend(); i != end; ++i, ++index) {
    BlockHeader* block = *i;
    if (!block) {
      addError("block %d is null", index);
      continue;
    }
    if (typeid(*block) != typeid(BlockHeader)) {
      addError("block %d is not a BlockHeader instance", index);
      continue;
    }
    if (block->index() != index) {
      addError("block %d(%Ix)'s index %d is inconsistent with its position", index, block, block->index());
      continue;
    }
  }

  // Check Variable consistency

  index = 0;
  for (auto i = cfg_->variables()->cbegin(), end = cfg_->variables()->cend(); i != end; ++i, ++index) {
    Variable* v = *i;
    if (!v) {
      addError("variable %d is null", index);
      continue;
    }
    if (typeid(*v) != typeid(Variable)) {
      addError("variable %d is not a BlockHeader instance", index);
      continue;
    }
    if (v->index() != index) {
      addError("variable %d(%Ix)'s index %d is inconsistent with its position", index, v, v->index());
      continue;
    }

    // Check defOpcode

    if (!v->defOpcode()) {
//      if (!cfg_->containsInInputs(v)) {
//        addError("variable %d(%Ix)'s defOpcode is null", index, v);
//      }
      continue;
    }

    OpcodeL* opl = dynamic_cast<OpcodeL*>(v->defOpcode());
    if (!opl) {
      addError("variable %d(%Ix)'s defOpcode %Ix is not OpcodeL", index, v, v->defOpcode());
      continue;
    }

    if (opl->lhs() != v) {
      OpcodeCall* opc = dynamic_cast<OpcodeCall*>(opl);
      if (!opc || opc->env() != v) {
        addError("variable %d(%Ix)'s defOpcode is %Ix, but that opcode's lhs variable is %Ix",
	index, v, v->defOpcode(), opl->lhs());
        continue;
      }
    }

    if (!v->defBlock()->containsOpcode(opl)) {
      addError("variable %d(%Ix)'s defOpcode is %Ix and defBlock is %Ix, but that block does not contain such an opcode",
	index, v, opl, v->defBlock());
      continue;
    }
  }

  if (!errors_.empty()) {
    return;
  }

  // Traverse opcodes

  work_.push_back(cfg_->entry());
  while (!work_.empty()) {
    current_ = work_.back();
    work_.pop_back();

    Opcode* op = current_;
    Opcode* prev = op->prev();
    for (; op; prev = op, op = op->next()) {
      if (op->prev() != prev) {
        addError(op, "prev() is different from the acutual previous opcode");
      }
      bool result = op->accept(this);
      if (!result || op == current_->footer()) {
        break;
      }
    }
  }

  for (auto i = visitedBlocks_.cbegin(), end = visitedBlocks_.cend(); i != end; ++i) {
    if (!*i) {
      int index = std::distance(visitedBlocks_.cbegin(), i);

      current_ = (*cfg_->blocks())[index];
      addError("referred to by no blocks");
    }
  }
}

} // anonymous namespace

bool
ControlFlowGraph::checkSanity() const
{
  SanityChecker checker(this);
  checker.check();

  return checker.errors().empty();
}

bool
ControlFlowGraph::checkSanityAndPrintErrors() const
{
  SanityChecker checker(this);
  checker.check();

  for (auto i = checker.errors().cbegin(), end = checker.errors().cend(); i != end; ++i) {
    fprintf(stderr, "%s\n", i->c_str());
  }

  return checker.errors().empty();
}

////////////////////////////////////////////////////////////
// Debugging tool: printing cfg

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

  void dumpAsDot(const ControlFlowGraph* cfg);

private:

  static std::string getOpcodeShortName(Opcode* op);
  void putCommonOutput(Opcode* op);
  void put(const char* format, ...);

  void dumpBlockHeaderAsDot(BlockHeader* b);

  std::string out_;
};

std::string
Dumper::getOpcodeShortName(Opcode* op)
{
  const char* opname;
  if (typeid(*op) == typeid(BlockHeader)) {
    opname = "Block";
  }
  else if (typeid(*op) == typeid(OpcodeImmediate)) {
    opname = "Imm";
  }
  else if (typeid(*op) == typeid(OpcodePrimitive)) {
    opname = "Prim";
  }
  else {
    int skip = strlen("const rbjit::Opcode"); // hopefully optimized out
    opname = typeid(*op).name() + skip;
  }

  return std::string(opname);
}

void
Dumper::putCommonOutput(Opcode* op)
{
  std::string opname = getOpcodeShortName(op);

  out_ += stringFormat("  " PTRF " " PTRF " " PTRF " %d:%d ",
    op, op->prev(), op->next(), op->file(), op->line());
  if (op->lhs()) {
    out_ += stringFormat(PTRF " %-7s", op->lhs(), opname.c_str());
  }
  else {
    out_ += stringFormat(SPCF " %-7s", opname.c_str());
  }
}

void
Dumper::put(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  out_ += stringFormatVarargs(format, args);
}

bool
Dumper::visitOpcode(BlockHeader* op)
{
  if (!op->footer()) {
    put("<fooer is null>");
    return false;
  }
  return true;
}

bool
Dumper::visitOpcode(OpcodeCopy* op)
{
  put("%Ix", op->rhs());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJump* op)
{
  put("%Ix", op->dest());
  return true;
}

bool
Dumper::visitOpcode(OpcodeJumpIf* op)
{
  put("%Ix %Ix %Ix",
    op->cond(), op->ifTrue(), op->ifFalse());
  return true;
}

bool
Dumper::visitOpcode(OpcodeImmediate* op)
{
  put("%Ix", op->value());
  return true;
}

bool
Dumper::visitOpcode(OpcodeEnv* op)
{
  return true;
}

bool
Dumper::visitOpcode(OpcodeLookup* op)
{
  put("%Ix '%s' [%Ix]",
    op->receiver(), mri::Id(op->methodName()).name(), op->env());
  return true;
}

bool
Dumper::visitOpcode(OpcodeCall* op)
{
  put("%Ix (%d)",
    op->lookup(), op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  put(" [%Ix]", op->env());
  return true;
}

bool
Dumper::visitOpcode(OpcodePrimitive* op)
{
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
  put("(%d)", op->rhsCount());
  for (Variable*const* i = op->rhsBegin(); i < op->rhsEnd(); ++i) {
    put(" %Ix", *i);
  }
  return true;
}

bool
Dumper::visitOpcode(OpcodeExit* op)
{
  return true;
}

void
Dumper::dumpCfgInfo(const ControlFlowGraph* cfg)
{
  put("[CFG: %Ix]\nentry=%Ix exit=%Ix output=%Ix env=%Ix\n",
    cfg, cfg->entry(), cfg->exit(), cfg->output(), cfg->env());
}

void
Dumper::dumpBlockHeader(BlockHeader* b)
{
  put("BLOCK %d: %Ix (%s)\n", b->index(), b, b->debugName());
  put("depth=%d footer=%Ix idom=%Ix backedges=",
    b->depth(), b->footer(), b->idom());
  for (BlockHeader::Backedge* e = b->backedge(); e; e = e->next()) {
    put("%Ix ", e->block());
  }
  out_ += '\n';

  Opcode* op = b;
  Opcode* footer = b->footer();
  do {
    putCommonOutput(op);
    bool ok = op->accept(this);
    out_ += '\n';
    if (!ok || op == footer) {
      break;
    }
    op = op->next();
  } while (op);
}

void
Dumper::dumpBlockHeaderAsDot(BlockHeader* b)
{
  // block
  put("BLOCK%d [label=\"BLOCK %d (%s)\\l", b->index(), b->index(), b->debugName());

  Opcode* op = b;
  Opcode* footer = b->footer();
  do {
    if (op->lhs()) {
      out_ += stringFormat(PTRF " " PTRF " ", op, op->lhs());
    }
    else {
      out_ += stringFormat(PTRF " " SPCF " ", op);
    }
    out_ += getOpcodeShortName(op);
    out_ += ' ';
    op->accept(this);
    out_ += "\\l";
    if (op == footer) {
      break;
    }
    op = op->next();
  } while (op);

  put("\"]\n");

  // edges
  if (typeid(*footer) == typeid(OpcodeJump)) {
    put("BLOCK%d -> BLOCK%d\n", b->index(), b->nextBlock()->index());
  }
  else if (typeid(*footer) == typeid(OpcodeJumpIf)) {
    put("BLOCK%d -> BLOCK%d [label=\"true\"]\n", b->index(), b->nextBlock()->index());
    put("BLOCK%d -> BLOCK%d [label=\"false\"]\n", b->index(), b->nextAltBlock()->index());
  }
}

void
Dumper::dumpAsDot(const ControlFlowGraph* cfg)
{
  std::string cfgInfo = stringFormat("entry=%Ix exit=%Ix output=%Ix env=%Ix",
    cfg->entry(), cfg->exit(), cfg->output(), cfg->env());
  out_ = "digraph {\n"
    "graph [label=\"" + cfgInfo + "\" labelloc=t labeljust=l fontname=\"Consolas\"]\n"
    "node [shape=box fontname=\"Consolas\"]\n";
  for (std::vector<BlockHeader*>::const_iterator i = cfg->blocks()->cbegin(); i != cfg->blocks()->cend(); ++i) {
    dumpBlockHeaderAsDot(*i);
  }
  out_ += "}\n";
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
ControlFlowGraph::debugPrintAsDot() const
{
  Dumper dumper;
  dumper.dumpAsDot(this);
  return dumper.output();
}

std::string
ControlFlowGraph::debugPrintBlock(BlockHeader* block) const
{
  std::string out = stringFormat("[Block %Ix (CFG: %Ix)]\n", block, this);
  Dumper dumper;
  dumper.dumpBlockHeader(block);
  out += dumper.output();
  return out;
}

std::string
ControlFlowGraph::debugPrintVariables() const
{
  std::string out = "[Inputs]\n";
  for (auto i = inputs_.cbegin(), end = inputs_.cend(); i != end; ++i) {
    out += (*i)->debugPrint();
  };

  out += "[Variables]\n";
  for (auto i = variables_.cbegin(), end = variables_.cend(); i != end; ++i) {
    out += (*i)->debugPrint();
  };

  return out;
}

RBJIT_NAMESPACE_END
