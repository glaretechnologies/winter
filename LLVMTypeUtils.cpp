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


llvm::Type* pointerType(llvm::Type* type)
{
	return llvm::PointerType::get(
		type, 
		0 // AddressSpace
	);
}


llvm::Type* voidPtrType(llvm::LLVMContext& context)
{
	// Not sure if LLVM has a pointer to void type, so just use a pointer to int32.
	return llvm::Type::getInt32PtrTy(context);
}


llvm::Type* getBaseCapturedVarStructType(llvm::Module& module)
{
	const std::string struct_name = "base_captured_var_struct";
	llvm::StructType* existing_struct_type = module.getTypeByName(struct_name);
	if(existing_struct_type)
		return existing_struct_type;

	std::vector<llvm::Type*> elements;
	//elements.push_back(llvm::Type::getIntNTy(module.getContext(), 64)); // Dummy struct member

	return llvm::StructType::create(
		module.getContext(),
		elements, // elements
		struct_name
	);
}


llvm::Type* getPtrToBaseCapturedVarStructType(llvm::Module& module)
{
	return pointerType(*getBaseCapturedVarStructType(module));
}


llvm::FunctionType* llvmFunctionType(const vector<TypeVRef>& arg_types, 
									 bool captured_var_struct_ptr_arg,
									 TypeVRef return_type, 
									 llvm::Module& module)
{
	if(return_type->passByValue())
	{
		llvm::SmallVector<llvm::Type*, 8> llvm_arg_types((unsigned int)arg_types.size());

		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types[i] = arg_types[i]->passByValue() ? arg_types[i]->LLVMType(module) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(module));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(module));

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
		// The return value is passed by reference, so that means the zero-th argument will be a pointer to memory where the return value will be placed (SRET).

		llvm::SmallVector<llvm::Type*, 8> llvm_arg_types(1 + (unsigned int)arg_types.size());
		llvm_arg_types[0] = LLVMTypeUtils::pointerType(*return_type->LLVMType(module)); // Arg 0 is SRET arg.

		// Set normal arguments
		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types[i + 1] = arg_types[i]->passByValue() ? arg_types[i]->LLVMType(module) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(module));

		if(captured_var_struct_ptr_arg)
			llvm_arg_types.push_back(getPtrToBaseCapturedVarStructType(module));

		//if(hidden_voidptr_arg)
		//	llvm_arg_types.push_back(voidPtrType(context));

		return llvm::FunctionType::get(
			llvm::Type::getVoidTy(module.getContext()), // return type - void as return value will be written to mem via zero-th arg.
			llvm_arg_types,
			false // varargs
		);
	}
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


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
