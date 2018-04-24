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


static float getFloatArg(const vector<ValueRef>& arg_values, int i)
{
	return checkedCast<const FloatValue>(arg_values[i])->value;
}


static double getDoubleArg(const vector<ValueRef>& arg_values, int i)
{
	return checkedCast<const DoubleValue>(arg_values[i])->value;
}


static int getIntArg(const vector<ValueRef>& arg_values, int i)
{
	return (int)checkedCast<const IntValue>(arg_values[i])->value;
}


static ValueRef makeFloat(float x)
{
	return new FloatValue(x);
}


static ValueRef makeDouble(double x)
{
	return new DoubleValue(x);
}


static ValueRef makeInt(int x)
{
	return new IntValue(x, /*signed=*/true);
}


static ValueRef makeBool(bool x)
{
	return new BoolValue(x);
}


static ValueRef tanFloatInterpreted(const vector<ValueRef>& args)
{
	return makeFloat(std::tan(getFloatArg(args, 0)));
}


static ValueRef tanDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(std::tan(getDoubleArg(args, 0)));
}


static ValueRef asinInterpreted(const vector<ValueRef>& args)
{
	return makeFloat(std::asin(getFloatArg(args, 0)));
}


static ValueRef asinDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(std::asin(getDoubleArg(args, 0)));
}


static ValueRef acosInterpreted(const vector<ValueRef>& args)
{
	return makeFloat(std::acos(getFloatArg(args, 0)));
}


static ValueRef acosDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(std::acos(getDoubleArg(args, 0)));
}


static ValueRef atanInterpreted(const vector<ValueRef>& args)
{
	return makeFloat(std::atan(getFloatArg(args, 0)));
}


static ValueRef atanDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(std::atan(getDoubleArg(args, 0)));
}


static ValueRef atan2Interpreted(const vector<ValueRef>& args)
{
	return makeFloat(std::atan2(getFloatArg(args, 0), getFloatArg(args, 1)));
}


static ValueRef atan2DoubleInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(std::atan2(getDoubleArg(args, 0), getDoubleArg(args, 1)));
}


static ValueRef floatModInterpreted(const vector<ValueRef>& args)
{
	return makeFloat(Maths::floatMod(getFloatArg(args, 0), getFloatArg(args, 1)));
}


static ValueRef doubleModInterpreted(const vector<ValueRef>& args)
{
	return makeDouble(Maths::doubleMod(getDoubleArg(args, 0), getDoubleArg(args, 1)));
}


static ValueRef intModInterpreted(const vector<ValueRef>& args)
{
	return makeInt(Maths::intMod(getIntArg(args, 0), getIntArg(args, 1)));
}


static ValueRef isFiniteInterpreted(const vector<ValueRef>& args)
{
	return makeBool(::isFinite(getFloatArg(args, 0)));
}


static ValueRef isFiniteDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeBool(::isFinite(getDoubleArg(args, 0)));
}


static ValueRef isNANInterpreted(const vector<ValueRef>& args)
{
	return makeBool(::isNAN(getFloatArg(args, 0)));
}


static ValueRef isNANDoubleInterpreted(const vector<ValueRef>& args)
{
	return makeBool(::isNAN(getDoubleArg(args, 0)));
}


void MathsFuncs::appendExternalMathsFuncs(std::vector<Winter::ExternalFunctionRef>& external_functions)
{
	TypeVRef float_type = new Float();
	TypeVRef double_type = new Double();
	TypeVRef int_type = new Int();
	TypeVRef bool_type = new Bool();

	external_functions.push_back(new ExternalFunction(
		(void*)(float(*)(float))std::tan, // func
		tanFloatInterpreted, // interpreted func
		FunctionSignature("tan", vector<TypeVRef>(1, float_type)), // function signature
		float_type // return type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))std::tan, // func
		tanDoubleInterpreted, // interpreted func
		FunctionSignature("tan", vector<TypeVRef>(1, double_type)), // function signature
		double_type // return type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(float(*)(float))std::asin,
		asinInterpreted,
		FunctionSignature("asin", vector<TypeVRef>(1, float_type)),
		float_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))std::asin,
		asinDoubleInterpreted,
		FunctionSignature("asin", vector<TypeVRef>(1, double_type)),
		double_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(float(*)(float))std::acos,
		acosInterpreted,
		FunctionSignature("acos", vector<TypeVRef>(1, float_type)),
		float_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))std::acos,
		acosDoubleInterpreted,
		FunctionSignature("acos", vector<TypeVRef>(1, double_type)),
		double_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(float(*)(float))std::atan,
		atanInterpreted,
		FunctionSignature("atan", vector<TypeVRef>(1, float_type)),
		float_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double))std::atan,
		atanDoubleInterpreted,
		FunctionSignature("atan", vector<TypeVRef>(1, double_type)),
		double_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(float(*)(float, float))std::atan2,
		atan2Interpreted,
		FunctionSignature("atan2", vector<TypeVRef>(2, float_type)),
		float_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(double(*)(double, double))std::atan2,
		atan2DoubleInterpreted,
		FunctionSignature("atan2", vector<TypeVRef>(2, double_type)),
		double_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)Maths::floatMod,
		floatModInterpreted,
		FunctionSignature("mod", vector<TypeVRef>(2, float_type)),
		float_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)Maths::doubleMod,
		doubleModInterpreted,
		FunctionSignature("mod", vector<TypeVRef>(2, double_type)),
		double_type
	));
	
	external_functions.push_back(new ExternalFunction(
		(void*)Maths::intMod,
		intModInterpreted,
		FunctionSignature("mod", vector<TypeVRef>(2, int_type)),
		int_type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(float))::isFinite,
		isFiniteInterpreted,
		FunctionSignature("isFinite", vector<TypeVRef>(1, float_type)),
		bool_type // return type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(double))::isFinite,
		isFiniteDoubleInterpreted,
		FunctionSignature("isFinite", vector<TypeVRef>(1, double_type)),
		bool_type // return type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(float))::isNAN,
		isNANInterpreted,
		FunctionSignature("isNAN", vector<TypeVRef>(1, float_type)),
		bool_type // return type
	));

	external_functions.push_back(new ExternalFunction(
		(void*)(bool(*)(double))::isNAN,
		isNANDoubleInterpreted,
		FunctionSignature("isNAN", vector<TypeVRef>(1, double_type)),
		bool_type // return type
	));
}


} // end namespace Winter
