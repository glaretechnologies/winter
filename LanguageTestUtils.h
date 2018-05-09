#pragma once


#include "VirtualMachine.h"
#include "../maths/SSE.h"
#include <string>


#if BUILD_TESTS


namespace Winter
{


const uint32 INVALID_OPENCL = 1; // Flag value
const uint32 ALLOW_UNSAFE = 2; // Flag value
const uint32 INCLUDE_EXTERNAL_MATHS_FUNCS = 32; // Flag value


struct TestResults
{
	ProgramStats stats;
	Reference<FunctionDefinition> maindef;
};


SSE_CLASS_ALIGN float4
{
public:
	float e[4];

	inline bool operator == (const float4& other) const
	{
		return 
			(e[0] == other.e[0]) &&
			(e[1] == other.e[1]) &&
			(e[2] == other.e[2]) &&
			(e[3] == other.e[3]);
	}
};


SSE_CLASS_ALIGN Float4Struct
{
public:
	Float4Struct(){}
	Float4Struct(float x, float y, float z, float w) { v.e[0] = x; v.e[1] = y; v.e[2] = z; v.e[3] = w; }

	float4 v;

	inline bool operator == (const Float4Struct& other)
	{
		return v == other.v;
	}
};


SSE_CLASS_ALIGN Float4StructPair
{
public:
	Float4StructPair(const Float4Struct& a_, const Float4Struct& b_) : a(a_), b(b_) {}

	inline bool operator == (const Float4StructPair& other)
	{
		return a == other.a && b == other.b;
	}

	Float4Struct a, b;
};


AVX_CLASS_ALIGN float8
{
public:
	float e[8];

	inline bool operator == (const float8& other) const
	{
		return 
			(e[0] == other.e[0]) &&
			(e[1] == other.e[1]) &&
			(e[2] == other.e[2]) &&
			(e[3] == other.e[3]) &&
			(e[4] == other.e[4]) &&
			(e[5] == other.e[5]) &&
			(e[6] == other.e[6]) &&
			(e[7] == other.e[7]);
	}
};


AVX_CLASS_ALIGN Float8Struct
{
public:
	float8 v;

	inline bool operator == (const Float8Struct& other)
	{
		return v == other.v;
	}
};


struct StructWithVec
{
	//int data;
	float4 a;
	float4 b;
	float data2;

	inline bool operator == (const StructWithVec& other)
	{
		return (a == other.a) && (b == other.b) && (data2 == other.data2);
	}
};


struct TestStruct
{
	float a;
	float b;
	float c;
	float d;

	bool operator == (const TestStruct& other) const { return (a == other.a) && (b == other.b); }
};


struct TestStructIn
{
	float x;
	float y;
};


TestResults testMainFloat(const std::string& src, float target_return_val);
void testMainFloatArgInvalidProgram(const std::string& src);
TestResults testMainFloatArg(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0);
TestResults testMainDoubleArg(const std::string& src, double argument, double target_return_val, uint32 test_flags = 0);
TestResults testMainFloatArgAllowUnsafe(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0);
void testMainFloatArgCheckConstantFolded(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0);
void testMainInteger(const std::string& src, int target_return_val);
void testMainStringArg(const std::string& src, const std::string& arg, const std::string& target_return_val, uint32 test_flags = 0);
TestResults testMainIntegerArg(const std::string& src, int x, int target_return_val, uint32 test_flags = 0);
void testMainInt64Arg(const std::string& src, int64 x, int64 target_return_val, uint32 test_flags = 0);
void testMainInt16Arg(const std::string& src, int16 x, int16 target_return_val, uint32 test_flags = 0);
void testMainUInt32Arg(const std::string& src, uint32 x, uint32 target_return_val, uint32 test_flags = 0);
void testMainIntegerArgInvalidProgram(const std::string& src);
void testFloat4StructPairRetFloat(const std::string& src, const Float4StructPair& a, const Float4StructPair& b, float target_return_val);
void testVectorInStruct(const std::string& src, const StructWithVec& struct_in, const StructWithVec& target_return_val);
void testFloat4Struct(const std::string& src, const Float4Struct& a, const Float4Struct& b, const Float4Struct& target_return_val);
void testFloat8Struct(const std::string& src, const Float8Struct& a, const Float8Struct& b, const Float8Struct& target_return_val);
void testIntArray(const std::string& src, const int* a, const int* b, const int* target_return_val, size_t len, uint32 test_flags = 0);
void testFloatArray(const std::string& src, const float* a, const float* b, const float* target_return_val, size_t len);

template <class StructType>
void testMainStruct(const std::string& src, const StructType& target_return_val);

template <class InStructType, class OutStructType>
void testMainStructInputAndOutput(const std::string& src, const InStructType& struct_in, const OutStructType& target_return_val);


} // end namespace Winter


#endif // BUILD_TESTS
