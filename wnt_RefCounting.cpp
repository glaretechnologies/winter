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


// Emit decrement ref count function (decrStringRefCount etc..)
// arg 0: pointer to refcounted value
// Returns: void

//TEMP: new version that checks for NULL ptr:
#if 0
llvm::Function* emitDecrRefCountFunc(llvm::Module* module, const llvm::DataLayout* target_data, const string& func_name, const TypeRef& refcounted_type,
									 FunctionDefinition* free_value_function)
{
	const std::vector<llvm::Type*> arg_types(1, refcounted_type->LLVMType(*module));

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		func_name, // Name
		functype // Type
	);

	// TODO: check cast.
	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

	// Create entry block
	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);

	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function
	llvm::BasicBlock* if_non_null_BB  = llvm::BasicBlock::Create(module->getContext(), "if_non_null", llvm_func);
	llvm::BasicBlock* ThenBB  = llvm::BasicBlock::Create(module->getContext(), "then", llvm_func);
	llvm::BasicBlock* ElseBB  = llvm::BasicBlock::Create(module->getContext(), "else", llvm_func);
	llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(module->getContext(), "merge", llvm_func);

	llvm::IRBuilder<> builder(entry_block);

	// Get a pointer to the current function
	//llvm::Function* the_function = builder.GetInsertBlock()->getParent();

	// Get arg 0.
	llvm::Value* ptr_to_recounted_val = LLVMTypeUtils::getNthArg(llvm_func, 0);
	
	// Branch to if_non_null_BB if !null, else branch to MergeBB
	llvm::Value* non_null_cond = builder.CreateIsNotNull(ptr_to_recounted_val, "non_null");
	builder.CreateCondBr(non_null_cond, if_non_null_BB, MergeBB);

	// Emit code for if_non_null_BB
	builder.SetInsertPoint(if_non_null_BB);
	// Load ref count
	llvm::Value* ref_ptr = builder.CreateStructGEP(ptr_to_recounted_val, 0, "ref_ptr");
	llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref_count");


	// Compare with ref count value of 1.
	llvm::Value* condition = builder.CreateICmpEQ(ref_count, llvm::ConstantInt::get(
		module->getContext(),
		llvm::APInt(64, 1, 
			true // signed
		)
	));

	builder.CreateCondBr(condition, ThenBB, ElseBB);

	// Emit then block
	builder.SetInsertPoint(ThenBB);

	// Create call to freeString() or equivalent
	llvm::Function* freeValueLLVMFunc = free_value_function->getOrInsertFunction(
		module,
		false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
		//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
	);

	builder.CreateCall(freeValueLLVMFunc, ptr_to_recounted_val);
	builder.CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = builder.GetInsertBlock();

	// Emit else block.
//	llvm_func->getBasicBlockList().push_back(ElseBB);
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
	//ElseBB = builder.GetInsertBlock();

	// Emit merge block.
//	llvm_func->getBasicBlockList().push_back(MergeBB);
	builder.SetInsertPoint(MergeBB);

	builder.CreateRetVoid();

	return llvm_func;
}
#endif


llvm::Function* emitDecrRefCountFunc(llvm::Module* module, const llvm::DataLayout* target_data, const string& func_name, const TypeRef& refcounted_type,
									 FunctionDefinition* free_value_function)
{
	const std::vector<llvm::Type*> arg_types(1, refcounted_type->LLVMType(*module));

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		func_name, // Name
		functype // Type
	);

	// TODO: check cast.
	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);

	llvm::IRBuilder<> builder(entry_block);

	// Get arg 0.
	llvm::Value* recounted_val = LLVMTypeUtils::getNthArg(llvm_func, 0);


	//recounted_val->dump();
	//recounted_val->getType()->dump();

	// Load ref count
	llvm::Value* ref_ptr = builder.CreateStructGEP(recounted_val, 0, "ref_ptr");
	llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref_count");

	// Get a pointer to the current function
	llvm::Function* the_function = builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock* ThenBB  = llvm::BasicBlock::Create(module->getContext(), "then", the_function);
	llvm::BasicBlock* ElseBB  = llvm::BasicBlock::Create(module->getContext(), "else");
	llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(module->getContext(), "merge");

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

	// Create call to freeString() or equivalent
	llvm::Function* freeValueLLVMFunc = free_value_function->getOrInsertFunction(
		module,
		false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
		//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
	);
	builder.CreateCall(freeValueLLVMFunc, recounted_val);

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

	return llvm_func;
}


// Emit increment ref count function (incrStringRefCount etc..)
// arg 0: pointer to refcounted value
// Returns: void
llvm::Function* emitIncrRefCountFunc(llvm::Module* module, const llvm::DataLayout* target_data, const string& func_name, const TypeRef& refcounted_type)
{
	const std::vector<llvm::Type*> arg_types(1, refcounted_type->LLVMType(*module));

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		func_name, // Name
		functype // Type
	);

	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);
	llvm::BasicBlock* block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(block);
		
	llvm::Value* refcounted_val = LLVMTypeUtils::getNthArg(llvm_func, 0); // Get arg 0.
	llvm::Value* ref_ptr = builder.CreateStructGEP(refcounted_val, 0, "ref_ptr"); // Get pointer to ref count
	llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref_count"); // Load the ref count
	llvm::Value* one = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1, /*signed=*/true));
	llvm::Value* new_ref_count = builder.CreateAdd(ref_count, one, "incremented_ref_count");
		
	builder.CreateStore(new_ref_count, ref_ptr); // Store the new ref count

	builder.CreateRetVoid();

	return llvm_func;
}


llvm::Function* RefCounting::getOrInsertDestructorForType(llvm::Module* module, const ConstTypeRef& type)
{
	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		llvm::makeArrayRef(type->passByValue() ? type->LLVMType(*module) : LLVMTypeUtils::pointerType(type->LLVMType(*module))),
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		"decr_" + type->toString(), // Name
		functype // Type
	);

	assert(llvm::isa<llvm::Function>(llvm_func_constant));
	return static_cast<llvm::Function*>(llvm_func_constant);
}


void emitDestructorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& refcounted_type)
{
	//----------- Create the function ------------
	llvm::Function* llvm_func = getOrInsertDestructorForType(module, refcounted_type);

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(entry_block);


	if(refcounted_type->getType() == Type::StructureTypeType)
	{
		const StructureType* struct_type = refcounted_type.downcastToPtr<StructureType>();

		// Get arg 0 - (a pointer to) the ref counted value
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

		for(size_t i=0; i<struct_type->component_types.size(); ++i)
		{
			// Call destructor on each element that has a destructor.
			if(struct_type->component_types[i]->hasDestructor())
			{
				// Get a pointer to the field memory.
				llvm::Value* field_ptr = builder.CreateStructGEP(struct_ptr, (unsigned int)i, struct_type->name + "." + struct_type->component_names[i] + " ptr");
				
				// If the field type is heap allocated, this means that the element is just a pointer to the field value.
				// So we need to load the pointer from the structure.
				// If the field is not heap allocated, for example another structure embedded in this structure, then the existing pointer is all we need.
				llvm::Value* field_val;
				if(struct_type->component_types[i]->isHeapAllocated())
					field_val = builder.CreateLoad(field_ptr);
				else
					field_val = field_ptr;

				llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, struct_type->component_types[i]);

				builder.CreateCall(elem_destructor_func, field_val);
			}
		}

		builder.CreateRetVoid();
		return;
	}
	else if(refcounted_type->getType() == Type::TupleTypeType)
	{
		const TupleType* tuple_type = refcounted_type.downcastToPtr<TupleType>();

		// Get arg 0 - (a pointer to) the ref counted value
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

		for(size_t i=0; i<tuple_type->component_types.size(); ++i)
		{
			// Call destructor on each element that has a destructor.
			if(tuple_type->component_types[i]->hasDestructor())
			{
				// Get a pointer to the field memory.
				llvm::Value* field_ptr = builder.CreateStructGEP(struct_ptr, (unsigned int)i, tuple_type->toString() + ".field_" + ::toString(i) + " ptr");
				
				// If the field type is heap allocated, this means that the element is just a pointer to the field value.
				// So we need to load the pointer from the structure.
				// If the field is not heap allocated, for example another structure embedded in this structure, then the existing pointer is all we need.
				llvm::Value* field_val;
				if(tuple_type->component_types[i]->isHeapAllocated())
					field_val = builder.CreateLoad(field_ptr);
				else
					field_val = field_ptr;

				llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, tuple_type->component_types[i]);

				builder.CreateCall(elem_destructor_func, field_val);
			}
		}

		builder.CreateRetVoid();
		return;
	}
	

	// Get arg 0 - (a pointer to) the ref counted value
	llvm::Value* refcounted_val = LLVMTypeUtils::getNthArg(llvm_func, 0);

	// Load ref count
	llvm::Value* ref_ptr = builder.CreateStructGEP(refcounted_val, 0, "ref_ptr");
	llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref_count");



	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock* ThenBB  = llvm::BasicBlock::Create(module->getContext(), "then", llvm_func);
	llvm::BasicBlock* ElseBB  = llvm::BasicBlock::Create(module->getContext(), "else");
	llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(module->getContext(), "merge");

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

	if(refcounted_type->getType() == Type::VArrayTypeType)
	{
		const TypeRef elem_type = refcounted_type.downcastToPtr<VArrayType>()->elem_type;

		// Load number of elements
		llvm::Value* num_elems_ptr = builder.CreateStructGEP(refcounted_val, 1, "ref count ptr");
		llvm::Value* num_elems = builder.CreateLoad(num_elems_ptr);

		// If elements have destructors, loop over them and call the destructor for each one.
		if(elem_type->hasDestructor())
		{

			//----------- Create a loop over vector elements ------------
			// Make the new basic block for the loop header, inserting after current
			// block.


			//llvm::BasicBlock* PreheaderBB = builder.GetInsertBlock();
			llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(module->getContext(), "loop", llvm_func);
  
			// Insert an explicit fall through from the current block to the LoopBB.
			builder.CreateBr(LoopBB);

			// Start insertion in LoopBB.
			builder.SetInsertPoint(LoopBB);
  

			// Create loop index (i) variable phi node
			llvm::PHINode* loop_index_var = builder.CreatePHI(llvm::Type::getInt64Ty(module->getContext()), 2, "loop_index_var");
			llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)); // Initial induction loop index value: Zero
			loop_index_var->addIncoming(initial_loop_index_value, ThenBB);

	
			// Emit the body of the loop, which is a call to the destructor for the element type (and ref decr?)
			//---------------------

			// Get ptr to varray element
			llvm::Value* data_ptr = builder.CreateStructGEP(refcounted_val, 2, "data_ptr"); // [0 x VArray<T>]*

			//data_ptr->getType()->dump();
			//std::cout << std::endl;

			// First index of zero selects [0 x VArray<T>], second index gets array element
			llvm::Value* indices[] = { llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)), loop_index_var, };
			llvm::Value* elem_ptr = builder.CreateInBoundsGEP(data_ptr, llvm::makeArrayRef(indices));

			//elem_ptr->getType()->dump();
			//std::cout << std::endl;

			llvm::Value* elem_value;
			if(elem_type->isHeapAllocated())
				elem_value = builder.CreateLoad(elem_ptr); // elem value is actually a pointer.
			else
				elem_value = elem_ptr;

			//elem_value->getType()->dump();
			//std::cout << std::endl;
			

			/*llvm::FunctionType* elem_destructor_type = llvm::FunctionType::get(
				llvm::Type::getVoidTy(module->getContext()), // return type
				llvm::makeArrayRef(elem_type->LLVMType(*module)),
				false // varargs
			);

			elem_destructor_type->dump();

			llvm::Constant* elem_destructor_func_constant = module->getOrInsertFunction(
				"decr_" + elem_type->toString(), // Name
				elem_destructor_type // Type
			);

			// TODO: check cast
			llvm::Function* elem_destructor_func = static_cast<llvm::Function*>(elem_destructor_func_constant);

			builder.CreateCall(elem_destructor_func, elem_value);*/
			llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, elem_type);
			builder.CreateCall(elem_destructor_func, elem_value);

			//---------------------
		
  
			// Create increment of loop index
			llvm::Value* step_val = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1));
			llvm::Value* next_var = builder.CreateAdd(loop_index_var, step_val, "next_var");

			// Compute the end condition.
			
  
			llvm::Value* end_cond = builder.CreateICmpNE(
				num_elems, 
				next_var,
				"loopcond"
			);
  
			// Create the "after loop" block and insert it.
			llvm::BasicBlock* LoopEndBB = builder.GetInsertBlock();
			llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(module->getContext(), "afterloop", llvm_func);
  
			// Insert the conditional branch into the end of LoopEndBB.
			builder.CreateCondBr(end_cond, LoopBB, AfterBB);
  
			// Any new code will be inserted in AfterBB.
			builder.SetInsertPoint(AfterBB);
  
			// Add a new entry to the PHI node for the backedge.
			loop_index_var->addIncoming(next_var, LoopEndBB);
		}


		// Create call to freeVArrayFunc()

		// Cast to required type
		const TypeRef dummy_varray_type = new VArrayType(new Int());
		llvm::Value* cast_val = builder.CreatePointerCast(refcounted_val, dummy_varray_type->LLVMType(*module));

		llvm::Function* freeValueLLVMFunc = common_functions.freeVArrayFunc->getOrInsertFunction(
			module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
		);
		builder.CreateCall(freeValueLLVMFunc, cast_val);
	}
	else if(refcounted_type->getType() == Type::StringType)
	{
		// Create call to freeString()

		// Cast to required type
		//const TypeRef dummy_varray_type = new VArrayType(new Int());
		//llvm::Value* cast_val = builder.CreatePointerCast(refcounted_val, dummy_varray_type->LLVMType(*module));

		llvm::Function* freeValueLLVMFunc = common_functions.freeStringFunc->getOrInsertFunction(
			module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
		);
		builder.CreateCall(freeValueLLVMFunc, refcounted_val);
	}

	builder.CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = builder.GetInsertBlock();

	// Emit else block.
	llvm_func->getBasicBlockList().push_back(ElseBB);
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
	llvm_func->getBasicBlockList().push_back(MergeBB);
	builder.SetInsertPoint(MergeBB);

	builder.CreateRetVoid();
}


void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions)
{
	const TypeRef string_type = new String();
	//common_functions.decrStringRefCountLLVMFunc = emitDecrRefCountFunc(module, target_data, "decrStringRefCount", string_type, common_functions.freeStringFunc);
	common_functions.incrStringRefCountLLVMFunc = emitIncrRefCountFunc(module, target_data, "incrStringRefCount", string_type);

	// Since the varray type is generic, decrVArrayRefCount and incrVArrayRefCount will just take an void* arg, and the calling code must cast to void*
	const TypeRef dummy_varray_type = new VArrayType(new Int());
	//common_functions.decrVArrayRefCountLLVMFunc = emitDecrRefCountFunc(module, target_data, "decrVArrayRefCount", dummy_varray_type, common_functions.freeVArrayFunc);
	common_functions.incrVArrayRefCountLLVMFunc = emitIncrRefCountFunc(module, target_data, "incrVArrayRefCount", dummy_varray_type);
}


#if 0
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


void emitIncrementVArrayRefCount(EmitLLVMCodeParams& params, llvm::Value* varray_val)
{
	const TypeRef dummy_varray_type = new VArrayType(new Int());

	// Cast to dummy_varray_type
	llvm::Value* cast_val = params.builder->CreatePointerCast(varray_val, dummy_varray_type->LLVMType(*params.module));

	params.builder->CreateCall(params.common_functions.incrVArrayRefCountLLVMFunc, cast_val);
}


void emitVArrayCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* varray_val)
{
	const TypeRef dummy_varray_type = new VArrayType(new Int());

	// Cast to dummy_varray_type
	llvm::Value* cast_val = params.builder->CreatePointerCast(varray_val, dummy_varray_type->LLVMType(*params.module));

	params.builder->CreateCall(params.common_functions.decrVArrayRefCountLLVMFunc, cast_val);
}


void emitStructureCleanupLLVMCode(EmitLLVMCodeParams& params, const StructureType& struct_type, llvm::Value* structure_val)
{
	for(size_t i=0; i<struct_type.component_types.size(); ++i)
	{
		// Recursivly call emitCleanupLLVMCode on each refcounted element.
		if(struct_type.component_types[i]->getType() == Type::StringType ||
			struct_type.component_types[i]->getType() == Type::VArrayTypeType/* ||
			struct_type.component_types[i]->getType() == Type::StructureTypeType*/) // TODO: handle this
		{

			llvm::Value* ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i);
			llvm::Value* val = params.builder->CreateLoad(ptr);

			emitCleanupLLVMCode(params, struct_type.component_types[i], val);
		}

		/*if(struct_type.component_types[i]->getType() == Type::StringType)
		{
			// Emit code to load the string value:
			//structure_val->dump();
			//structure_val->getType()->dump();
			llvm::Value* str_ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i, struct_type.name + ".str ptr");

			// Load the string value
			//str_ptr->dump();
			//str_ptr->getType()->dump();
			llvm::Value* str = params.builder->CreateLoad(str_ptr, struct_type.name + ".str");

			//str->dump();
			//str->getType()->dump();
			emitStringCleanupLLVMCode(params, str);
		}
		else if(struct_type.component_types[i]->getType() == Type::VArrayTypeType)
		{
			// Emit code to load the varray value:
			llvm::Value* ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i, struct_type.name + ".varray ptr");

			// Load the  value
			llvm::Value* varray_val = params.builder->CreateLoad(ptr, struct_type.name + ".str");

			emitVArrayCleanupLLVMCode(params, varray_val);
		}*/
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
	case Type::VArrayTypeType:
		{
			emitVArrayCleanupLLVMCode(params, val);
			break;
		}
	default:
		//assert(0);
		break;
	};
}
#endif


} // end namespace RefCounting


} // end namespace Winter

