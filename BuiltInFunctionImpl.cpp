#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_RefCounting.h"
#include <vector>
#include "LLVMTypeUtils.h"
#include "utils/platformutils.h"
#if USE_LLVM
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
#endif

#include <iostream>//TEMP

using std::vector;


namespace Winter
{


//----------------------------------------------------------------------------------------------


class CreateLoopBodyCallBack
{
public:
	virtual ~CreateLoopBodyCallBack(){}
	virtual llvm::Value* emitLoopBody(EmitLLVMCodeParams& params, /*llvm::Value* loop_value_var, */llvm::Value* loop_iter_val) = 0;
};


// Make a for loop.  Adapted from http://llvm.org/docs/tutorial/LangImpl5.html#for-loop-expression
static llvm::Value* makeForLoop(EmitLLVMCodeParams& params, int num_iterations, llvm::Type* loop_value_type, /*llvm::Value* initial_value, */CreateLoopBodyCallBack* create_loop_body_callback)
{
	// Make the new basic block for the loop header, inserting after current
	// block.

	llvm::IRBuilder<>& Builder = *params.builder;

	llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();
	llvm::BasicBlock* PreheaderBB = Builder.GetInsertBlock();
	llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*params.context, "loop", TheFunction);
  
	// Insert an explicit fall through from the current block to the LoopBB.
	Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder.SetInsertPoint(LoopBB);
  
	

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = Builder.CreatePHI(llvm::Type::getInt32Ty(*params.context), 2, "loop_index_var");
	llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // Initial induction loop index value: Zero
	loop_index_var->addIncoming(initial_loop_index_value, PreheaderBB);

	// Create loop body/value variable phi node
	//llvm::PHINode* loop_value_var = Builder.CreatePHI(loop_value_type, 2, "loop_value_var");
	//loop_value_var->addIncoming(initial_value, PreheaderBB);
  

	// Emit the body of the loop.
	llvm::Value* updated_value = create_loop_body_callback->emitLoopBody(params, /*loop_value_var, */loop_index_var);
  
	// Create increment of loop index
	llvm::Value* step_val = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1));
	llvm::Value* next_var = Builder.CreateAdd(loop_index_var, step_val, "next_var");

	// Compute the end condition.
	llvm::Value* end_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, num_iterations));//TEMP HACK
  
	llvm::Value* end_cond = Builder.CreateICmpNE(
		end_value, 
		next_var,
		"loopcond"
	);
  
	// Create the "after loop" block and insert it.
	llvm::BasicBlock* LoopEndBB = Builder.GetInsertBlock();
	llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*params.context, "afterloop", TheFunction);
  
	// Insert the conditional branch into the end of LoopEndBB.
	Builder.CreateCondBr(end_cond, LoopBB, AfterBB);
  
	// Any new code will be inserted in AfterBB.
	Builder.SetInsertPoint(AfterBB);
  
	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, LoopEndBB);

	//loop_value_var->addIncoming(updated_value, LoopEndBB);
  
	return updated_value;
}


//----------------------------------------------------------------------------------------------


//llvm::Value* BuiltInFunctionImpl::getConstantLLVMValue(EmitLLVMCodeParams& params) const
//{
//	assert(0);
//	return NULL;
//}


//----------------------------------------------------------------------------------------------



Constructor::Constructor(Reference<StructureType>& struct_type_)
:	struct_type(struct_type_)
{
}


ValueRef Constructor::invoke(VMState& vmstate)
{
	vector<ValueRef> field_values(this->struct_type->component_names.size());

	const size_t func_args_start = vmstate.func_args_start.back();
	
	for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		field_values[i] = vmstate.argument_stack[func_args_start + i];

	return ValueRef(new StructureValue(field_values));
}


llvm::Value* Constructor::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	//TEMP: add type alias for structure type to the module while we're at it.
	//TEMP not accepted as of llvm 3.0 
	//params.module->addTypeName(this->struct_type->name, this->struct_type->LLVMType(*params.context));

	if(this->struct_type->passByValue())
	{
		llvm::Value* s = llvm::UndefValue::get(this->struct_type->LLVMType(*params.context));

		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			llvm::Value* arg_value = LLVMTypeUtils::getNthArg(params.currently_building_func, i);

			s = params.builder->CreateInsertValue(
				s,
				arg_value,
				i
			);
		}
		
		return s;
	}
	else
	{
		// Pointer to structure memory will be in 0th argument.
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		// For each field in the structure
		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			// Get the pointer to the structure field.
			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)));
			
			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices
			);

			llvm::Value* arg_value = LLVMTypeUtils::getNthArg(params.currently_building_func, i + 1);
			if(!this->struct_type->component_types[i]->passByValue())
			{
				// Load the value from memory
				arg_value = params.builder->CreateLoad(
					arg_value // ptr
				);
			}

			params.builder->CreateStore(
				arg_value, // value
				field_ptr // ptr
			);

			// If the field is of string type, we need to increment its reference count
			if(this->struct_type->component_types[i]->getType() == Type::StringType)
				RefCounting::emitIncrementStringRefCount(params, arg_value);
		}

		//assert(0);
		//return struct_ptr;
		//params.builder->
		return NULL;
	}
}


/*llvm::Value* Constructor::getConstantLLVMValue(EmitLLVMCodeParams& params) const
{
	const int arg_offset = this->struct_type->passByValue() ? 0 : 1;

	vector<llvm::Constant*> vals;
	for(size_t i=0; i<this->struct_type->component_types.size(); ++i)
	{
		//LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + i)->dump();

		vals.push_back((llvm::Constant*)LLVMTypeUtils::getNthArg(params.currently_building_func, i + 1));
	}

	return llvm::ConstantStruct::get(
		(llvm::StructType*)this->struct_type->LLVMType(*params.context),
		vals
	);
}*/


ValueRef GetField::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	// Top param on arg stack should be a structure
	assert(dynamic_cast<const StructureValue*>(vmstate.argument_stack[func_args_start].getPointer()));
	const StructureValue* s = static_cast<const StructureValue*>(vmstate.argument_stack[func_args_start].getPointer());

	assert(s);
	assert(this->index < s->fields.size());

	return s->fields[this->index];
}


llvm::Value* GetField::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(this->struct_type->passByValue())
	{
		return params.builder->CreateExtractValue(
			LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
			this->index,
			this->struct_type->component_names[this->index] // name
		);
	}
	else
	{
		TypeRef field_type = this->struct_type->component_types[this->index];
		const std::string field_name = this->struct_type->component_names[this->index];

		if(field_type->passByValue())
		{
			// Pointer to structure will be in 0th argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index, true)));

			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices,
				field_name + " ptr" // name
			);

			llvm::Value* loaded_val = params.builder->CreateLoad(
				field_ptr,
				field_name // name
			);

			// TEMP NEW: increment ref count if this is a string
			if(field_type->getType() == Type::StringType)
				RefCounting::emitIncrementStringRefCount(params, loaded_val);

			return loaded_val;
		}
		else
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index, true)));

			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices,
				field_name + " ptr" // name
			);

			llvm::Value* field_val = params.builder->CreateLoad(
				field_ptr,
				field_name // name
			);

			params.builder->CreateStore(
				field_val, // value
				return_ptr // ptr
			);

			return NULL;
		}
	}
}


ValueRef GetVectorElement::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const VectorValue* vec = static_cast<const VectorValue*>(vmstate.argument_stack[func_args_start].getPointer());

	assert(vec);
	assert(this->index < vec->e.size());

	return vec->e[this->index];
}


llvm::Value* GetVectorElement::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* vec_value = NULL;
	if(true) // TEMP shouldPassByValue(*this->type()))
	{
		vec_value = LLVMTypeUtils::getNthArg(
			params.currently_building_func, 
			0
		);
	}
	else
	{
		vec_value = params.builder->CreateLoad(
			LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
			false, // true,// TEMP: volatile = true to pick up returned vector);
			"argument" // name
		);
	}

	return params.builder->CreateExtractElement(
		vec_value, // vec
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index))
	);
}


//------------------------------------------------------------------------------------


ValueRef ArrayMapBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const FunctionValue* f = static_cast<const FunctionValue*>(vmstate.argument_stack[func_args_start].getPointer());
	const ArrayValue* from = static_cast<const ArrayValue*>(vmstate.argument_stack[func_args_start + 1].getPointer());

	assert(f);
	assert(from);

	ArrayValue* retval = new ArrayValue();
	retval->e.resize(from->e.size());

	for(unsigned int i=0; i<from->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(from->e[i]); // Push value arg
		
		retval->e[i] = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();
	}

	return ValueRef(retval);
}


class ArrayMapBuiltInFunc_CreateLoopBodyCallBack : public CreateLoopBodyCallBack
{
public:
	virtual llvm::Value* emitLoopBody(EmitLLVMCodeParams& params, /*llvm::Value* loop_value_var, */llvm::Value* i)
	{
		// Load element from input array
		vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th array
		indices[1] = i; // get the indexed element in the array

		// Get pointer to input element
		llvm::Value* elem_ptr = params.builder->CreateGEP(
			input_array, // ptr
			indices
		);

		llvm::Value* elem = params.builder->CreateLoad(
			elem_ptr
		);

		// Call function on element
		vector<llvm::Value*> args;
		args.push_back(elem);
		if(true) // target_takes_voidptr_arg) // params.hidden_voidptr_arg)
			args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		llvm::Value* mapped_elem = params.builder->CreateCall(
			function, // Callee
			args, // Args
			"map function call" // Name
		);

		// Get pointer to output element
		llvm::Value* out_elem_ptr = params.builder->CreateGEP(
			return_ptr, // ptr
			indices
		);

		// Store the element in the output array
		return params.builder->CreateStore(
			mapped_elem, // value
			out_elem_ptr // ptr
		);
	}

	llvm::Value* return_ptr;
	llvm::Value* function;
	llvm::Value* input_array;
};


llvm::Value* ArrayMapBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Pointer to result array
	llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

	// Closure ptr
	/*llvm::Value* closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	// Get function ptr from closure ptr
	vector<llvm::Value*> indices(2);
	indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th closure
	indices[1] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1)); // get the 1st field (function ptr)
	
	llvm::Value* function_ptr = params.builder->CreateGEP(
		closure_ptr,
		indices
	);

	llvm::Value* function = params.builder->CreateLoad(function_ptr);*/
	llvm::Value* function = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	//llvm::Value* function_ptr = params.builder->CreateLoad(
	//	closure_ptr,
	//	vector<uint32_t>(1, 1) // 1st field (function ptr)
	//);

	// Input array
	llvm::Value* input_array = LLVMTypeUtils::getNthArg(params.currently_building_func, 2);




	llvm::Value* initial_value = return_ptr;
	
	ArrayMapBuiltInFunc_CreateLoopBodyCallBack callback;
	callback.return_ptr = return_ptr;
	callback.function = function;
	callback.input_array = input_array;
	

	return makeForLoop(
		params,
		from_type->num_elems, // num iterations
		from_type->elem_type->LLVMType(*params.context), // Loop value type
		//initial_value, // initial val
		&callback
	);
}


//------------------------------------------------------------------------------------


ValueRef ArrayFoldBuiltInFunc::invoke(VMState& vmstate)
{
	// fold(function<T, T, T> func, array<T> array, T initial val) T
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const ArrayValue* arr = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	const ValueRef initial_val = vmstate.argument_stack[vmstate.func_args_start.back() + 2];

	assert(f && arr && initial_val.nonNull());

	ValueRef running_val = initial_val;
	for(unsigned int i=0; i<arr->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(arr->e[i]); // Push value arg
		
		ValueRef new_running_val = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();

		//delete running_val;
		running_val = new_running_val;
	}

	return running_val;
}


llvm::Value* ArrayFoldBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


//------------------------------------------------------------------------------------


ArraySubscriptBuiltInFunc::ArraySubscriptBuiltInFunc(const Reference<ArrayType>& array_type_, const TypeRef& index_type_)
:	array_type(array_type_), index_type(index_type_)
{}


ValueRef ArraySubscriptBuiltInFunc::invoke(VMState& vmstate)
{
	// Array pointer is in arg 0.
	// Index or index vector is in arg 1.
	const ArrayValue* arr = static_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	if(index_type->getType() == Type::IntType)
	{
		const IntValue* index = static_cast<const IntValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		if(index->value >= 0 && index->value < arr->e.size())
			return arr->e[index->value];
		else
			return this->array_type->elem_type->getInvalidValue();
	}
	else
	{
		const VectorValue* index_vec = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		vector<ValueRef> res(index_vec->e.size());
		for(size_t i=0; i<index_vec->e.size(); ++i)
			res[i] = arr->e[index_vec->e[i].downcast<IntValue>()->value]; // TODO: bounds check

		return new ArrayValue(res);
	}
}


static llvm::Value* loadElement(EmitLLVMCodeParams& params, int arg_offset)
{
	llvm::Value* array_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	llvm::Value* indices[] = {
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the array
		index, // get the indexed element in the array
	};

	llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(
		array_ptr, // ptr
		indices
	);

	return params.builder->CreateLoad(
		elem_ptr
	);
}



// Returns a vector value
static llvm::Value* loadGatherElements(EmitLLVMCodeParams& params, int arg_offset, const TypeRef& array_elem_type, int index_vec_num_elems)
{
	llvm::Value* array_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index_vec = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// We have a single ptr, we need to shuffle it to an array of ptrs.

	std::cout << "array_ptr:" << std::endl;
	array_ptr->dump();
	std::cout << std::endl;
	std::cout << "array_ptr type:" << std::endl;
	array_ptr->getType()->dump();
	std::cout << std::endl;

	// TEMP: Get pointer to index 0 of the array:
	llvm::Value* array_elem0_ptr = params.builder->CreateConstInBoundsGEP2_32(array_ptr, 0, 0);

	std::cout << "array_elem0_ptr" << std::endl;
	array_elem0_ptr->dump();
	array_elem0_ptr->getType()->dump();
	std::cout << std::endl;

	//llvm::Value* shuffled_ptr = params.builder->CreateShuffleVector(

	llvm::Value* shuffled_ptr = params.builder->CreateVectorSplat(
		index_vec_num_elems,
		array_elem0_ptr
	);

	std::cout << "shuffled_ptr:" << std::endl;
	shuffled_ptr->dump();
	std::cout << std::endl;
	std::cout << "shuffled_ptr type:" << std::endl;
	shuffled_ptr->getType()->dump();
	std::cout << std::endl;

	std::cout << "index_vec:" << std::endl;
	index_vec->dump();//TEMP
	std::cout << std::endl;
	std::cout << "index_vec type:" << std::endl;
	index_vec->getType()->dump();//TEMP
	std::cout << std::endl;

	llvm::Value* indices[] = {
		//llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the array
		index_vec // get the indexed element in the array
	};

	llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(
		shuffled_ptr, // ptr
		indices
	);

	std::cout << "elem_ptr:" << std::endl;
	elem_ptr->dump();//TEMP
	std::cout << std::endl;
	std::cout << "elem_ptr type:" << std::endl;
	elem_ptr->getType()->dump();//TEMP
	std::cout << std::endl;


	// TEMP: LLVM does not currently support loading from a vector of pointers.  So just do the loads individually.

	/*return params.builder->CreateLoad(
		elem_ptr
	);*/

	// Start with a vector of Undefs.
	llvm::Value* vec = llvm::ConstantVector::getSplat(
		index_vec_num_elems,
		llvm::UndefValue::get(array_elem_type->LLVMType(*params.context))
	);

	for(int i=0; i<index_vec_num_elems; ++i)
	{
		// Emit code to load value i
		llvm::Value* val_i = params.builder->CreateLoad(
			params.builder->CreateExtractElement( // Get pointer i out of the elem_ptr vector
				elem_ptr, 
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, i))
			)
		);

		vec = params.builder->CreateInsertElement(
			vec, // vec
			val_i, // new element
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, i)) // index
		);
	}
	
	return vec;
}


/*
static llvm::Value* loadElements(EmitLLVMCodeParams& params, int arg_offset)
{
	llvm::Value* array_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	llvm::Value* indices[] = {
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the array
		index, // get the indexed element in the array
	};

	llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(
		array_ptr, // ptr
		indices
	);

	return params.builder->CreateLoad(
		elem_ptr
	);
}*/


llvm::Value* ArraySubscriptBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeRef field_type = this->array_type->elem_type;

	// Bounds check the index
	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	const bool do_bounds_check = false;//TEMP

	if(do_bounds_check)
	{
		// Code for out of bounds array access result.
		llvm::Value* out_of_bounds_val = field_type->getInvalidLLVMValue(*params.context);

		

		/*std::cout << "out_of_bounds_val:" << std::endl;
		out_of_bounds_val->dump();
		std::cout << "elem_val:" << std::endl;
		elem_val->dump();*/

		// Create bounds check condition code
		llvm::Value* condition = params.builder->CreateAnd(
			params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
			params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->array_type->num_elems, true))) // index < array num elems
		);



		// Get a pointer to the current function
		llvm::Function* the_function = params.builder->GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
		llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*params.context, "in-bounds", the_function);
		llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*params.context, "out-of-bounds");
		llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*params.context, "ifcont");

		params.builder->CreateCondBr(condition, ThenBB, ElseBB);

		// Emit then value.
		params.builder->SetInsertPoint(ThenBB);

		// Code for in-bounds access result
		llvm::Value* elem_val = loadElement(
			params, 
			arg_offset // arg offset - we have an sret arg.
		);

		params.builder->CreateBr(MergeBB);

		// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
		ThenBB = params.builder->GetInsertBlock();

		// Emit else block.
		the_function->getBasicBlockList().push_back(ElseBB);
		params.builder->SetInsertPoint(ElseBB);

		params.builder->CreateBr(MergeBB);

		// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
		ElseBB = params.builder->GetInsertBlock();


		// Emit merge block.
		the_function->getBasicBlockList().push_back(MergeBB);
		params.builder->SetInsertPoint(MergeBB);
		llvm::PHINode *PN = params.builder->CreatePHI(
			field_type->LLVMType(*params.context), //field_type->passByValue() ? field_type->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*field_type->LLVMType(*params.context)),
			0, // num reserved values
			"iftmp"
		);

		PN->addIncoming(elem_val, ThenBB);
		PN->addIncoming(out_of_bounds_val, ElseBB);

		llvm::Value* phi_result = PN;

		if(field_type->passByValue())
		{
			return phi_result;
		}
		else // Else if element type is pass-by-pointer
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Store the element
			params.builder->CreateStore(
				phi_result, // value
				return_ptr // ptr
			);
			return NULL;
		}
	}
	else // Else if no bounds check:
	{
		if(index_type->getType() == Type::IntType)
		{
			// Scalar index

			if(field_type->passByValue())
			{
				return loadElement(
					params, 
					0 // arg offset - zero as no sret zeroth arg.
				);
			}
			else // Else if element type is pass-by-pointer
			{
				// Pointer to memory for return value will be 0th argument.
				llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

				llvm::Value* elem_val = loadElement(
					params, 
					1 // arg offset - we have an sret arg.
				);

				// Store the element
				params.builder->CreateStore(
					elem_val, // value
					return_ptr // ptr
				);

				return NULL;
			}
		}
		else if(index_type->getType() == Type::ArrayTypeType);
		{
			// Gather (vector) index.
			// Since we are returning a vector, and we are assuming vectors are not pass by pointer, we know the return type is not pass by pointer.
			
			// Pointer to memory for return value will be 0th argument.
			//llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* result_vector = loadGatherElements(
				params, 
				0, // arg offset - we have an sret arg.
				this->array_type->elem_type,
				this->index_type.downcast<ArrayType>()->num_elems // index_vec_num_elems
			);

			//TEMP:
			result_vector->dump();
			//return_ptr->dump();

			// Store the element
			//params.builder->CreateStore(
			//	elem_val, // value
			//	return_ptr // ptr
			//);

			return result_vector;
		}
	}
}


//------------------------------------------------------------------------------------


VectorSubscriptBuiltInFunc::VectorSubscriptBuiltInFunc(const Reference<VectorType>& vec_type_, const TypeRef& index_type_)
:	vec_type(vec_type_), index_type(index_type_)
{}


ValueRef VectorSubscriptBuiltInFunc::invoke(VMState& vmstate)
{
	// Vector is in arg 0.
	// Index is in arg 1.
	const VectorValue* vec = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back()    ].getPointer());
	const IntValue* index  = static_cast<const IntValue*>   (vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	//if(index->value >= 0 && index->value < vec->e.size())
		return vec->e[index->value];
	//else
	//	return this->vec_type->elem_type->getInvalidValue();
}


llvm::Value* VectorSubscriptBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Vectors are pass-by-value.
	// Vector elements are also pass-by-value.

	// Bounds check the index
	llvm::Value* vec       = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	const bool do_bounds_check = false;//TEMP

	if(do_bounds_check)
	{
		// Code for out of bounds array access result.
		/*llvm::Value* out_of_bounds_val = vec_type->getInvalidLLVMValue(*params.context);

		// Create bounds check condition code
		llvm::Value* condition = params.builder->CreateAnd(
			params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
			params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->vec_type->num, true))) // index < array num elems
		);



		// Get a pointer to the current function
		llvm::Function* the_function = params.builder->GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
		llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*params.context, "in-bounds", the_function);
		llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*params.context, "out-of-bounds");
		llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*params.context, "ifcont");

		params.builder->CreateCondBr(condition, ThenBB, ElseBB);

		// Emit then value.
		params.builder->SetInsertPoint(ThenBB);

		// Code for in-bounds access result
		llvm::Value* elem_val = loadElement(
			params, 
			arg_offset // arg offset - we have an sret arg.
		);

		params.builder->CreateBr(MergeBB);

		// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
		ThenBB = params.builder->GetInsertBlock();

		// Emit else block.
		the_function->getBasicBlockList().push_back(ElseBB);
		params.builder->SetInsertPoint(ElseBB);

		params.builder->CreateBr(MergeBB);

		// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
		ElseBB = params.builder->GetInsertBlock();


		// Emit merge block.
		the_function->getBasicBlockList().push_back(MergeBB);
		params.builder->SetInsertPoint(MergeBB);
		llvm::PHINode *PN = params.builder->CreatePHI(
			field_type->LLVMType(*params.context), //field_type->passByValue() ? field_type->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*field_type->LLVMType(*params.context)),
			0, // num reserved values
			"iftmp"
		);

		PN->addIncoming(elem_val, ThenBB);
		PN->addIncoming(out_of_bounds_val, ElseBB);

		llvm::Value* phi_result = PN;

		if(field_type->passByValue())
		{
			return phi_result;
		}
		else // Else if element type is pass-by-pointer
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Store the element
			params.builder->CreateStore(
				phi_result, // value
				return_ptr // ptr
			);
			return NULL;
		}*/
		assert(0);
		return NULL;
	}
	else // Else if no bounds check:
	{
		return params.builder->CreateExtractElement(
			vec,
			index
		);
	}
}


//------------------------------------------------------------------------------------


ArrayInBoundsBuiltInFunc::ArrayInBoundsBuiltInFunc(const Reference<ArrayType>& array_type_, const TypeRef& index_type_)
:	array_type(array_type_), index_type(index_type_)
{}


ValueRef ArrayInBoundsBuiltInFunc::invoke(VMState& vmstate)
{
	// Array pointer is in arg 0.
	// Index is in arg 1.
	const ArrayValue* arr = static_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const IntValue* index = static_cast<const IntValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return new BoolValue(index->value >= 0 && index->value < arr->e.size());
}


llvm::Value* ArrayInBoundsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeRef field_type = this->array_type->elem_type;

	// Bounds check the index
	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// Create bounds check condition code
	return params.builder->CreateAnd(
		params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
		params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->array_type->num_elems, true))) // index < array num elems
	);
}


//------------------------------------------------------------------------------------


VectorInBoundsBuiltInFunc::VectorInBoundsBuiltInFunc(const Reference<VectorType>& vector_type_, const TypeRef& index_type_)
:	vector_type(vector_type_), index_type(index_type_)
{}


ValueRef VectorInBoundsBuiltInFunc::invoke(VMState& vmstate)
{
	// Vector pointer is in arg 0.
	// Index is in arg 1.
	const VectorValue* arr = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const IntValue* index = static_cast<const IntValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return new BoolValue(index->value >= 0 && index->value < arr->e.size());
}


llvm::Value* VectorInBoundsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeRef field_type = this->vector_type->elem_type;

	// Bounds check the index
	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// Create bounds check condition code
	return params.builder->CreateAnd(
		params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
		params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->vector_type->num, true))) // index < vector num elems
	);
}


//----------------------------------------------------------------------------------------------


ValueRef IfBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const Value* condition = vmstate.argument_stack[func_args_start].getPointer();
	assert(dynamic_cast<const BoolValue*>(condition));

	if(static_cast<const BoolValue*>(condition)->value) // If condition is true
	{
		return vmstate.argument_stack[func_args_start + 1];
	}
	else
	{
		return vmstate.argument_stack[func_args_start + 2];
	}
}


llvm::Value* IfBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM

	const int arg_offset = this->T->passByValue() ? 0 : 1;

	llvm::Value* condition_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 0 + arg_offset);


	llvm::Value* child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);
	llvm::Value* child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);

#if 0
	llvm::Value* child_a_code = NULL;
	llvm::Value* child_b_code = NULL;
	if(this->T->passByValue())
	{
		child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);
		child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);
	}
	else
	{

		child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);

		/*child_a_code = params.builder->CreateLoad(
			a_ptr
		);*/

		//child_a_code = params.builder->CreateStore(
		//	a_val, // value
		//	return_val_ptr // ptr
		//	);

		child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);

		/*child_b_code = params.builder->CreateLoad(
		b_ptr
		);*/

		//child_b_code = params.builder->CreateStore(
		//	b_val, // value
		//	return_val_ptr // ptr
		//	);
	}
#endif


	// Get a pointer to the current function
	llvm::Function* the_function = params.builder->GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*params.context, "then", the_function);
	llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*params.context, "else");
	llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*params.context, "ifcont");

	params.builder->CreateCondBr(condition_code, ThenBB, ElseBB);

	// Emit then value.
	params.builder->SetInsertPoint(ThenBB);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = params.builder->GetInsertBlock();

	// Emit else block.
	the_function->getBasicBlockList().push_back(ElseBB);
	params.builder->SetInsertPoint(ElseBB);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = params.builder->GetInsertBlock();


	// Emit merge block.
	the_function->getBasicBlockList().push_back(MergeBB);
	params.builder->SetInsertPoint(MergeBB);
	llvm::PHINode *PN = params.builder->CreatePHI(
		this->T->passByValue() ? this->T->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*this->T->LLVMType(*params.context)),
		0, // num reserved values
		"iftmp"
	);

	PN->addIncoming(child_a_code, ThenBB);
	PN->addIncoming(child_b_code, ElseBB);

	llvm::Value* phi_result = PN;

	if(this->T->passByValue())
		return phi_result;
	else
	{
		llvm::Value* arg_val = params.builder->CreateLoad(
			phi_result
		);

		llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		params.builder->CreateStore(
			arg_val, // value
			return_val_ptr // ptr
		);
		return NULL;
	}

#else
	return NULL;
#endif
}


//----------------------------------------------------------------------------------------------


ValueRef DotProductBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);

	FloatValue* res = new FloatValue(0.0f);

	for(unsigned int i=0; i<vector_type->num; ++i)
	{
		res->value += static_cast<const FloatValue*>(a->e[i].getPointer())->value + static_cast<const FloatValue*>(b->e[i].getPointer())->value;
	}

	return ValueRef(res);
}


llvm::Value* DotProductBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	// If have SSE4.1 and this is a 4-vector, using DPPS instruction
	if(this->vector_type->num == 4 && params.cpu_info->sse4_1)
	{
		// Emit dot product intrinsic
		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 255))); // SSE DPPS control bits

		llvm::Function* dot_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse41_dpps);

		// dot product intrinsic returns a 4-vector.
		llvm::Value* vector_res = params.builder->CreateCall(dot_func, args, "Vector_res");

		return params.builder->CreateExtractElement(
			vector_res, // vec
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)) // index
		);
	}
	else
	{
		// x = a[0] * b[0]
		llvm::Value* x = params.builder->CreateBinOp(
			llvm::Instruction::FMul, 
			params.builder->CreateExtractElement(a, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0))),
			params.builder->CreateExtractElement(b, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)))
		);
			
		for(unsigned int i=1; i<this->vector_type->num; ++i)
		{
			// y = a[i] * b[i]
			llvm::Value* y = params.builder->CreateBinOp(
				llvm::Instruction::FMul, 
				params.builder->CreateExtractElement(a, llvm::ConstantInt::get(*params.context, llvm::APInt(32, i))),
				params.builder->CreateExtractElement(b, llvm::ConstantInt::get(*params.context, llvm::APInt(32, i)))
			);

			// x = x + y
			x = params.builder->CreateBinOp(
				llvm::Instruction::FAdd, 
				x,
				y
			);
		}

		return x;
	}
}






//----------------------------------------------------------------------------------------------


class VectorMin_CreateLoopBodyCallBack : public CreateLoopBodyCallBack
{
public:
	virtual llvm::Value* emitLoopBody(EmitLLVMCodeParams& params, llvm::Value* loop_value_var, llvm::Value* i)
	{
		// Extract element i from vector 'a'.
		llvm::Value* vec_a_elem = params.builder->CreateExtractElement(
			vec_a, // vec
			i // index
		);

		// Extract element i from vector 'b'.
		llvm::Value* vec_b_elem = params.builder->CreateExtractElement(
			vec_b, // vec
			i // index
		);

		// TEMP: Add
		llvm::Value* elem_res = params.builder->CreateFAdd(
			vec_a_elem,
			vec_b_elem
		);

		// Insert in result vector
		return params.builder->CreateInsertElement(
			loop_value_var,
			elem_res,
			i // index
		);
	}

	llvm::Value* vec_a;
	llvm::Value* vec_b;
	//llvm::Value* vec_result;
};


ValueRef VectorMinBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);

	
	vector<ValueRef> res_values(vector_type->num);

	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
			const float y = static_cast<const FloatValue*>(b->e[i].getPointer())->value;
			res_values[i] = ValueRef(new FloatValue(x < y ? x : y));
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const IntValue*>(a->e[i].getPointer())->value;
			const float y = static_cast<const IntValue*>(b->e[i].getPointer())->value;
			res_values[i] = ValueRef(new IntValue(x > y ? x : y));
		}
	}
	else
	{
		assert(0);
	}

	return ValueRef(new VectorValue(res_values));
}


llvm::Value* VectorMinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	llvm::Value* condition;
	
	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		condition = params.builder->CreateFCmpOLT(a, b);
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		condition = params.builder->CreateICmpSLT(a, b);
	}
	else
	{
		assert(0);
	}

	return params.builder->CreateSelect(condition, a, b);

	// Start with a vector of Undefs.
	/*llvm::Value* initial_value = llvm::ConstantVector::getSplat(
		vector_type->num,
		llvm::UndefValue::get(vector_type->t->LLVMType(*params.context))
	);

	//TEMP:
	VectorMin_CreateLoopBodyCallBack callback;
	callback.vec_a = a;
	callback.vec_b = b;
	//callback.vec_result = initial_value;
	

	return makeForLoop(
		params,
		vector_type->num, // num iterations
		vector_type->LLVMType(*params.context), // Loop value type
		initial_value, // initial val
		&callback
	);*/



	/*if(params.cpu_info->sse1)
	{
		// emit dot product intrinsic

		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);

		llvm::Function* minps_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse_min_ps);

		return params.builder->CreateCall(minps_func, args);
	}
	else
	{
		assert(!"VectorMinBuiltInFunc::emitLLVMCode assumes sse");
		return NULL;
	}*/
}


//----------------------------------------------------------------------------------------------


ValueRef VectorMaxBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);


	vector<ValueRef> res_values(vector_type->num);

	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
			const float y = static_cast<const FloatValue*>(b->e[i].getPointer())->value;
			res_values[i] = ValueRef(new FloatValue(x > y ? x : y));
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const IntValue*>(a->e[i].getPointer())->value;
			const float y = static_cast<const IntValue*>(b->e[i].getPointer())->value;
			res_values[i] = ValueRef(new IntValue(x > y ? x : y));
		}
	}
	else
	{
		assert(0);
	}


	return ValueRef(new VectorValue(res_values));
}


llvm::Value* VectorMaxBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	llvm::Value* condition;
	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		condition = params.builder->CreateFCmpOGT(a, b);
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		condition = params.builder->CreateICmpSGT(a, b);
	}
	else
	{
		assert(0);
	}

	return params.builder->CreateSelect(condition, a, b);
}


//----------------------------------------------------------------------------------------------


ValueRef ShuffleBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* index_vec = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && index_vec);


	vector<ValueRef> res_values(index_vec->e.size());

	for(unsigned int i=0; i<index_vec->e.size(); ++i)
	{
		res_values[i] = a->e[ index_vec->e[i].downcast<IntValue>()->value ];
	}

	return ValueRef(new VectorValue(res_values));
}


llvm::Value* ShuffleBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* index_vec = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);


	llvm::Constant* mask;
	if(shuffle_mask.empty()) // if shuffle mask has not been set yet, just set to a zero mask vector of the final size.
	{
		assert(0);

		std::vector<llvm::Constant*> elems(index_type->num);
		for(size_t i=0; i<index_type->num; ++i)
			elems[i] = llvm::ConstantInt::get(
				*params.context, 
				llvm::APInt(
					32, // num bits
					0, // value
					true // signed
				)
			);

		mask = llvm::ConstantVector::get(elems);
	}
	else
	{
		std::vector<llvm::Constant*> elems(shuffle_mask.size());
		for(size_t i=0; i<shuffle_mask.size(); ++i)
			elems[i] = llvm::ConstantInt::get(
				*params.context, 
				llvm::APInt(
					32, // num bits
					shuffle_mask[i], // value
					true // signed
				)
			);

		mask = llvm::ConstantVector::get(elems);
	}

	// TEMP: just use 'a' for the second vector arg as well
	return params.builder->CreateShuffleVector(a, a, mask, "shuffle");
}


void ShuffleBuiltInFunc::setShuffleMask(const std::vector<int>& shuffle_mask_)
{
	shuffle_mask = shuffle_mask_;
}

//----------------------------------------------------------------------------------------------


PowBuiltInFunc::PowBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef PowBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const FloatValue* b = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return ValueRef(new FloatValue(std::pow(a->value, b->value)));
}


llvm::Value* PowBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	vector<llvm::Value*> args(2);
	args[0] = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	args[1] = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	vector<llvm::Type*> types(1, this->type->LLVMType(*params.context));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::pow, types);

	assert(func);
	assert(func->isIntrinsic());

	return params.builder->CreateCall(
		func,
		args
	);
}


//----------------------------------------------------------------------------------------------


static llvm::Value* emitUnaryIntrinsic(EmitLLVMCodeParams& params, const TypeRef& type, llvm::Intrinsic::ID id)
{
	assert(type->getType() == Type::FloatType || (type->getType() == Type::VectorTypeType));

	vector<llvm::Value*> args(1, LLVMTypeUtils::getNthArg(params.currently_building_func, 0));

	vector<llvm::Type*> types(1, type->LLVMType(*params.context));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, id, types);

	return params.builder->CreateCall(
		func,
		args
	);
}


SqrtBuiltInFunc::SqrtBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef SqrtBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return ValueRef(new FloatValue(std::sqrt(a->value)));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
			res_values[i] = ValueRef(new FloatValue(std::sqrt(x)));
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* SqrtBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	//return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::x86_sse_sqrt_ps);
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::sqrt);
}


//----------------------------------------------------------------------------------------------


ExpBuiltInFunc::ExpBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef ExpBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::exp(a->value)));
}


llvm::Value* ExpBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::exp);
}


//----------------------------------------------------------------------------------------------


LogBuiltInFunc::LogBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef LogBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::log(a->value)));
}


llvm::Value* LogBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::log);
}


//----------------------------------------------------------------------------------------------


SinBuiltInFunc::SinBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef SinBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::sin(a->value)));
}


llvm::Value* SinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::sin);
}


//----------------------------------------------------------------------------------------------


CosBuiltInFunc::CosBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef CosBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::cos(a->value)));
}


llvm::Value* CosBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::cos);
}


//----------------------------------------------------------------------------------------------


AbsBuiltInFunc::AbsBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef AbsBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::fabs(a->value)));
}


llvm::Value* AbsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::fabs);
}


//----------------------------------------------------------------------------------------------


FloorBuiltInFunc::FloorBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef FloorBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return ValueRef(new FloatValue(std::floor(a->value)));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
			res_values[i] = ValueRef(new FloatValue(std::floor(x)));
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* FloorBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::floor);
}


//----------------------------------------------------------------------------------------------


CeilBuiltInFunc::CeilBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


ValueRef CeilBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return ValueRef(new FloatValue(std::ceil(a->value)));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = static_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
			res_values[i] = ValueRef(new FloatValue(std::ceil(x)));
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* CeilBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::ceil);
}


//----------------------------------------------------------------------------------------------


TruncateToIntBuiltInFunc::TruncateToIntBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


TypeRef TruncateToIntBuiltInFunc::getReturnType(const TypeRef& arg_type)
{
	if(arg_type->getType() == Type::FloatType)
		return new Int();
	else if(arg_type->getType() == Type::VectorTypeType) // If vector of floats
		return new VectorType(new Int(), arg_type.downcast<VectorType>()->num);
	else
	{
		assert(0);
		return NULL;
	}
}


ValueRef TruncateToIntBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new IntValue((int)a->value));
}


llvm::Value* TruncateToIntBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Work out destination type.
	TypeRef dest_type = getReturnType(type);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.context);

	return params.builder->CreateFPToSI(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type
	);
}


//----------------------------------------------------------------------------------------------


ToFloatBuiltInFunc::ToFloatBuiltInFunc(const TypeRef& type_)
:	type(type_)
{}


TypeRef ToFloatBuiltInFunc::getReturnType(const TypeRef& arg_type)
{
	if(arg_type->getType() == Type::IntType)
		return new Float();
	else if(arg_type->getType() == Type::VectorTypeType) // If vector of ints
		return new VectorType(new Float(), arg_type.downcast<VectorType>()->num);
	else
	{
		assert(0);
		return NULL;
	}
}


ValueRef ToFloatBuiltInFunc::invoke(VMState& vmstate)
{
	const IntValue* a = static_cast<const IntValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue((float)a->value));
}


llvm::Value* ToFloatBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Work out destination type.
	TypeRef dest_type = getReturnType(type);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.context);

	return params.builder->CreateSIToFP(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


//----------------------------------------------------------------------------------------------


//ValueRef AllocateRefCountedStructure::invoke(VMState& vmstate)
//{
//	assert(0);
//	return ValueRef();
//	//const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
//
//	//return ValueRef(new FloatValue(std::cos(a->value)));
//}
//
//AllocateRefCountedStructure::emitLLVMCode(EmitLLVMCodeParams& params) const
//{
//	//return emitFloatFloatIntrinsic(params, llvm::Intrinsic::cos);
//}


}
