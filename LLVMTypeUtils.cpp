/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#include "LLVMTypeUtils.h"


#if USE_LLVM
#include "llvm/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
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


const llvm::Type* pointerType(const llvm::Type& type)
{
	return llvm::PointerType::get(
		&type, 
		0 // AddressSpace
	);
}


llvm::FunctionType* llvmInternalFunctionType(const vector<TypeRef>& arg_types, TypeRef return_type, llvm::LLVMContext& context)
{
	if(return_type->passByValue())
	{
		vector<const llvm::Type*> llvm_arg_types;

		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(context) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(context)));

		return llvm::FunctionType::get(
			return_type->LLVMType(context), // return type
			llvm_arg_types,
			false // varargs
			);
	}
	else
	{
		// The return value is passed by reference, so that means the zero-th argument will be a pointer to memory where the return value will be placed.

		vector<const llvm::Type*> llvm_arg_types;
		llvm_arg_types.push_back(LLVMTypeUtils::pointerType(*return_type->LLVMType(context)));

		// Append normal arguments
		for(unsigned int i=0; i<arg_types.size(); ++i)
			llvm_arg_types.push_back(arg_types[i]->passByValue() ? arg_types[i]->LLVMType(context) : LLVMTypeUtils::pointerType(*arg_types[i]->LLVMType(context)));

		return llvm::FunctionType::get(
			//LLVMTypeUtils::pointerType(*return_type->LLVMType(context)), 
			llvm::Type::getVoidTy(context), // return type - void as return value will be written to mem via zero-th arg.
			llvm_arg_types,
			false // varargs
			);
	}
}


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
