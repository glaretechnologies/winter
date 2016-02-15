/*=====================================================================
FunctionSignature.cpp
---------------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Tue Jun 17 05:29:02 2008
=====================================================================*/
#include "wnt_FunctionSignature.h"


#include "wnt_FunctionDefinition.h"
#include "utils/StringUtils.h"


namespace Winter
{



/*
FunctionSignature::FunctionSignature()
{
	
}


FunctionSignature::~FunctionSignature()
{
	
}*/


const std::string FunctionSignature::toString() const
{
	std::string s = name + "(";

	for(int i=0; i<(int)param_types.size(); ++i)
	{
		if(param_types[i].isNull())
			s += "[Unknown]";
		else
			s += param_types[i]->toString();
		if(i < (int)param_types.size() - 1)
			s += ", ";
	}
	return s + ")";
}


const std::string FunctionSignature::typeMangledName() const // Return something like f_float_int
{
	return makeSafeStringForFunctionName(this->toString());
}


/*const FunctionSignature FunctionSignature::makeSig(const std::string& sig)
{
	const std::vector<std::string> t = ::split(sig, ' ');

	assert(t.size() >= 1);

	try
	{
		std::vector<Type> args;
		for(unsigned int i=1; i<t.size(); ++i)
			args.push_back(getTypeForTypeName(t[i]));

		return FunctionSignature(t[0], args);
	}
	catch(TypeExcep& )
	{
		assert(0);
		return FunctionSignature(t[0], std::vector<Type>());
	}
}*/


} //end namespace Lang






