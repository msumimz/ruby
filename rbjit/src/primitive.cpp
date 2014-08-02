#include <stdarg.h>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "rbjit/primitive.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/idstore.h"
#include "rbjit/runtimefunctions.h"

RBJIT_NAMESPACE_BEGIN

class IRBuilder: public llvm::IRBuilder<false> {};

////////////////////////////////////////////////////////////
// Helper functions

static llvm::Function*
getIntrinsic(IRBuilder* builder, llvm::Intrinsic::ID name, int typeCount)
{
  llvm::Module* mod = builder->GetInsertBlock()->getParent()->getParent();
  std::vector<llvm::Type*> argTypes(typeCount, llvm::Type::getIntNTy(builder->getContext(), sizeof(VALUE) * CHAR_BIT));
  return llvm::Intrinsic::getDeclaration(mod, name, argTypes);
}

////////////////////////////////////////////////////////////
// Primitive

llvm::Value*
Primitive::getValue(IRBuilder* builder, size_t value)
{
  return llvm::ConstantInt::get(builder->getContext(), llvm::APInt(sizeof(VALUE) * CHAR_BIT, value));
}

llvm::Value*
Primitive::getValue(IRBuilder* builder, mri::Object value)
{
  return getValue(builder, value.value());
}

llvm::Value*
Primitive::getTrueOrFalseObject(IRBuilder* builder, llvm::Value* i1)
{
  return builder->CreateSelect(
    i1,
    getValue(builder, mri::Object::trueObject()),
    getValue(builder, mri::Object::falseObject()));
}

llvm::Value*
Primitive::getFunction(IRBuilder* builder, void* func, int argCount, bool isVarArg)
{
  llvm::Type* type = llvm::Type::getIntNTy(builder->getContext(), sizeof(VALUE) * CHAR_BIT);
  std::vector<llvm::Type*> paramTypes(argCount, type);
  llvm::FunctionType* ft = llvm::FunctionType::get(type, paramTypes, isVarArg);
  return builder->CreateIntToPtr(getValue(builder, func), ft->getPointerTo(0));
}

void
Primitive::checkArgCount(int count)
{
  int c = argCount();
  if (c != -1 && c != count) {
    throw std::runtime_error("wrong number of arguments");
  }
}

llvm::Value*
Primitive::emit(IRBuilder* builder, const std::vector<llvm::Value*>& args)
{
  checkArgCount(args.size());
  return emitInternal(builder, args);
}

////////////////////////////////////////////////////////////
// PrimitiveTester

class PrimitiveTester : public Primitive {
public:

  int argCount() const { return 1; }

  TypeConstraint* returnType() const
  {
    static TypeConstraint* type = TypeSelection::create(
      TypeConstant::create(mri::Object::trueObject()),
      TypeConstant::create(mri::Object::falseObject()));
    return type;
  }

};

class PrimitiveComparer : public Primitive {
public:

  int argCount() const { return 2; }

  TypeConstraint* returnType() const
  {
    static TypeConstraint* type = TypeSelection::create(
      TypeConstant::create(mri::Object::trueObject()),
      TypeConstant::create(mri::Object::falseObject()));
    return type;
  }

};

class PrimitiveBitwiseBinaryOperator : public Primitive {
public:

  int argCount() const { return 2; }

  TypeConstraint* returnType() const
  {
    static TypeConstraint* type = TypeAny::create();
    return type;
  }

};

////////////////////////////////////////////////////////////
// PrimiveTest: rbjit__test, rbjit__test_not
//
class PrimitiveTest : public PrimitiveTester {
public:

  PrimitiveTest(bool positive) : positive_(positive) {}

  // define %VALUE @rbjit__test(%VALUE %v) nounwind readnone alwaysinline {
  //   %rtest = and %VALUE %v, xor (%VALUE 4, %VALUE -1)
  //   %cmp = icmp ne %VALUE %rtest, 0
  //   %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  //   ret %VALUE %ret
  // }
  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* rtest = builder->CreateAnd(args[0], getValue(builder, ~(size_t)mri::Object::nilObject()));
    llvm::Value* cmp;
    if (!positive_) {
      cmp = builder->CreateICmpEQ(rtest, getValue(builder, 0));
    }
    else {
      cmp = builder->CreateICmpNE(rtest, getValue(builder, 0));
    }
    return getTrueOrFalseObject(builder, cmp);
  }

private:

  bool positive_;
};

////////////////////////////////////////////////////////////
// PrimiveAdd: rbjit__bitwise_add
//
class PrimitiveAdd : public PrimitiveBitwiseBinaryOperator {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    return builder->CreateAdd(args[0], args[1]);
  }
};

////////////////////////////////////////////////////////////
// PrimiveSub: rbjit__bitwise_add
//
class PrimitiveSub : public PrimitiveBitwiseBinaryOperator {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    return builder->CreateSub(args[0], args[1]);
  }
};

////////////////////////////////////////////////////////////
// PrimiveOpOverflow: rbjit__bitwise_add_overflow, rbjit__bitwise_sub_overflow
//
class PrimitiveOpOverflow : public PrimitiveBitwiseBinaryOperator {
public:

  PrimitiveOpOverflow(bool add) : add_(add) {}

  // %result = call {%VALUE, i1} @llvm.sadd.with.overflow.i32(%VALUE %v1, %VALUE %v2)
  // %overflow = extractvalue {%VALUE, i1} %result, 1
  // %ret = select i1 %overflow, %VALUE 2, %VALUE 0
  // ret %VALUE %ret
  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Function* f = getIntrinsic(builder, add_ ? llvm::Intrinsic::sadd_with_overflow : llvm::Intrinsic::ssub_with_overflow, 1);
    llvm::Value* add = builder->CreateCall(f, args);
    std::vector<unsigned> idx(1, 1);
    llvm::Value* overflow = builder->CreateExtractValue(add, idx);
    return getTrueOrFalseObject(builder, overflow);
  }

private:

  bool add_;
};

class PrimitiveCmp : public PrimitiveComparer {
public:

  PrimitiveCmp(llvm::CmpInst::Predicate pred) : pred_(pred) {}

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* cmp = builder->CreateICmp(pred_, args[0], args[1]);
    return getTrueOrFalseObject(builder, cmp);
  }

private:

  llvm::CmpInst::Predicate pred_;
};

////////////////////////////////////////////////////////////
// PrimitiveConvertToArray : rbjit__convert_to_array

class PrimitiveConvertToArray : public Primitive {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* f = getFunction(builder, (void*)rbjit_convertToArray, 1, false);
    return builder->CreateCall(f, args);
  }

  int argCount() const { return 1; }

  TypeConstraint* returnType() const
  { return TypeExactClass::create(mri::Class::arrayClass()); }

};

////////////////////////////////////////////////////////////
// PrimitiveConcatArrays : rbjit__concat_arrays

class PrimitiveConcatArrays : public Primitive {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* f = getFunction(builder, (void*)rbjit_concatArrays, 2, false);
    return builder->CreateCall(f, args);
  }

  int argCount() const { return 2; }

  TypeConstraint* returnType() const
  {
    return TypeExactClass::create(mri::Class::arrayClass());
  }

};

////////////////////////////////////////////////////////////
// PrimitivePushToArray : rbjit__push_to_array

class PrimitivePushToArray : public Primitive {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* f = getFunction(builder, (void*)rbjit_pushToArray, 2, false);
    return builder->CreateCall(f, args);
  }

  int argCount() const { return 2; }

  TypeConstraint* returnType() const
  {
    return TypeExactClass::create(mri::Class::arrayClass());
  }

};

////////////////////////////////////////////////////////////
// PrimitiveConvertToString : rbjit__convert_to_string

class PrimitiveConvertToString : public Primitive {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* f = getFunction(builder, (void*)rbjit_convertToString, 1, false);
    return builder->CreateCall(f, args);
  }

  int argCount() const { return 1; }

  TypeConstraint* returnType() const
  {
    return TypeExactClass::create(mri::Class::stringClass());
  }

};

////////////////////////////////////////////////////////////
// PrimitiveConcatStrings : rbjit__concat_strings

class PrimitiveConcatStrings : public Primitive {
public:

  llvm::Value* emitInternal(IRBuilder* builder, const std::vector<llvm::Value*>& args)
  {
    llvm::Value* f = getFunction(builder, (void*)rbjit_concatStrings, 1, true);
    return builder->CreateCall(f, args);
  }

  int argCount() const { return -1; }

  TypeConstraint* returnType() const
  {
    return TypeExactClass::create(mri::Class::stringClass());
  }

};

////////////////////////////////////////////////////////////
// Static methods

std::unordered_map<ID, Primitive*> Primitive::prims_;

void
Primitive::setup()
{
  prims_[IdStore::get(ID_rbjit__test)] = new PrimitiveTest(true);
  prims_[IdStore::get(ID_rbjit__test_not)] = new PrimitiveTest(false);

  prims_[IdStore::get(ID_rbjit__bitwise_add)] = new PrimitiveAdd;
  prims_[IdStore::get(ID_rbjit__bitwise_sub)] = new PrimitiveSub;

  prims_[IdStore::get(ID_rbjit__bitwise_add_overflow)] = new PrimitiveOpOverflow(true);
  prims_[IdStore::get(ID_rbjit__bitwise_sub_overflow)] = new PrimitiveOpOverflow(false);

  prims_[IdStore::get(ID_rbjit__bitwise_compare_eq)] = new PrimitiveCmp(llvm::CmpInst::ICMP_EQ);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_ne)] = new PrimitiveCmp(llvm::CmpInst::ICMP_NE);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_ugt)] = new PrimitiveCmp(llvm::CmpInst::ICMP_UGT);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_uge)] = new PrimitiveCmp(llvm::CmpInst::ICMP_UGE);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_ult)] = new PrimitiveCmp(llvm::CmpInst::ICMP_ULT);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_ule)] = new PrimitiveCmp(llvm::CmpInst::ICMP_ULE);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_sgt)] = new PrimitiveCmp(llvm::CmpInst::ICMP_SGT);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_sge)] = new PrimitiveCmp(llvm::CmpInst::ICMP_SGE);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_slt)] = new PrimitiveCmp(llvm::CmpInst::ICMP_SLT);
  prims_[IdStore::get(ID_rbjit__bitwise_compare_sle)] = new PrimitiveCmp(llvm::CmpInst::ICMP_SLE);

  prims_[IdStore::get(ID_rbjit__convert_to_array)] = new PrimitiveConvertToArray;
  prims_[IdStore::get(ID_rbjit__concat_arrays)] = new PrimitiveConcatArrays;
  prims_[IdStore::get(ID_rbjit__push_to_array)] = new PrimitivePushToArray;

  prims_[IdStore::get(ID_rbjit__convert_to_string)] = new PrimitiveConvertToString;
  prims_[IdStore::get(ID_rbjit__concat_strings)] = new PrimitiveConcatStrings;
}

Primitive*
Primitive::find(ID name)
{
  return prims_[name];
}

RBJIT_NAMESPACE_END
