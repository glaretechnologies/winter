/*=====================================================================
wnt_ExternalFunction.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 27 16:44:39 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include "utils/refcounted.h"
#include "utils/reference.h"


namespace Winter
{


/*=====================================================================
wnt_ExternalFunction
-------------------

=====================================================================*/
class ExternalFunction : public RefCounted
{
public:
	ExternalFunction();
	~ExternalFunction();

	void* func;
	Value* (* interpreted_func)(const std::vector<const Value*>& arg_values);

	FunctionSignature sig;
	TypeRef return_type;
private:

};


typedef Reference<ExternalFunction> ExternalFunctionRef;


}
