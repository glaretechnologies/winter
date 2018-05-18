/*=====================================================================
wnt_MathsFuncs.cpp
------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "wnt_MathsFuncs.h"


#include "Value.h"
#include "maths/mathstypes.h"


using std::vector;
using std::string;


namespace Winter
{


static int getIntArg(const vector<ValueRef>& arg_values, int i)
{
	return (int)checkedCast<const IntValue>(arg_values[i])->value;
}


static ValueRef makeInt(int x)
{
	return new IntValue(x, /*signed=*/true);
}


static ValueRef intModInterpreted(const vector<ValueRef>& args)
{
	return makeInt(Maths::intMod(getIntArg(args, 0), getIntArg(args, 1)));
}


void MathsFuncs::appendExternalMathsFuncs(std::vector<Winter::ExternalFunctionRef>& external_functions)
{
	TypeVRef float_type = new Float();
	TypeVRef double_type = new Double();
	TypeVRef int_type = new Int();
	TypeVRef bool_type = new Bool();

	// NOTE: for maths functions, Using e.g. tanf is faster than std::tan.
	// This is because (with MSVC at least), Using std::tan just results in a call to std::tan from the winter code,
	// which then in turn calls tanf, resulting in an extra call.

	external_functions.push_back(new ExternalFunction(
		(void*)tanf, // func
		NULL, // interpreted func.  Can be null here since this is a float -> float function which can be called directly from the interpreter.
		FunctionSignature("tan", vector<TypeVRef>(1, float_type)), // function signature
		float_type, // return type
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))tan, // func - Use cast to pick the correct overload.
		NULL, // interpreted func
		FunctionSignature("tan", vector<TypeVRef>(1, double_type)), // function signature
		double_type, // return type
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)asinf,
		NULL,
		FunctionSignature("asin", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))asin,
		NULL,
		FunctionSignature("asin", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)acosf,
		NULL,
		FunctionSignature("acos", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))acos,
		NULL,
		FunctionSignature("acos", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)atanf,
		NULL,
		FunctionSignature("atan", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)sinhf,
		NULL,
		FunctionSignature("sinh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))sinh,
		NULL,
		FunctionSignature("sinh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)asinhf,
		NULL,
		FunctionSignature("asinh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))asinh,
		NULL,
		FunctionSignature("asinh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)coshf,
		NULL,
		FunctionSignature("cosh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))cosh,
		NULL,
		FunctionSignature("cosh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)acoshf,
		NULL,
		FunctionSignature("acosh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))acosh,
		NULL,
		FunctionSignature("acosh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)tanhf,
		NULL,
		FunctionSignature("tanh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))tanh,
		NULL,
		FunctionSignature("tanh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)atanhf,
		NULL,
		FunctionSignature("atanh", vector<TypeVRef>(1, float_type)),
		float_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))atanh,
		NULL,
		FunctionSignature("atanh", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))atan,
		NULL,
		FunctionSignature("atan", vector<TypeVRef>(1, double_type)),
		double_type,
		30 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)atan2f,
		NULL,
		FunctionSignature("atan2", vector<TypeVRef>(2, float_type)),
		float_type,
		60 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double, double))atan2,
		NULL,
		FunctionSignature("atan2", vector<TypeVRef>(2, double_type)),
		double_type,
		60 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)Maths::floatMod,
		NULL,
		FunctionSignature("mod", vector<TypeVRef>(2, float_type)),
		float_type,
		10 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)Maths::doubleMod,
		NULL,
		FunctionSignature("mod", vector<TypeVRef>(2, double_type)),
		double_type,
		10 // time_bound
	));
	
	external_functions.push_back(new ExternalFunction(
		(void*)Maths::intMod,
		intModInterpreted,
		FunctionSignature("mod", vector<TypeVRef>(2, int_type)),
		int_type,
		10 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(float))::isFinite,
		NULL,
		FunctionSignature("isFinite", vector<TypeVRef>(1, float_type)),
		bool_type, // return type
		10 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(double))::isFinite,
		NULL,
		FunctionSignature("isFinite", vector<TypeVRef>(1, double_type)),
		bool_type, // return type
		10 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(float))::isNAN,
		NULL,
		FunctionSignature("isNAN", vector<TypeVRef>(1, float_type)),
		bool_type, // return type
		10 // time_bound
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(double))::isNAN,
		NULL,
		FunctionSignature("isNAN", vector<TypeVRef>(1, double_type)),
		bool_type, // return type
		10 // time_bound
	));
}


} // end namespace Winter
