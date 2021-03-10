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
	llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name = "");
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
	llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name = "");
#endif


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
	return llvm::ElementCount((unsigned int)num, /*scalable=*/false);
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
llvm::Value* createCallWithValue(Builder* builder, llvm::Value* target_llvm_func, llvm::ArrayRef<llvm::Value*> args, const llvm::Twine& name = "")
{
#if TARGET_LLVM_VERSION >= 110

	llvm::Type* ptr_function_type = target_llvm_func->getType();

	llvm::Type* function_type = ptr_function_type->getPointerElementType();

	assert(llvm::isa<llvm::FunctionType>(function_type));

	return builder->CreateCall(llvm::FunctionCallee(llvm::cast<llvm::FunctionType>(function_type), target_llvm_func), args, name);
#else
	return builder->CreateCall(target_llvm_func, args, name);
#endif
}

}; // end namespace LLVMUtils


}; // end namespace Winter
