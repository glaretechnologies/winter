#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "VirtualMachine.h"
#include "Value.h"
#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_FunctionExpression.h"
#include "wnt_RefCounting.h"
#include "wnt_LLVMVersion.h"
#include <vector>
#include "LLVMTypeUtils.h"
#include "utils/PlatformUtils.h"
#include "utils/StringUtils.h"
#include "utils/TaskManager.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#if TARGET_LLVM_VERSION <= 34
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#endif
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;


namespace Winter
{


//----------------------------------------------------------------------------------------------


class CreateLoopBodyCallBack
{
public:
	virtual ~CreateLoopBodyCallBack(){}
	virtual llvm::Value* emitLoopBody(llvm::IRBuilder<>& Builder, llvm::Module* module, /*llvm::Value* loop_value_var, */llvm::Value* loop_iter_val) = 0;
};



/*

Make a for loop.

PreheaderBB
--------------
jump to condition_BB


condition_BB
------------
cond = compute condition

cond branch(cond, loop_body_BB, after_BB)


loop_body_BB
-------------
do loop body

increment loop index var

jump to condition BB


after_BB
---------
*/


static void makeForLoop(llvm::IRBuilder<>& builder, llvm::Module* module, llvm::Value* begin_index, llvm::Value* end_index, CreateLoopBodyCallBack* create_loop_body_callback)
{
	// Make the new basic block for the loop header, inserting after current block.
	llvm::Function* current_func = builder.GetInsertBlock()->getParent();
	llvm::BasicBlock* preheader_BB = builder.GetInsertBlock();
	llvm::BasicBlock* condition_BB = llvm::BasicBlock::Create(module->getContext(), "condition", current_func);
	llvm::BasicBlock* loop_body_BB = llvm::BasicBlock::Create(module->getContext(), "loop_body", current_func);
	llvm::BasicBlock* after_BB = llvm::BasicBlock::Create(module->getContext(), "after_loop", current_func);

	builder.SetInsertPoint(preheader_BB);
	// Insert an explicit fall through from the current block to the condition_BB.
	builder.CreateBr(condition_BB);

	//============================= condition_BB ================================
	builder.SetInsertPoint(condition_BB);

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = builder.CreatePHI(llvm::Type::getInt64Ty(module->getContext()), 2, "loop_index_var");
	loop_index_var->addIncoming(begin_index, preheader_BB);

	// Compute the end condition.
	llvm::Value* end_cond = builder.CreateICmpNE(end_index, loop_index_var, "loopcond");

	// Insert the conditional branch
	builder.CreateCondBr(end_cond, loop_body_BB, after_BB);


	//============================= loop_body_BB ================================
	builder.SetInsertPoint(loop_body_BB);

	// TODO: deal with result?
	create_loop_body_callback->emitLoopBody(builder, module, loop_index_var);

	llvm::Value* one = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1));
	llvm::Value* next_var = builder.CreateAdd(loop_index_var, one, "next_var");

	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, loop_body_BB);

	// Do jump back to condition basic block
	builder.CreateBr(condition_BB);
	
	//============================= after_BB ================================
	builder.SetInsertPoint(after_BB);
}


#if 0
static llvm::Value* makeForLoop(llvm::IRBuilder<>& builder, llvm::Module* module, llvm::Value* begin_index, llvm::Value* end_index, llvm::Type* loop_value_type, /*llvm::Value* initial_value, */
								CreateLoopBodyCallBack* create_loop_body_callback)
{
	// Make the new basic block for the loop header, inserting after current
	// block.
	llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();
	llvm::BasicBlock* PreheaderBB = Builder.GetInsertBlock();
	llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(module->getContext(), "loop", TheFunction);
  
	// Insert an explicit fall through from the current block to the LoopBB.
	Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder.SetInsertPoint(LoopBB);
  
	
	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = Builder.CreatePHI(llvm::Type::getInt64Ty(module->getContext()), 2, "loop_index_var");
	//llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)); // Initial induction loop index value: Zero
	loop_index_var->addIncoming(begin_index, PreheaderBB);

	// Create loop body/value variable phi node
	//llvm::PHINode* loop_value_var = Builder.CreatePHI(loop_value_type, 2, "loop_value_var");
	//loop_value_var->addIncoming(initial_value, PreheaderBB);
  
	// Emit the body of the loop.
	llvm::Value* updated_value = create_loop_body_callback->emitLoopBody(Builder, module, /*loop_value_var, */loop_index_var);
  
	// Create increment of loop index
	llvm::Value* step_val = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1));
	llvm::Value* next_var = Builder.CreateAdd(loop_index_var, step_val, "next_var");

	// Compute the end condition.
	llvm::Value* end_value = end_index; // llvm::ConstantInt::get(*params.context, llvm::APInt(32, num_iterations));//TEMP HACK
  
	llvm::Value* end_cond = Builder.CreateICmpNE(
		end_value, 
		next_var,
		"loopcond"
	);
  
	// Create the "after loop" block and insert it.
	llvm::BasicBlock* LoopEndBB = Builder.GetInsertBlock();
	llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(module->getContext(), "afterloop", TheFunction);
  
	// Insert the conditional branch into the end of LoopEndBB.
	Builder.CreateCondBr(end_cond, LoopBB, AfterBB);
  
	// Any new code will be inserted in AfterBB.
	Builder.SetInsertPoint(AfterBB);
  
	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, LoopEndBB);

	//loop_value_var->addIncoming(updated_value, LoopEndBB);
  
	return updated_value;


}
#endif


//----------------------------------------------------------------------------------------------



Constructor::Constructor(VRef<StructureType>& struct_type_)
:	BuiltInFunctionImpl(BuiltInType_Constructor),
	struct_type(struct_type_)
{
}


ValueRef Constructor::invoke(VMState& vmstate)
{
	vector<ValueRef> field_values(this->struct_type->component_names.size());

	const size_t func_args_start = vmstate.func_args_start.back();
	
	for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		field_values[i] = vmstate.argument_stack[func_args_start + i];

	return new StructureValue(field_values);
}


llvm::Value* Constructor::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(this->struct_type->passByValue())
	{
		// Structs are not passed by value
		assert(0);

		llvm::Value* s = llvm::UndefValue::get(this->struct_type->LLVMType(*params.module));

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
			llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, i);

			llvm::Value* arg_value_or_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, i + 1);

			if(this->struct_type->component_types[i]->passByValue())
			{
				params.builder->CreateStore(arg_value_or_ptr, field_ptr);
			}
			else
			{
				LLVMTypeUtils::createCollectionCopy(
					this->struct_type->component_types[i], 
					field_ptr, // dest ptr
					arg_value_or_ptr, // src ptr
					params
				);
			}

			// If the field is a ref-counted type, we need to increment its reference count, since the newly constructed struct now holds a reference to it. 
			// (and to compensate for the decrement of the argument in the function application code)
			this->struct_type->component_types[i]->emitIncrRefCount(params, arg_value_or_ptr, "Constructor::emitLLVMCode() for type " + this->struct_type->toString());
		}

		return NULL;
	}
}


ValueRef GetField::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	// Top param on arg stack should be a structure
	const StructureValue* s = checkedCast<const StructureValue>(vmstate.argument_stack[func_args_start].getPointer());

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
		const TypeVRef field_type = this->struct_type->component_types[this->index];
		const std::string field_name = this->struct_type->component_names[this->index];

		if(field_type->passByValue())
		{
			// Pointer to structure will be in 0th argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, this->index, field_name + " ptr");

			llvm::Value* loaded_val = params.builder->CreateLoad(
				field_ptr,
				field_name // name
			);

			// TEMP NEW: increment ref count if this is a string
			//if(field_type->getType() == Type::StringType)
			//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

			return loaded_val;
		}
		else
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

			llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, this->index, field_name + " ptr");

			LLVMTypeUtils::createCollectionCopy(
				field_type, 
				return_ptr, // dest ptr
				field_ptr, // src ptr
				params
			);

			return NULL;
		}
	}
}


//------------------------------------------------------------------------------------


UpdateElementBuiltInFunc::UpdateElementBuiltInFunc(const TypeVRef& collection_type_)
:	BuiltInFunctionImpl(BuiltInType_UpdateElementBuiltInFunc),
	collection_type(collection_type_)
{}


// def update(CollectionType c, int index, T newval) CollectionType
ValueRef UpdateElementBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	//const Value* collection = vmstate.argument_stack[func_args_start].getPointer();

	const IntValue* inv_val = checkedCast<const IntValue>(vmstate.argument_stack[func_args_start + 1].getPointer());
	const int64 index = inv_val->value;

	const ValueRef newval = vmstate.argument_stack[func_args_start + 2];

	if(collection_type->getType() == Type::ArrayTypeType)
	{
		const ArrayValue* array_val = checkedCast<const ArrayValue>(vmstate.argument_stack[func_args_start]);

		if(index < 0 || index >= (int64)array_val->e.size())
			throw BaseException("Index out of bounds");

		ValueRef new_collection = array_val->clone();
		static_cast<ArrayValue*>(new_collection.getPointer())->e[index] = newval;

		return new_collection;
	}
	else
	{
		// TODO: handle other types.
		throw BaseException("invalid type");
	}
}


// def update(CollectionType c, int index, T newval) CollectionType
llvm::Value* UpdateElementBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(collection_type->getType() == Type::ArrayTypeType)
	{
		// Pointer to memory for return value will be 0th argument.
		llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		// Pointer to structure will be in 1st argument.
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

		// Index will be in 2nd argument.
		llvm::Value* index = LLVMTypeUtils::getNthArg(params.currently_building_func, 2);

		// New val will be in 3rd argument.  TEMP: assuming pass by value.
		llvm::Value* newval = LLVMTypeUtils::getNthArg(params.currently_building_func, 3);


		// Copy old collection to new collection
//		llvm::Value* collection_val = params.builder->CreateLoad(struct_ptr, "collection val");
//		params.builder->CreateStore(collection_val, return_ptr);
	
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * collection_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
		params.builder->CreateMemCpy(return_ptr, struct_ptr, size, 4);

		// Update element with new val
		vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th array
		indices[1] = index; // get the indexed element in the array
		llvm::Value* new_elem_ptr = params.builder->CreateInBoundsGEP(return_ptr, indices, "new elem ptr");

		params.builder->CreateStore(newval, new_elem_ptr);
	}

	return NULL;
}


//------------------------------------------------------------------------------------


ValueRef GetTupleElementBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	// Top param on arg stack should be a tuple
	const TupleValue* s = checkedCast<const TupleValue>(vmstate.argument_stack[func_args_start].getPointer());

	if(index >= s->e.size())
		throw BaseException("Index out of bounds");

	return s->e[this->index];
}


llvm::Value* GetTupleElementBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(this->tuple_type->passByValue())
	{
		assert(0);
		//return params.builder->CreateExtractValue(
		//	LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
		//	this->index,
		//	this->tuple_type->component_names[this->index] // name
		//);
		return NULL;
	}
	else
	{
		const TypeVRef field_type = this->tuple_type->component_types[this->index];
		const std::string field_name = "field " + ::toString(this->index);

		if(field_type->passByValue())
		{
			// Pointer to structure will be in 0th argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, this->index, field_name + " ptr");

			llvm::Value* loaded_val = params.builder->CreateLoad(
				field_ptr,
				field_name // name
			);

			// TEMP NEW: increment ref count if this is a string
			//if(field_type->getType() == Type::StringType)
			//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

			return loaded_val;
		}
		else
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

			llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, this->index, field_name + " ptr");

			LLVMTypeUtils::createCollectionCopy(
				field_type, 
				return_ptr, // dest ptr
				field_ptr, // src ptr
				params
			);

			return NULL;
		}
	}
}


//------------------------------------------------------------------------------------


ValueRef GetVectorElement::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const VectorValue* vec = checkedCast<const VectorValue>(vmstate.argument_stack[func_args_start].getPointer());

	if(this->index >= vec->e.size())
		throw BaseException("Index out of bounds");

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



void ArrayMapBuiltInFunc::specialiseForFunctionArg(FunctionDefinition* f)
{
	specialised_f = f;
}


ValueRef ArrayMapBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const FunctionValue* f = checkedCast<const FunctionValue>(vmstate.argument_stack[func_args_start].getPointer());
	const ArrayValue* from = checkedCast<const ArrayValue>(vmstate.argument_stack[func_args_start + 1].getPointer());

	ArrayValueRef retval = new ArrayValue();
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

	return retval;
}


class ArrayMapBuiltInFunc_CreateLoopBodyCallBack : public CreateLoopBodyCallBack
{
public:
	virtual llvm::Value* emitLoopBody(llvm::IRBuilder<>& builder, llvm::Module* module, /*llvm::Value* loop_value_var, */llvm::Value* i)
	{
		// Load element from input array
		vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)); // get the zero-th array
		indices[1] = i; // get the indexed element in the array

		llvm::Value* elem_ptr = builder.CreateInBoundsGEP(input_array, indices);
		llvm::Value* elem = builder.CreateLoad(elem_ptr);

		llvm::Value* args[2] = { elem, captured_var_struct_ptr };

		// Call function on element
		llvm::Value* mapped_elem = builder.CreateCall(
			function, // Callee
			args, // Args
			"map function call" // Name
		);

		// Get pointer to output element
		llvm::Value* out_elem_ptr = builder.CreateInBoundsGEP(return_ptr, indices);
		return builder.CreateStore(mapped_elem, out_elem_ptr); // Store the element in the output array
	}

	llvm::Value* return_ptr;
	llvm::Value* function;
	llvm::Value* captured_var_struct_ptr;
	llvm::Value* input_array;
};


// This function computes the map on a slice of the array given by [begin, end).
// typedef void (WINTER_JIT_CALLING_CONV * ARRAY_WORK_FUNCTION) (void* output, void* input, void* map_function, size_t begin, size_t end); // Winter code
llvm::Value* ArrayMapBuiltInFunc::insertWorkFunction(EmitLLVMCodeParams& params) const
{
	llvm::Type* int64_type = llvm::IntegerType::get(*params.context, 64);

	std::vector<llvm::Type*> arg_types(2, LLVMTypeUtils::pointerType(this->from_type->LLVMType(*params.module))); // output, input
	arg_types.push_back(this->func_type->LLVMType(*params.module)); // map_function
	arg_types.push_back(int64_type); // size_t begin
	arg_types.push_back(int64_type); // size_t end

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(params.module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Constant* llvm_func_constant = params.module->getOrInsertFunction(
		"work_function", // Name
		functype // Type
	);

	assert(llvm::isa<llvm::Function>(llvm_func_constant));
	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);
	llvm::BasicBlock* block = llvm::BasicBlock::Create(params.module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(block);
		

	ArrayMapBuiltInFunc_CreateLoopBodyCallBack callback;
	callback.return_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

	// If we have a specialised function to use, insert a call to it directly, else used the function pointer passed in as an argument.
	if(this->specialised_f)
		callback.function = this->specialised_f->getOrInsertFunction(params.module, false);
	else
		callback.function = LLVMTypeUtils::getNthArg(llvm_func, 2);

	callback.input_array = LLVMTypeUtils::getNthArg(llvm_func, 1);

	//llvm_func->addAttribute(1, llvm::Attribute::getWithAlignment(*params.context, 3));
	// Set some attributes

	llvm::Function::arg_iterator AI = llvm_func->arg_begin();
	{
		llvm::AttrBuilder attr_builder;
		attr_builder.addAlignmentAttr(32);
		attr_builder.addAttribute(llvm::Attribute::NoAlias);
		llvm::AttributeSet set = llvm::AttributeSet::get(*params.context, 1, attr_builder);
		AI->addAttr(set);
	}

	AI++;

	{
		llvm::AttrBuilder attr_builder;
		attr_builder.addAlignmentAttr(32);
		attr_builder.addAttribute(llvm::Attribute::NoAlias);
		llvm::AttributeSet set = llvm::AttributeSet::get(*params.context, 1, attr_builder);
		AI->addAttr(set);
	}
	

	makeForLoop(
		builder,
		params.module,
		llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), // TEMP BEGIN=0    LLVMTypeUtils::getNthArg(llvm_func, 3), // begin index
		LLVMTypeUtils::getNthArg(llvm_func, 4), // end index
		//from_type->elem_type->LLVMType(*params.module), // Loop value type
		&callback
	);
	

	builder.CreateRetVoid();

	return llvm_func;
}


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

	llvm::Value* closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);
	llvm::Value* function_ptr = params.builder->CreateStructGEP(closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = params.builder->CreateStructGEP(closure_ptr, Function::capturedVarStructIndex());


	//llvm::Value* function_ptr = params.builder->CreateLoad(
	//	closure_ptr,
	//	vector<uint32_t>(1, 1) // 1st field (function ptr)
	//);

	// Input array
	llvm::Value* input_array = LLVMTypeUtils::getNthArg(params.currently_building_func, 2);




	//llvm::Value* initial_value = return_ptr;
	const bool do_multithreaded_map = false;
	if(do_multithreaded_map)
	{
		llvm::Value* work_function = insertWorkFunction(params);

		// Want to call back into function 
		//
		// void execArrayMap(void* output, void* input, size_t array_size, void* map_function, ARRAY_WORK_FUNCTION work_function)
		//
		// from host code

		llvm::Type* voidptr = LLVMTypeUtils::voidPtrType(*params.context);

		std::vector<llvm::Type*> arg_types(2, LLVMTypeUtils::voidPtrType(*params.context)); // void* output, void* input
		const TypeVRef int64_type = new Int(64);
		arg_types.push_back(int64_type->LLVMType(*params.module)); // array_size
		arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context)); // map_function
		arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context)); // work_function


		llvm::FunctionType* functype = llvm::FunctionType::get(
			llvm::Type::getVoidTy(*params.context), // return type
			arg_types,
			false // varargs
		);

		llvm::Constant* llvm_func_constant = params.module->getOrInsertFunction("execArrayMap", functype);

		llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

		vector<llvm::Value*> args;
		args.push_back(params.builder->CreatePointerCast(return_ptr, voidptr)); // output
		args.push_back(params.builder->CreatePointerCast(input_array, voidptr)); // input
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(64, this->from_type->num_elems))); // array_size
		args.push_back(params.builder->CreatePointerCast(function, voidptr)); // map_function

		//TODO: captured_var_struct_ptr stuff

		args.push_back(params.builder->CreatePointerCast(work_function, voidptr)); // work_function
		params.builder->CreateCall(llvm_func, args);

		return return_ptr;
	}
	else
	{
		ArrayMapBuiltInFunc_CreateLoopBodyCallBack callback;
		callback.return_ptr = return_ptr;
		callback.function = function;
		callback.captured_var_struct_ptr = captured_var_struct_ptr;
		callback.input_array = input_array;
	
		makeForLoop(
			*params.builder,
			params.module,
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), // begin index
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, from_type->num_elems)), // end index
			//from_type->elem_type->LLVMType(*params.module), // Loop value type
			&callback
		);

		return return_ptr;
	}
}


//------------------------------------------------------------------------------------


ArrayFoldBuiltInFunc::ArrayFoldBuiltInFunc(const VRef<Function>& func_type_, const VRef<ArrayType>& array_type_, const TypeVRef& state_type_)
:	BuiltInFunctionImpl(BuiltInType_ArrayFoldBuiltInFunc),
	func_type(func_type_), array_type(array_type_), state_type(state_type_), specialised_f(NULL)
{}


void ArrayFoldBuiltInFunc::specialiseForFunctionArg(FunctionDefinition* f)
{
	specialised_f = f;
}


ValueRef ArrayFoldBuiltInFunc::invoke(VMState& vmstate)
{
	// fold(function<State, T, State> f, array<T> array, State initial_state) State

	const FunctionValue* f = checkedCast<const FunctionValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const ArrayValue* arr = checkedCast<const ArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
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

		running_val = new_running_val;
	}

	return running_val;
}


// fold(function<State, T, State> f, array<T> array, State initial_state) State
llvm::Value* ArrayFoldBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Get argument pointers/values
	llvm::Value* return_ptr = NULL;
	llvm::Value* closure_ptr;
	llvm::Value* array_arg;
	llvm::Value* initial_state_ptr_or_value;

	if(state_type->passByValue())
	{
		closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0); // Pointer to function
		array_arg = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);
		initial_state_ptr_or_value = LLVMTypeUtils::getNthArg(params.currently_building_func, 2); // Pointer to, or value of initial state
	}
	else
	{
		return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0); // Pointer to result structure
		closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1); // Pointer to function
		array_arg = LLVMTypeUtils::getNthArg(params.currently_building_func, 2);
		initial_state_ptr_or_value = LLVMTypeUtils::getNthArg(params.currently_building_func, 3); // Pointer to, or value of initial state
	}

	llvm::Value* function_ptr = params.builder->CreateStructGEP(closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = params.builder->CreateStructGEP(closure_ptr, Function::capturedVarStructIndex());


	// Emit the alloca in the entry block for better code-gen.
	// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
	llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

	//=======================================Begin specialisation ====================================================

	// TODO: check args to update as well to make sure it is bound to correct update etc..
	// Also check first arg of update is bound to first arg to specialised_f.
	if(specialised_f && specialised_f->body->nodeType() == ASTNode::FunctionExpressionType && specialised_f->body.downcast<FunctionExpression>()->static_function_name == "update")
	{
		llvm::Value* running_state_alloca = entry_block_builder.CreateAlloca(
			state_type->LLVMType(*params.module), // State
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"Running state"
		);

		// Copy initial state to running state alloca
		if(state_type->passByValue())
		{
			params.builder->CreateStore(initial_state_ptr_or_value, running_state_alloca);
		}
		else
		{
			if(state_type->getType() == Type::ArrayTypeType)
			{
				llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
				params.builder->CreateMemCpy(running_state_alloca, initial_state_ptr_or_value, size, 4);
			}
			else
			{
				params.builder->CreateStore(params.builder->CreateLoad(initial_state_ptr_or_value), running_state_alloca);
			}
		}

		// Make the new basic block for the loop header, inserting after current block.
		llvm::Function* TheFunction = params.builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* PreheaderBB = params.builder->GetInsertBlock();
		llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*params.context, "loop", TheFunction);
  
		// Insert an explicit fall through from the current block to the LoopBB.
		params.builder->CreateBr(LoopBB);

		// Start insertion in LoopBB.
		params.builder->SetInsertPoint(LoopBB);


		// Create loop index (i) variable phi node
		llvm::PHINode* loop_index_var = params.builder->CreatePHI(llvm::Type::getInt32Ty(*params.context), 2, "loop_index_var");
		llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // Initial induction loop index value: Zero
		loop_index_var->addIncoming(initial_loop_index_value, PreheaderBB);


		//=========================== Emit the body of the loop. =========================

		// fold(function<State, T, State> f, array<T> array, State initial_state) State

		//TEMP: assuming array elements (T) are pass by value.
		assert(array_type->elem_type->passByValue());
		vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th array
		indices[1] = loop_index_var; // get the indexed element in the array

		llvm::Value* array_elem_ptr = params.builder->CreateInBoundsGEP(array_arg, indices, "array elem ptr");
		llvm::Value* array_elem = params.builder->CreateLoad(array_elem_ptr, "array elem");

		// Set up params.argument_values to override the existing values.
		params.argument_values.resize(2);
		params.argument_values[0] = running_state_alloca; // current state
		params.argument_values[1] = array_elem; // array element


		// Instead of calling function 'f', just emit f's body code, without the update.
		// so update(current_state, index, new_value)
		// becomes
		// running_state[index] = new_value
		FunctionExpressionRef update_func_expr = specialised_f->body.downcast<FunctionExpression>();
		ASTNodeRef index_expr = update_func_expr->argument_expressions[1];
		ASTNodeRef new_value_expr = update_func_expr->argument_expressions[2];

		llvm::Value* index_llvm_val = index_expr->emitLLVMCode(params);
		llvm::Value* new_value_llvm_val = new_value_expr->emitLLVMCode(params); // TEMP: assuming pass by value

		// Store the new value to running_state_alloca at the correct index
		indices[1] = index_llvm_val;
		llvm::Value* target_elem_ptr = params.builder->CreateInBoundsGEP(running_state_alloca, indices, "target elem ptr");

		params.builder->CreateStore(new_value_llvm_val, target_elem_ptr);


		params.argument_values.resize(0); // Reset


		// Create increment of loop index
		llvm::Value* step_val = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1));
		llvm::Value* next_var = params.builder->CreateAdd(loop_index_var, step_val, "next_var");

		// Compute the end condition.
		llvm::Value* end_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, array_type->num_elems));
  
		llvm::Value* end_cond = params.builder->CreateICmpNE(
			end_value, 
			next_var,
			"loopcond"
		);
  
		// Create the "after loop" block and insert it.
		llvm::BasicBlock* LoopEndBB = params.builder->GetInsertBlock();
		llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*params.context, "afterloop", TheFunction);
  
		// Insert the conditional branch into the end of LoopEndBB.
		params.builder->CreateCondBr(end_cond, LoopBB, AfterBB);
  
		// Any new code will be inserted in AfterBB.
		params.builder->SetInsertPoint(AfterBB);
  
		// Add a new entry to the PHI node for the backedge.
		loop_index_var->addIncoming(next_var, LoopEndBB);
	
		
		if(state_type->passByValue())
		{
			// The running state needs to be loaded from running_state_alloca and returned directly.
			return params.builder->CreateLoad(running_state_alloca);
		}
		else
		{
			// Finally load and store the running state value to the SRET return ptr.
			if(state_type->getType() == Type::ArrayTypeType)
			{
				llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
				params.builder->CreateMemCpy(return_ptr, running_state_alloca, size, 4);
				return return_ptr;
			}
			else
			{
				params.builder->CreateStore(params.builder->CreateLoad(running_state_alloca), return_ptr);
				return return_ptr;
			}
		}
	}
	//======================================= End specialisation ====================================================


	// Allocate space on stack for the running state, if the state type is not pass-by-value.
	llvm::Value* new_state_alloca = NULL;
	llvm::Value* running_state_alloca = NULL;
	//if(!state_type->passByValue())
	{
		new_state_alloca = entry_block_builder.CreateAlloca(
			state_type->LLVMType(*params.module), // State
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"New running state"
		);

		running_state_alloca = entry_block_builder.CreateAlloca(
			state_type->LLVMType(*params.module), // State
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"Running state"
		);

		if(state_type->passByValue())
		{
			params.builder->CreateStore(initial_state_ptr_or_value, new_state_alloca); // running_state_alloca);
		}
		else
		{
			// Load and store initial state in new state // running state
			if(state_type->getType() == Type::ArrayTypeType)
			{
				llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
				//params.builder->CreateMemCpy(running_state_alloca, initial_state_ptr_or_value, size, 4);
				params.builder->CreateMemCpy(new_state_alloca, initial_state_ptr_or_value, size, 4);
			}
			else
			{
				llvm::Value* initial_state = params.builder->CreateLoad(initial_state_ptr_or_value);
				params.builder->CreateStore(initial_state, new_state_alloca); // running_state_alloca);
			}
		}
	}
	

	// Make the new basic block for the loop header, inserting after current
	// block.
	llvm::Function* TheFunction = params.builder->GetInsertBlock()->getParent();
	llvm::BasicBlock* PreheaderBB = params.builder->GetInsertBlock();
	llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*params.context, "loop", TheFunction);
  
	// Insert an explicit fall through from the current block to the LoopBB.
	params.builder->CreateBr(LoopBB);

	// Start insertion in LoopBB.
	params.builder->SetInsertPoint(LoopBB);

	// Create running state value variable phi node
	/*llvm::PHINode* running_state_value = NULL;
	if(state_type->passByValue())
	{
		running_state_value = params.builder->CreatePHI(state_type->LLVMType(*params.context), 2, "running_state_value");
		running_state_value->addIncoming(initial_state_ptr_or_value, PreheaderBB);
	}*/
  
	

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = params.builder->CreatePHI(llvm::Type::getInt32Ty(*params.context), 2, "loop_index_var");
	llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // Initial induction loop index value: Zero
	loop_index_var->addIncoming(initial_loop_index_value, PreheaderBB);


	//=========================== Emit the body of the loop. =========================
	// For now, the state at the beginning and end of the loop will be in new_state_alloca.

	// fold(function<State, T, State> f, array<T> array, State initial_state) State

	//TEMP: assuming array elements (T) are pass by value.
	assert(array_type->elem_type->passByValue());
	vector<llvm::Value*> indices(2);
	indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th array
	indices[1] = loop_index_var; // get the indexed element in the array

	llvm::Value* array_elem_ptr = params.builder->CreateInBoundsGEP(array_arg, indices, "array elem ptr");
	llvm::Value* array_elem = params.builder->CreateLoad(array_elem_ptr, "array elem");

	// Copy the state from new_state_alloca to running_state_alloca
	if(state_type->getType() == Type::ArrayTypeType)
	{
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
		params.builder->CreateMemCpy(running_state_alloca, new_state_alloca, size, 4);
	}
	else
	{
		llvm::Value* state = params.builder->CreateLoad(new_state_alloca); // Load the state from new_state_alloca
		params.builder->CreateStore(state, running_state_alloca); // Store the state in running_state_alloca
	}


	if(state_type->passByValue())
	{
		// Load running state
		llvm::Value* running_state_value = params.builder->CreateLoad(running_state_alloca);

		// Call function on element
		llvm::Value* next_running_state_value = params.builder->CreateCall3(function, running_state_value, array_elem, captured_var_struct_ptr);

		// Store new value in running_state_alloca
		params.builder->CreateStore(next_running_state_value, new_state_alloca); // running_state_alloca);
	}
	else
	{
		// Call function on element
		params.builder->CreateCall4(function, 
			new_state_alloca, // SRET return value arg
			running_state_alloca, // current state
			array_elem, // array element
			captured_var_struct_ptr
		);

		// Copy the state from new_state_alloca to running_state_alloca
		//if(state_type->getType() == Type::ArrayTypeType)
		//{
		//	llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
		//	params.builder->CreateMemCpy(running_state_alloca, new_state_alloca, size, 4);
		//}
		//else
		//{
		//	llvm::Value* state = params.builder->CreateLoad(new_state_alloca); // Load the state from new_state_alloca
		//	params.builder->CreateStore(state, running_state_alloca); // Store the state in running_state_alloca
		//}
	}

	// Create increment of loop index
	llvm::Value* step_val = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1));
	llvm::Value* next_var = params.builder->CreateAdd(loop_index_var, step_val, "next_var");

	// Compute the end condition.
	llvm::Value* end_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, array_type->num_elems));
  
	llvm::Value* end_cond = params.builder->CreateICmpNE(
		end_value, 
		next_var,
		"loopcond"
	);

	//=========================== End loop body =========================
  
	// Create the "after loop" block and insert it.
	llvm::BasicBlock* LoopEndBB = params.builder->GetInsertBlock();
	llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*params.context, "afterloop", TheFunction);
  
	// Insert the conditional branch into the end of LoopEndBB.
	params.builder->CreateCondBr(end_cond, LoopBB, AfterBB);
  
	// Any new code will be inserted in AfterBB.
	params.builder->SetInsertPoint(AfterBB);
  
	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, LoopEndBB);


	// Finally load and store the running state value to the SRET return ptr.
	if(state_type->passByValue())
	{
		llvm::Value* running_state = params.builder->CreateLoad(new_state_alloca);// running_state_alloca);
		return running_state;
	}
	else
	{
		// Copy from new_state_alloca to return_ptr
		if(state_type->getType() == Type::ArrayTypeType)
		{
			llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
			params.builder->CreateMemCpy(return_ptr, new_state_alloca/*running_state_alloca*/, size, 4);
			return return_ptr;
		}
		else
		{
			llvm::Value* running_state = params.builder->CreateLoad(new_state_alloca/*running_state_alloca*/);
			params.builder->CreateStore(running_state, return_ptr);
			return return_ptr;
		}
	}
}


//------------------------------------------------------------------------------------


ArraySubscriptBuiltInFunc::ArraySubscriptBuiltInFunc(const VRef<ArrayType>& array_type_, const TypeVRef& index_type_)
:	BuiltInFunctionImpl(BuiltInType_ArraySubscriptBuiltInFunc),
	array_type(array_type_), index_type(index_type_)
{}


ValueRef ArraySubscriptBuiltInFunc::invoke(VMState& vmstate)
{
	// Array pointer is in arg 0.
	// Index or index vector is in arg 1.
	const ArrayValue* arr = checkedCast<const ArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	if(index_type->getType() == Type::IntType)
	{
		const IntValue* index = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		if(index->value >= 0 && index->value < (int64)arr->e.size())
			return arr->e[index->value];
		else
			throw BaseException("Array index out of bounds"); // return this->array_type->elem_type->getInvalidValue();
	}
	else // else index vector
	{
		const VectorValue* index_vec = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		vector<ValueRef> res(index_vec->e.size());

		for(size_t i=0; i<index_vec->e.size(); ++i)
		{
			ValueRef index_val = index_vec->e[i];
			const int64 index = checkedCast<IntValue>(index_val.getPointer())->value;
			if(index < 0 || index >= (int64)arr->e.size())
				throw BaseException("Index out of bounds");

			res[i] = arr->e[index];
		}

		return new VectorValue(res);
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
static llvm::Value* loadGatherElements(EmitLLVMCodeParams& params, int arg_offset, const TypeVRef& array_elem_type, int index_vec_num_elems)
{
	llvm::Value* array_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index_vec = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// We have a single ptr, we need to shuffle it to an array of ptrs.

	/*std::cout << "array_ptr:" << std::endl;
	array_ptr->dump();
	std::cout << std::endl;
	std::cout << "array_ptr type:" << std::endl;
	array_ptr->getType()->dump();
	std::cout << std::endl;*/

	// TEMP: Get pointer to index 0 of the array:
	llvm::Value* array_elem0_ptr = params.builder->CreateStructGEP(array_ptr, 0);

	/*std::cout << "array_elem0_ptr" << std::endl;
	array_elem0_ptr->dump();
	array_elem0_ptr->getType()->dump();
	std::cout << std::endl;*/

	//llvm::Value* shuffled_ptr = params.builder->CreateShuffleVector(

	llvm::Value* shuffled_ptr = params.builder->CreateVectorSplat(
		index_vec_num_elems,
		array_elem0_ptr
	);

	//std::cout << "shuffled_ptr:" << std::endl;
	//shuffled_ptr->dump();
	//std::cout << std::endl;
	//std::cout << "shuffled_ptr type:" << std::endl;
	//shuffled_ptr->getType()->dump();
	//std::cout << std::endl;

	//std::cout << "index_vec:" << std::endl;
	//index_vec->dump();//TEMP
	//std::cout << std::endl;
	//std::cout << "index_vec type:" << std::endl;
	//index_vec->getType()->dump();//TEMP
	//std::cout << std::endl;

	llvm::Value* indices[] = {
		//llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the array
		index_vec // get the indexed element in the array
	};

	llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(
		shuffled_ptr, // ptr
		indices
	);

	//std::cout << "elem_ptr:" << std::endl;
	//elem_ptr->dump();//TEMP
	//std::cout << std::endl;
	//std::cout << "elem_ptr type:" << std::endl;
	//elem_ptr->getType()->dump();//TEMP
	//std::cout << std::endl;


	// LLVM does not currently support loading from a vector of pointers.  So just do the loads individually.

	// Start with a vector of Undefs.
	llvm::Value* vec = llvm::ConstantVector::getSplat(
		index_vec_num_elems,
		llvm::UndefValue::get(array_elem_type->LLVMType(*params.module))
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


llvm::Value* ArraySubscriptBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeVRef field_type = this->array_type->elem_type;

	// Bounds check the index
	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	const bool do_bounds_check = false;//TEMP

	if(do_bounds_check)
	{
		// Code for out of bounds array access result.
		llvm::Value* out_of_bounds_val = field_type->getInvalidLLVMValue(*params.module);

		

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
			field_type->LLVMType(*params.module), //field_type->passByValue() ? field_type->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*field_type->LLVMType(*params.context)),
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
		else if(index_type->getType() == Type::VectorTypeType)
		{
			// Gather (vector) index.
			// Since we are returning a vector, and we are assuming vectors are not pass by pointer, we know the return type is not pass by pointer.
			
			// Pointer to memory for return value will be 0th argument.
			//llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* result_vector = loadGatherElements(
				params, 
				0, // arg offset - we have an sret arg.
				this->array_type->elem_type,
				this->index_type.downcast<VectorType>()->num // index_vec_num_elems
			);

			//TEMP:
			//result_vector->dump();
			//return_ptr->dump();

			// Store the element
			//params.builder->CreateStore(
			//	elem_val, // value
			//	return_ptr // ptr
			//);

			return result_vector;
		}
		else
		{
			assert(0);
			return NULL;
		}
	}
}


//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------


VArraySubscriptBuiltInFunc::VArraySubscriptBuiltInFunc(const VRef<VArrayType>& array_type_, const TypeVRef& index_type_)
:	BuiltInFunctionImpl(BuiltInType_VArraySubscriptBuiltInFunc),
	array_type(array_type_), index_type(index_type_)
{}


ValueRef VArraySubscriptBuiltInFunc::invoke(VMState& vmstate)
{
	// Array pointer is in arg 0.
	// Index or index vector is in arg 1.
	const VArrayValue* arr = checkedCast<const VArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	if(index_type->getType() == Type::IntType)
	{
		const IntValue* index = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		if(index->value >= 0 && index->value < (int64)arr->e.size())
			return arr->e[index->value];
		else
			throw BaseException("VArray index out of bounds"); // return this->array_type->elem_type->getInvalidValue();
	}
	else // else index vector
	{
		const VectorValue* index_vec = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		vector<ValueRef> res(index_vec->e.size());

		for(size_t i=0; i<index_vec->e.size(); ++i)
		{
			ValueRef index_val = index_vec->e[i];
			const int64 index = checkedCast<IntValue>(index_val.getPointer())->value;
			if(index < 0 || index >= (int64)arr->e.size())
				throw BaseException("Index out of bounds");

			res[i] = arr->e[index];
		}

		return new VectorValue(res);
	}
}


llvm::Value* VArraySubscriptBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume VArrays are always pass-by-pointer for now.

	TypeVRef field_type = this->array_type->elem_type;

	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	
	if(index_type->getType() == Type::IntType)
	{
		// Scalar index

		if(field_type->passByValue())
		{
			llvm::Value* varray_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
			
			//varray_ptr->dump();
			//varray_ptr->getType()->dump();

			//llvm::Value* data_ptr_ptr = params.builder->CreateStructGEP(varray_ptr, 1, "data ptr ptr");
			//llvm::Value* data_ptr = params.builder->CreateLoad(data_ptr_ptr);
			llvm::Value* data_ptr = params.builder->CreateStructGEP(varray_ptr, 3, "data_ptr"); // [0 x T]*

			llvm::Value* indices[] = { llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), index };
			llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(data_ptr, llvm::makeArrayRef(indices));

			
			return params.builder->CreateLoad(elem_ptr);
		}
		else // Else if element type is pass-by-pointer
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* varray_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 0);
			llvm::Value* data_ptr = params.builder->CreateStructGEP(varray_ptr, 3, "data_ptr"); // [0 x T]*

			llvm::Value* indices[] = { llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), index };
			llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(data_ptr, llvm::makeArrayRef(indices));
			llvm::Value* elem_val = params.builder->CreateLoad(elem_ptr);

			// Store the element
			params.builder->CreateStore(
				elem_val, // value
				return_ptr // ptr
			);

			return return_ptr;
		}
	}
	else if(index_type->getType() == Type::VectorTypeType)
	{
		//TODO
		assert(0);
		//return NULL;
		throw BaseException("not implemented 1553");
	}
	else
	{
		assert(0);
		//return NULL;
		throw BaseException("internal error 1559");
	}
}


//------------------------------------------------------------------------------------


MakeVArrayBuiltInFunc::MakeVArrayBuiltInFunc(const VRef<VArrayType>& array_type_)
:	BuiltInFunctionImpl(BuiltInType_MakeVArrayBuiltInFunc),
	array_type(array_type_)
{
}


ValueRef MakeVArrayBuiltInFunc::invoke(VMState& vmstate)
{
	const ValueRef elem_value = vmstate.argument_stack[vmstate.func_args_start.back()    ];
	const IntValue* count_value  = checkedCast<const IntValue>   (vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	// TODO: max value check?
	if(count_value->value < 0)
		throw BaseException("Count value is invalid.");

	return new VArrayValue(vector<ValueRef>(count_value->value, elem_value));
}


class MakeVArrayBuiltInFunc_CreateLoopBodyCallBack : public CreateLoopBodyCallBack
{
public:
	virtual llvm::Value* emitLoopBody(llvm::IRBuilder<>& builder, llvm::Module* module, /*llvm::Value* loop_value_var, */llvm::Value* i)
	{
		llvm::Value* indices[2] = {llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0)), i };
		llvm::Value* element_ptr = builder.CreateInBoundsGEP(data_ptr, indices);

		if(array_type->elem_type->passByValue())
		{
			// Store the element in the array
			builder.CreateStore(
				element_value, // value
				element_ptr // ptr
			);
		}
		else
		{
			// Element is pass-by-pointer, for example a structure.
			// So just emit code that will store it directly in the array.
			//this->element_value->emitLLVMCode(params, element_ptr);

			//TEMP:
			assert(0);
			throw BaseException("not implemented");
		}

		return NULL;
	}

	llvm::Value* element_value;
	llvm::Value* data_ptr;
	Reference<VArrayType> array_type;
};


llvm::Value* MakeVArrayBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* elem_val  = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* count_val = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	// Make call to allocateVArray

	// NOTE: this code take from VArrayLiteral.cpp.  TODO: Deduplicate.

	params.stats->num_heap_allocation_calls++;

	// Emit a call to allocateVArray:
	// allocateVArray(const int elem_size_B, const int num_elems)
	llvm::Function* allocateVArrayLLVMFunc = params.common_functions.allocateVArrayFunc->getOrInsertFunction(
		params.module,
		false // use_cap_var_struct_ptr
	);

	const TypeVRef elem_type = array_type->elem_type;

	const uint64_t size_B = params.target_data->getTypeAllocSize(elem_type->LLVMType(*params.module)); // Get size of element
	llvm::Value* size_B_constant = llvm::ConstantInt::get(*params.context, llvm::APInt(64, size_B, /*signed=*/false));

	llvm::CallInst* call_inst = params.builder->CreateCall2(allocateVArrayLLVMFunc, size_B_constant, count_val, "varray");

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	call_inst->setCallingConv(llvm::CallingConv::C);

	// Cast resulting allocated void* down to VArrayRep for the right type, e.g. varray<T>
	llvm::Type* varray_T_type = this->array_type->LLVMType(*params.module);
	assert(varray_T_type->isPointerTy());

	llvm::Value* varray_ptr = params.builder->CreatePointerCast(call_inst, varray_T_type);
	uint64 initial_flags = 1; // flag = 1 = heap allocated


	// Set the reference count to 1
	llvm::Value* ref_ptr = params.builder->CreateStructGEP(varray_ptr, 0, "varray_literal_ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, /*signed=*/true));
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, ref_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "makeVArray result set initial ref count to 1");

	// Set VArray length
	llvm::Value* length_ptr = params.builder->CreateStructGEP(varray_ptr, 1, "varray_literal_length_ptr");
	llvm::StoreInst* store_length_inst = params.builder->CreateStore(count_val, length_ptr);
	addMetaDataCommentToInstruction(params, store_length_inst, "makeVArray result set initial length count.");

	// Set the flags
	llvm::Value* flags_ptr = params.builder->CreateStructGEP(varray_ptr, 2, "varray_literal_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "makeVArray result set initial flags to " + toString(initial_flags));


	llvm::Value* data_ptr = params.builder->CreateStructGEP(varray_ptr, 3, "varray_literal_data_ptr");


	// Emit for loop to write count copies of the element to the varray.

	MakeVArrayBuiltInFunc_CreateLoopBodyCallBack callback;
	callback.array_type = this->array_type;
	callback.data_ptr = data_ptr;
	callback.element_value = elem_val;

	makeForLoop(
		*params.builder,
		params.module,
		llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), // begin index
		count_val, // end index
		//array_type->elem_type->LLVMType(*params.module), // Loop value type. TODO: remove
		&callback
	);

	return varray_ptr;
}








//------------------------------------------------------------------------------------


VectorSubscriptBuiltInFunc::VectorSubscriptBuiltInFunc(const VRef<VectorType>& vec_type_, const TypeVRef& index_type_)
:	BuiltInFunctionImpl(BuiltInType_VectorSubscriptBuiltInFunc),
	vec_type(vec_type_), index_type(index_type_)
{}


ValueRef VectorSubscriptBuiltInFunc::invoke(VMState& vmstate)
{
	// Vector is in arg 0.
	// Index is in arg 1.
	const VectorValue* vec = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()    ].getPointer());
	const IntValue* index  = checkedCast<const IntValue>   (vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	if(index->value >= 0 && index->value < (int64)vec->e.size())
		return vec->e[index->value];
	else
		throw BaseException("Vector index out of bounds");
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


ArrayInBoundsBuiltInFunc::ArrayInBoundsBuiltInFunc(const VRef<ArrayType>& array_type_, const TypeVRef& index_type_)
:	BuiltInFunctionImpl(BuiltInType_ArrayInBoundsBuiltInFunc),
	array_type(array_type_), index_type(index_type_)
{}


ValueRef ArrayInBoundsBuiltInFunc::invoke(VMState& vmstate)
{
	// Array pointer is in arg 0.
	// Index is in arg 1.
	const ArrayValue* arr = checkedCast<const ArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const IntValue* index = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return new BoolValue(index->value >= 0 && index->value < (int64)arr->e.size());
}


llvm::Value* ArrayInBoundsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeVRef field_type = this->array_type->elem_type;

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


VectorInBoundsBuiltInFunc::VectorInBoundsBuiltInFunc(const VRef<VectorType>& vector_type_, const TypeVRef& index_type_)
:	BuiltInFunctionImpl(BuiltInType_VectorInBoundsBuiltInFunc),
	vector_type(vector_type_), index_type(index_type_)
{}


ValueRef VectorInBoundsBuiltInFunc::invoke(VMState& vmstate)
{
	// Vector pointer is in arg 0.
	// Index is in arg 1.
	const VectorValue* arr = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const IntValue* index = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return new BoolValue(index->value >= 0 && index->value < (int64)arr->e.size());
}


llvm::Value* VectorInBoundsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Let's assume Arrays are always pass-by-pointer for now.

	TypeVRef field_type = this->vector_type->elem_type;

	// Bounds check the index
	const int arg_offset = field_type->passByValue() ? 0 : 1;
	llvm::Value* index     = LLVMTypeUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// Create bounds check condition code
	return params.builder->CreateAnd(
		params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
		params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->vector_type->num, true))) // index < vector num elems
	);
}


//------------------------------------------------------------------------------------


IterateBuiltInFunc::IterateBuiltInFunc(const VRef<Function>& func_type_, const TypeVRef& state_type_, const vector<TypeVRef>& invariant_data_types_)
:	BuiltInFunctionImpl(BuiltInType_IterateBuiltInFunc),
	func_type(func_type_), state_type(state_type_), invariant_data_types(invariant_data_types_)
{}


ValueRef IterateBuiltInFunc::invoke(VMState& vmstate)
{
	// iterate(function<State, int, tuple<State, bool>> f, State initial_state) State

	const FunctionValue* f = checkedCast<const FunctionValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const ValueRef initial_state = vmstate.argument_stack[vmstate.func_args_start.back() + 1];
	vector<ValueRef> invariant_data(invariant_data_types.size());
	for(size_t i=0; i<invariant_data_types.size(); ++i)
		invariant_data[i] = vmstate.argument_stack[vmstate.func_args_start.back() + 2 + i];

	assert(f && initial_state.nonNull());

	ValueRef running_val = initial_state;
	int64 iteration = 0;
	while(1)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(new IntValue(iteration, true)); // Push iteration
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			vmstate.argument_stack.push_back(invariant_data[i]);
		
		// Call f
		ValueRef result = f->func_def->invoke(vmstate);
		
		// Unpack result
		const TupleValue* tuple_result = checkedCast<const TupleValue>(result.ptr());

		ValueRef new_running_val = tuple_result->e[0];
		bool continue_bool = checkedCast<const BoolValue>(tuple_result->e[1].ptr())->value;

		for(size_t i=0; i<invariant_data_types.size(); ++i)
			vmstate.argument_stack.pop_back();

		vmstate.argument_stack.pop_back(); // Pop iteration arg
		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();

		if(!continue_bool)
			return new_running_val;

		running_val = new_running_val;
		iteration++;
	}

	return running_val;
}




/*
iterate(function<State, int, tuple<State, bool>> f, State initial_state) State
or
iterate(function<State, int, LoopInvariantData, tuple<State, bool>> f, State initial_state, LoopInvariantData invariant_data) State


Compile as 

state_alloca = alloca space for State
tuple_alloca = alloca space for tuple<State, bool>

State state = initial_state;
Store initial_state in state_alloca

iteration = 0;
while(1)
{
	//res = f(state, iteration);
	f(tuple_alloca, state_alloca, iteration)

	if(tuple_alloca->second == false)
		copy tuple_alloca->first to result
		return

	iteration++;
	
	// state = res.first;
	copy tuple_alloca->first to state_alloca
}

*/
llvm::Value* IterateBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Get argument pointers/values
	llvm::Value* return_ptr = NULL;
	llvm::Value* closure_ptr;
	llvm::Value* initial_state_ptr_or_value;
	vector<llvm::Value*> invariant_data_ptr_or_value(invariant_data_types.size());


	if(state_type->passByValue())
	{
		closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0); // Pointer to function
		initial_state_ptr_or_value = LLVMTypeUtils::getNthArg(params.currently_building_func, 1); // Pointer to, or value of initial state
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			invariant_data_ptr_or_value[i] = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + (int)i); // Pointer to, or value of invariant_data
	}
	else
	{
		return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0); // Pointer to result structure
		closure_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1); // Pointer to function
		initial_state_ptr_or_value = LLVMTypeUtils::getNthArg(params.currently_building_func, 2); // Pointer to, or value of initial state
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			invariant_data_ptr_or_value[i] = LLVMTypeUtils::getNthArg(params.currently_building_func, 3 + (int)i); // Pointer to, or value of invariant_data
	}


	llvm::Value* function_ptr = params.builder->CreateStructGEP(closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = params.builder->CreateStructGEP(closure_ptr, Function::capturedVarStructIndex());


	// Allocate space on stack for tuple<State, bool> returned from f.
		
	// Emit the alloca in the entry block for better code-gen.
	// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
	llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

	llvm::Value* tuple_alloca = entry_block_builder.CreateAlloca(
		func_type->return_type->LLVMType(*params.module), // tuple<State, bool>
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
		"Tuple space"
	);

	// Allocate space on stack for the running state, if the state type is not pass-by-value.
	llvm::Value* state_alloca = NULL;

	//if(!state_type->passByValue())
	//{
		state_alloca = entry_block_builder.CreateAlloca(
			state_type->LLVMType(*params.module), // State
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"Running state"
		);

	// Load and store initial state in running state
	if(state_type->passByValue())
	{
		params.builder->CreateStore(initial_state_ptr_or_value, state_alloca);
	}
	else
	{
		/*if(state_type->getType() == Type::ArrayTypeType)
		{
			llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
			params.builder->CreateMemCpy(state_alloca, initial_state_ptr_or_value, size, 4);
		}
		else
		{*/
			params.builder->CreateStore(params.builder->CreateLoad(initial_state_ptr_or_value), state_alloca);
		//}
		// Load and store initial state in running state
		//llvm::Value* initial_state = params.builder->CreateLoad(initial_state_ptr_or_value);
		//params.builder->CreateStore(initial_state, state_alloca);
	}
	

	// Make the new basic block for the loop header, inserting after current
	// block.
	llvm::Function* TheFunction = params.builder->GetInsertBlock()->getParent();
	llvm::BasicBlock* PreheaderBB = params.builder->GetInsertBlock();
	llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(*params.context, "loop", TheFunction);
  
	// Insert an explicit fall through from the current block to the LoopBB.
	params.builder->CreateBr(LoopBB);

	// Start insertion in LoopBB.
	params.builder->SetInsertPoint(LoopBB);

	// Create running state value variable phi node
	/*llvm::PHINode* running_state_value = NULL;
	if(state_type->passByValue())
	{
		running_state_value = params.builder->CreatePHI(state_type->LLVMType(*params.context), 2, "running_state_value");
		running_state_value->addIncoming(initial_state_ptr_or_value, PreheaderBB);
	}*/
  
	

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = params.builder->CreatePHI(llvm::Type::getInt32Ty(*params.context), 2, "loop_index_var");
	llvm::Value* initial_loop_index_value = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // Initial induction loop index value: Zero
	loop_index_var->addIncoming(initial_loop_index_value, PreheaderBB);


	//=========================== Emit the body of the loop. =========================

	// Call function f
	vector<llvm::Value*> args;
	args.push_back(tuple_alloca); // SRET return value arg
	args.push_back(state_type->passByValue() ? params.builder->CreateLoad(state_alloca) : state_alloca); // current state
	args.push_back(loop_index_var); // iteration
	for(size_t i=0; i<invariant_data_types.size(); ++i)
		args.push_back(invariant_data_ptr_or_value[i]);

	args.push_back(captured_var_struct_ptr); // captured var struct ptr

	params.builder->CreateCall(
		function, // Callee
		args // Args
	);

	// The result of the function (tuple<State, bool>) should now be stored in 'tuple_alloca'.

	// copy tuple_alloca->first to state_alloca

	llvm::Value* state = params.builder->CreateLoad(params.builder->CreateStructGEP(tuple_alloca, 0)); // Load the state from tuple_alloca
	params.builder->CreateStore(state, state_alloca); // Store the state in state_alloca

	/*llvm::Value* next_running_state_value = NULL;
	if(state_type->passByValue())
	{
		// Load the state
		llvm::Value* state_ptr = params.builder->CreateStructGEP(tuple_alloca, 0);
		next_running_state_value = params.builder->CreateLoad(state_ptr);
	}
	else
	{
		// Load the state
		llvm::Value* state_ptr = params.builder->CreateStructGEP(tuple_alloca, 0);
		llvm::Value* state = params.builder->CreateLoad(state_ptr);

		// Store the state in running_state_alloca
		params.builder->CreateStore(state, running_state_alloca);
	}*/

	// Load the 'continue boolean'
	llvm::Value* continue_bool_ptr = params.builder->CreateStructGEP(tuple_alloca, 1);
	llvm::Value* continue_bool = params.builder->CreateLoad(continue_bool_ptr);


  
	// Create increment of loop index
	llvm::Value* step_val = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1));
	llvm::Value* next_var = params.builder->CreateAdd(loop_index_var, step_val, "next_var");

	// Compute the end condition.
	llvm::Value* false_value = llvm::ConstantInt::get(*params.context, llvm::APInt(1, 0));
  
	llvm::Value* end_cond = params.builder->CreateICmpNE(
		false_value, 
		continue_bool,
		"loopcond"
	);
  
	// Create the "after loop" block and insert it.
	llvm::BasicBlock* LoopEndBB = params.builder->GetInsertBlock();
	llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(*params.context, "afterloop", TheFunction);
  
	// Insert the conditional branch into the end of LoopEndBB.
	params.builder->CreateCondBr(end_cond, LoopBB, AfterBB);
  
	// Any new code will be inserted in AfterBB.
	params.builder->SetInsertPoint(AfterBB);
  
	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, LoopEndBB);


	
	// Finally load and store the running state value to the SRET return ptr.
	llvm::Value* running_state = params.builder->CreateLoad(state_alloca);
	if(state_type->passByValue())
	{
		return running_state;
	}
	else
	{
		// Copy from state_alloca to return_ptr
		llvm::Value* running_state = params.builder->CreateLoad(state_alloca);
		params.builder->CreateStore(running_state, return_ptr);
		return return_ptr;
	}
}


/*
iterate(function<State, int, LoopInvariantData0, LoopInvariantData1, ..., LoopInvariantDataN, tuple<State, bool>> f, State initial_state, LoopInvariantData0, LoopInvariantData1, ..., LoopInvariantDataN) State


State state = initial_state;
Store initial_state in state_alloca

iteration = 0;
while(1)
{
	//res = f(state, iteration);
	f(tuple_alloca, state_alloca, iteration)

	if(tuple_alloca->second == false)
		copy tuple_alloca->first to result
		return

	iteration++;
	
	// state = res.first;
	copy tuple_alloca->first to state_alloca
}
*/
const std::string IterateBuiltInFunc::emitOpenCLForFunctionArg(EmitOpenCLCodeParams& params,
		const FunctionDefinition* f, // arg 0
		const std::vector<ASTNodeRef>& argument_expressions
	)
{
	const std::string state_typename = argument_expressions[1]->type()->OpenCLCType();
	const std::string tuple_typename = f->returnType()->OpenCLCType();

	std::string s;
	// Emit
	s = state_typename + " iterate_" + toString(params.uid++) + "(" + state_typename + " initial_state";

	// Add invariant data args
	for(size_t i = 0; i<invariant_data_types.size(); ++i)
		s += ", " + invariant_data_types[i]->OpenCLCType() + " LoopInvariantData" + toString(i);

	s += ")\n";
	s += "{\n";
	s += "\t" + state_typename + " state = initial_state;\n";
	s += "\tint iteration = 0;\n";
	s += "\twhile(1)\n";
	s += "\t{\n";

	// Emit "tuple<State, bool> res = f(state, iteration, LoopInvariantData0, LoopInvariantData1, ..., LoopInvariantDataN)"
	s += "\t\t" + tuple_typename + " res = " + f->sig.typeMangledName() + "(state, iteration";
	for(size_t i = 0; i<invariant_data_types.size(); ++i)
		s += ", LoopInvariantData" + toString(i);
	s += ");\n";

	// Emit "if(tuple_alloca->second == false)"
	s += "\t\tif(res.field_1 == false)\n";
	s += "\t\t\treturn res.field_0;\n";
	s += "\t\titeration++;\n";
	s += "\t\tstate = res.field_0;\n";
	s += "\t}\n";
	s += "}\n";
	params.file_scope_code += s;

	// Return a call to the function
	std::string call_code = "iterate_" + toString(params.uid - 1) + "(" + argument_expressions[1]->emitOpenCLC(params);
	for(size_t i = 0; i<invariant_data_types.size(); ++i)
		call_code += ", " + argument_expressions[2 + i]->emitOpenCLC(params);
	call_code += ")";
	return call_code;
}


//----------------------------------------------------------------------------------------------


ValueRef DotProductBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	if(vector_type->elem_type->getType() == Type::FloatType)
	{
		FloatValueRef res = new FloatValue(0.0f);
		for(unsigned int i=0; i<vector_type->num; ++i)
			res->value += checkedCast<const FloatValue>(a->e[i].getPointer())->value * checkedCast<const FloatValue>(b->e[i].getPointer())->value;
		return res;
	}
	else
	{
		DoubleValueRef res = new DoubleValue(0.0f);
		for(unsigned int i=0; i<vector_type->num; ++i)
			res->value += checkedCast<const DoubleValue>(a->e[i].getPointer())->value * checkedCast<const DoubleValue>(b->e[i].getPointer())->value;
		return res;
	}
}


llvm::Value* DotProductBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	// If have SSE4.1 and this is a 4-vector, using DPPS instruction
	if(vector_type->elem_type->getType() == Type::FloatType && this->vector_type->num == 4 && params.cpu_info->sse4_1)
	{
		// Emit dot product intrinsic
		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);
#if TARGET_LLVM_VERSION <= 34
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 255))); // SSE DPPS control bits
#else
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(8, 255))); // SSE DPPS control bits
#endif

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


/*class VectorMin_CreateLoopBodyCallBack : public CreateLoopBodyCallBack
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
};*/


ValueRef VectorMinBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	
	vector<ValueRef> res_values(vector_type->num);

	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			const float y = checkedCast<const FloatValue>(b->e[i].getPointer())->value;
			res_values[i] = new FloatValue(x < y ? x : y);
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::DoubleType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const double x = checkedCast<const DoubleValue>(a->e[i].getPointer())->value;
			const double y = checkedCast<const DoubleValue>(b->e[i].getPointer())->value;
			res_values[i] = new DoubleValue(x < y ? x : y);
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const int64 x = checkedCast<const IntValue>(a->e[i].getPointer())->value;
			const int64 y = checkedCast<const IntValue>(b->e[i].getPointer())->value;
			res_values[i] = new IntValue(x > y ? x : y, checkedCast<const IntValue>(a->e[i].getPointer())->is_signed);
		}
	}
	else
	{
		throw BaseException("Invalid type.");
	}

	return new VectorValue(res_values);
}


llvm::Value* VectorMinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	llvm::Value* condition;
	
	if(this->vector_type->elem_type->getType() == Type::FloatType || this->vector_type->elem_type->getType() == Type::DoubleType)
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
		throw BaseException("Internal error - VectorMinBuiltInFunc");
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
	const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	vector<ValueRef> res_values(vector_type->num);

	if(this->vector_type->elem_type->getType() == Type::FloatType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			const float y = checkedCast<const FloatValue>(b->e[i].getPointer())->value;
			res_values[i] = new FloatValue(x > y ? x : y);
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::DoubleType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const double x = checkedCast<const DoubleValue>(a->e[i].getPointer())->value;
			const double y = checkedCast<const DoubleValue>(b->e[i].getPointer())->value;
			res_values[i] = new DoubleValue(x > y ? x : y);
		}
	}
	else if(this->vector_type->elem_type->getType() == Type::IntType)
	{
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const int64 x = checkedCast<const IntValue>(a->e[i].getPointer())->value;
			const int64 y = checkedCast<const IntValue>(b->e[i].getPointer())->value;
			res_values[i] = new IntValue(x > y ? x : y, checkedCast<const IntValue>(a->e[i].getPointer())->is_signed);
		}
	}
	else
	{
		throw BaseException("Invalid type.");
	}


	return new VectorValue(res_values);
}


llvm::Value* VectorMaxBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	llvm::Value* condition;
	if(this->vector_type->elem_type->getType() == Type::FloatType || this->vector_type->elem_type->getType() == Type::DoubleType)
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
		throw BaseException("Internal error - VectorMaxBuiltInFunc");
	}

	return params.builder->CreateSelect(condition, a, b);
}


//----------------------------------------------------------------------------------------------


ValueRef ShuffleBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* index_vec = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && index_vec);


	vector<ValueRef> res_values(index_vec->e.size());

	for(unsigned int i=0; i<index_vec->e.size(); ++i)
	{
		const int64 index_val = index_vec->e[i].downcast<IntValue>()->value;
		if(index_val < 0 || index_val >= (int64)a->e.size())
			throw BaseException("invalid index");

		res_values[i] = a->e[index_val];
	}

	return new VectorValue(res_values);
}


llvm::Value* ShuffleBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	//llvm::Value* index_vec = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);


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
		{
			if(shuffle_mask[i] < 0 || shuffle_mask[i] >= (int)vector_type->num)
				throw BaseException("Shuffle mask index " + toString(shuffle_mask[i]) + " out of bounds: " + errorContext(params.currently_building_func_def));

			elems[i] = llvm::ConstantInt::get(
				*params.context, 
				llvm::APInt(
					32, // num bits
					shuffle_mask[i], // value
					true // signed
				)
			);
		}

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


PowBuiltInFunc::PowBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_PowBuiltInFunc),
	type(type_)
{}


ValueRef PowBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
		const FloatValue* b = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
		return new FloatValue(std::pow(a->value, b->value));
	}
	else
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
		const DoubleValue* b = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
		return new DoubleValue(std::pow(a->value, b->value));
	}
}


llvm::Value* PowBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	vector<llvm::Value*> args(2);
	args[0] = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	args[1] = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	vector<llvm::Type*> types(1, this->type->LLVMType(*params.module));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::pow, types);

	assert(func);
	assert(func->isIntrinsic());

	return params.builder->CreateCall(
		func,
		args
	);
}


//----------------------------------------------------------------------------------------------


static llvm::Value* emitUnaryIntrinsic(EmitLLVMCodeParams& params, const TypeVRef& type, llvm::Intrinsic::ID id)
{
	assert(type->getType() == Type::FloatType || type->getType() == Type::DoubleType || (type->getType() == Type::VectorTypeType));

	vector<llvm::Value*> args(1, LLVMTypeUtils::getNthArg(params.currently_building_func, 0));

	vector<llvm::Type*> types(1, type->LLVMType(*params.module));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, id, types);

	return params.builder->CreateCall(
		func,
		args
	);
}


SqrtBuiltInFunc::SqrtBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_SqrtBuiltInFunc),
	type(type_)
{}


ValueRef SqrtBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::sqrt(a->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::sqrt(a->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			res_values[i] = new FloatValue(std::sqrt(x));
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


ExpBuiltInFunc::ExpBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_ExpBuiltInFunc),
	type(type_)
{}


ValueRef ExpBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::exp(a->value));
	}
	else
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::exp(a->value));
	}
}


llvm::Value* ExpBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::exp);
}


//----------------------------------------------------------------------------------------------


LogBuiltInFunc::LogBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_LogBuiltInFunc),
	type(type_)
{}


ValueRef LogBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::log(a->value));
	}
	else
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::log(a->value));
	}
}


llvm::Value* LogBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::log);
}


//----------------------------------------------------------------------------------------------


SinBuiltInFunc::SinBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_SinBuiltInFunc),
	type(type_)
{}


ValueRef SinBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::sin(a->value));
	}
	else
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::sin(a->value));
	}
}


llvm::Value* SinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::sin);
}


//----------------------------------------------------------------------------------------------


CosBuiltInFunc::CosBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_CosBuiltInFunc),
	type(type_)
{}


ValueRef CosBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::cos(a->value));
	}
	else
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::cos(a->value));
	}
}


llvm::Value* CosBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::cos);
}


//----------------------------------------------------------------------------------------------


AbsBuiltInFunc::AbsBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_AbsBuiltInFunc),
	type(type_)
{}


ValueRef AbsBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::fabs(a->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::fabs(a->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);
		throw BaseException("AbsBuiltInFunc vector type");
	}
}


llvm::Value* AbsBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::fabs);
}


//----------------------------------------------------------------------------------------------


FloorBuiltInFunc::FloorBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_FloorBuiltInFunc),
	type(type_)
{}


ValueRef FloorBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::floor(a->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::floor(a->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			res_values[i] = new FloatValue(std::floor(x));
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* FloorBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::floor);
}


//----------------------------------------------------------------------------------------------


CeilBuiltInFunc::CeilBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_CeilBuiltInFunc),
	type(type_)
{}


ValueRef CeilBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(std::ceil(a->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(std::ceil(a->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			res_values[i] = new FloatValue(std::ceil(x));
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* CeilBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitUnaryIntrinsic(params, this->type, llvm::Intrinsic::ceil);
}


//----------------------------------------------------------------------------------------------


SignBuiltInFunc::SignBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_SignBuiltInFunc),
	type(type_)
{}


ValueRef SignBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new FloatValue(a->value >= 0 ? 1.0f : -1.0f);
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(a->value >= 0 ? 1.0 : -1.0);
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		for(unsigned int i=0; i<vector_type->num; ++i)
		{
			const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
			res_values[i] = new FloatValue(x >= 0 ? 1.0f : -1.0f);
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* SignBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	vector<llvm::Type*> types(1, this->type->LLVMType(*params.module));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::copysign, types);

	assert(func);
	assert(func->isIntrinsic());

	llvm::Value* scalar_one = llvm::ConstantFP::get(*params.context, llvm::APFloat(1.f));

	llvm::Value* one;
	if(type->getType() == Type::VectorTypeType)
	{
		one = params.builder->CreateVectorSplat(
			this->type.downcastToPtr<VectorType>()->num, // num elements
			scalar_one // value
		);
	}
	else
		one = scalar_one;

	// "The ‘llvm.copysign.*‘ intrinsics return a value with the magnitude of the first operand and the sign of the second operand."
	return params.builder->CreateCall2(
		func,
		one,
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0)
	);
}


//----------------------------------------------------------------------------------------------


TruncateToIntBuiltInFunc::TruncateToIntBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_TruncateToIntBuiltInFunc),
	type(type_)
{}


TypeVRef TruncateToIntBuiltInFunc::getReturnType(const TypeVRef& arg_type)
{
	if(arg_type->getType() == Type::FloatType)
		return new Int();
	else if(arg_type->getType() == Type::DoubleType)
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
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new IntValue((int)a->value, /*is_signed=*/true);
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new IntValue((int)a->value, /*is_signed=*/true);
	}
	else
		throw BaseException("TruncateToIntBuiltInFunc todo");
}


llvm::Value* TruncateToIntBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Work out destination type.
	TypeVRef dest_type = getReturnType(type);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.module);

	return params.builder->CreateFPToSI(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type
	);
}


//----------------------------------------------------------------------------------------------


ToFloatBuiltInFunc::ToFloatBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_ToFloatBuiltInFunc),
	type(type_)
{}


TypeVRef ToFloatBuiltInFunc::getReturnType(const TypeVRef& arg_type)
{
	if(arg_type->getType() == Type::IntType)
		return new Float();
	else if(arg_type->getType() == Type::VectorTypeType) // If vector of ints
		return new VectorType(new Float(), arg_type.downcast<VectorType>()->num);
	else
	{
		assert(0);
		throw BaseException("ToFloatBuiltInFunc::getReturnType todo");
	}
}


ValueRef ToFloatBuiltInFunc::invoke(VMState& vmstate)
{
	const IntValue* a = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	return new FloatValue((float)a->value);
}


llvm::Value* ToFloatBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Work out destination type.
	TypeVRef dest_type = getReturnType(type);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.module);

	if(type->getType() == Type::IntType)
	{
		if(type.downcastToPtr<Int>()->is_signed)
			return params.builder->CreateSIToFP(
				LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
		else
			return params.builder->CreateUIToFP(
				LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
	}
	else if(type->getType() == Type::VectorTypeType) 
	{
		// TODO: TEST THIS vector stuff.  quite possible doesn't work.

		if(type.downcastToPtr<VectorType>()->elem_type.downcastToPtr<Int>()->is_signed)
			return params.builder->CreateSIToFP(
				LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
		else
			return params.builder->CreateUIToFP(
				LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
	}
	else
		throw BaseException("ToFloatBuiltInFunc::emitLLVMCode todo");
}


//----------------------------------------------------------------------------------------------


ToDoubleBuiltInFunc::ToDoubleBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_ToDoubleBuiltInFunc),
	type(type_)
{}


TypeVRef ToDoubleBuiltInFunc::getReturnType(const TypeVRef& arg_type)
{
	if(arg_type->getType() == Type::IntType)
		return new Double();
	else if(arg_type->getType() == Type::VectorTypeType) // If vector of ints
		return new VectorType(new Double(), arg_type.downcast<VectorType>()->num);
	else
	{
		assert(0);
		return NULL;
	}
}


ValueRef ToDoubleBuiltInFunc::invoke(VMState& vmstate)
{
	const IntValue* a = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	return new DoubleValue((double)a->value);
}


llvm::Value* ToDoubleBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Work out destination type.
	TypeRef dest_type = getReturnType(type);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.module);

	return params.builder->CreateSIToFP(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


//----------------------------------------------------------------------------------------------


ToInt64BuiltInFunc::ToInt64BuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_ToInt64BuiltInFunc),
	type(type_)
{}


ValueRef ToInt64BuiltInFunc::invoke(VMState& vmstate)
{
	return vmstate.argument_stack[vmstate.func_args_start.back()];
}


llvm::Value* ToInt64BuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	TypeVRef dest_type = new Int(64);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.module);

	return params.builder->CreateSExt(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


//----------------------------------------------------------------------------------------------


ToInt32BuiltInFunc::ToInt32BuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_ToInt32BuiltInFunc),
	type(type_)
{}


ValueRef ToInt32BuiltInFunc::invoke(VMState& vmstate)
{
	const IntValue* a = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	if(a->value >= -2147483648LL && a->value <= 2147483647LL)
		return new IntValue(a->value, /*is_signed=*/true);
	else
		throw BaseException("argument for toInt32 is out of domain.");
}


llvm::Value* ToInt32BuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	TypeVRef dest_type = new Int(32);

	// Get destination LLVM type
	llvm::Type* dest_llvm_type = dest_type->LLVMType(*params.module);

	return params.builder->CreateTrunc(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


//----------------------------------------------------------------------------------------------


VoidPtrToInt64BuiltInFunc::VoidPtrToInt64BuiltInFunc(const TypeVRef& type)
:	BuiltInFunctionImpl(BuiltInType_VoidPtrToInt64BuiltInFunc)	
{}


ValueRef VoidPtrToInt64BuiltInFunc::invoke(VMState& vmstate)
{
	const VoidPtrValue* a = checkedCast<const VoidPtrValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return new IntValue((int64)a->value, /*is_signed*/true);
}


llvm::Value* VoidPtrToInt64BuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	TypeVRef int_type = new Int(64, true);
	return params.builder->CreatePtrToInt(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		int_type->LLVMType(*params.module) // dest type
	);
}


//----------------------------------------------------------------------------------------------


LengthBuiltInFunc::LengthBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_LengthBuiltInFunc),
	type(type_)
{}


ValueRef LengthBuiltInFunc::invoke(VMState& vmstate)
{
	switch(type->getType())
	{
	case Type::ArrayTypeType:
		return new IntValue(checkedCast<const ArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer())->e.size(), /*is_signed=*/true);
	case Type::VArrayTypeType:
		return new IntValue(checkedCast<const VArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer())->e.size(), /*is_signed=*/true);
	case Type::TupleTypeType:
		return new IntValue(checkedCast<const TupleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer())->e.size(), /*is_signed=*/true);
	case Type::VectorTypeType:
		return new IntValue(checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer())->e.size(), /*is_signed=*/true);
	default:
		throw BaseException("unhandled type.");
	}
}


llvm::Value* LengthBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	switch(type->getType())
	{
	case Type::ArrayTypeType:
		return llvm::ConstantInt::get(*params.context, llvm::APInt(64, type.downcastToPtr<ArrayType>()->num_elems));
	case Type::VArrayTypeType:
		{
			llvm::Value* varray_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
			llvm::Value* len_field_ptr = params.builder->CreateStructGEP(varray_ptr, 1, "len_field_ptr");
			return params.builder->CreateLoad(len_field_ptr, false, "len_value");
		}
	case Type::TupleTypeType:
		return llvm::ConstantInt::get(*params.context, llvm::APInt(64, type.downcastToPtr<TupleType>()->component_types.size()));
	case Type::VectorTypeType:
		return llvm::ConstantInt::get(*params.context, llvm::APInt(64, type.downcastToPtr<VectorType>()->num));
	default:
		throw BaseException("unhandled type.");
	}
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
