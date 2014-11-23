#include <string>
#include <stdarg.h>
#include "rbjit/cfgsanitychecker.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

CfgSanityChecker::CfgSanityChecker(const ControlFlowGraph* cfg)
  : cfg_(cfg),
    visitedVariables_(cfg->variableCount(), false),
    visitedBlocks_(cfg->blockCount(), false),
    current_(0)
{}

void
CfgSanityChecker::addBlock(Block* block)
{
  if (!visitedBlocks_[block->index()]) {
    work_.push_back(block);
  }
}

bool
CfgSanityChecker::canContinue()
{
  if (errors_.size() >= 10) {
    errors_.push_back(std::string("Too many inconsistencies. Aborted."));
    return false;
  }
  return true;
}

void
CfgSanityChecker::addError(const char* format, ...)
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
CfgSanityChecker::addError(Opcode* op, const char* format, ...)
{
  char buf[256];

  // header
  std::string error = stringFormat("Block %d(%Ix):%s(%Ix): ", current_->index(), current_, op->typeName(), op);

  va_list args;
  va_start(args, format);
  error += stringFormatVarargs(format, args);

  errors_.push_back(error);
}

void
CfgSanityChecker::checkLhs(OpcodeL* op, bool nullable)
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
CfgSanityChecker::checkRhs(OP op, int pos, bool nullable)
{
  Variable* rhs = op->rhs(pos);
  if (!rhs) {
    if (!nullable) {
      addError(op, "rhs variable at %d is null", pos);
    }
  }
  else if (!cfg_->containsVariable(rhs)) {
    addError(op, "rhs variable %Ix does not belong to the cfg", rhs);
  }
}

template <class OP> void
CfgSanityChecker::checkRhs(OP op, bool nullable)
{
  for (int i = 0; i < op->rhsCount(); ++i) {
    checkRhs(op, i, nullable);
  }
}

void
CfgSanityChecker::check()
{
  // Check Block consistency

  int index = 0;
  for (auto i = cfg_->cbegin(), end = cfg_->cend(); i != end; ++i, ++index) {
    Block* block = *i;
    if (!block) {
      addError("block %d is null", index);
      continue;
    }
    if (typeid(*block) != typeid(Block)) {
      addError("block %d is not a Block instance", index);
      continue;
    }
    if (block->index() != index) {
      addError("block %d(%Ix)'s index %d is inconsistent with its position", index, block, block->index());
      continue;
    }
  }

  // Check Variable consistency

  index = 0;
  for (auto i = cfg_->constVariableBegin(), end = cfg_->constVariableEnd(); i != end; ++i, ++index) {
    Variable* v = *i;
    if (!v) {
      addError("variable %d is null", index);
      continue;
    }
    if (typeid(*v) != typeid(Variable)) {
      addError("variable %d is not a Block instance", index);
      continue;
    }
    if (v->index() != index) {
      addError("variable %d(%Ix)'s index %d is inconsistent with its position", index, v, v->index());
      continue;
    }

    // Check an env

    if (OpcodeEnv::isEnv(v)) {
      Opcode* op = v->defOpcode();
      if (!op) {
	addError("variable %d(%Ix)'s defOpcode is null", index, v);
	continue;
      }
      if (typeid(*op) == typeid(OpcodeEnv)) {
	continue;
      }
      if (typeid(*op) == typeid(OpcodeCall)) {
	OpcodeCall* call = static_cast<OpcodeCall*>(op);
	if (call->outEnv() != v) {
	  addError("variable %d(%Ix)'s defOpcode is %Ix, which outEnv is %Ix", index, v, op, call->outEnv());
	}
	continue;
      }
      if (typeid(*op) == typeid(OpcodeCopy)) {
	OpcodeCopy* copy = static_cast<OpcodeCopy*>(op);
	if (!copy->rhs()) {
	  addError("variable %d(%Ix)'s rhs is null", index, v);
	  continue;
	}
	if (!OpcodeEnv::isEnv(copy->rhs())) {
	  addError("variable %d(%Ix) is copied from variable %d(%Ix), which is not an env", index, v, copy->rhs()->index(), copy->rhs());
	  continue;
	}
      }
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
      if (opl->outEnv() != v) {
        addError("env variable %d(%Ix)'s defOpcode is %Ix, but that opcode's env is %Ix",
                 index, v, v->defOpcode(), opl->outEnv());
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

  work_.push_back(cfg_->entryBlock());
  while (!work_.empty()) {
    current_ = work_.back();
    work_.pop_back();

    checkBlock(current_);
  }

  for (auto i = visitedBlocks_.cbegin(), end = visitedBlocks_.cend(); i != end; ++i) {
    if (!*i) {
      int index = std::distance(visitedBlocks_.cbegin(), i);

      current_ = cfg_->block(index);
      addError("referred to by no blocks");
    }
  }
}

bool
CfgSanityChecker::checkBlock(Block* block)
{
  visitedBlocks_[block->index()] =  true;

  OpcodeTerminator* footer = block->footer();
  if (!block->footer()) {
    addError("footer is not a terminator");
  }

  // Check backedge consistency

  for (auto i = block->constBackedgeBegin(), end = block->constBackedgeEnd(); i != end; ++i) {
    Block* b = *i;
    if (!b) {
      addError("Backedge %d's block is null", std::distance(block->constBackedgeBegin(), i));
    }
    else if (!cfg_->containsBlock(b)) {
      addError("Backedge %Ix does not belong to the cfg", b);
    }
    else if (b->nextBlock() != block && b->nextAltBlock() != block) {
      addError("Backedge %d refers to the block %d(%Ix), which has no edges to this block",
	std::distance(block->constBackedgeBegin(), i), b->index(), b);
    }
  }

  // Traverse opcodes

  for (auto i = block->cbegin(), end = block->cend(); i != end; ++i) {
    (*i)->accept(this);
  }


  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeCopy* op)
{
  checkLhs(op, false);
  checkRhs(op, false);
  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeJump* op)
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
CfgSanityChecker::visitOpcode(OpcodeJumpIf* op)
{
  if (!op->nextBlock()) {
    addError(op, "true block is null");
  }
  else if (!cfg_->containsBlock(op->nextBlock())) {
    addError(op, "true block does not belong to the cfg");
  }
  else if (!op->nextBlock()->containsBackedge(current_)) {
    addError(op, "block %Ix has not backedge to %Ix", op->nextBlock(), current_);
    addBlock(op->nextBlock());
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
  else if (!op->nextAltBlock()->containsBackedge(current_)) {
    addError(op, "block %Ix has not backedge to %Ix", op->nextAltBlock(), current_);
    addBlock(op->nextAltBlock());
  }
  else {
    addBlock(op->nextAltBlock());
  }

  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeImmediate* op)
{
  checkLhs(op, false);
  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeEnv* op)
{
  checkLhs(op, false);
  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeLookup* op)
{
  checkLhs(op, false);
  checkRhs(op, false);
  return canContinue();
}

bool
CfgSanityChecker::visitOpcode(OpcodeCall* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeConstant* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodePrimitive* op)
{
  checkLhs(op, true);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodePhi* op)
{
  checkLhs(op, false);
  checkRhs(op, false);

  if (op->rhsCount() <= 1) {
    addError(op, "the number of nodes %d is too small for a phi node", op->rhsCount());
    return true;
  }

  for (auto i = op->begin(), end = op->end() - 1; i < end; ++i) {
    auto j = i + 1;
    if ((*i)->original() != *i && (*j)->original() != *j && (*i)->original() != (*j)->original()) {
      addError(op, "The original variable of operand %Ix and %Ix does not match", *i, *j);
    }
    if (*i != cfg_->undefined() && *j != cfg_->undefined() &&(*i)->nameRef() != (*j)->nameRef()) {
      addError(op, "The name reference of operand %Ix and %Ix does not match", *i, *j);
    }
  }

  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeExit* op)
{
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeArray* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeRange* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeString* op)
{
  checkLhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeHash* op)
{
  checkLhs(op, true);
  checkRhs(op, false);
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeEnter* op)
{
  if (!op->scope()) {
    addError(op, "scope is null");
  }
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeLeave* op)
{
  return true;
}

bool
CfgSanityChecker::visitOpcode(OpcodeCheckArg* op)
{
  // TODO
  return true;
}

RBJIT_NAMESPACE_END
