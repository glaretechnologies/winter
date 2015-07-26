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


/*


*/
llvm::Function* getOrInsertDecrementorForType(llvm::Module* module, const ConstTypeRef& type)
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


llvm::Function* getOrInsertDestructorForType(llvm::Module* module, const ConstTypeRef& type)
{
	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		llvm::makeArrayRef(type->passByValue() ? type->LLVMType(*module) : LLVMTypeUtils::pointerType(type->LLVMType(*module))),
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		"destructor_" + type->toString(), // Name
		functype // Type
	);

	assert(llvm::isa<llvm::Function>(llvm_func_constant));
	return static_cast<llvm::Function*>(llvm_func_constant);
}


void emitFreeCall(llvm::Module* module, llvm::IRBuilder<>& builder, const CommonFunctions& common_functions, const ConstTypeRef& refcounted_type, llvm::Value* refcounted_val)
{
	if(refcounted_type->getType() == Type::VArrayTypeType)
	{
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
		llvm::Function* freeValueLLVMFunc = common_functions.freeStringFunc->getOrInsertFunction(
			module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
		);
		builder.CreateCall(freeValueLLVMFunc, refcounted_val);
	}
	else if(refcounted_type->getType() == Type::FunctionType)
	{
		// Cast to required type
		const TypeRef dummy_closure_type = Function::dummyFunctionType();
		llvm::Value* cast_val = builder.CreatePointerCast(refcounted_val, dummy_closure_type->LLVMType(*module));

		// Create call to freeClosure()
		llvm::Function* freeClosureLLVMFunc = common_functions.freeClosureFunc->getOrInsertFunction(
			module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			//true // target_takes_voidptr_arg // params.hidden_voidptr_arg
		);
		builder.CreateCall(freeClosureLLVMFunc, cast_val);
	}
	else
	{
		assert(0);
	}
}


void emitDecrementorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& refcounted_type)
{
	if(!refcounted_type->isHeapAllocated())
		return;

	//----------- Create the function ------------
	llvm::Function* llvm_func = getOrInsertDecrementorForType(module, refcounted_type);

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(entry_block);

	// Get arg 0 - (a pointer to) the ref counted value
	llvm::Value* refcounted_val = LLVMTypeUtils::getNthArg(llvm_func, 0);

	// Load ref count
	llvm::Value* ref_ptr = builder.CreateStructGEP(refcounted_val, 0, "ref_ptr");
	llvm::Value* ref_count = builder.CreateLoad(ref_ptr, "ref_count");

	// Load flags
	llvm::Value* flags_ptr = builder.CreateStructGEP(refcounted_val, 2, "flags_ptr");
	llvm::Value* flags_val = builder.CreateLoad(flags_ptr, "flags_val");


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

	// Call destructor for value.
	llvm::Function* destructor = getOrInsertDestructorForType(module, refcounted_type);
	builder.CreateCall(destructor, refcounted_val);


	// if heap allocated:
	//		call free
	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock* heap_allocated_then_BB  = llvm::BasicBlock::Create(module->getContext(), "heap_allocated_then_BB", llvm_func);
	
	// Compare with flag value of 1.
	llvm::Value* heap_allocated_condition = builder.CreateICmpEQ(flags_val, llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1, /*signed=*/true)));
	builder.CreateCondBr(heap_allocated_condition, heap_allocated_then_BB, MergeBB);

	builder.SetInsertPoint(heap_allocated_then_BB);

	// Now call free function for type.
	emitFreeCall(module, builder, common_functions, refcounted_type, refcounted_val);

	// 'Branch' to MergeBB
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


void emitDestructorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& type)
{
	//----------- Create the function ------------
	llvm::Function* llvm_func = getOrInsertDestructorForType(module, type);

	// Create initial block and builder
	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(entry_block);

	if(type->getType() == Type::StructureTypeType)
	{
		const StructureType* struct_type = type.downcastToPtr<StructureType>();

		// Get arg 0 - (a pointer to) the ref counted value
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

		for(size_t i=0; i<struct_type->component_types.size(); ++i)
		{
			// Call decrementor on each element that has a destructor.
			if(struct_type->component_types[i]->hasDestructor())
			{
				// Get a pointer to the field memory.
				llvm::Value* field_ptr = builder.CreateStructGEP(struct_ptr, (unsigned int)i, struct_type->name + "." + struct_type->component_names[i] + " ptr");
				
				// If the field type is heap allocated, this means that the element is just a pointer to the field value.
				// So we need to load the pointer from the structure.
				// If the field is not heap allocated, for example another structure embedded in this structure, then the existing pointer is all we need.
				
				if(struct_type->component_types[i]->isHeapAllocated())
				{
					// Emit call to decrementor
					llvm::Value* field_val = builder.CreateLoad(field_ptr);
					llvm::Function* elem_decrementor_func = getOrInsertDecrementorForType(module, struct_type->component_types[i]);
					builder.CreateCall(elem_decrementor_func, field_val);
				}
				else
				{
					// Emit call to destructor.
					llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, struct_type->component_types[i]);
					builder.CreateCall(elem_destructor_func, field_ptr);
				}
			}
		}

		builder.CreateRetVoid();
		return;
	}
	else if(type->getType() == Type::TupleTypeType)
	{
		const TupleType* tuple_type = type.downcastToPtr<TupleType>();

		// Get arg 0 - (a pointer to) the ref counted value
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

		for(size_t i=0; i<tuple_type->component_types.size(); ++i)
		{
			// Call destructor on each element that has a destructor.
			if(tuple_type->component_types[i]->hasDestructor())
			{
				// Get a pointer to the field memory.
				llvm::Value* field_ptr = builder.CreateStructGEP(struct_ptr, (unsigned int)i, tuple_type->toString() + ".field_" + ::toString(i) + " ptr");
				
				if(tuple_type->component_types[i]->isHeapAllocated())
				{
					// Emit call to decrementor
					llvm::Value* field_val = builder.CreateLoad(field_ptr);
					llvm::Function* elem_decrementor_func = getOrInsertDecrementorForType(module, tuple_type->component_types[i]);
					builder.CreateCall(elem_decrementor_func, field_val);
				}
				else
				{
					// Emit call to destructor.
					llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, tuple_type->component_types[i]);
					builder.CreateCall(elem_destructor_func, field_ptr);
				}
			}
		}

		builder.CreateRetVoid();
		return;
	}
	

	// Get arg 0 - (a pointer to) the value
	llvm::Value* val = LLVMTypeUtils::getNthArg(llvm_func, 0);

	llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(module->getContext(), "merge");


	if(type->getType() == Type::VArrayTypeType)
	{
		const TypeRef elem_type = type.downcastToPtr<VArrayType>()->elem_type;

		// Load number of elements
		llvm::Value* num_elems_ptr = builder.CreateStructGEP(val, 1, "num elems ptr");
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
			loop_index_var->addIncoming(initial_loop_index_value, entry_block);

	
			// Emit the body of the loop, which is a call to the destructor or decrementor for the element type.
			//---------------------

			// Get ptr to varray element
			llvm::Value* data_ptr = builder.CreateStructGEP(val, 3, "data_ptr"); // [0 x VArray<T>]*

			//data_ptr->getType()->dump();
			//std::cout << std::endl;

			// First index of zero selects [0 x VArray<T>], second index gets array element
			llvm::Value* indices[] = { llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)), loop_index_var, };
			llvm::Value* elem_ptr = builder.CreateInBoundsGEP(data_ptr, llvm::makeArrayRef(indices));

			//elem_ptr->getType()->dump();
			//std::cout << std::endl;

			llvm::Value* elem_value;
			if(elem_type->isHeapAllocated())
			{
				// Emit call to decrementor
				elem_value = builder.CreateLoad(elem_ptr); // elem value is actually a pointer.
				llvm::Function* elem_destructor_func = getOrInsertDecrementorForType(module, elem_type);
				builder.CreateCall(elem_destructor_func, elem_value);
			}
			else
			{
				// Emit call to destructor
				elem_value = elem_ptr;
				llvm::Function* elem_destructor_func = getOrInsertDestructorForType(module, elem_type);
				builder.CreateCall(elem_destructor_func, elem_value);
			}

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
	}
	

	builder.CreateBr(MergeBB);


	// Emit merge block.
	llvm_func->getBasicBlockList().push_back(MergeBB);
	builder.SetInsertPoint(MergeBB);

	builder.CreateRetVoid();
}


void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions)
{
	const TypeRef string_type = new String();
	common_functions.incrStringRefCountLLVMFunc = emitIncrRefCountFunc(module, target_data, "incrStringRefCount", string_type);

	// Since the varray type is generic, incrVArrayRefCount will just take an varray<int> arg, and the calling code must cast to varray<int>.
	const TypeRef dummy_varray_type = new VArrayType(new Int());
	common_functions.incrVArrayRefCountLLVMFunc = emitIncrRefCountFunc(module, target_data, "incrVArrayRefCount", dummy_varray_type);

	// Since the closure type is generic, emitIncrRefCountFunc will just take dummy closure type arg, and the calling code must cast to it.
	const TypeRef dummy_closure_type = Function::dummyFunctionType();
	common_functions.incrClosureRefCountLLVMFunc = emitIncrRefCountFunc(module, target_data, "incrClosureRefCount", dummy_closure_type);
}


} // end namespace RefCounting


} // end namespace Winter

