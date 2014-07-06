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
// CMethodInfo

CMethodInfo::~CMethodInfo()
{
  if (returnType_) {
    returnType_->destroy();
  }
}

CMethodInfo*
CMethodInfo::construct(mri::Class cls, const char* methodName, bool mutator, TypeConstraint* returnType)
{
  auto me = cls.findMethod(methodName);
  CMethodInfo* mi = new CMethodInfo(me, mutator, returnType);
  me.setMethodInfo(mi);

  return mi;
}

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

PrecompiledMethodInfo::~PrecompiledMethodInfo()
{
  if (returnType_) {
    returnType_->destroy();
  }
  delete typeContext_;
  delete cfg_;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::construct(mri::MethodEntry me)
{
  if (!me.methodDefinition().hasAstNode()) {
    return nullptr;
  }

  PrecompiledMethodInfo* mi = new PrecompiledMethodInfo(me);
  me.setMethodInfo(mi);

  return mi;
}

PrecompiledMethodInfo*
PrecompiledMethodInfo::construct(mri::Class cls, ID methodName)
{
  mri::MethodEntry me(cls, methodName);
  return construct(me);
}

TypeConstraint*
PrecompiledMethodInfo::returnType()
{
  if (!returnType_) {
    if (lock_) {
      returnType_ = TypeRecursion::create(this);
      return returnType_;
    }
    analyzeTypes();
  }

  return returnType_;
}

RNode*
PrecompiledMethodInfo::astNode() const
{
  if (methodEntry().methodDefinition().hasAstNode()) {
    return methodEntry().methodDefinition().astNode();
  }
  assert(!origDef_.isNull());
  return origDef_.astNode();
}

bool
PrecompiledMethodInfo::isMutator()
{
  return false;
}

void
PrecompiledMethodInfo::compile()
{
  void* code = generateCode();
  assert(code);

  if (origDef_.isNull()) {
    origDef_ = methodEntry().methodDefinition();

    mri::MethodDefinition def(methodEntry().methodName(), code, origDef_.argc());
    methodEntry().setMethodDefinition(def);
  }
}

void
PrecompiledMethodInfo::buildCfg()
{
  {
    CfgBuilder builder;
    cfg_ = builder.buildMethod(astNode(), this);
  }

  RBJIT_DPRINT(debugPrintBanner("AST"));
  RBJIT_DPRINT(debugPrintAst());

  RBJIT_DPRINT(debugPrintBanner("CFG building"));
  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintDotHeader());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());

  {
    LTDominatorFinder domFinder(cfg_);
    domFinder.setDominatorsToCfg();
  }

  RBJIT_DPRINT(debugPrintBanner("SSA translation"));
  RBJIT_DPRINT(cfg_->domTree()->debugPrint());

  SsaTranslator ssa(cfg_, true);
  ssa.translate();

  cfg_->clearDefInfo();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintDotHeader());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
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

  RBJIT_DPRINT(debugPrintBanner("Type analysis"));

  TypeAnalyzer ta(cfg_);

  // Set self's type
  ta.setInputTypeConstraint(0, TypeClassOrSubclass(methodEntry().class_()));
  typeContext_ = ta.analyze();

  lock_ = false;

  if (returnType_) {
    returnType_->destroy();
  }
  returnType_ = typeContext_->typeConstraintOf(cfg_->output())->independantClone();

  RBJIT_DPRINT(typeContext_->debugPrint());
}

void*
PrecompiledMethodInfo::generateCode()
{
  if (!cfg_) {
    buildCfg();
  }

  analyzeTypes();

  RBJIT_DPRINT(debugPrintBanner("Inlining"));

  Inliner inliner(this);
  inliner.doInlining();

  RBJIT_DPRINT(debugPrintBanner("LLVM Compilation"));

  void* code = NativeCompiler::instance()->compileMethod(cfg_, typeContext_, mri::Id(methodEntry().methodName()).name());

  RBJIT_DPRINT(NativeCompiler::instance()->debugPrint());

  return code;
}

////////////////////////////////////////////////////////////
// debugging methods

std::string
PrecompiledMethodInfo::debugPrintBanner(const char* stage) const
{
  return stringFormat(
    "============================================================\n"
    "%s#%s (%Ix) [%s]\n"
    "============================================================\n",
    class_().debugClassName().c_str(), mri::Id(methodName()).name(), this, stage);
}

// In node.h
extern "C" {
VALUE rb_parser_dump_tree(RNode* node, int);
}

std::string
PrecompiledMethodInfo::debugPrintAst() const
{
  RNode* node = astNode();
  std::string out = stringFormat("[AST %Ix]\n", node);
  if (node) {
    mri::String ast = rb_parser_dump_tree(node, 0);
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
