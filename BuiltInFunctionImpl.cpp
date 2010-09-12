#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
#include "ASTNode.h"
#include <vector>
#if USE_LLVM
#include "llvm/Type.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CallingConv.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#endif

using std::vector;


namespace Winter
{

Constructor::Constructor(Reference<StructureType>& struct_type_)
:	struct_type(struct_type_)
{
}



Value* Constructor::invoke(VMState& vmstate)
{
	vector<Value*> field_values(this->struct_type->component_names.size());
	
	for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		field_values[i] = vmstate.argument_stack[vmstate.argument_stack.size() - this->struct_type->component_types.size() + i]->clone();

	return new StructureValue(field_values);
}


llvm::Value* Constructor::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


Value* GetField::invoke(VMState& vmstate)
{
	// Top param on arg stack should be a structure
	const StructureValue* s = dynamic_cast<const StructureValue*>(vmstate.argument_stack.back());

	assert(s);
	assert(this->index < s->fields.size());

	return s->fields[this->index]->clone();
}


llvm::Value* GetField::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


Value* GetVectorElement::invoke(VMState& vmstate)
{
	// Top param on arg stack should be a vector
	const VectorValue* vec = dynamic_cast<const VectorValue*>(vmstate.argument_stack.back());

	assert(vec);
	assert(this->index < vec->e.size());

	return vec->e[this->index]->clone();
}


// TEMP HACK copied
static llvm::Value* getNthArg(llvm::Function *func, int n)
{
	llvm::Function::arg_iterator args = func->arg_begin();
	for(int i=0; i<n; ++i)
		args++;
	return args;
}


llvm::Value* GetVectorElement::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* vec_value = NULL;
	if(true) // TEMP shouldPassByValue(*this->type()))
	{
		vec_value = getNthArg(
			params.currently_building_func, 
			0
		);
	}
	else
	{
		vec_value = params.builder->CreateLoad(
			getNthArg(params.currently_building_func, 0),
			false, // true,// TEMP: volatile = true to pick up returned vector);
			"argument" // name
		);
	}

	return params.builder->CreateExtractElement(
		vec_value, // vec
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index))
	);
}


Value* ArrayMapBuiltInFunc::invoke(VMState& vmstate)
{
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()]);
	const ArrayValue* from = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1]);

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

	return retval;
}


llvm::Value* ArrayMapBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}

//------------------------------------------------------------------------------------


Value* ArrayFoldBuiltInFunc::invoke(VMState& vmstate)
{
	// fold(function<T, T, T> func, array<T> array, T initial val) T
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()]);
	const ArrayValue* arr = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1]);
	const Value* initial_val = vmstate.argument_stack[vmstate.func_args_start.back() + 2];

	assert(f && arr && initial_val);

	Value* running_val = initial_val->clone();
	for(unsigned int i=0; i<arr->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(arr->e[i]); // Push value arg
		
		Value* new_running_val = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();

		delete running_val;
		running_val = new_running_val;
	}

	return running_val;
}


llvm::Value* ArrayFoldBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


}
