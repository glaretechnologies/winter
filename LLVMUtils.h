/*=====================================================================
LLVMUtils.h
-----------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include <llvm/IR/IRBuilder.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include "wnt_Type.h"


namespace llvm { class Function; class FunctionType; class Type; class LLVMContext; class Twine; class Module; class Value; }
class EmitLLVMCodeParams;


namespace Winter
{


/*=====================================================================
LLVMUtils
---------

=====================================================================*/
namespace LLVMUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n);
llvm::Value* getLastArg(llvm::Function *func);


#if TARGET_LLVM_VERSION >= 60
llvm::Value* createStructGEP(llvm::IRBuilder</*true, */llvm::ConstantFolder, llvm::IRBuilderDefaultInserter/*<true>*/ >* builder,
	llvm::Value* struct_ptr, unsigned int field_index, llvm::Type* elem_type, const llvm::Twine& name = "");
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
	llvm::Value* struct_ptr, unsigned int field_index, llvm::Type* elem_type, const llvm::Twine& name = "");
#endif


template <class BuilderClass>
llvm::Value* createInBoundsGEP(BuilderClass& builder, llvm::Value* data_ptr, llvm::Type* type, llvm::ArrayRef<llvm::Value*> indices, const llvm::Twine& name = "")
{
#if TARGET_LLVM_VERSION >= 150
	return builder.CreateInBoundsGEP(type, data_ptr, indices, name);
#else
	return builder.CreateInBoundsGEP(data_ptr, indices, name);
#endif
}


//llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, llvm::IRBuilder<>* builder, const llvm::Twine& name);


// Returns store instruction or memcpy
llvm::Value* createCollectionCopy(const TypeVRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params);

template <class Builder>
inline llvm::Value* createMemCpy(Builder* builder, llvm::Value* dest_ptr, llvm::Value* src_ptr, llvm::Value* size, unsigned int alignment)
{
#if TARGET_LLVM_VERSION >= 110
	return builder->CreateMemCpy(dest_ptr, /*dst align=*/llvm::MaybeAlign(alignment), /*src=*/src_ptr, /*src align=*/llvm::MaybeAlign(alignment), /*size=*/size);
#elif TARGET_LLVM_VERSION >= 80
	return builder->CreateMemCpy(dest_ptr, /*dst align=*/alignment, /*src=*/src_ptr, /*src align=*/alignment, /*size=*/size);
#else
	return builder->CreateMemCpy(dest_ptr, src_ptr, size, /*align=*/alignment);
#endif
}


#if TARGET_LLVM_VERSION >= 110
inline llvm::ElementCount makeVectorElemCount(size_t num)
{
#if TARGET_LLVM_VERSION >= 150
	return llvm::ElementCount::get((unsigned int)num, /*scalable=*/false);
#else
	return llvm::ElementCount((unsigned int)num, /*scalable=*/false);
#endif
}
#else
inline unsigned int makeVectorElemCount(size_t num)
{
	return (unsigned int)num;
}
#endif

#if TARGET_LLVM_VERSION >= 110
typedef llvm::FunctionCallee FunctionCalleeType;
#else
typedef llvm::Function* FunctionCalleeType;
#endif


llvm::Function* getFunctionFromModule(llvm::Module* module, const std::string& func_name, llvm::FunctionType* functype);


template <class Builder>
llvm::Value* createCallWithValue(Builder* builder, llvm::Value* target_llvm_func, llvm::Type* llvm_function_type, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name = "")
{
	assert(llvm::isa<llvm::FunctionType>(llvm_function_type));
#if TARGET_LLVM_VERSION >= 110
	return builder->CreateCall(llvm::FunctionCallee(llvm::cast<llvm::FunctionType>(llvm_function_type), target_llvm_func), args, name);
#else
	return builder->CreateCall(target_llvm_func, args, name);
#endif
}


static inline llvm::LoadInst* createLoad(llvm::IRBuilder<>* builder, llvm::Value* src_ptr, const TypeVRef& type, llvm::Module* module, const llvm::Twine& name = "")
{
#if TARGET_LLVM_VERSION >= 150
	return builder->CreateLoad(type->LLVMType(*module), src_ptr, name);
#else
	return builder->CreateLoad(src_ptr, name);
#endif
}

static inline llvm::LoadInst* createLoad(llvm::IRBuilder<>* builder, llvm::Value* src_ptr, llvm::Type* type, const llvm::Twine& name = "")
{
#if TARGET_LLVM_VERSION >= 150
	return builder->CreateLoad(type, src_ptr, name);
#else
	return builder->CreateLoad(src_ptr, name);
#endif
}

static inline llvm::LoadInst* createLoad(EmitLLVMCodeParams& params, llvm::Value* src_ptr, const TypeVRef& type, const llvm::Twine& name = "")
{
#if TARGET_LLVM_VERSION >= 150
	return params.builder->CreateLoad(type->LLVMType(*params.module), src_ptr, name);
#else
	return params.builder->CreateLoad(src_ptr, name);
#endif
}


llvm::LoadInst* createLoadFromStruct(llvm::IRBuilder<>* builder, llvm::Value* src_ptr, unsigned int field_index, llvm::Type* llvm_type, const llvm::Twine& field_val_name = "");


static inline void pushBasicBlocKToBackOfFunc(llvm::Function* llvm_func, llvm::BasicBlock* block)
{
#if TARGET_LLVM_VERSION >= 160
	llvm_func->insert(llvm_func->end(), block);
#else
	llvm_func->getBasicBlockList().push_back(block);
#endif
}


}; // end namespace LLVMUtils


}; // end namespace Winter
