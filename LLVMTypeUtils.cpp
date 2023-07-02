/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2018 -
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
	llvm::StructType* existing_struct_type = getStructureTypeForName(struct_name, module);
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


llvm::StructType* getStructureTypeForName(const std::string& name, llvm::Module& module)
{
#if TARGET_LLVM_VERSION >= 150
	return llvm::StructType::getTypeByName(module.getContext(), name);
#else
	return module.getTypeByName(name);
#endif
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

		return llvm::FunctionType::get(
			llvm::Type::getVoidTy(module.getContext()), // return type - void as return value will be written to mem via zero-th arg.
			llvm_arg_types,
			false // varargs
		);
	}
}


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
