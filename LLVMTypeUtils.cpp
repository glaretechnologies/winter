/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#include "LLVMTypeUtils.h"


#pragma warning(push, 0) // Disable warnings
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/PassManager.h"
#include <llvm/IR/DataLayout.h>
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include <llvm/IR/IRBuilder.h>
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#pragma warning(pop) // Re-enable warnings


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
									 llvm::LLVMContext& context)
{
	if(return_type->passByValue())
	{
		vector<llvm::Type*> llvm_arg_types;

		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(context) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(context)));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(context));

		//if(hidden_voidptr_arg)
		//	llvm_arg_types.push_back(voidPtrType(context));

		return llvm::FunctionType::get(
			return_type->LLVMType(context), // return type
			llvm_arg_types,
			false // varargs
			);
	}
	else
	{
		// The return value is passed by reference, so that means the zero-th argument will be a pointer to memory where the return value will be placed.

		vector<llvm::Type*> llvm_arg_types;
		llvm_arg_types.push_back(LLVMTypeUtils::pointerType(*return_type->LLVMType(context)));

		// Append normal arguments
		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(context) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(context)));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(context));

		//if(hidden_voidptr_arg)
		//	llvm_arg_types.push_back(voidPtrType(context));

		return llvm::FunctionType::get(
			//LLVMTypeUtils::pointerType(*return_type->LLVMType(context)), 
			llvm::Type::getVoidTy(context), // return type - void as return value will be written to mem via zero-th arg.
			llvm_arg_types,
			false // varargs
			);
	}
}


llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, 
							 llvm::IRBuilder<>* builder, llvm::LLVMContext& context, const llvm::Twine& Name)
{
	vector<llvm::Value*> indices;
	indices.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, 0, true))); // array index
	indices.push_back(llvm::ConstantInt::get(context, llvm::APInt(32, field_index, true))); // field index
		
	llvm::Value* field_ptr = builder->CreateGEP(
		structure_ptr, // ptr
		indices,
		"field_ptr"
	);

	return builder->CreateLoad(field_ptr, Name);
}



}; // end namespace LLVMTypeUtils


}; // end namespace Winter
