/*=====================================================================
RefCounting.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:40 +0100
=====================================================================*/
#include "wnt_RefCounting.h"


#include "wnt_ASTNode.h"
#include "wnt_Type.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "utils/StringUtils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DataLayout.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;
using std::string;


namespace Winter
{


namespace RefCounting
{


void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions)
{
	//====================================================================================================================
	// Emit decrement ref count function
	// arg 0: string pointer
	// Returns: void
	{

		/*std::vector<TypeRef> arg_types(1, new String());

		llvm::FunctionType* functype = LLVMTypeUtils::llvmFunctionType(
			arg_types, 
			false, // use_cap_var_struct_ptr, // this->use_captured_vars, // use captured var struct ptr arg
			new Int(), 
			module->getContext(),
			true // hidden_voidptr_arg
		);*/

		TypeRef string_type = new String();
		std::vector<llvm::Type*> arg_types(1, string_type->LLVMType(module->getContext()));
		//arg_types.push_back(LLVMTypeUtils::voidPtrType(module->getContext())); // hidden_voidptr_arg

		llvm::FunctionType* functype = llvm::FunctionType::get(
			llvm::Type::getVoidTy(module->getContext()), // return type
			arg_types,
			false // varargs
		);

		llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
			"decrStringRefCount", // Name
			functype // Type
		);

		llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

		common_functions.decrStringRefCountLLVMFunc = llvm_func;

		llvm::BasicBlock* block = llvm::BasicBlock::Create(
			module->getContext(), 
			"entry", 
			llvm_func
		);


		llvm::IRBuilder<> builder(block);

		// Get arg 0.
		llvm::Value* string_val = LLVMTypeUtils::getNthArg(llvm_func, 0);

		// Load ref count
		//string_val->dump();
		llvm::Value* ref_ptr = builder.CreateStructGEP(string_val, 0, "ref ptr");

		llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref count");

		// Get a pointer to the current function
		llvm::Function* the_function = builder.GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
		llvm::BasicBlock* ThenBB  = llvm::BasicBlock::Create(module->getContext(), "then", the_function);
		llvm::BasicBlock* ElseBB  = llvm::BasicBlock::Create(module->getContext(), "else");
		llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(module->getContext(), "ifcont");

		// Compare with ref count value of 1.
		llvm::Value* condition = builder.CreateICmpEQ(ref_count, llvm::ConstantInt::get(
			module->getContext(),
			llvm::APInt(64, 1, 
				true // signed
			)
		));

		builder.CreateCondBr(condition, ThenBB, ElseBB);

		// Emit then value.
		builder.SetInsertPoint(ThenBB);

		// Create call to freeString()
		llvm::Function* freeStringLLVMFunc = common_functions.freeStringFunc->getOrInsertFunction(
			module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
		);

		// Set hidden voidptr argument
		vector<llvm::Value*> args(1, string_val);
		//const bool target_takes_voidptr_arg = true;
		//if(target_takes_voidptr_arg)
		//	args.push_back(LLVMTypeUtils::getLastArg(llvm_func));

		//string_val->dump();
		//freeStringLLVMFunc->dump();
		/*llvm::CallInst* call_inst = */builder.CreateCall(freeStringLLVMFunc, args);


		builder.CreateBr(MergeBB);

		// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
		ThenBB = builder.GetInsertBlock();

		// Emit else block.
		the_function->getBasicBlockList().push_back(ElseBB);
		builder.SetInsertPoint(ElseBB);

		// Emit decrement of ref count
		llvm::Value* decr_ref = builder.CreateSub(ref_count, llvm::ConstantInt::get(
			module->getContext(),
			llvm::APInt(64, 1, 
				true // signed
			)
		));

		// Store the decremented ref count
		builder.CreateStore(decr_ref, ref_ptr);

		builder.CreateBr(MergeBB);

		// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
		ElseBB = builder.GetInsertBlock();

		// Emit merge block.
		the_function->getBasicBlockList().push_back(MergeBB);
		builder.SetInsertPoint(MergeBB);

		builder.CreateRetVoid();
	}

	//====================================================================================================================
	// Emit increment ref count function
	// arg 0: string pointer
	// Returns: void
	{
		TypeRef string_type = new String();
		std::vector<llvm::Type*> arg_types(1, string_type->LLVMType(module->getContext()));
		//arg_types.push_back(LLVMTypeUtils::voidPtrType(module->getContext())); // hidden_voidptr_arg

		llvm::FunctionType* functype = llvm::FunctionType::get(
			llvm::Type::getVoidTy(module->getContext()), // return type
			arg_types,
			false // varargs
		);

		llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
			"incrStringRefCount", // Name
			functype // Type
		);

		llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

		common_functions.incrStringRefCountLLVMFunc = llvm_func;

		llvm::BasicBlock* block = llvm::BasicBlock::Create(
			module->getContext(), 
			"entry", 
			llvm_func
		);


		llvm::IRBuilder<> builder(block);

		// Get arg 0.
		llvm::Value* string_val = LLVMTypeUtils::getNthArg(llvm_func, 0);

		// Get pointer to ref count
		llvm::Value* ref_ptr = builder.CreateStructGEP(string_val, 0, "ref ptr");

		// Load the ref count
		llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref count");

		llvm::Value* one = llvm::ConstantInt::get(
			module->getContext(),
			llvm::APInt(64, 1, 
				true // signed
			)
		);

		llvm::Value* new_ref_count = builder.CreateAdd(
			ref_count, 
			one,
			"incr ref count"
		);

		// Store the new ref count
		builder.CreateStore(new_ref_count, ref_ptr);

		builder.CreateRetVoid();
	}
}


void emitIncrementStringRefCount(EmitLLVMCodeParams& params, llvm::Value* string_val)
{
	vector<llvm::Value*> args(1, string_val);

	// Set hidden voidptr argument
	//const bool target_takes_voidptr_arg = true;
	//if(target_takes_voidptr_arg)
	//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));


	params.builder->CreateCall(params.common_functions.incrStringRefCountLLVMFunc, args);
}


void emitStringCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val)
{
	vector<llvm::Value*> args(1, string_val);

	// Set hidden voidptr argument
	//const bool target_takes_voidptr_arg = true;
	//if(target_takes_voidptr_arg)
	//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));


	params.builder->CreateCall(params.common_functions.decrStringRefCountLLVMFunc, args);
}


void emitStructureCleanupLLVMCode(EmitLLVMCodeParams& params, const StructureType& struct_type, llvm::Value* structure_val)
{
	for(size_t i=0; i<struct_type.component_types.size(); ++i)
	{
		if(struct_type.component_types[i]->getType() == Type::StringType)
		{
			// Emit code to load the string value:
			structure_val->dump();
			structure_val->getType()->dump();
			llvm::Value* str_ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i, struct_type.name + ".str ptr");

			// Load the string value
			str_ptr->dump();
			str_ptr->getType()->dump();
			llvm::Value* str = params.builder->CreateLoad(str_ptr, struct_type.name + ".str");

			str->dump();
			str->getType()->dump();
			emitStringCleanupLLVMCode(params, str);
		}
	}
}


void emitCleanupLLVMCode(EmitLLVMCodeParams& params, const TypeRef& type, llvm::Value* val)
{
	switch(type->getType())
	{
	case Type::StringType:
		{
			emitStringCleanupLLVMCode(params, val);
			break;
		}
	case Type::StructureTypeType:
		{
			emitStructureCleanupLLVMCode(params, *type.downcast<StructureType>(), val);
			break;
		}
	default:
		assert(0);
	};
}


} // end namespace RefCounting


} // end namespace Winter

