/*=====================================================================
Value.cpp
---------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "Value.h"


#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "utils/StringUtils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Constants.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;


namespace Winter
{


//------------------------------------------------------------------------------------------


const std::string FloatValue::toString() const
{
	return ::floatLiteralString(this->value);
}


//------------------------------------------------------------------------------------------


const std::string DoubleValue::toString() const
{
	return ::doubleLiteralString(this->value);
}


//------------------------------------------------------------------------------------------


StructureValue::~StructureValue()
{
}


const std::string StructureValue::toString() const
{
	std::string s = "{";
	for(size_t i=0; i<fields.size(); ++i)
		s += fields[i]->toString() + ((i + 1 < fields.size()) ? ", " : "");
	return s + "}";
}


//------------------------------------------------------------------------------------------


ArrayValue::~ArrayValue()
{
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


//------------------------------------------------------------------------------------------


VArrayValue::~VArrayValue()
{
}


const std::string VArrayValue::toString() const
{
	std::string s = "varray[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


//------------------------------------------------------------------------------------------


VectorValue::~VectorValue()
{
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


//------------------------------------------------------------------------------------------


TupleValue::~TupleValue()
{
}


const std::string TupleValue::toString() const
{
	std::string s = "tuple[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


//------------------------------------------------------------------------------------------


const std::string VoidPtrValue::toString() const
{
	return "void* " + ::toString((uint64)this->value);
}


} // end namespace Winter
