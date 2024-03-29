/*=====================================================================
FunctionSignature.h
-------------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Tue Jun 17 05:29:02 2008
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include <vector>
#include <string>


namespace Winter
{


/*=====================================================================
FunctionSignature
-----------------

=====================================================================*/
class FunctionSignature
{
public:
	FunctionSignature(const std::string& name_, const std::vector<TypeVRef>& param_types_) 
	:	name(name_), param_types(param_types_) 
	{}

	~FunctionSignature(){}

	//static const FunctionSignature makeSig(const std::string& sig);

	const std::string toString() const;

	const std::string typeMangledName() const; // Return something like f_float_int

	std::string name;
	std::vector<TypeVRef> param_types;
};


inline bool operator < (const FunctionSignature& a, const FunctionSignature& b)
{
	if(a.name < b.name)
		return true;
	else if(a.name > b.name)
		return false;
	else
	{
		if(a.param_types.size() < b.param_types.size())
			return true;
		else if(a.param_types.size() > b.param_types.size())
			return false;
		else
		{
			for(unsigned int i=0; i<a.param_types.size(); ++i)
			{
				if(*a.param_types[i] < *b.param_types[i])
					return true;
				else if(*b.param_types[i] < *a.param_types[i]) // else if a > b
					return false;
			}
		}
	}
	// If we got here a == b
	return false;
}


} //end namespace Winter
