/*=====================================================================
wnt_ExternalFunction.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 27 16:44:39 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include "Value.h"


namespace Winter
{


/*=====================================================================
wnt_ExternalFunction
-------------------

=====================================================================*/
class ExternalFunction : public RefCounted
{
public:
	typedef ValueRef (* INTERPRETED_FUNC)(const std::vector<ValueRef>& arg_values);

	ExternalFunction();
	ExternalFunction(void* func_, INTERPRETED_FUNC interpreted_func_, const FunctionSignature& sig_, const TypeRef& return_type_)
	:	func(func_),
		interpreted_func(interpreted_func_),
		sig(sig_),
		return_type(return_type_),
		has_side_effects(false)
	{}

	~ExternalFunction();

	void* func;
	ValueRef (* interpreted_func)(const std::vector<ValueRef>& arg_values);

	FunctionSignature sig;
	TypeRef return_type;

	bool has_side_effects; // Such as the function freeString()
private:

};


typedef Reference<ExternalFunction> ExternalFunctionRef;


}
