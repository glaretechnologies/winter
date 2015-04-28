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
#include "llvm/PassManager.h"
#include <llvm/IR/DataLayout.h>
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include <llvm/IR/IRBuilder.h>
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;


namespace Winter
{


namespace LLVMTypeUtils
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


llvm::Type* pointerType(llvm::Type& type)
{
	return llvm::PointerType::get(
		&type, 
		0 // AddressSpace
	);
}


llvm::Type* voidPtrType(llvm::LLVMContext& context)
{
	// Not sure if LLVM has a pointer to void type, so just use a pointer to int32.
	return llvm::Type::getInt32PtrTy(context);
}


llvm::Type* getBaseCapturedVarStructType(llvm::LLVMContext& context)
{
	const std::vector<llvm::Type*> params;
	return llvm::StructType::get(
		context,
		params // params
	);
}


llvm::Type* getPtrToBaseCapturedVarStructType(llvm::LLVMContext& context)
{
	return pointerType(*getBaseCapturedVarStructType(context));
}


llvm::FunctionType* llvmFunctionType(const vector<TypeRef>& arg_types, 
									 bool captured_var_struct_ptr_arg,
									 TypeRef return_type, 
									 llvm::Module& module)
{
	if(return_type->passByValue())
	{
		vector<llvm::Type*> llvm_arg_types;

		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(module) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(module)));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(module.getContext()));

		//if(hidden_voidptr_arg)
		//	llvm_arg_types.push_back(voidPtrType(context));

		return llvm::FunctionType::get(
			return_type->LLVMType(module), // return type
			llvm_arg_types,
			false // varargs
			);
	}
	else
	{
		// The return value is passed by reference, so that means the zero-th argument will be a pointer to memory where the return value will be placed.

		vector<llvm::Type*> llvm_arg_types;
		llvm_arg_types.push_back(LLVMTypeUtils::pointerType(*return_type->LLVMType(module)));

		// Append normal arguments
		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(module) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(module)));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(module.getContext()));

		//if(hidden_voidptr_arg)
		//	llvm_arg_types.push_back(voidPtrType(context));

		return llvm::FunctionType::get(
			//LLVMTypeUtils::pointerType(*return_type->LLVMType(context)), 
			llvm::Type::getVoidTy(module.getContext()), // return type - void as return value will be written to mem via zero-th arg.
			llvm_arg_types,
			false // varargs
			);
	}
}


llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, 
							 llvm::IRBuilder<>* builder, const llvm::Twine& name)
{
	llvm::Value* field_ptr = builder->CreateStructGEP(structure_ptr, 
		field_index, // field index
		name
	);

	return builder->CreateLoad(field_ptr, name);
}


void createCollectionCopy(const TypeRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params)
{
	//if(collection_type->getType() == Type::ArrayTypeType)
	//{
	//	const ArrayType* array_type = collection_type.downcastToPtr<ArrayType>();
	//	const TypeRef elem_type = array_type->elem_type;
	//	params.target_data->getABITypeAlignment

	const bool use_memcpy = false; // collection_type->getType() == Type::ArrayTypeType;

	if(use_memcpy)
	{
		llvm::Type* llvm_type = collection_type->LLVMType(*params.module);
		const size_t size_B = params.target_data->getTypeAllocSize(llvm_type);
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(64, size_B, /*signed=*/false));
		params.builder->CreateMemCpy(dest_ptr, src_ptr, size,
			32 // align
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


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
