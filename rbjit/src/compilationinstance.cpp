#include "rbjit/compilationinstance.h"
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
#include "rbjit/codeduplicator.h"
#include "rbjit/recompilationmanager.h"

#ifdef RBJIT_DEBUG
#include "rbjit/domtree.h"
#endif

RBJIT_NAMESPACE_BEGIN

CompilationInstance::CompilationInstance(mri::Class holderClass, ID name, RNode* source, mri::CRef cref)
  : holderClass_(holderClass), name_(name), source_(source), cref_(cref),
    cfg_(nullptr), origCfg_(nullptr),
    typeContext_(nullptr), returnType_(nullptr),
    mutator_(UNKNOWN), jitOnly_(UNKNOWN), lock_(false)
{}

CompilationInstance::~CompilationInstance()
{
  if (returnType_) {
    returnType_->destroy();
  }
  delete typeContext_;
  delete cfg_;
}


void
CompilationInstance::reset()
{
  if (!cfg_) {
    return;
  }

  if (origCfg_) {
    delete cfg_;
    cfg_ = origCfg_;
  }

  delete typeContext_;
  typeContext_ = 0;

  returnType_->destroy();
  returnType_ = 0;

  mutator_ = UNKNOWN;
}

ControlFlowGraph*
CompilationInstance::cfg()
{
  buildCfg();
  return cfg_;
}

TypeContext*
CompilationInstance::typeContext()
{
  if (!typeContext_) {
    analyzeTypes();
  }
  assert(typeContext_);
  return typeContext_;
}

TypeConstraint*
CompilationInstance::returnType()
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

bool
CompilationInstance::isMutator()
{
  if (mutator_ == UNKNOWN) {
    if (lock_) {
      // If this is a query from a recursive call, we can safely return false
      // because the overall result would depend on the caller's mutator state,
      // and the caller is itself.
      return false;
    }
    analyzeTypes();
  }

  return mutator_ == YES;
}

bool
CompilationInstance::isJitOnly()
{
  if (jitOnly_ == UNKNOWN) {
    if (lock_) {
      return false;
    }
    analyzeTypes();
  }

  return jitOnly_ == YES;
}

void
CompilationInstance::buildCfg()
{
  if (cfg_) {
    return;
  }

  RBJIT_DPRINT(debugPrintBanner("AST"));
  RBJIT_DPRINT(debugPrintAst());

  {
    CfgBuilder builder;
    cfg_ = builder.buildMethod(source_, name_);
  }

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
  assert(cfg_->checkSsaAndPrintErrors());
}

void
CompilationInstance::analyzeTypes()
{
  if (lock_) {
    return;
  }

  buildCfg();

  lock_ = true;

  if (typeContext_) {
    delete typeContext_;
  }

  RBJIT_DPRINT(debugPrintBanner("Type analysis"));

  TypeAnalyzer ta(cfg_, holderClass_, cref_);

  // Set self's type
  ta.setInputTypeConstraint(0, TypeClassOrSubclass(holderClass_));

  auto result = ta.analyze();
  typeContext_ = std::get<0>(result);
  mutator_ = std::get<1>(result) ? YES : NO;
  jitOnly_ = std::get<2>(result) ? YES : NO;

  lock_ = false;

  if (returnType_) {
    returnType_->destroy();
  }
  returnType_ = typeContext_->typeConstraintOf(cfg_->output())->independantClone();

  RBJIT_DPRINT(typeContext_->debugPrint());
}

void*
CompilationInstance::generateCode(PrecompiledMethodInfo* mi)
{
  buildCfg();

  {
    RBJIT_DPRINT(debugPrintBanner("Code Duplication"));
    CodeDuplicator dup;
    origCfg_ = dup.duplicate(cfg_);
    RBJIT_DPRINT(origCfg_->debugPrint());
    assert(origCfg_->checkSanityAndPrintErrors());
  }

  analyzeTypes();

  {
    RBJIT_DPRINT(debugPrintBanner("Inlining"));
    Inliner inliner(mi);
    inliner.doInlining();
  }

  RBJIT_DPRINT(debugPrintBanner("LLVM Compilation"));

  void* code = NativeCompiler::instance()->compileMethod(mi);

  return code;
}

////////////////////////////////////////////////////////////
// Debugging methods

std::string
CompilationInstance::debugPrintBanner(const char* stage) const
{
  return stringFormat(
    "============================================================\n"
    "%s#%s (%Ix) [%s]\n"
    "============================================================\n",
    holderClass_.debugClassName().c_str(), mri::Id(name_).name(), this, stage);
}

// In node.h
extern "C" {
VALUE rb_parser_dump_tree(RNode* node, int);
}

std::string
CompilationInstance::debugPrintAst() const
{
  RNode* node = source_;
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
