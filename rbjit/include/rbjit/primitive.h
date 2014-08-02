#pragma once
#include <vector>
#include <unordered_map>
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

namespace llvm {
class Value;
class Context;
}

RBJIT_NAMESPACE_BEGIN

class IRBuilder;

class TypeConstraint;

class Primitive {
public:

  llvm::Value* emit(IRBuilder*, const std::vector<llvm::Value*>& args);

  virtual int argCount() = 0;
  virtual TypeConstraint* returnType() = 0;

  static void setup();
  static Primitive* find(ID name);

protected:

  llvm::Value* getValue(IRBuilder* builder, size_t value);
  llvm::Value* getValue(IRBuilder* builder, mri::Object value);
  llvm::Value* getTrueOrFalseObject(IRBuilder* builder, llvm::Value* i1);
  void checkArgCount(int count);

  virtual llvm::Value* emitInternal(IRBuilder*, const std::vector<llvm::Value*>& args) = 0;

  static std::unordered_map<ID, Primitive*> prims_;
};

RBJIT_NAMESPACE_END
