#pragma once


#include "Type.h"


namespace Winter
{


class Value;
class VMState;


class BuiltInFunctionImpl
{
public:
	virtual ~BuiltInFunctionImpl(){}

	virtual Value* invoke(VMState& vmstate) = 0;
};


class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(Reference<StructureType>& struct_type);
	virtual ~Constructor(){}

	virtual Value* invoke(VMState& vmstate);

private:
	Reference<StructureType> struct_type;
};


class GetField : public BuiltInFunctionImpl
{
public:
	GetField(Reference<StructureType>& struct_type_, unsigned int index_) : struct_type(struct_type_), index(index_) {}
	virtual ~GetField(){}

	virtual Value* invoke(VMState& vmstate);
private:
	Reference<StructureType> struct_type;
	unsigned int index;
};


}