/*=====================================================================
wnt_MathsFuncs.h
----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once

#include "VirtualMachine.h"


namespace Winter
{


/*=====================================================================
MathsFuncs
----------
Adds some external maths functions from the C++ standard library.
These functions will have both float and double overloads.

tan
asin
acos
atan
sinh
asinh
cosh
acosh
tanh
atanh
atan2

Some maths functions are already built into Winter because they are supported as LLVM intrinsics, e.g.
sin, pow, log, exp.  Such functions are not added with appendExternalMathsFuncs(). 

Also adds some other functions:

mod  (Euclidean modulo)
isFinite
isNAN

=====================================================================*/
namespace MathsFuncs
{
	void appendExternalMathsFuncs(std::vector<Winter::ExternalFunctionRef>& external_functions);
};



} // end namespace Winter
