#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
#include "ASTNode.h"
#include <vector>
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


Value* GetField::invoke(VMState& vmstate)
{
	// Top param on arg stack should be a structure
	const StructureValue* s = dynamic_cast<const StructureValue*>(vmstate.argument_stack.back());

	assert(s);
	assert(this->index < s->fields.size());

	return s->fields[this->index]->clone();
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


}
