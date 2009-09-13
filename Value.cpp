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


ArrayValue::~ArrayValue()
{
	for(unsigned int i=0; i<this->e.size(); ++i)
		delete e[i];
}


Value* ArrayValue::clone() const
{
	ArrayValue* ret = new ArrayValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = this->e[i]->clone();

	return ret;
}


const std::string ArrayValue::toString() const
{
	std::string s = "array[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


}
