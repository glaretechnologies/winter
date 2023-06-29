#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "VirtualMachine.h"
#include "Value.h"
#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_FunctionExpression.h"
#include "wnt_RefCounting.h"
#include <vector>
#include "LLVMUtils.h"
#include "LLVMTypeUtils.h"
#include "utils/PlatformUtils.h"
#include "utils/StringUtils.h"
#include "utils/TaskManager.h"
#include "utils/ConPrint.h"
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
#if TARGET_LLVM_VERSION >= 110
#include <llvm/IR/IntrinsicsX86.h> // New for ~11.0
#endif
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
			llvm::Value* arg_value = LLVMUtils::getNthArg(params.currently_building_func, i);

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
		llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

		// For each field in the structure
		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			// Get the pointer to the structure field.
			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, i);

			llvm::Value* arg_value_or_ptr = LLVMUtils::getNthArg(params.currently_building_func, i + 1);

			if(this->struct_type->component_types[i]->passByValue())
			{
				params.builder->CreateStore(arg_value_or_ptr, field_ptr);
			}
			else
			{
				LLVMUtils::createCollectionCopy(
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


size_t Constructor::getTimeBound(GetTimeBoundParams& params) const
{
	return struct_type->component_types.size(); // Copy time
}


GetSpaceBoundResults Constructor::getSpaceBound(GetSpaceBoundParams& params) const
{
	// TEMP HACK work out properly
	return GetSpaceBoundResults(0, this->struct_type->memSize());
}


//------------------------------------------------------------------------------------


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
			LLVMUtils::getNthArg(params.currently_building_func, 0),
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
			llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, this->index, field_name + " ptr");

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
			llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1);

			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, this->index, field_name + " ptr");

			LLVMUtils::createCollectionCopy(
				field_type, 
				return_ptr, // dest ptr
				field_ptr, // src ptr
				params
			);

			return NULL;
		}
	}
}


size_t GetField::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults GetField::getSpaceBound(GetSpaceBoundParams& params) const
{
	const TypeVRef field_type = this->struct_type->component_types[this->index];
	// TEMP HACK TODO: work out properly
	return GetSpaceBoundResults(0, field_type->memSize());
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

		ArrayValue* new_collection = new ArrayValue();
		new_collection->e.resize(array_val->e.size());

		for(unsigned int i=0; i<array_val->e.size(); ++i)
			new_collection->e[i] = array_val->e[i];

		new_collection->e[index] = newval;

		return new_collection;
	}
	else
	{
		// TODO: handle other types.
		throw BaseException("UpdateElementBuiltInFunc::invoke: invalid type");
	}
}


// def update(CollectionType c, int index, T newval) CollectionType
llvm::Value* UpdateElementBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(collection_type->getType() == Type::ArrayTypeType)
	{
		// Pointer to memory for return value will be 0th argument.
		llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

		// Pointer to structure will be in 1st argument.
		llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1);

		// Index will be in 2nd argument.
		llvm::Value* index = LLVMUtils::getNthArg(params.currently_building_func, 2);

		// New val will be in 3rd argument.  TEMP: assuming pass by value.
		llvm::Value* newval = LLVMUtils::getNthArg(params.currently_building_func, 3);


		// Copy old collection to new collection
//		llvm::Value* collection_val = params.builder->CreateLoad(struct_ptr, "collection val");
//		params.builder->CreateStore(collection_val, return_ptr);
	
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * collection_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
		LLVMUtils::createMemCpy(params.builder, return_ptr, struct_ptr, size, 4);

		// Update element with new val
		llvm::Value* indices[] = {
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the zero-th array
			index // get the indexed element in the array
		};
		llvm::Value* new_elem_ptr = params.builder->CreateInBoundsGEP(return_ptr, indices, "new elem ptr");

		params.builder->CreateStore(newval, new_elem_ptr);
	}

	return NULL;
}


size_t UpdateElementBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	throw BaseException("UpdateElementBuiltInFunc::getTimeBound: unimplemented");
}


GetSpaceBoundResults UpdateElementBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	throw BaseException("UpdateElementBuiltInFunc::getTimeBound: unimplemented");
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
		//	LLVMUtils::getNthArg(params.currently_building_func, 0),
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
			llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, this->index, field_name + " ptr");

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
			llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1);

			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, this->index, field_name + " ptr");

			LLVMUtils::createCollectionCopy(
				field_type, 
				return_ptr, // dest ptr
				field_ptr, // src ptr
				params
			);

			return NULL;
		}
	}
}


size_t GetTupleElementBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults GetTupleElementBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	const TypeVRef field_type = this->tuple_type->component_types[this->index];
	// TEMP HACK TODO: work out properly
	return GetSpaceBoundResults(0, field_type->memSize());
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
		vec_value = LLVMUtils::getNthArg(
			params.currently_building_func, 
			0
		);
	}
	else
	{
		vec_value = params.builder->CreateLoad(
			LLVMUtils::getNthArg(params.currently_building_func, 0),
			false, // true,// TEMP: volatile = true to pick up returned vector);
			"argument" // name
		);
	}

	return params.builder->CreateExtractElement(
		vec_value, // vec
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index))
	);
}


size_t GetVectorElement::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults GetVectorElement::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		llvm::Value* indices[] = {
			llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)), // get the zero-th array
			i // get the indexed element in the array
		};

		llvm::Value* elem_ptr = builder.CreateInBoundsGEP(input_array, indices);
		llvm::Value* elem = builder.CreateLoad(elem_ptr);

		llvm::Value* args[] = { elem, captured_var_struct_ptr };

		// Call function on element
		llvm::Value* mapped_elem = LLVMUtils::createCallWithValue(&builder, function, args, "map function call");

		// Get pointer to output element
		llvm::Value* out_elem_ptr = builder.CreateInBoundsGEP(return_ptr, indices);
		return builder.CreateStore(mapped_elem, out_elem_ptr); // Store the element in the output array
	}

	llvm::Value* return_ptr;
	llvm::Value* function;
	llvm::Value* captured_var_struct_ptr;
	llvm::Value* input_array;
};


static void setArgumentAttributes(llvm::LLVMContext* context, llvm::Function::arg_iterator it, unsigned int index, llvm::AttrBuilder& attr_builder)
{
#if TARGET_LLVM_VERSION >= 60
		it->addAttrs(attr_builder);
#else
		llvm::AttributeSet set = llvm::AttributeSet::get(*context, index, attr_builder);
		it->addAttr(set);
#endif
}


// This function computes the map on a slice of the array given by [begin, end).
// typedef void (WINTER_JIT_CALLING_CONV * ARRAY_WORK_FUNCTION) (void* output, void* input, void* map_function, size_t begin, size_t end); // Winter code
llvm::Value* ArrayMapBuiltInFunc::insertWorkFunction(EmitLLVMCodeParams& params) const
{
	llvm::Type* int64_type = llvm::IntegerType::get(*params.context, 64);

	llvm::SmallVector<llvm::Type*, 8> arg_types(2, LLVMTypeUtils::pointerType(this->from_type->LLVMType(*params.module))); // output, input
	arg_types.push_back(this->func_type->LLVMType(*params.module)); // map_function
	arg_types.push_back(int64_type); // size_t begin
	arg_types.push_back(int64_type); // size_t end

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(params.module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Function* llvm_func = LLVMUtils::getFunctionFromModule(params.module, "work_function", functype);
	llvm::BasicBlock* block = llvm::BasicBlock::Create(params.module->getContext(), "entry", llvm_func);
	llvm::IRBuilder<> builder(block);
		

	ArrayMapBuiltInFunc_CreateLoopBodyCallBack callback;
	callback.return_ptr = LLVMUtils::getNthArg(llvm_func, 0);

	// If we have a specialised function to use, insert a call to it directly, else used the function pointer passed in as an argument.
	if(this->specialised_f)
		callback.function = this->specialised_f->getOrInsertFunction(params.module, false);
	else
		callback.function = LLVMUtils::getNthArg(llvm_func, 2);

	callback.input_array = LLVMUtils::getNthArg(llvm_func, 1);

	//llvm_func->addAttribute(1, llvm::Attribute::getWithAlignment(*params.context, 3));
	// Set some attributes

	llvm::Function::arg_iterator AI = llvm_func->arg_begin();
	{
		llvm::AttrBuilder attr_builder;
		attr_builder.addAlignmentAttr(32);
		attr_builder.addAttribute(llvm::Attribute::NoAlias);		
		setArgumentAttributes(params.context, AI, /*index=*/1, attr_builder);
	}

	AI++;

	{
		llvm::AttrBuilder attr_builder;
		attr_builder.addAlignmentAttr(32);
		attr_builder.addAttribute(llvm::Attribute::NoAlias);
		setArgumentAttributes(params.context, AI, /*index=*/2, attr_builder);
	}
	

	makeForLoop(
		builder,
		params.module,
		llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), // TEMP BEGIN=0    LLVMUtils::getNthArg(llvm_func, 3), // begin index
		LLVMUtils::getNthArg(llvm_func, 4), // end index
		//from_type->elem_type->LLVMType(*params.module), // Loop value type
		&callback
	);
	

	builder.CreateRetVoid();

	return llvm_func;
}


llvm::Value* ArrayMapBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Pointer to result array
	llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

	// Closure ptr
	/*llvm::Value* closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1);

	// Get function ptr from closure ptr
	vector<llvm::Value*> indices(2);
	indices[0] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)); // get the zero-th closure
	indices[1] = llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1)); // get the 1st field (function ptr)
	
	llvm::Value* function_ptr = params.builder->CreateGEP(
		closure_ptr,
		indices
	);

	llvm::Value* function = params.builder->CreateLoad(function_ptr);*/

	llvm::Value* closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1);
	llvm::Value* function_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::capturedVarStructIndex());


	//llvm::Value* function_ptr = params.builder->CreateLoad(
	//	closure_ptr,
	//	vector<uint32_t>(1, 1) // 1st field (function ptr)
	//);

	// Input array
	llvm::Value* input_array = LLVMUtils::getNthArg(params.currently_building_func, 2);




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

		llvm::SmallVector<llvm::Type*, 8> arg_types(2, LLVMTypeUtils::voidPtrType(*params.context)); // void* output, void* input
		const TypeVRef int64_type = new Int(64);
		arg_types.push_back(int64_type->LLVMType(*params.module)); // array_size
		arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context)); // map_function
		arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context)); // work_function


		llvm::FunctionType* functype = llvm::FunctionType::get(
			llvm::Type::getVoidTy(*params.context), // return type
			arg_types,
			false // varargs
		);

		llvm::Function* llvm_func = LLVMUtils::getFunctionFromModule(params.module, "execArrayMap", functype);

		llvm::SmallVector<llvm::Value*, 8> args;
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


size_t ArrayMapBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	if(specialised_f)
		return specialised_f->getTimeBound(params) * from_type->num_elems;
	else
		throw BaseException("Unable to bound time of array map function.");
}


GetSpaceBoundResults ArrayMapBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	// TODO
	throw BaseException("TODO: Unable to bound space of array map function.");
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
		closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0); // Pointer to function
		array_arg = LLVMUtils::getNthArg(params.currently_building_func, 1);
		initial_state_ptr_or_value = LLVMUtils::getNthArg(params.currently_building_func, 2); // Pointer to, or value of initial state
	}
	else
	{
		return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0); // Pointer to result structure
		closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1); // Pointer to function
		array_arg = LLVMUtils::getNthArg(params.currently_building_func, 2);
		initial_state_ptr_or_value = LLVMUtils::getNthArg(params.currently_building_func, 3); // Pointer to, or value of initial state
	}

	llvm::Value* function_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::capturedVarStructIndex());


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
				LLVMUtils::createMemCpy(params.builder, running_state_alloca, initial_state_ptr_or_value, size, 4);
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
		llvm::Value* indices[] = {
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the zero-th array
			loop_index_var // get the indexed element in the array
		};

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
				LLVMUtils::createMemCpy(params.builder, return_ptr, running_state_alloca, size, 4);
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
				LLVMUtils::createMemCpy(params.builder, new_state_alloca, initial_state_ptr_or_value, size, 4);
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
	llvm::Value* indices[] = {
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)), // get the zero-th array
		loop_index_var // get the indexed element in the array
	};

	llvm::Value* array_elem_ptr = params.builder->CreateInBoundsGEP(array_arg, indices, "array elem ptr");
	llvm::Value* array_elem = params.builder->CreateLoad(array_elem_ptr, "array elem");

	// Copy the state from new_state_alloca to running_state_alloca
	if(state_type->getType() == Type::ArrayTypeType)
	{
		llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
		LLVMUtils::createMemCpy(params.builder, running_state_alloca, new_state_alloca, size, 4);
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
		llvm::Value* args[] = { running_state_value, array_elem, captured_var_struct_ptr };
		llvm::Value* next_running_state_value = LLVMUtils::createCallWithValue(params.builder, function, args);

		// Store new value in running_state_alloca
		params.builder->CreateStore(next_running_state_value, new_state_alloca); // running_state_alloca);
	}
	else
	{
		// Call function on element
		llvm::Value* args[] = { new_state_alloca, // SRET return value arg
			running_state_alloca, // current state
			array_elem, // array element
			captured_var_struct_ptr };

		LLVMUtils::createCallWithValue(params.builder, function, args);

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
			LLVMUtils::createMemCpy(params.builder, return_ptr, new_state_alloca/*running_state_alloca*/, size, 4);
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


size_t ArrayFoldBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	if(specialised_f)
		return specialised_f->getTimeBound(params) * array_type->num_elems;
	else
		throw BaseException("Unable to bound time of array fold function.");
}


GetSpaceBoundResults ArrayFoldBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	// TODO
	throw BaseException("TODO: Unable to bound space of array fold function.");
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
	llvm::Value* array_ptr = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

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
	llvm::Value* array_ptr = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 0);
	llvm::Value* index_vec = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// We have a single ptr, we need to shuffle it to an array of ptrs.

	/*std::cout << "array_ptr:" << std::endl;
	array_ptr->dump();
	std::cout << std::endl;
	std::cout << "array_ptr type:" << std::endl;
	array_ptr->getType()->dump();
	std::cout << std::endl;*/

	// TEMP: Get pointer to index 0 of the array:
	llvm::Value* array_elem0_ptr = LLVMUtils::createStructGEP(params.builder, array_ptr, 0);

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
		LLVMUtils::makeVectorElemCount(index_vec_num_elems),
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
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

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
			llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

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
				llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

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
			//llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

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


size_t ArraySubscriptBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults ArraySubscriptBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	const size_t elem_size = this->array_type->elem_type->memSize();
	return GetSpaceBoundResults(0, elem_size);
}


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
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	
	if(index_type->getType() == Type::IntType)
	{
		// Scalar index

		if(field_type->passByValue())
		{
			llvm::Value* varray_ptr = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 0);
			
			//varray_ptr->dump();
			//varray_ptr->getType()->dump();

			//llvm::Value* data_ptr_ptr = params.builder->CreateStructGEP(varray_ptr, 1, "data ptr ptr");
			//llvm::Value* data_ptr = params.builder->CreateLoad(data_ptr_ptr);
			llvm::Value* data_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 3, "data_ptr"); // [0 x T]*

			llvm::Value* indices[] = { llvm::ConstantInt::get(*params.context, llvm::APInt(64, 0)), index };
			llvm::Value* elem_ptr = params.builder->CreateInBoundsGEP(data_ptr, llvm::makeArrayRef(indices));

			
			return params.builder->CreateLoad(elem_ptr);
		}
		else // Else if element type is pass-by-pointer
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

			llvm::Value* varray_ptr = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 0);
			llvm::Value* data_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 3, "data_ptr"); // [0 x T]*

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


size_t VArraySubscriptBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults VArraySubscriptBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	const size_t elem_size = this->array_type->elem_type->memSize();
	return GetSpaceBoundResults(0, elem_size);
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
	virtual llvm::Value* emitLoopBody(llvm::IRBuilder<>& builder, llvm::Module* module, llvm::Value* i)
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
			LLVMUtils::createCollectionCopy(
				array_type->elem_type, 
				element_ptr, // dest ptr
				element_value, // src ptr
				*params
			);

			// If the element has a ref-counted type, we need to increment its reference count, since the newly constructed array now holds a reference to it. 
			// (and to compensate for the decrement of the argument in the function application code)
			array_type->elem_type->emitIncrRefCount(*params, element_value, "makeVArray() for type " + this->array_type->toString());
		}

		return NULL;
	}

	EmitLLVMCodeParams* params;
	llvm::Value* element_value;
	llvm::Value* data_ptr;
	Reference<VArrayType> array_type;
};


llvm::Value* MakeVArrayBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* elem_val  = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* count_val = LLVMUtils::getNthArg(params.currently_building_func, 1);

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

	llvm::Value* args[] = { size_B_constant, count_val };
	llvm::CallInst* call_inst = params.builder->CreateCall(allocateVArrayLLVMFunc, args, "varray");

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	call_inst->setCallingConv(llvm::CallingConv::C);

	// Cast resulting allocated void* down to VArrayRep for the right type, e.g. varray<T>
	llvm::Type* varray_T_type = this->array_type->LLVMType(*params.module);
	assert(varray_T_type->isPointerTy());

	llvm::Value* varray_ptr = params.builder->CreatePointerCast(call_inst, varray_T_type);
	uint64 initial_flags = 1; // flag = 1 = heap allocated


	// Set the reference count to 1
	llvm::Value* ref_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 0, "varray_literal_ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, /*signed=*/true));
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, ref_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "makeVArray result set initial ref count to 1");

	// Set VArray length
	llvm::Value* length_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 1, "varray_literal_length_ptr");
	llvm::StoreInst* store_length_inst = params.builder->CreateStore(count_val, length_ptr);
	addMetaDataCommentToInstruction(params, store_length_inst, "makeVArray result set initial length count.");

	// Set the flags
	llvm::Value* flags_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 2, "varray_literal_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "makeVArray result set initial flags to " + toString(initial_flags));


	llvm::Value* data_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 3, "varray_literal_data_ptr");


	// Emit for loop to write count copies of the element to the varray.

	MakeVArrayBuiltInFunc_CreateLoopBodyCallBack callback;
	callback.params = &params;
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


size_t MakeVArrayBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	// TODO: this is a tricky one.  The runtime of this should be bounded by the size argument, which is only known at runtime.
	throw BaseException("Unable to bound time of makeVArray function.");
}


GetSpaceBoundResults MakeVArrayBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	throw BaseException("Unable to bound space of makeVArray function.");
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
	llvm::Value* vec       = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, 1);

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
			llvm::Value* return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

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


size_t VectorSubscriptBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults VectorSubscriptBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	const size_t elem_size = this->vec_type->elem_type->memSize();
	return GetSpaceBoundResults(0, elem_size);
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
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// Create bounds check condition code
	return params.builder->CreateAnd(
		params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
		params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->array_type->num_elems, true))) // index < array num elems
	);
}


size_t ArrayInBoundsBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults ArrayInBoundsBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
	llvm::Value* index     = LLVMUtils::getNthArg(params.currently_building_func, arg_offset + 1);

	// Create bounds check condition code
	return params.builder->CreateAnd(
		params.builder->CreateICmpSGE(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))), // index >= 0
		params.builder->CreateICmpSLT(index, llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->vector_type->num, true))) // index < vector num elems
	);
}


size_t VectorInBoundsBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults VectorInBoundsBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		const size_t inital_arg_stack_size = vmstate.argument_stack.size();
		vmstate.func_args_start.push_back((unsigned int)inital_arg_stack_size);
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(new IntValue(iteration, true)); // Push iteration
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			vmstate.argument_stack.push_back(invariant_data[i]);
		
		// Call f
		if(f->func_def->is_anon_func)
			vmstate.argument_stack.push_back(f->captured_vars.getPointer());

		ValueRef result = f->func_def->invoke(vmstate);
		
		// Unpack result
		const TupleValue* tuple_result = checkedCast<const TupleValue>(result.ptr());

		ValueRef new_running_val = tuple_result->e[0];
		bool continue_bool = checkedCast<const BoolValue>(tuple_result->e[1].ptr())->value;

		vmstate.argument_stack.resize(inital_arg_stack_size); // Pop args off stack.
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
		closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0); // Pointer to function
		initial_state_ptr_or_value = LLVMUtils::getNthArg(params.currently_building_func, 1); // Pointer to, or value of initial state
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			invariant_data_ptr_or_value[i] = LLVMUtils::getNthArg(params.currently_building_func, 2 + (int)i); // Pointer to, or value of invariant_data
	}
	else
	{
		return_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0); // Pointer to result structure
		closure_ptr = LLVMUtils::getNthArg(params.currently_building_func, 1); // Pointer to function
		initial_state_ptr_or_value = LLVMUtils::getNthArg(params.currently_building_func, 2); // Pointer to, or value of initial state
		for(size_t i=0; i<invariant_data_types.size(); ++i)
			invariant_data_ptr_or_value[i] = LLVMUtils::getNthArg(params.currently_building_func, 3 + (int)i); // Pointer to, or value of invariant_data
	}


	llvm::Value* function_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::functionPtrIndex());
	llvm::Value* function = params.builder->CreateLoad(function_ptr);
	llvm::Value* captured_var_struct_ptr = LLVMUtils::createStructGEP(params.builder, closure_ptr, Function::capturedVarStructIndex());


	// Allocate space on stack for tuple<State, bool> returned from f.
		
	// Emit the alloca in the entry block for better code-gen.
	// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
	llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

	llvm::Value* tuple_alloca = entry_block_builder.CreateAlloca(
		func_type->return_type->LLVMType(*params.module), // tuple<State, bool>
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
		"tuple_space"
	);

	// Allocate space on stack for the running state, if the state type is not pass-by-value.
	llvm::Value* state_alloca = NULL;

	//if(!state_type->passByValue())
	//{
		state_alloca = entry_block_builder.CreateAlloca(
			state_type->LLVMType(*params.module), // State
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"running_state"
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
	llvm::SmallVector<llvm::Value*, 8> args;
	args.push_back(tuple_alloca); // SRET return value arg
	args.push_back(state_type->passByValue() ? params.builder->CreateLoad(state_alloca) : state_alloca); // current state
	args.push_back(loop_index_var); // iteration
	for(size_t i=0; i<invariant_data_types.size(); ++i)
		args.push_back(invariant_data_ptr_or_value[i]);

	args.push_back(captured_var_struct_ptr); // captured var struct ptr

	LLVMUtils::createCallWithValue(params.builder, function, args);

	// The result of the function (tuple<State, bool>) should now be stored in 'tuple_alloca'.

	// copy tuple_alloca->first to state_alloca

	llvm::Value* state = params.builder->CreateLoad(LLVMUtils::createStructGEP(params.builder, tuple_alloca, 0)); // Load the state from tuple_alloca
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
	llvm::Value* continue_bool_ptr = LLVMUtils::createStructGEP(params.builder, tuple_alloca, 1);
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
		params.builder->CreateStore(running_state, return_ptr);
		return return_ptr;
	}
}


/*
Emit something like, at file scope:

--------------------------------------
State iterate_0(State initial_state, LoopInvariantData0)
{
	State state = initial_state;

	int iteration = 0;
	while(1)
	{
		tuple_State_bool res = f(state, iteration, LoopInvariantData0);

		if(res.field1 == false)
			return res.field0;

		iteration++;
		state = res.field_0;
	}
}
--------------------------------------
and then

--------------------------------------
iterate_0(initial_state_expr)
--------------------------------------

directly.



And in the case where f is a lambda expression:

--------------------------------------
State iterate_0(State initial_state, LoopInvariantData0Type LoopInvariantData0, CapturedVarStruct* captured_vars)
{
	State state = initial_state;

	int iteration = 0;
	while(1)
	{
		tuple_State_bool res = f(state, iteration, LoopInvariantData0, captured_vars);

		if(res.field1 == false)
			return res.field0;

		iteration++;
		state = res.field_0;
	}
}
--------------------------------------

and then

--------------------------------------
CapturedVarStruct captured_vars_0;
captured_vars_0.captured_var_0 = ...
captured_vars_0.captured_var_1 = ...

iterate_0(initial_state_expr, &captured_vars_0)
--------------------------------------

directly.

*/
const std::string IterateBuiltInFunc::emitOpenCLForFunctionArg(EmitOpenCLCodeParams& params,
		const FunctionDefinition* f, // arg 0
		const std::vector<ASTNodeRef>& argument_expressions
	)
{
	assert(*this->state_type == *argument_expressions[1]->type());
	const std::string state_typename = state_type->OpenCLCType(params);
	const std::string tuple_typename = f->returnType()->OpenCLCType(params);
	const VRef<StructureType> captured_var_struct_type = f->getCapturedVariablesStructType();

	std::string s;
	// Emit return type, function name and arguments
	const int use_uid = params.uid++;
	s = state_typename + " iterate_" + toString(use_uid) + "(" + FunctionDefinition::openCLCArgumentCode(params, state_type, "initial_state");

	// Add invariant data args
	for(size_t i = 0; i<invariant_data_types.size(); ++i)
		s += ", " + FunctionDefinition::openCLCArgumentCode(params, invariant_data_types[i], "LoopInvariantData" + toString(i));

	// Add cap_var_struct arg if needed
	if(f->is_anon_func)
		s += ", " + FunctionDefinition::openCLCArgumentCode(params, captured_var_struct_type, "cap_var_struct");

	s += ")\n";
	s += "{\n";
	s += "\t" + state_typename + " state = " + (state_type->OpenCLPassByPointer() ? "*" : "") + "initial_state;\n";
	s += "\tint iteration = 0;\n";
	s += "\twhile(1)\n";
	s += "\t{\n";

	// Emit something like "tuple_State_bool res = f(state, iteration, LoopInvariantData0, captured_vars);"
	s += "\t\t" + tuple_typename + " res = " + f->sig.typeMangledName() + "(";
	if(state_type->OpenCLPassByPointer())
		s += "&";
	s += "state, iteration";
	for(size_t i = 0; i<invariant_data_types.size(); ++i)
	{
		s += ", LoopInvariantData" + toString(i);
	}
	if(f->is_anon_func)
		s += ", cap_var_struct";
	s += ");\n";

	// Emit "if(tuple_alloca->second == false)"
	s += "\t\tif(res.field_1 == false)\n";
	s += "\t\t\treturn res.field_0;\n";
	s += "\t\titeration++;\n";
	s += "\t\tstate = res.field_0;\n";
	s += "\t}\n";
	s += "}\n";
	params.file_scope_func_defs += s;

	// Return a call to the iterate function:

	if(f->is_anon_func)
	{
		// Emit code to capture variables:
		std::string cap_code;
		cap_code += captured_var_struct_type->OpenCLCType(params) + " captured_vars_" + toString(use_uid) + ";\n";
		size_t i=0;
		const auto unique_free_vars = f->getUniqueFreeVarList();
		for(auto z = unique_free_vars.begin(); z != unique_free_vars.end(); ++z, ++i)
		{
			Variable* var = *z;
			const bool need_deref = (var->nodeType() == ASTNode::VariableASTNodeType) && (var->binding_type == Variable::BindingType_Argument) && var->type()->OpenCLPassByPointer();

			cap_code += "captured_vars_" + toString(use_uid) + ".captured_var_" + toString(i) + " = " + 
				(need_deref ? "*" : "") + mapOpenCLCVarName(params.opencl_c_keywords, var->name) + ";\n";
		}
		cap_code += "\n";
		params.blocks.back() += cap_code;
	}

	std::string call_code;
	call_code += "iterate_" + toString(use_uid) + "(" + FunctionExpression::emitCodeForFuncArg(params, argument_expressions[1], f, f->args[1].name);

	for(size_t i = 0; i<invariant_data_types.size(); ++i)
		call_code += ", " + FunctionExpression::emitCodeForFuncArg(params, argument_expressions[2 + i], f, f->args[2 + i].name);

	if(f->is_anon_func)
		call_code += ", &captured_vars_" + toString(use_uid);

	call_code += ")";
	return call_code;
}


size_t IterateBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	throw BaseException("Unable to bound time of iterate function.");
}


GetSpaceBoundResults IterateBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	throw BaseException("Unable to bound space of iterate function.");
}


//----------------------------------------------------------------------------------------------


ValueRef DotProductBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	const int use_num_comp = (this->num_components != -1) ? myMin((int)vector_type->num, this->num_components) : (int)vector_type->num;

	if(vector_type->elem_type->getType() == Type::FloatType)
	{
		FloatValueRef res = new FloatValue(0.0f);
		for(int i=0; i<use_num_comp; ++i)
			res->value += checkedCast<const FloatValue>(a->e[i].getPointer())->value * checkedCast<const FloatValue>(b->e[i].getPointer())->value;
		return res;
	}
	else
	{
		DoubleValueRef res = new DoubleValue(0.0f);
		for(int i=0; i<use_num_comp; ++i)
			res->value += checkedCast<const DoubleValue>(a->e[i].getPointer())->value * checkedCast<const DoubleValue>(b->e[i].getPointer())->value;
		return res;
	}
}


llvm::Value* DotProductBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMUtils::getNthArg(params.currently_building_func, 1);

	// TODO: Add Neon (ARM64) instruction for dot product

	// If have SSE4.1 and this is a 4-vector of floats, using DPPS instruction
	if(vector_type->elem_type->getType() == Type::FloatType && this->vector_type->num == 4 && (params.cpu_info->isX64() && params.cpu_info->cpu_info.sse4_1))
	{
		// Emit dot product intrinsic
		llvm::SmallVector<llvm::Value*, 3> args;
		args.push_back(a);
		args.push_back(b);

		const int use_num_components = (this->num_components != -1) ? myMin((int)vector_type->num, this->num_components) : 4;
		/*
		use_num_components = 1  =>    upper_mask = 0001
		use_num_components = 2  =>    upper_mask = 0011
		use_num_components = 3  =>    upper_mask = 0111
		use_num_components = 4  =>    upper_mask = 1111
		*/
		const int upper_mask = (1 << use_num_components) - 1;
		const int mask = (upper_mask << 4) | 15;

#if TARGET_LLVM_VERSION <= 34
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, mask))); // SSE DPPS control bits
#else
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/8, mask))); // SSE DPPS control bits
#endif

		llvm::Function* dot_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse41_dpps);

		// dot product intrinsic returns a 4-vector.
		llvm::Value* vector_res = params.builder->CreateCall(dot_func, args, "Vector_res");

		return params.builder->CreateExtractElement(
			vector_res, // vec
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)) // index
		);
	}
	// If have SSE4.1 and this is a 2-vector of doubles, using DPPD instruction
	else if(vector_type->elem_type->getType() == Type::DoubleType && this->vector_type->num == 2 && (params.cpu_info->isX64() && params.cpu_info->cpu_info.sse4_1))
	{
		// Emit dot product intrinsic
		llvm::SmallVector<llvm::Value*, 3> args;
		args.push_back(a);
		args.push_back(b);

		const int use_num_components = (this->num_components != -1) ? myMin((int)vector_type->num, this->num_components) : 2;
		/*
		use_num_components = 1  =>    upper_mask = 0001
		use_num_components = 2  =>    upper_mask = 0011
		*/
		const int upper_mask = (1 << use_num_components) - 1;
		const int mask = (upper_mask << 4) | 15;

#if TARGET_LLVM_VERSION <= 34
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, mask))); // SSE DPPD control bits
#else
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/8, mask))); // SSE DPPD control bits
#endif

		llvm::Function* dot_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse41_dppd);

		// dot product intrinsic returns a 4-vector.
		llvm::Value* vector_res = params.builder->CreateCall(dot_func, args, "Vector_res");

		return params.builder->CreateExtractElement(
			vector_res, // vec
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)) // index
		);
	}
	else
	{
		const int use_num_components = (this->num_components != -1) ? myMin((int)vector_type->num, this->num_components) : vector_type->num;
		assert(use_num_components >= 1);

		// x = a[0] * b[0]
		llvm::Value* x = params.builder->CreateBinOp(
			llvm::Instruction::FMul, 
			params.builder->CreateExtractElement(a, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0))),
			params.builder->CreateExtractElement(b, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)))
		);
			
		for(int i=1; i<use_num_components; ++i)
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


size_t DotProductBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return vector_type->num;
}


GetSpaceBoundResults DotProductBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
	llvm::Value* a = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMUtils::getNthArg(params.currently_building_func, 1);

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


size_t VectorMinBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return vector_type->num;
}


GetSpaceBoundResults VectorMinBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
	llvm::Value* a = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMUtils::getNthArg(params.currently_building_func, 1);

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


size_t VectorMaxBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return vector_type->num;
}


GetSpaceBoundResults VectorMaxBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
	llvm::Value* a = LLVMUtils::getNthArg(params.currently_building_func, 0);
	//llvm::Value* index_vec = LLVMUtils::getNthArg(params.currently_building_func, 1);


	llvm::Constant* mask;
	if(shuffle_mask.empty()) // if shuffle mask has not been set yet, just set to a zero mask vector of the final size.
	{
		assert(0);

		llvm::SmallVector<llvm::Constant*, 8> elems(index_type->num);
		for(unsigned int i=0; i<index_type->num; ++i)
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
		llvm::SmallVector<llvm::Constant*, 8> elems((unsigned int)shuffle_mask.size());
		for(size_t i=0; i<shuffle_mask.size(); ++i)
		{
			if(shuffle_mask[i] < 0 || shuffle_mask[i] >= (int)vector_type->num)
				throw ExceptionWithPosition("Shuffle mask index " + toString(shuffle_mask[i]) + " out of bounds: ", errorContext(params.currently_building_func_def));

			elems[(unsigned int)i] = llvm::ConstantInt::get(
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


size_t ShuffleBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return vector_type->num;
}


GetSpaceBoundResults ShuffleBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
	llvm::Value* args[] = {
		LLVMUtils::getNthArg(params.currently_building_func, 0),
		LLVMUtils::getNthArg(params.currently_building_func, 1),
	};

	llvm::Type* types[] = { this->type->LLVMType(*params.module) };

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::pow, types);

	assert(func);
	assert(func->isIntrinsic());

	return params.builder->CreateCall(
		func,
		args
	);
}


size_t PowBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 200;
}


GetSpaceBoundResults PowBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(256, 0); // The CMath function called by the intrinsic may use some stack space.
}


//----------------------------------------------------------------------------------------------


static llvm::Value* emitUnaryIntrinsic(EmitLLVMCodeParams& params, const TypeVRef& type, llvm::Intrinsic::ID id)
{
	assert(type->getType() == Type::FloatType || type->getType() == Type::DoubleType || (type->getType() == Type::VectorTypeType));

	llvm::Value* args[] = { LLVMUtils::getNthArg(params.currently_building_func, 0) };

	llvm::Type* types[] = { type->LLVMType(*params.module) };

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


size_t SqrtBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 15;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults SqrtBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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


size_t ExpBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 40;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults ExpBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(256, 0); // The CMath function called by the intrinsic may use some stack space.
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


size_t LogBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 40;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults LogBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(256, 0); // The CMath function called by the intrinsic may use some stack space.
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


size_t SinBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 30;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults SinBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(256, 0); // The CMath function called by the intrinsic may use some stack space.
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


size_t CosBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 30;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults CosBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(256, 0); // The CMath function called by the intrinsic may use some stack space.
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


size_t AbsBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults AbsBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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


size_t FloorBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults FloorBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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


size_t CeilBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults CeilBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		return new FloatValue(Maths::sign(a->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
		return new DoubleValue(Maths::sign(a->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		if(vector_type->elem_type->getType() == Type::FloatType)
		{
			for(unsigned int i=0; i<vector_type->num; ++i)
			{
				const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
				res_values[i] = new FloatValue(Maths::sign(x));
			}
		}
		else
		{
			assert(vector_type->elem_type->getType() == Type::DoubleType);

			for(unsigned int i=0; i<vector_type->num; ++i)
			{
				const double x = checkedCast<const DoubleValue>(a->e[i].getPointer())->value;
				res_values[i] = new DoubleValue(Maths::sign(x));
			}
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* SignBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Type* types[] = {this->type->LLVMType(*params.module)};

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::copysign, types);

	assert(func);
	assert(func->isIntrinsic());

	llvm::Value* scalar_zero;
	llvm::Value* scalar_one;
	if(type->getType() == Type::FloatType || (type->getType() == Type::VectorTypeType && type.downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType))
	{
		scalar_zero = llvm::ConstantFP::get(*params.context, llvm::APFloat(0.f));
		scalar_one  = llvm::ConstantFP::get(*params.context, llvm::APFloat(1.f));
	}
	else
	{
		scalar_zero = llvm::ConstantFP::get(*params.context, llvm::APFloat(0.0));
		scalar_one  = llvm::ConstantFP::get(*params.context, llvm::APFloat(1.0));
	}

	llvm::Value* one;
	llvm::Value* zero;
	if(type->getType() == Type::VectorTypeType)
	{
		zero = params.builder->CreateVectorSplat(/*num elements=*/this->type.downcastToPtr<VectorType>()->num, scalar_zero);
		one  = params.builder->CreateVectorSplat(/*num elements=*/this->type.downcastToPtr<VectorType>()->num, scalar_one);
	}
	else
	{
		zero = scalar_zero;
		one  = scalar_one;
	}

	llvm::Value* arg0 = LLVMUtils::getNthArg(params.currently_building_func, 0);

	llvm::Value* magnitude = params.builder->CreateSelect(
		params.builder->CreateFCmpOEQ(arg0, zero), // condition: arg0 == zero
		zero, // true
		one // false
	);

	// "The ‘llvm.copysign.*‘ intrinsics return a value with the magnitude of the first operand and the sign of the second operand."
	llvm::Value* args[] = { magnitude, arg0 };
	return params.builder->CreateCall(func, args);
}


size_t SignBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 5;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults SignBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
}


//----------------------------------------------------------------------------------------------


FRemBuiltInFunc::FRemBuiltInFunc(const TypeVRef& type_)
	: BuiltInFunctionImpl(BuiltInType_FRemBuiltInFunc),
	type(type_)
{}


ValueRef FRemBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		const FloatValue* a = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back()    ].getPointer());
		const FloatValue* b = checkedCast<const FloatValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
		return new FloatValue(::fmodf(a->value, b->value));
	}
	else if(type->getType() == Type::DoubleType)
	{
		const DoubleValue* a = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back()    ].getPointer());
		const DoubleValue* b = checkedCast<const DoubleValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
		return new DoubleValue(::fmod(a->value, b->value));
	}
	else
	{
		assert(type->getType() == Type::VectorTypeType);

		const VectorType* vector_type = static_cast<const VectorType*>(type.getPointer());

		const VectorValue* a = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back()    ].getPointer());
		const VectorValue* b = checkedCast<const VectorValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

		vector<ValueRef> res_values(vector_type->num);
		if(vector_type->elem_type->getType() == Type::FloatType)
		{
			for(unsigned int i=0; i<vector_type->num; ++i)
			{
				const float x = checkedCast<const FloatValue>(a->e[i].getPointer())->value;
				const float y = checkedCast<const FloatValue>(b->e[i].getPointer())->value;
				res_values[i] = new FloatValue(::fmodf(x, y));
			}
		}
		else
		{
			assert(vector_type->elem_type->getType() == Type::DoubleType);

			for(unsigned int i=0; i<vector_type->num; ++i)
			{
				const double x = checkedCast<const DoubleValue>(a->e[i].getPointer())->value;
				const double y = checkedCast<const DoubleValue>(b->e[i].getPointer())->value;
				res_values[i] = new DoubleValue(::fmod(x, y));
			}
		}

		return new VectorValue(res_values);
	}
}


llvm::Value* FRemBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return params.builder->CreateFRem(
		LLVMUtils::getNthArg(params.currently_building_func, 0),
		LLVMUtils::getNthArg(params.currently_building_func, 1)
	);
}


size_t FRemBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 30;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults FRemBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(512, 0); // The CMath function called (fmod) may use some stack space.
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
		LLVMUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type
	);
}


size_t TruncateToIntBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults TruncateToIntBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
				LLVMUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
		else
			return params.builder->CreateUIToFP(
				LLVMUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
	}
	else if(type->getType() == Type::VectorTypeType) 
	{
		// TODO: TEST THIS vector stuff.  quite possible doesn't work.

		if(type.downcastToPtr<VectorType>()->elem_type.downcastToPtr<Int>()->is_signed)
			return params.builder->CreateSIToFP(
				LLVMUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
		else
			return params.builder->CreateUIToFP(
				LLVMUtils::getNthArg(params.currently_building_func, 0),
				dest_llvm_type // dest type
			);
	}
	else
		throw BaseException("ToFloatBuiltInFunc::emitLLVMCode todo");
}


size_t ToFloatBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults ToFloatBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		LLVMUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


size_t ToDoubleBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults ToDoubleBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		LLVMUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


size_t ToInt64BuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults ToInt64BuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		LLVMUtils::getNthArg(params.currently_building_func, 0), 
		dest_llvm_type // dest type
	);
}


size_t ToInt32BuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	return (type->getType() == Type::VectorTypeType) ? (type.downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;
}


GetSpaceBoundResults ToInt32BuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
		LLVMUtils::getNthArg(params.currently_building_func, 0), 
		int_type->LLVMType(*params.module) // dest type
	);
}


size_t VoidPtrToInt64BuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults VoidPtrToInt64BuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
			llvm::Value* varray_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);
			llvm::Value* len_field_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 1, "len_field_ptr");
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


size_t LengthBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults LengthBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
}


//----------------------------------------------------------------------------------------------


CompareEqualBuiltInFunc::CompareEqualBuiltInFunc(const TypeVRef& arg_type_, bool is_compare_not_equal_)
:	BuiltInFunctionImpl(BuiltInType_CompareEqualBuiltInFunc),
	arg_type(arg_type_),
	is_compare_not_equal(is_compare_not_equal_)
{}


// Tests if two values are equal.
// Throws an exception if the values do not belong to 'required_type'.
// This is a standalone function so it can be called recursively.
static bool areValuesEqual(const TypeVRef& required_type, const ValueRef& a, const ValueRef& b)
{
	switch(required_type->getType())
	{
	case Type::FloatType:
		return checkedCast<const FloatValue>(a)->value == checkedCast<const FloatValue>(b)->value;
	case Type::DoubleType:
		return checkedCast<const DoubleValue>(a)->value == checkedCast<const DoubleValue>(b)->value;
	case Type::IntType:
		return checkedCast<const IntValue>(a)->value == checkedCast<const IntValue>(b)->value;
	case Type::StringType:
		return checkedCast<const StringValue>(a)->value == checkedCast<const StringValue>(b)->value;
	case Type::CharTypeType:
		return checkedCast<const CharValue>(a)->value == checkedCast<const CharValue>(b)->value;
	case Type::BoolType:
		return checkedCast<const BoolValue>(a)->value == checkedCast<const BoolValue>(b)->value;
	//case Type::MapType: // TODO: handle
	//	return checkedCast<const MapValue>(a)->value == checkedCast<const MapValue>(b)->value;
	case Type::ArrayTypeType:
	{
		const ArrayType* array_type = required_type.downcastToPtr<ArrayType>();
		const ArrayValue* a_array_val = checkedCast<const ArrayValue>(a);
		const ArrayValue* b_array_val = checkedCast<const ArrayValue>(b);

		if(a_array_val->e.size() != array_type->num_elems)
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");
		if(b_array_val->e.size() != array_type->num_elems)
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");

		for(size_t i=0; i<a_array_val->e.size(); ++i)
			if(!areValuesEqual(array_type->elem_type, a_array_val->e[i], b_array_val->e[i]))
				return false;
		return true;
	}
	case Type::VArrayTypeType:
	{
		const VArrayType* array_type = required_type.downcastToPtr<VArrayType>();
		const VArrayValue* a_array_val = checkedCast<const VArrayValue>(a);
		const VArrayValue* b_array_val = checkedCast<const VArrayValue>(b);

		if(a_array_val->e.size() != b_array_val->e.size())
			return false;

		for(size_t i=0; i<a_array_val->e.size(); ++i)
			if(!areValuesEqual(array_type->elem_type, a_array_val->e[i], b_array_val->e[i]))
				return false;
		return true;
	}
	//case Type::FunctionType:  // TODO: handle
	case Type::StructureTypeType:
	{
		const StructureType* struct_type = required_type.downcastToPtr<StructureType>();
		const StructureValue* a_struct_val = checkedCast<const StructureValue>(a);
		const StructureValue* b_struct_val = checkedCast<const StructureValue>(b);

		if(a_struct_val->fields.size() != struct_type->component_types.size())
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");
		if(b_struct_val->fields.size() != struct_type->component_types.size())
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");

		for(size_t i=0; i<a_struct_val->fields.size(); ++i)
			if(!areValuesEqual(struct_type->component_types[i], a_struct_val->fields[i], b_struct_val->fields[i]))
				return false;
		return true;
	}
	case Type::TupleTypeType:
	{
		const TupleType* tuple_type = required_type.downcastToPtr<TupleType>();
		const TupleValue* a_tuple_val = checkedCast<const TupleValue>(a);
		const TupleValue* b_tuple_val = checkedCast<const TupleValue>(b);

		if(a_tuple_val->e.size() != tuple_type->component_types.size())
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");
		if(b_tuple_val->e.size() != tuple_type->component_types.size())
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");

		for(size_t i=0; i<a_tuple_val->e.size(); ++i)
			if(!areValuesEqual(tuple_type->component_types[i], a_tuple_val->e[i], b_tuple_val->e[i]))
				return false;
		return true;
	}
	case Type::VectorTypeType:
	{
		const VectorType* vector_type = required_type.downcastToPtr<VectorType>();
		const VectorValue* a_vec_val = checkedCast<const VectorValue>(a);
		const VectorValue* b_vec_val = checkedCast<const VectorValue>(b);

		if(a_vec_val->e.size() != vector_type->num)
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");
		if(b_vec_val->e.size() != vector_type->num)
			throw BaseException("CompareEqualBuiltInFunc::invoke(): Invalid args.");

		for(unsigned int i=0; i<vector_type->num; ++i)
			if(!areValuesEqual(vector_type->elem_type, a_vec_val->e[i], b_vec_val->e[i]))
				return false;
		return true;
	}
	default:
		assert(0);
		throw BaseException("CompareEqualBuiltInFunc::invoke(): unhandled type " + required_type->toString());
	}
}


ValueRef CompareEqualBuiltInFunc::invoke(VMState& vmstate)
{
	const bool equal = areValuesEqual(
		arg_type, // required type
		vmstate.argument_stack[vmstate.func_args_start.back() + 0],
		vmstate.argument_stack[vmstate.func_args_start.back() + 1]
	);

	return new BoolValue(is_compare_not_equal ? !equal : equal);
}


static llvm::Value* emitElemCompareEqualLLVMCode(llvm::IRBuilder<>* builder, llvm::Module* module, const TypeVRef& type, 
	llvm::Value* a_code, llvm::Value* b_code, bool is_compare_not_equal);


/*
Pseudocode for ==:
------------------
res = true
for each element of arrays:
	if a_elem != b_elem
		res = false;
		break for loop;
return res


Pseudocode for !=:
------------------
res = false
for each element of arrays:
	if a_elem != b_elem
		res = true;
		break for loop;
return res
*/
static llvm::Value* emitArrayCompareEqualLLVMCode(llvm::IRBuilder<>* builder_, llvm::Module* module, const ArrayType& array_type,
	llvm::Value* a_code, llvm::Value* b_code, bool is_compare_not_equal)
{
	llvm::IRBuilder<>& builder = *builder_;

	llvm::Value* begin_index = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0));
	llvm::Value* end_index   = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, array_type.num_elems));

	// Make the new basic block for the loop header, inserting after current block.
	llvm::Function* current_func = builder.GetInsertBlock()->getParent();
	llvm::BasicBlock* preheader_BB = builder.GetInsertBlock();
	llvm::BasicBlock* condition_BB = llvm::BasicBlock::Create(module->getContext(), "condition", current_func);
	llvm::BasicBlock* loop_body_BB = llvm::BasicBlock::Create(module->getContext(), "loop_body", current_func);
	llvm::BasicBlock* elems_notequal_BB = llvm::BasicBlock::Create(module->getContext(), "elems_notequal_BB", current_func);
	llvm::BasicBlock* elems_equal_BB = llvm::BasicBlock::Create(module->getContext(), "elems_equal_BB", current_func);
	llvm::BasicBlock* after_BB = llvm::BasicBlock::Create(module->getContext(), "after_loop", current_func);

	// Allocate space for a boolean which is either 'elems are not equal' or 'elems are all equal' depending on is_compare_not_equal.
	llvm::Value* res_space = builder.CreateAlloca(llvm::Type::getInt1Ty(module->getContext()), 
		llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1)), "res_space");

	if(is_compare_not_equal)
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 0)), res_space); // Store 'false' in res_space
	else
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 1)), res_space); // Store 'true' in res_space

	builder.SetInsertPoint(preheader_BB);
	builder.CreateBr(condition_BB); // Insert an explicit fall through from the current block to the condition_BB.

	//============================= condition_BB ================================
	builder.SetInsertPoint(condition_BB);

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = builder.CreatePHI(llvm::Type::getInt64Ty(module->getContext()), 2, "loop_index_var");
	loop_index_var->addIncoming(begin_index, preheader_BB);

	// Compute the end condition. (i != end)
	llvm::Value* end_cond = builder.CreateICmpNE(end_index, loop_index_var, "loopcond");

	// Insert the conditional branch
	builder.CreateCondBr(end_cond, /*true=*/loop_body_BB, /*false=*/after_BB);

	//============================= loop_body_BB ================================
	builder.SetInsertPoint(loop_body_BB);

	// Load element from input array
	llvm::Value* indices[] = {
		llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)), // get the zero-th array
		loop_index_var // get the indexed element in the array
	};

	llvm::Value* a_elem_ptr = builder.CreateInBoundsGEP(a_code, indices);
	llvm::Value* b_elem_ptr = builder.CreateInBoundsGEP(b_code, indices);
	llvm::Value* compare_elems_res = emitElemCompareEqualLLVMCode(&builder, module, array_type.elem_type, 
		a_elem_ptr, b_elem_ptr, /*is_compare_not_equal=*/false);

	// Add a branch to to after_BB to take if the elements are not equal
	builder.CreateCondBr(compare_elems_res, /*true=*/elems_equal_BB, /*false=*/elems_notequal_BB);

	//============================= elems_notequal_BB ================================
	builder.SetInsertPoint(elems_notequal_BB);

	if(is_compare_not_equal)
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 1)), res_space); // Store 'true' in res_space
	else
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 0)), res_space); // Store 'false' in res_space

	builder.CreateBr(after_BB); // Jump to 'after' basic block

	//============================= elems_equal_BB ================================
	// Increments the loop index then jumps to condition basic block.
	builder.SetInsertPoint(elems_equal_BB);

	llvm::Value* one = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1));
	llvm::Value* next_var = builder.CreateAdd(loop_index_var, one, "next_var");

	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, elems_equal_BB);

	// Do jump back to condition basic block
	builder.CreateBr(condition_BB);
	
	//============================= after_BB ================================
	builder.SetInsertPoint(after_BB);

	return builder.CreateLoad(res_space);
}


/*
// In-memory string representation for a Winter VArray
class VArrayRep
{
public:
	uint64 refcount;
	uint64 len;
	uint64 flags;
	// Data follows..
};
*/
static llvm::Value* emitVArrayCompareEqualLLVMCode(llvm::IRBuilder<>* builder_, llvm::Module* module, const VArrayType& varray_type,
	llvm::Value* a_code, llvm::Value* b_code, bool is_compare_not_equal)
{
	llvm::IRBuilder<>& builder = *builder_;

	// Load length fields from varray objects:
	llvm::Value* a_len = builder.CreateLoad(LLVMUtils::createStructGEP(builder_, a_code, 1, "a_len"));
	llvm::Value* b_len = builder.CreateLoad(LLVMUtils::createStructGEP(builder_, b_code, 1, "b_len"));

	// Get data pointers for the varray objects:
	llvm::Value* a_data_ptr = LLVMUtils::createStructGEP(builder_, a_code, 3, "a_data_ptr"); // [0 x T]*
	llvm::Value* b_data_ptr = LLVMUtils::createStructGEP(builder_, b_code, 3, "b_data_ptr"); // [0 x T]*

	llvm::Value* begin_index = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0));
	llvm::Value* end_index   = a_len;

	// Make the new basic block for the loop header, inserting after current block.
	llvm::Function* current_func = builder.GetInsertBlock()->getParent();
	llvm::BasicBlock* preheader_BB = builder.GetInsertBlock();
	llvm::BasicBlock* condition_BB = llvm::BasicBlock::Create(module->getContext(), "condition", current_func);
	llvm::BasicBlock* loop_body_BB = llvm::BasicBlock::Create(module->getContext(), "loop_body", current_func);
	llvm::BasicBlock* elems_notequal_BB = llvm::BasicBlock::Create(module->getContext(), "elems_notequal_BB", current_func);
	llvm::BasicBlock* elems_equal_BB = llvm::BasicBlock::Create(module->getContext(), "elems_equal_BB", current_func);
	llvm::BasicBlock* after_BB = llvm::BasicBlock::Create(module->getContext(), "after_loop", current_func);

	// Allocate space for a boolean which is either 'elems are not equal' or 'elems are all equal' depending on is_compare_not_equal.
	llvm::Value* res_space = builder.CreateAlloca(llvm::Type::getInt1Ty(module->getContext()), 
		llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1)), "res_space");

	if(is_compare_not_equal)
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 0)), res_space); // Store 'false' in res_space
	else
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 1)), res_space); // Store 'true' in res_space

	//============================= preheader_BB ================================
	builder.SetInsertPoint(preheader_BB);

	// Compare a_len and b_len
	llvm::Value* compare_len_res =  builder.CreateICmpEQ(a_len, b_len, "compare_len_res");

	// Add a branch to elems_notequal_BB to take if the lengths are not equal
	builder.CreateCondBr(compare_len_res, /*true=*/condition_BB, /*false=*/elems_notequal_BB);

	//============================= condition_BB ================================
	builder.SetInsertPoint(condition_BB);

	// Create loop index (i) variable phi node
	llvm::PHINode* loop_index_var = builder.CreatePHI(llvm::Type::getInt64Ty(module->getContext()), 2, "loop_index_var");
	loop_index_var->addIncoming(begin_index, preheader_BB);

	// Compute the end condition. (i != end)
	llvm::Value* end_cond = builder.CreateICmpNE(end_index, loop_index_var, "loopcond");

	// Insert the conditional branch
	builder.CreateCondBr(end_cond, /*true=*/loop_body_BB, /*false=*/after_BB);

	//============================= loop_body_BB ================================
	builder.SetInsertPoint(loop_body_BB);

	// Load element from input array
	llvm::Value* indices[] = {
		llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 0)), // get the zero-th array
		loop_index_var // get the indexed element in the array
	};


	llvm::Value* a_elem_ptr = builder.CreateInBoundsGEP(a_data_ptr, indices);
	llvm::Value* b_elem_ptr = builder.CreateInBoundsGEP(b_data_ptr, indices);

	llvm::Value* compare_elems_res = emitElemCompareEqualLLVMCode(&builder, module, varray_type.elem_type, 
		a_elem_ptr, b_elem_ptr, /*is_compare_not_equal=*/false);

	// Add a branch to to elems_notequal_BB to take if the elements are not equal
	builder.CreateCondBr(compare_elems_res, /*true=*/elems_equal_BB, /*false=*/elems_notequal_BB);

	//============================= elems_notequal_BB ================================
	builder.SetInsertPoint(elems_notequal_BB);

	if(is_compare_not_equal)
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 1)), res_space); // Store 'true' in res_space
	else
		builder.CreateStore(llvm::ConstantInt::get(module->getContext(), llvm::APInt(1, 0)), res_space); // Store 'false' in res_space

	builder.CreateBr(after_BB); // Jump to 'after' basic block

	//============================= elems_equal_BB ================================
	// Increments the loop index then jumps to condition basic block.
	builder.SetInsertPoint(elems_equal_BB);

	llvm::Value* one = llvm::ConstantInt::get(module->getContext(), llvm::APInt(64, 1));
	llvm::Value* next_var = builder.CreateAdd(loop_index_var, one, "next_var");

	// Add a new entry to the PHI node for the backedge.
	loop_index_var->addIncoming(next_var, elems_equal_BB);

	// Do jump back to condition basic block
	builder.CreateBr(condition_BB);
	
	//============================= after_BB ================================
	builder.SetInsertPoint(after_BB);

	return builder.CreateLoad(res_space);
}


static llvm::Value* emitCallToBinaryFunction(llvm::IRBuilder<>* builder, llvm::Module* module, const std::string& name, const TypeVRef& arg_type, llvm::Value* a_code, llvm::Value* b_code)
{
	const FunctionSignature compare_sig(name, std::vector<TypeVRef>(2, arg_type));

	llvm::FunctionType* llvm_func_type = LLVMTypeUtils::llvmFunctionType(
		compare_sig.param_types, 
		false, // use captured var struct ptr arg
		new Bool(),
		*module
	);

	llvm::Function* llvm_func_constant = LLVMUtils::getFunctionFromModule(
		module,
		compare_sig.typeMangledName(),
		llvm_func_type // Type
	);

	llvm::Value* args[] = { a_code, b_code };
	return builder->CreateCall(llvm_func_constant, args);
}


// a_code and b_code will be pointers to structure or array elements
static llvm::Value* emitElemCompareEqualLLVMCode(llvm::IRBuilder<>* builder, llvm::Module* module, const TypeVRef& type, 
	llvm::Value* a_code, llvm::Value* b_code, bool is_compare_not_equal)
{
	const std::string compare_func_name = is_compare_not_equal ? "__compare_not_equal" : "__compare_equal";

	switch(type->getType())
	{
	case Type::FloatType:
	case Type::DoubleType:
		return is_compare_not_equal ? 
			builder->CreateFCmpONE(builder->CreateLoad(a_code), builder->CreateLoad(b_code)) : 
			builder->CreateFCmpOEQ(builder->CreateLoad(a_code), builder->CreateLoad(b_code));
	case Type::IntType:
	case Type::BoolType:
		return is_compare_not_equal ? 
			builder->CreateICmpNE(builder->CreateLoad(a_code), builder->CreateLoad(b_code)) :
			builder->CreateICmpEQ(builder->CreateLoad(a_code), builder->CreateLoad(b_code));
	case Type::StructureTypeType:
	case Type::TupleTypeType:
	{
		// a_code and b_code will have pointer-to-struct type already.
		return emitCallToBinaryFunction(builder, module, compare_func_name, type, a_code, b_code);
	}
	case Type::VectorTypeType:
	{
		return emitCallToBinaryFunction(builder, module, compare_func_name, type, builder->CreateLoad(a_code), builder->CreateLoad(b_code));
	}
	case Type::StringType:
	{
		// For string equality we will emit a call to the external func compareNotEqualString() or compareEqualString().
		return emitCallToBinaryFunction(builder, module, is_compare_not_equal ? "compareNotEqualString" : "compareEqualString", type, builder->CreateLoad(a_code), builder->CreateLoad(b_code));
	}
	case Type::ArrayTypeType:
	{
		return emitCallToBinaryFunction(builder, module, compare_func_name, type, a_code, b_code);
	}
	case Type::VArrayTypeType:
	{
		return emitCallToBinaryFunction(builder, module, compare_func_name, type, a_code, b_code);
	}
	default:
		assert(0);
		throw BaseException("emitCompareEqualLLVMCode(): unhandled type " + type->toString());
	}
}


llvm::Value* CompareEqualBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a_code = LLVMUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b_code = LLVMUtils::getNthArg(params.currently_building_func, 1);

	switch(arg_type->getType())
	{
	case Type::ArrayTypeType:
	{
		return emitArrayCompareEqualLLVMCode(params.builder, params.module, *arg_type.downcastToPtr<ArrayType>(), a_code, b_code, is_compare_not_equal);
	}
	case Type::VArrayTypeType:
	{
		return emitVArrayCompareEqualLLVMCode(params.builder, params.module, *arg_type.downcastToPtr<VArrayType>(), a_code, b_code, is_compare_not_equal);
	}
	case Type::TupleTypeType:
	{
		const TupleType* a_tuple_type = arg_type.downcastToPtr<TupleType>();

		// For each element:
		llvm::Value* elem_0_equal = emitElemCompareEqualLLVMCode(params.builder, params.module, a_tuple_type->component_types[0],
			LLVMUtils::createStructGEP(params.builder, a_code, 0),
			LLVMUtils::createStructGEP(params.builder, b_code, 0),
			is_compare_not_equal);

		llvm::Value* conjunction = elem_0_equal;
		for(size_t i=1; i<a_tuple_type->component_types.size(); ++i)
		{
			llvm::Value* elem_i_equal = emitElemCompareEqualLLVMCode(params.builder, params.module, a_tuple_type->component_types[i],
				LLVMUtils::createStructGEP(params.builder, a_code, (unsigned int)i),
				LLVMUtils::createStructGEP(params.builder, b_code, (unsigned int)i),
				is_compare_not_equal);

			conjunction = params.builder->CreateBinOp(is_compare_not_equal ? llvm::Instruction::Or : llvm::Instruction::And, conjunction, elem_i_equal);
		}
		return conjunction;
	}
	case Type::StructureTypeType:
	{
		const StructureType* a_struct_type = arg_type.downcastToPtr<StructureType>();

		// For each element:
		llvm::Value* elem_0_equal = emitElemCompareEqualLLVMCode(params.builder, params.module, a_struct_type->component_types[0],
			LLVMUtils::createStructGEP(params.builder, a_code, 0),
			LLVMUtils::createStructGEP(params.builder, b_code, 0),
			is_compare_not_equal);

		llvm::Value* conjunction = elem_0_equal;
		for(size_t i=1; i<a_struct_type->component_types.size(); ++i)
		{
			llvm::Value* elem_i_equal = emitElemCompareEqualLLVMCode(params.builder, params.module, a_struct_type->component_types[i],
				LLVMUtils::createStructGEP(params.builder, a_code, (unsigned int)i),
				LLVMUtils::createStructGEP(params.builder, b_code, (unsigned int)i),
				is_compare_not_equal);

			conjunction = params.builder->CreateBinOp(is_compare_not_equal ? llvm::Instruction::Or : llvm::Instruction::And, conjunction, elem_i_equal);
		}
		return conjunction;
	}
	case Type::VectorTypeType:
	{
		// parallel (element-wise) compare
		llvm::Value* par_eq = is_compare_not_equal ? 
			params.builder->CreateFCmpONE(a_code, b_code) :
			params.builder->CreateFCmpOEQ(a_code, b_code);

		// For later LLVMs, use experimental horizontal reduce here to do the reduction.
#if TARGET_LLVM_VERSION >= 60
#if TARGET_LLVM_VERSION >= 110
		llvm::Type* types[] = { par_eq->getType() }; // vector type
#else
		llvm::Type* types[] = { llvm::Type::getIntNTy(*params.context, 1), // elem type
			par_eq->getType() }; // vector type
#endif

		llvm::Function* vec_reduce_and = llvm::Intrinsic::getDeclaration(params.module, 
			is_compare_not_equal ? llvm::Intrinsic::experimental_vector_reduce_or : llvm::Intrinsic::experimental_vector_reduce_and, 
			types);

		return params.builder->CreateCall(vec_reduce_and, par_eq);
#else
		const VectorType* a_vector_type = arg_type.downcastToPtr<VectorType>();

		llvm::Value* elem_0 = params.builder->CreateExtractElement(par_eq, 
			llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, /*value=*/0)));

		llvm::Value* conjunction = elem_0;
		for(unsigned int i=1; i<a_vector_type->num; ++i)
		{
			llvm::Value* elem_i = params.builder->CreateExtractElement(par_eq, 
				llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, /*value=*/i)));

			conjunction = params.builder->CreateBinOp(is_compare_not_equal ? llvm::Instruction::Or : llvm::Instruction::And, conjunction, elem_i);
		}
		return conjunction;
#endif
	}
	case Type::StringType:
		return emitCallToBinaryFunction(params.builder, params.module, is_compare_not_equal ? "compareNotEqualString" : "compareEqualString", arg_type, a_code, b_code);
	default:
		throw BaseException("CompareEqualBuiltInFunc::emitLLVMCode(): unhandled type " + arg_type->toString());
	}
}


static void addCompareFunctionIfNeeeded(TraversalPayload& payload, const TypeVRef& type)
{
	if(type->requiresCompareEqualFunction())
	{
		{
			const FunctionSignature compare_sig("__compare_equal", std::vector<TypeVRef>(2, type));
			const auto res = payload.linker->sig_to_function_map.find(compare_sig);
			if(res != payload.linker->sig_to_function_map.end())
			{
				FunctionDefinition* compare_def = res->second.getPointer();
				payload.reachable_nodes.insert(compare_def); // Mark as alive
				if(payload.processed_nodes.count(compare_def) == 0) // If not processed yet:
					payload.nodes_to_process.push_back(compare_def); // Add to to-process list
			}
		}
		{
			const FunctionSignature compare_sig("__compare_not_equal", std::vector<TypeVRef>(2, type));
			const auto res = payload.linker->sig_to_function_map.find(compare_sig);
			if(res != payload.linker->sig_to_function_map.end())
			{
				FunctionDefinition* compare_def = res->second.getPointer();
				payload.reachable_nodes.insert(compare_def); // Mark as alive
				if(payload.processed_nodes.count(compare_def) == 0) // If not processed yet:
					payload.nodes_to_process.push_back(compare_def); // Add to to-process list
			}
		}
	}
}


void CompareEqualBuiltInFunc::deadFunctionEliminationTraverse(TraversalPayload& payload) const
{
	// If this is a comparison function for a composite type, need to add comparison functions we will call to the alive set.
	switch(arg_type->getType())
	{
	case Type::ArrayTypeType:
	{
		const ArrayType* a_array_type = arg_type.downcastToPtr<ArrayType>();
		addCompareFunctionIfNeeeded(payload, a_array_type->elem_type);
		break;
	}
	case Type::VArrayTypeType:
	{
		const VArrayType* a_varray_type = arg_type.downcastToPtr<VArrayType>();
		addCompareFunctionIfNeeeded(payload, a_varray_type->elem_type);
		break;
	}
	case Type::TupleTypeType:
	{
		const TupleType* a_tuple_type = arg_type.downcastToPtr<TupleType>();

		for(size_t i=0; i<a_tuple_type->component_types.size(); ++i)
			addCompareFunctionIfNeeeded(payload, a_tuple_type->component_types[i]);
		break;
	}
	case Type::StructureTypeType:
	{
		const StructureType* a_struct_type = arg_type.downcastToPtr<StructureType>();

		for(size_t i=0; i<a_struct_type->component_types.size(); ++i)
			addCompareFunctionIfNeeeded(payload, a_struct_type->component_types[i]);
		break;
	}
	case Type::VectorTypeType:
		break; // vectors can only have basic types in them that don't require a compare equal function.
	case Type::StringType:
		break;
	default:
		throw BaseException("CompareEqualBuiltInFunc::deadFunctionEliminationTraverse(): unhandled type " + arg_type->toString());
	}
}


static void linkInCompareFunctions(TraversalPayload& payload, const TypeVRef& type)
{
	if(type->requiresCompareEqualFunction())
	{
		const FunctionSignature compare_sig("__compare_equal", std::vector<TypeVRef>(2, type));
		payload.linker->findMatchingFunction(compare_sig, Winter::SrcLocation::invalidLocation(),
			/*effective_callsite_order_num=*/-1);

		const FunctionSignature compare_neq_sig("__compare_not_equal", std::vector<TypeVRef>(2, type));
		payload.linker->findMatchingFunction(compare_neq_sig, Winter::SrcLocation::invalidLocation(),
			/*effective_callsite_order_num=*/-1);


		// If this is a composite type, need to add comparison functions for child types.
		switch(type->getType())
		{
		case Type::ArrayTypeType:
		{
			const ArrayType* a_array_type = type.downcastToPtr<ArrayType>();
			linkInCompareFunctions(payload, a_array_type->elem_type);
			break;
		}
		case Type::VArrayTypeType:
		{
			const VArrayType* a_varray_type = type.downcastToPtr<VArrayType>();
			linkInCompareFunctions(payload, a_varray_type->elem_type);
			break;
		}
		case Type::TupleTypeType:
		{
			const TupleType* a_tuple_type = type.downcastToPtr<TupleType>();

			for(size_t i=0; i<a_tuple_type->component_types.size(); ++i)
				linkInCompareFunctions(payload, a_tuple_type->component_types[i]);
			break;
		}
		case Type::StructureTypeType:
		{
			const StructureType* a_struct_type = type.downcastToPtr<StructureType>();

			for(size_t i=0; i<a_struct_type->component_types.size(); ++i)
				linkInCompareFunctions(payload, a_struct_type->component_types[i]);
			break;
		}
		case Type::VectorTypeType:
			break; // vectors can only have basic types in them that don't require a compare equal function.
		case Type::StringType:
			break;
		default:
			throw BaseException("linkInCompareFunctions(): unhandled type " + type->toString());
		}
	}
}


void CompareEqualBuiltInFunc::linkInCalledFunctions(TraversalPayload& payload) const
{
	linkInCompareFunctions(payload, this->arg_type);
}


static size_t getCompareEqualTimeBound(const TypeVRef& type)
{
	switch(type->getType())
	{
	case Type::FloatType:
	case Type::DoubleType:
	case Type::IntType:
	case Type::BoolType:
	case Type::CharTypeType:
		return 1;
	case Type::StructureTypeType:
	{
		size_t sum = 0;
		const StructureType* a_struct_type = type.downcastToPtr<StructureType>();
		for(size_t i=0; i<a_struct_type->component_types.size(); ++i)
			sum += getCompareEqualTimeBound(a_struct_type->component_types[i]);
		return sum;
	}
	case Type::TupleTypeType:
	{
		size_t sum = 0;
		const TupleType* a_struct_type = type.downcastToPtr<TupleType>();
		for(size_t i=0; i<a_struct_type->component_types.size(); ++i)
			sum += getCompareEqualTimeBound(a_struct_type->component_types[i]);
		return sum;
	}
	case Type::VectorTypeType:
	{
		const VectorType* vector_type = type.downcastToPtr<VectorType>();
		return getCompareEqualTimeBound(vector_type->elem_type) * vector_type->num;
	}
	case Type::StringType:
	{
		// TODO: depends on string length (runtime value)
		throw BaseException("Unable to bound time of compare function for string type.");
	}
	case Type::ArrayTypeType:
	{
		const ArrayType* array_type = type.downcastToPtr<ArrayType>();
		return getCompareEqualTimeBound(array_type->elem_type) * array_type->num_elems;
	}
	case Type::VArrayTypeType:
	{
		// TODO: depends on string length (runtime value)
		throw BaseException("Unable to bound time of compare function for varray type.");
	}
	default:
		assert(0);
		throw BaseException("getCompareEqualTimeBound(): unhandled type " + type->toString());
	}
}

size_t CompareEqualBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return getCompareEqualTimeBound(this->arg_type);
}


GetSpaceBoundResults CompareEqualBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
}


//----------------------------------------------------------------------------------------------


NaNBuiltInFunc::NaNBuiltInFunc(const TypeVRef& type_)
:	BuiltInFunctionImpl(BuiltInType_NaNBuiltInFunc),
	type(type_)
{}


ValueRef NaNBuiltInFunc::invoke(VMState& vmstate)
{
	if(type->getType() == Type::FloatType)
	{
		return new FloatValue(std::numeric_limits<float>::quiet_NaN());
	}
	else if(type->getType() == Type::DoubleType)
	{
		return new DoubleValue(std::numeric_limits<double>::quiet_NaN());
	}
	else
		throw BaseException("invalid type.");
}


llvm::Value* NaNBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(type->getType() == Type::FloatType)
	{
		return llvm::ConstantFP::get(
			*params.context,
			llvm::APFloat::getNaN(
#if TARGET_LLVM_VERSION >= 60
				llvm::APFloat::IEEEsingle()
#else
				llvm::APFloat::IEEEsingle
#endif
			)
		);
	}
	else if(type->getType() == Type::DoubleType)
	{
		return llvm::ConstantFP::get(
			*params.context,
			llvm::APFloat::getNaN(
#if TARGET_LLVM_VERSION >= 60
				llvm::APFloat::IEEEdouble()
#else
				llvm::APFloat::IEEEdouble
#endif
			)
		);
	}
	else
		throw BaseException("invalid type.");
}


size_t NaNBuiltInFunc::getTimeBound(GetTimeBoundParams& params) const
{
	return 1;
}


GetSpaceBoundResults NaNBuiltInFunc::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
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
