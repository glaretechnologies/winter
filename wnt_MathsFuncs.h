/*=====================================================================
wnt_MathsFuncs.h
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once

#include "VirtualMachine.h"


namespace Winter
{


/*=====================================================================
MathsFuncs
----------
Adds some external maths functions from the C++ standard library,
such as 
tan, asin, acos, atan2 etc..

Some maths functions are built into Winter because they are supported as LLVM intrinsics, e.g.
sin, pow, log, exp.  Such functions are not added with appendExternalMathsFuncs(). 

Also addd some other functions like
mod,
isFinite,
isNAN
=====================================================================*/
namespace MathsFuncs
{
	void appendExternalMathsFuncs(std::vector<Winter::ExternalFunctionRef>& external_functions);
};



} // end namespace Winter
