#include "rbjit/methodinfo.h"
#include "rbjit/cfgbuilder.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/ssatranslator.h"
#include "rbjit/debugprint.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/rubymethod.h"
#include "rbjit/typeanalyzer.h"
#include "rbjit/variable.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/debugprint.h"
#include "rbjit/typecontext.h"
#include "rbjit/inliner.h"

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// MethodInfo

MethodInfo::~MethodInfo()
{
  if (returnType_) {
    returnType_->destroy();
  }
}

void
MethodInfo::addToNativeMethod(mri::Class cls, const char* methodName, unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  me.methodDefinition().setMethodInfo(new MethodInfo(cls, hasDef, hasEval, returnType));
}

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

PrecompiledMethodInfo*
PrecompiledMethodInfo::addToExistingMethod(mri::MethodEntry me)
{
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    return nullptr;
  }

  PrecompiledMethodInfo* mi =
    new PrecompiledMethodInfo(me.class_(), def.astNode(), mri::Id(me.methodName()).name());
  def.setMethodInfo(mi);

  return mi;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::addToExistingMethod(mri::Class cls, ID methodName)
{
  mri::MethodEntry me(cls, methodName);
  return addToExistingMethod(me);
}

void
PrecompiledMethodInfo::buildCfg()
{
  RBJIT_DPRINT(debugPrintBanner("AST"));
  RBJIT_DPRINT(debugPrintAst());

  CfgBuilder builder;
  cfg_ = builder.buildMethod(node_, this);

  RBJIT_DPRINT(debugPrintBanner("CFG building"));
  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());

  LTDominatorFinder domFinder(cfg_);
  domFinder.setDominatorsToCfg();

  RBJIT_DPRINT(debugPrintBanner("SSA translation"));
  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

  cfg_->clearDefInfo();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());
}

void
PrecompiledMethodInfo::analyzeTypes()
{
  if (lock_) {
    return;
  }

  if (!cfg_) {
    buildCfg();
  }

  lock_ = true;

  if (typeContext_) {
    delete typeContext_;
  }

  TypeAnalyzer ta(cfg_);

  // Set self's type
  ta.setInputTypeConstraint(0, TypeClassOrSubclass(cls_));
  typeContext_ = ta.analyze();

  lock_ = false;

  if (returnType_) {
    returnType_->destroy();
  }
  returnType_ = typeContext_->typeConstraintOf(cfg_->output())->independantClone();

  RBJIT_DPRINT(debugPrintBanner("Type analysis"));
  RBJIT_DPRINT(typeContext_->debugPrint());
}

void
PrecompiledMethodInfo::compile()
{
  if (!cfg_) {
    buildCfg();
  }

  analyzeTypes();

  RBJIT_DPRINT(debugPrintBanner("Inlining"));

  Inliner inliner(cfg_, typeContext_);
  inliner.doInlining();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(typeContext_->debugPrint());
  assert(cfg_->checkSanityAndPrintErrors());

  RBJIT_DPRINT(debugPrintBanner("Compilation"));

  methodBody_ = NativeCompiler::instance()->compileMethod(cfg_, typeContext_, methodName_);

  RBJIT_DPRINT(NativeCompiler::instance()->debugPrint());
}

ControlFlowGraph*
PrecompiledMethodInfo::cfg()
{
  if (!cfg_) {
    buildCfg();
  }
  return cfg_;
}

TypeConstraint*
PrecompiledMethodInfo::returnType()
{
  if (returnType_) {
    return returnType_;
  }

  if (lock_) {
    returnType_ = TypeRecursion::create(this);
    return returnType_;
  }

  analyzeTypes();

  return returnType_;
}

////////////////////////////////////////////////////////////
// debugPrintBanner

std::string
PrecompiledMethodInfo::debugPrintBanner(const char* stage) const
{
  return stringFormat(
    "============================================================\n"
    "%s#%s (%Ix) [%s]\n"
    "============================================================\n",
    class_().debugClassName().c_str(), methodName_, this, stage);
}

////////////////////////////////////////////////////////////
// debugPrintAst

// In node.h
extern "C" {
VALUE rb_parser_dump_tree(RNode* node, int);
}

std::string
PrecompiledMethodInfo::debugPrintAst() const
{
  std::string out = stringFormat("[AST %Ix]\n", node_);
  if (node_) {
    mri::String ast = rb_parser_dump_tree(node_, 0);
    std::string s = ast.toString();
    out += s.substr(s.find("\n\n#") + 2, std::string::npos);
    out += '\n';
  }
  else {
    out += "(null)\n";
  }

  return out;
}

RBJIT_NAMESPACE_END
