#include "Value.h"


#include "utils/stringutils.h"


namespace Winter
{


const std::string FloatValue::toString() const
{
	return ::toString(this->value);
}


StructureValue::~StructureValue()
{
	//for(unsigned int i=0; i<this->fields.size(); ++i)
	//	delete fields[i];
}


Value* StructureValue::clone() const
{
	vector<ValueRef> field_clones(this->fields.size());

	for(unsigned int i=0; i<this->fields.size(); ++i)
		field_clones[i] = ValueRef(this->fields[i]->clone());

	return new StructureValue(field_clones);
}


ArrayValue::~ArrayValue()
{
	//for(unsigned int i=0; i<this->e.size(); ++i)
	//	delete e[i];
}


Value* ArrayValue::clone() const
{
	ArrayValue* ret = new ArrayValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = ValueRef(this->e[i]->clone());

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


//=====================================================================


VectorValue::~VectorValue()
{
	//for(unsigned int i=0; i<this->e.size(); ++i)
	//	delete e[i];
}


Value* VectorValue::clone() const
{
	VectorValue* ret = new VectorValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = ValueRef(this->e[i]->clone());

	return ret;
}


const std::string VectorValue::toString() const
{
	std::string s = "vector[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


//==============================================================================


const std::string VoidPtrValue::toString() const
{
	return ::toString((uint64)this->value);
}


}
