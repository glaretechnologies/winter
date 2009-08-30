#include "Value.h"


#include "../../indigosvn/trunk/utils/stringutils.h"


namespace Winter
{


const std::string FloatValue::toString() const
{
	return ::toString(this->value);
}


StructureValue::~StructureValue()
{
	for(unsigned int i=0; i<this->fields.size(); ++i)
		delete fields[i];
}


Value* StructureValue::clone() const
{
	vector<Value*> field_clones(this->fields.size());

	for(unsigned int i=0; i<this->fields.size(); ++i)
		field_clones[i] = this->fields[i]->clone();

	return new StructureValue(field_clones);
}

}

