#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
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


}
