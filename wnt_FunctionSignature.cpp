/*=====================================================================
FunctionSignature.cpp
---------------------
File created by ClassTemplate on Tue Jun 17 05:29:02 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "wnt_FunctionSignature.h"


#include "utils/stringutils.h"


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
		s += param_types[i]->toString();
		if(i < (int)param_types.size() - 1)
			s += ", ";
	}
	return s + ")";
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





