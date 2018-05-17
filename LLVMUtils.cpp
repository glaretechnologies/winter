/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#include "LLVMTypeUtils.h"


#include "wnt_ASTNode.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Module.h"
#include <llvm/IR/DataLayout.h>
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include <llvm/IR/IRBuilder.h>
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/LLVMContext.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


namespace Winter
{


namespace LLVMUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n)
{
	llvm::Function::arg_iterator args = func->arg_begin();
	for(int i=0; i<n; ++i)
		args++;
	return args;
}


llvm::Value* getLastArg(llvm::Function *func)
{
	return getNthArg(func, (int)func->arg_size() - 1);
}


#if TARGET_LLVM_VERSION >= 60
llvm::Value* createStructGEP(llvm::IRBuilder</*true, */llvm::ConstantFolder, llvm::IRBuilderDefaultInserter/*<true>*/ >* builder,
		llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name)
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
		llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name)
#endif
{
#if TARGET_LLVM_VERSION >= 60 // not sure the actual version the type arg was introduced.
	return builder->CreateStructGEP(NULL, struct_ptr, field_index);
#else
	return builder->CreateStructGEP(      struct_ptr, field_index);
#endif
}


//llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, 
//							 llvm::IRBuilder<>* builder, const llvm::Twine& name)
//{
//	llvm::Value* field_ptr = builder->CreateStructGEP(structure_ptr->get, structure_ptr, 
//		field_index, // field index
//		name
//	);
//
//	return builder->CreateLoad(field_ptr, name);
//}


void createCollectionCopy(const TypeVRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params)
{
	//if(collection_type->getType() == Type::ArrayTypeType)
	//{
	//	const ArrayType* array_type = collection_type.downcastToPtr<ArrayType>();
	//	const TypeRef elem_type = array_type->elem_type;
	//	params.target_data->getABITypeAlignment

	const bool use_memcpy = 
		collection_type->getType() == Type::ArrayTypeType; //||
		// gives module verification errors: collection_type->getType() == Type::StructureTypeType;

	if(use_memcpy)
	{
		const unsigned int type_alignment = params.target_data->getABITypeAlignment(collection_type->LLVMType(*params.module));

		llvm::Type* llvm_type = collection_type->LLVMType(*params.module);
		const uint64_t size_B = params.target_data->getTypeAllocSize(llvm_type);
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(64, size_B, /*signed=*/false));
		params.builder->CreateMemCpy(dest_ptr, src_ptr, size,
			type_alignment // align
		);
	}
	else
	{
		params.builder->CreateStore(
			params.builder->CreateLoad(src_ptr),
			dest_ptr
		);
	}
}


}; // end namespace LLVMUtils


}; // end namespace Winter
