/*=====================================================================
LLVMUtils.cpp
-------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "LLVMUtils.h"


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
		llvm::Value* struct_ptr, unsigned int field_index, llvm::Type* struct_llvm_type, const llvm::Twine& name)
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
		llvm::Value* struct_ptr, unsigned int field_index, llvm::Type* struct_llvm_type, const llvm::Twine& name)
#endif
{
	assert(llvm::isa<llvm::StructType>(struct_llvm_type));

#if TARGET_LLVM_VERSION >= 60 // not sure the actual version the type arg was introduced.
	return builder->CreateStructGEP(struct_llvm_type, struct_ptr, field_index);
#else
	return builder->CreateStructGEP(      struct_ptr, field_index);
#endif
}


llvm::LoadInst* createLoadFromStruct(llvm::IRBuilder<>* builder, llvm::Value* src_ptr, unsigned int field_index, llvm::Type* struct_llvm_type, const llvm::Twine& field_val_name)
{
	assert(llvm::isa<llvm::StructType>(struct_llvm_type));
	llvm::Value* field_ptr = createStructGEP(builder, src_ptr, field_index, struct_llvm_type);
	return createLoad(builder, field_ptr, llvm::cast<llvm::StructType>(struct_llvm_type)->getElementType(field_index), field_val_name);
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



llvm::Value* createCollectionCopy(const TypeVRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params)
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
		const auto type_alignment = params.target_data->getABITypeAlignment(collection_type->LLVMType(*params.module));

		llvm::Type* llvm_type = collection_type->LLVMType(*params.module);
		const uint64_t size_B = params.target_data->getTypeAllocSize(llvm_type);
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(64, size_B, /*signed=*/false));
		return createMemCpy(params.builder, dest_ptr, src_ptr, size, /*align=*/(unsigned int)type_alignment);
	}
	else
	{
		return params.builder->CreateStore(
			createLoad(params.builder, src_ptr, collection_type, params.module),
			dest_ptr
		);
	}
}


llvm::Function* getFunctionFromModule(llvm::Module* module, const std::string& func_name, llvm::FunctionType* functype)
{
#if TARGET_LLVM_VERSION >= 110
	llvm::FunctionCallee callee = module->getOrInsertFunction(
		func_name, // Name
		functype // Type
	);
	llvm::Value* llvm_func_constant = callee.getCallee();
#else
	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		func_name, // Name
		functype // Type
	);
#endif

	assert(llvm::isa<llvm::Function>(llvm_func_constant));
	if(!llvm::isa<llvm::Function>(llvm_func_constant))
		throw BaseException("Internal error: While tring to get function " + func_name + " with getOrInsertFunction(): was not a function.");

	return static_cast<llvm::Function*>(llvm_func_constant);
}


}; // end namespace LLVMUtils


}; // end namespace Winter
