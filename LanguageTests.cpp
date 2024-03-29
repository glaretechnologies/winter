/*=====================================================================
LanguageTests.cpp
-----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "LanguageTests.h"


#include "LanguageTestUtils.h"
#include "FuzzTests.h"
#include "wnt_Lexer.h"
#include "wnt_ArrayLiteral.h"
#include "wnt_VectorLiteral.h"
#include "wnt_TupleLiteral.h"
#include "wnt_IfExpression.h"
#include "wnt_FunctionExpression.h"
#include "wnt_LetBlock.h"
#include "wnt_LetASTNode.h"
#include <utils/Timer.h>
#include <utils/Task.h>
#include <utils/TaskManager.h>
#include <utils/MemMappedFile.h>
#include <utils/FileUtils.h>
#include <utils/ConPrint.h>
#include <utils/StringUtils.h>
#include <utils/Vector.h>
#include <unordered_set>


#if BUILD_TESTS


namespace Winter
{


LanguageTests::LanguageTests()
{}


LanguageTests::~LanguageTests()
{}


#define testAssert(expr) (doTestAssert((expr), (#expr), (__LINE__), (__FILE__)))
void doTestAssert(bool expr, const char* test, long line, const char* file)
{
	if(!expr)
	{
		stdErrPrint("Test assertion failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + std::string(test));
#if defined(_WIN32)
		__debugbreak();
#endif
		exit(1);
	}
}

/*

def main(int x) int : 
	if x < 5 then
		10 
	else
		20

def main(int x) int : 
	if x < 5
		10 
	else
		20

def main(int x) int : if x < 5 then 10 else	20

def main(int x) int : if (x < 5) then 10 else 20
def main(int x) int : if (x < 5) then 10 else 20

def main(int x) int : if (x < 5, 10, 20)

parse 'if ('
parse expression 'x < 5'
if next token == ')', then it's if-then-else.
if next token == ',', then it's if(,,)
*/

/*
static Value* identity(Value* v)
{
	return v;
}


static Value* h(Value* v, float x)
{
	if(x < 0.5)
	{
		Value* ret = new IntValue(99);
		ret->incRefCount(); // Set ref count to 1
		return ret;
	}
	else
		return v;
}


static int length(Value* v)
{
	return v->toString().size();
}

// no ref counting on v done in function
static int f(Value* v)
{
	return f(v);
}



static int bleh(float x)
{
	Value* v = new IntValue(99);
	v->incRefCount(); // Set v ref count to 1

	Value* v2 = h(v, x); // If we don't know how h is defined, then we may have v2 = v, or we may have v2 = new value
	// If it just returned v, we have one allocation in total.  Otherwise we have two.  
	// There is no way of knowing at compile time.

	const int len = length(v2);

	delete v;
}


static int meh()
{
	Value* v = new IntValue(99);
	v->incRefCount(); // Set v ref count to 1

	const int len = length(v);

	delete v; // No ref count check required. Still need to delete though
}


// Function meh2 akes a ref-counted value, no reference counting code required for the argument though, as doesn't return a value of the same type.
static int meh2(Value* v)
{
	const int len = length(v);
	return len + 2;
}


// Can do another optimisation:  Since meh3 doesn't take a Value as argument, we know that the return value must have refcount = 1.
static Value* meh3(int x)
{
	Value* v = new IntValue(99);
	v->incRefCount(); // Set v ref count to 1
	return v;
}


// So take something like:
static int useKnownReturnRefCountOptimsiation(int x)
{
	Value* v = meh3(x);
	// We know rc(v) = 1
	const int len = length(v);
	// We still know rc(v) = 1

	// At this point we can just delete v;
	assert(v->getRefCount() == 1);
	v->decRefCount();
	delete v;

	return len + 2;
}*/


void LanguageTests::doLLVMInit()
{
	testMainFloatArg("def main(float x) float : sqrt(4)", 2.0f, 2.0f);
}


static void doCKeywordTests()
{
	// =================================================================== 
	// Test using variable names etc.. that are OpenCL C keywords
	// ===================================================================
	testMainFloatArg("def main(float switch) float : switch", 10.0f, 10.f); // "switch" is an OpenCL C keyword
	testMainFloatArg("def main(float float16) float : float16", 10.0f, 10.f);

	// Test let var names
	testMainFloatArg("def main(float x) float : let switch = x in switch", 10.0f, 10.f);

	// Test function names.  Actually because of type decoration, function names (always?) won't clash with OpenCL C keywords.
	testMainFloatArg("def switch(float x) float : x      def main(float x) : switch(x)", 10.0f, 10.f);
	testMainFloatArg("def switch(float x) !noinline float : x      def main(float x) : switch(x)", 10.0f, 10.f);

	// Test structure names
	testMainFloatArg(
		"struct switch { float x }			\n"
		"def main(float x) float : switch(x).x", 10.0f, 10.f);

	// Test use of 'complex' struct (complex is a reserved word in OpenCL and can't be used)
	testMainFloatArg(
		"struct complex { float re, float im }   \n"
		"	def f(complex c) !noinline float : c.re	\n"
		"	def main(float x) float : let c = complex(x, x + 1.0) in f(c)",
		10.0f, 10.f);
}


static void doUnsignedIntegerTests()
{
	// ===================================================================
	// Test unsigned integers
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x", 1, 1);

	testMainUInt32Arg("def main(uint32 x) : 10u", 1, 10);

	testMainUInt32Arg("def main(uint32 x) : 4294967295u", 1, 4294967295u);

	// TODO: this should fail to compile:
	//testMainUInt32Arg("def main(uint32 x) : 429496729500000u", 1, 4294967295u);

	// Test hex literals
	testMainUInt32Arg("def main(uint32 x) : 0x12345678u", 1, 0x12345678u);
	testMainUInt32Arg("def main(uint32 x) : 0x90abcdefu", 1, 0x90abcdefu);
	testMainUInt32Arg("def main(uint32 x) : 0xABCDEF01u", 1, 0xABCDEF01u);

	testMainUInt32Arg("def main(uint32 x) : 0xFFFFFFFFu", 1, 0xFFFFFFFFu);

	testMainUInt32Arg("def main(uint32 x) : 10u32", 1, 10);

	testMainUInt32Arg("def main(uint32 x) : x + 10u32", 1, 11);

	testMainUInt32Arg("def main(uint32 x) : x + 1u32", 4294967294u, 4294967295u);

	testMainUInt32Arg("def main(uint32 x) : x * 10u32", 2, 20);
}


static void doBitWiseOpTests()
{
	// ===================================================================
	// Test bitwise AND
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x & 7u", 0xFFFFFFFF, 7);

	// ===================================================================
	// Test bitwise OR
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x | 7u", 8, 15);

	// ===================================================================
	// Test bitwise XOR
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x ^ 15u", 7, 8);

	// ===================================================================
	// Test left shift
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x << 10u", 3, 3 << 10);

	// ===================================================================
	// Test right shift
	// ===================================================================
	testMainUInt32Arg("def main(uint32 x) : x >> 2u", 356, 356 >> 2);
}


static void doHexLiteralParsingTests()
{
	// ===================================================================
	// Test hex literal parsing (some more)
	// ===================================================================
	testMainIntegerArgInvalidProgram("def main(int x) : 0x");
	testMainIntegerArgInvalidProgram("def main(int x) : 0xg");
	testMainIntegerArgInvalidProgram("def main(int x) : 0xG");
	testMainIntegerArgInvalidProgram("def main(int x) : 0x0x");
	testMainIntegerArgInvalidProgram("def main(int x) : 0x-");
}


static void testVariableShadowing()
{
	// ===================================================================
	// Test variable shadowing
	// ===================================================================
	testMainFloatArg("def main(float x) float :						\n\
		let															\n\
			y = x + 1												\n\
		in															\n\
			let														\n\
				y =	x + 2											\n\
			in														\n\
				y													\n\
			", 0.0f, 2.0f);

	testMainFloatArg("def main(float x) float :						\n\
		let															\n\
			y = x												\n\
		in															\n\
			let														\n\
				y1 = y + 1											\n\
				y =	y1 + 2											\n\
			in														\n\
				y													\n\
			", 0.0f, 3.0f);
	
	testMainFloatArgInvalidProgram("def main(float x) float :						\n\
		let															\n\
			y = x + 1												\n\
		in															\n\
			let														\n\
				y =	y + 10											\n\
			in														\n\
				y													\n\
			");

	testMainFloatArgInvalidProgram("def main(float x) float : let x = x in x");
	testMainFloatArgInvalidProgram("def main(float x) float : let x = x + 1 in x");
}


static void testFunctionInlining()
{
	// ===================================================================
	// Test function inlining
	// ===================================================================
	Winter::TestResults results;

	// Start with something simple, test that f is inlined.
	results = testMainIntegerArg("def f(int y) : y    def main(int x) int : f(x)", 1, 1); // f should be inlined.
	testAssert(results.maindef->body->nodeType() == ASTNode::VariableASTNodeType);

	/*
	Check that clamp is not inlined, since it duplicates an expensive arg.
	max gets inlined into clmap, so we have
	def clamp(real x, real lo, real hi) real : min(hi, if(x > lo, x, lo))			   \n\
	so arg x appears twice
	*/
	results = testMainFloatArg(
		"def min(real a, real b) real : if(a < b, a, b)							   \n\
		def max(real a, real b) real : if(a > b, a, b)							   \n\
		def clamp(real x, real lo, real hi) real : min(hi, max(x, lo))			   \n\
		def main(float x) float : clamp(sin(x), 0.f, 1.f)", 0.3f, std::sin(0.3f));
	testAssert(results.maindef->body->nodeType() == ASTNode::FunctionExpressionType);
	testAssert(results.maindef->body.downcastToPtr<FunctionExpression>()->static_function_name == "clamp");



	// A function that has a body which is just a function call should be inlined.
	results = testMainFloatArg("def f1(float z) !noinline float : z*z						\n\
		def f2(float z) float : f1(z)										\n\
		def entryPoint2(float x) float : f1(x) + f2(x)						\n\
		def main(float x) float : f2(x)", 4.0f, 16.0f); // Call to f2 should be inlined, and replaced with the call to f1
	testAssert(results.maindef->body->nodeType() == ASTNode::FunctionExpressionType);
	testAssert(results.maindef->body.downcastToPtr<FunctionExpression>()->static_function_name == "f1");

	
	// Test 'expensive' arg detection.  Test that getfield is not considered expensive.
	results = testMainFloatArg("struct S { float x }  						\n\
		def f1(float x) !noinline float : x*x											\n\
		def f2(float x) float : f1(x)										\n\
		def entryPoint2(float x) float : f1(x) + f2(x)						\n\
		def main(float x) float : let s = S(x) in f2(s.x)", 4.0f, 16.0f); // Call to f should be inlined, even tho argument expression is a function.
	testAssert(results.maindef->body->nodeType() == ASTNode::LetBlockType);
	testAssert(results.maindef->body.downcastToPtr<LetBlock>()->expr->nodeType() == ASTNode::FunctionExpressionType);
	testAssert(results.maindef->body.downcastToPtr<LetBlock>()->expr.downcastToPtr<FunctionExpression>()->static_function_name == "f1");




	results = testMainIntegerArg("def f(int y) : y + 1    def main(int x) int : f(x)", 1, 2); // f should be inlined.
	testAssert(results.maindef->body->nodeType() == ASTNode::AdditionExpressionType);


	testMainFloatArg("def f(float b) float : let x = b in x					\n\
		def main(float x) float : f(x * 10)									\n\
			", 1.0f, 10.0f);

	// Naive inlining of f() without variable renaming results in
	// def main(float x) float : let x = x * 10 in x	

	// If we rename all let vars, we get something like:
	testMainFloatArg("def f(float b) float : let x = b in x					\n\
		def main(float x) float : let x_new = x * 10 in x_new				\n\
			", 1.0f, 10.0f);


	testMainFloatArg("def f(float b) float : let x = b in x					\n\
		def main(float x) float : f(x)										\n\
			", 1.0f, 1.0f);

	testMainFloatArg("def f(float b) float : let x, y = (b, b) in x			\n\
		def main(float x) float : f(x * 10.0)								\n\
			", 1.0f, 10.0f);

	testMainFloatArg("def f(float b) float : let y, x = (b, b) in x			\n\
		def main(float x) float : f(x * 10.0)								\n\
			", 1.0f, 10.0f);


	// Test that new names are checked properly
	testMainFloatArg("def f(float b) float : let x, x_0 = (b, b) in x					\n\
		def main(float x) float : f(x * 10)									\n\
			", 1.0f, 10.0f);

	// Test that new names are checked properly
	testMainFloatArg("def f(float b) float : let x = b in x					\n\
		def main(float x) float : let x_0 = 10 in f(x * x_0)									\n\
			", 1.0f, 10.0f);


	// Test that new names are checked properly
	testMainFloatArg("def f(float b) float : let x = b in x					\n\
					 def main(float x) float : let x_0 = 10 x_1 = 11 x_2 = 12 x_3 = 13 x_4 = 14 x_5 = 15 x_6 = 16 x_7 = 17 x_8 = 18 x_9 = 19 x_10 = 20    in f(x * x_0)									\n\
			", 1.0f, 10.0f);

	// Test with two functions being inlined that both have the same let var name (x), it should be assigned different names.
	testMainFloatArg("def f(float b) float : let x = b in x					\n\
					 def g(float b) float : let x = b in x					\n\
			def main(float x) float : f(x * 10) + g(x * 20)									\n\
			", 1.0f, 30.0f);

	// Test with two functions being inlined that both have the same let var name (x), it should be assigned different names.
	testMainFloatArg("def f(float b) float : let x = b in x					\n\
					 def g(float b) float : let x = b in x					\n\
			def main(float x) float : let x_0 = 10 x_1 = 11 x_2 = 12 x_3 = 13 x_4 = 14 x_5 = 15 x_6 = 16 x_7 = 17 x_8 = 18 x_9 = 19 x_10 = 20    in f(x * 10) + g(x * 20)									\n\
			", 1.0f, 30.0f);


	testMainIntegerArg("def f(int x) : x + 1    def main(int x) int : f(x)", 1, 2); // f should be inlined.

	testMainIntegerArg("def f(int x) : x + 1    def main(int x) int : f(x) + f(x + 1)", 1, 5); // f should not be inlined, duplicate calls to it.

	// Test inlining where the inlined function body contains a variable with the same name as the call location context.
	testMainIntegerArg("def f(int x) : let a = 1 in x + a						\n\
		def main(int x) int :													\n\
			let																	\n\
				a = f(x) # When f is inlined here, need to be careful with 'a'	\n\
			in																	\n\
				a", 1, 2); // f should be inlined.

	// Test inlining, where the argument expression is expensive to evaluate, e.g. sin(x) in this case.
	// Because of that, we don't want to duplicate the argument expression.  
	// Naive inlining in this case would give
	// def main(float) float : sin(x) + sin(x) + sin(x) + sin(x)
	// Although common subexpression elimination (CSE) should in theory be able to remove the duplicate sin(x)'s,
	// we don't want to rely on that. 
	// For example Winter doesn't do CSE itself, so generated OpenCL code would contain duplicate sin calls.
	results = testMainFloatArg("def f(float x) : x + x + x + x							\n\
		def main(float x) float : f(sin(x))", 1, 4*sin(1.f)); // f should not be inlined.
	testAssert(results.maindef->body->nodeType() == ASTNode::FunctionExpressionType);
	testAssert(results.maindef->body.downcastToPtr<FunctionExpression>()->static_function_name == "f");

	// Test inlining, where the argument expression is expensive to evaluate, e.g. sin(x) in this case.
	// However, the function being called, f, only uses the argument once in the function body.  
	// So the expensive expression won't be duplicated, hence we can inline.
	results = testMainFloatArg("def f(float x) : x + 1.0							\n\
		def main(float x) float : f(sin(x))", 1, sin(1.f) + 1.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::AdditionExpressionType);

	testMainFloatArgAllowUnsafe("def f(varray<float> v) varray<float> : v      def main(float x) float : elem(f([99.0]va), 0)", 1.f, 99.0f, INVALID_OPENCL);
}


static void testDeadCodeElimination()
{
	// ===================================================================
	// Test dead code elimination
	// ===================================================================
	testMainIntegerArg("def main(int x) int :				\n\
		let													\n\
			a = 1 + x	# dead								\n\
			b = 3 + x	# alive								\n\
			c = b + x	# alive								\n\
		in													\n\
			c + x			# c is alive						\n\
		", 1, 6);


	testMainIntegerArg("def main(int x) int :				\n\
		let													\n\
			a = 1 + x	# referred to by b, but still dead	\n\
			b = 3 + a	# dead								\n\
			c = 4 + x	# alive								\n\
			d = c + x	# alive								\n\
		in													\n\
			d + x			# d is alive						\n\
		", 1, 7);


	// Test that a let block with no let vars is removed, and is replaced with the value expression (x var)
	Winter::TestResults results = testMainIntegerArg("def main(int x) int :		\n\
		let													\n\
			a = 1 + x	# dead								\n\
		in													\n\
			x												\n\
		", 4, 4);
	testAssert(results.maindef->body->nodeType() == ASTNode::VariableASTNodeType);
}


static void testDeadFunctionElimination()
{
	// ===================================================================
	// Test dead function elimination
	// ===================================================================
	testMainInt64Arg("def func_1(float x) : x   def func_2(float x) : x    def main(int64 x) int64 : x", 1, 1);

	try
	{
		const std::string src = "def func_1(int64 x) : x   def func_2(int64 x) : x    def main(int64 x) int64 : func_2(x)";
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int(64)));
		const FunctionSignature func_1_sig("func_1", std::vector<TypeVRef>(1, new Int(64)));
		const FunctionSignature func_2_sig("func_2", std::vector<TypeVRef>(1, new Int(64)));

		vm_args.entry_point_sigs.push_back(mainsig);
		VirtualMachine vm(vm_args);

		testAssert(vm.findMatchingFunction(mainsig).nonNull());
		testAssert(vm.findMatchingFunction(func_1_sig).isNull()); // func_1 should be removed by dead code elim.
		testAssert(vm.findMatchingFunction(func_2_sig).isNull()); // func_2 will be removed by inlining and then dead code elim.
	}
	catch(Winter::BaseException& e)
	{
		conPrint(e.what());
		assert(0);
		exit(1);
	}
}


static void testArrays()
{
	// ===================================================================
	// Test compare-equal operators
	// ===================================================================

	// Test == with !noline to test LLVM codgen
	testMainFloat("def eq(array<float, 2> a, array<float, 2> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]a, [1.0, 3.0]a) ? 1.0 : 2.0", 
				  2.0f);

	testMainFloat("def eq(array<float, 2> a, array<float, 2> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]a, [1.0, 2.0]a) ? 1.0 : 2.0", 
				  1.0f);

	// Test == without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : ([1.0, 2.0]a == [1.0, 3.0]a) ? 1.0 : 2.0", 2.0f);
	testMainFloat("def main() float : ([1.0, 2.0]a == [3.0, 2.0]a) ? 1.0 : 2.0", 2.0f);
	testMainFloat("def main() float : ([1.0, 2.0]a == [1.0, 2.0]a) ? 1.0 : 2.0", 1.0f);


	// Test !=
	testMainFloat("def neq(array<float, 2> a, array<float, 2> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]a, [1.0, 3.0]a) ? 1.0 : 2.0", 
				  1.0f);

	testMainFloat("def neq(array<float, 2> a, array<float, 2> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]a, [1.0, 2.0]a) ? 1.0 : 2.0", 
				  2.0f);

	// Test != without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : ([1.0, 2.0]a != [1.0, 3.0]a) ? 1.0 : 2.0", 1.0f);
	testMainFloat("def main() float : ([1.0, 2.0]a != [3.0, 2.0]a) ? 1.0 : 2.0", 1.0f);
	testMainFloat("def main() float : ([1.0, 2.0]a != [1.0, 2.0]a) ? 1.0 : 2.0", 2.0f);
}


static void testVArrays()
{
	// ===================================================================
	// Test makeVArray(elem, count) varray<T>
	// ===================================================================

	// Test makeVArray with constant count arg
	testMainInt64Arg("def main(int64 x) int64 : length(makeVArray(1, 10i64))", 10, 10);

	// Test makeVArray with runtime-variable, but bounded count arg
	testMainInt64Arg("def main(int64 x) int64 : length(makeVArray(1, x > 10i64 ? 10i64 : x))", 10, 10, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : length(makeVArray(1, x))", 1, 1, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(makeVArray(1, x))", 4, 4, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(makeVArray(1, x))", 0, 0, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : makeVArray(x, x)[0]", 1, 1, ALLOW_UNSAFE | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : makeVArray(x, x)[0]", 2, 2, ALLOW_UNSAFE | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : makeVArray(x, x)[1]", 2, 2, ALLOW_UNSAFE | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	

//	testMainIntegerArg("def f(int i) varray<float> : [10]va			def main(int x) int : if i > 0 && i < 4 then f(x)[i] else 0", 4, 4);

//	testMainIntegerArg("def f(int i) varray<float> : [i, i, i, i]va			def main(int x) int : if i > 0 && i < 4 then f(x)[i] else 0", 4, 4);

//	testMainFloatArg("def f(float x) varray<float> : [x]va			def main(float x) float : f(x)[0]", 4.0f, 4.0f);

	testMainFloatArg("def main(float x) float : [4.0]va[0]", 0, 4.0f, INVALID_OPENCL);

	// Test varrays with pass-by-value elements (e.g. structures)
	testMainFloatArg("struct s { float x }		def main(float x) float : [s(4.0)]va[0].x", 0, 4.0f, INVALID_OPENCL);
	testMainFloatArg("struct s { float x }		def main(float x) float : [s(10.0), s(11.0), s(12.0), s(13.0)]va[2].x", 0, 12.0f, INVALID_OPENCL);

	// Test varrays with heap-allocated/ref counted elements
	//testMainIntegerArg("struct s { float x }		def main(float x) float : [\"abc\"]va[0].x", 0, 4.0f);


	// In VArray literal
	testMainFloatArgAllowUnsafe("struct s { float x }									\n\
				  def op_add(s a, s b) : s(a.x + b.x)					\n\
				  def main(float x) float : [s(1) + s(3)]va[0].x", 1.0f, 4.0f, INVALID_OPENCL);
	

	// ===================================================================
	// Test constant indices into varray literals.
	// ===================================================================

	// Test with literal indices
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[0]", 0, 10, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[1]", 0, 11, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[2]", 0, 12, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[3]", 0, 13, INVALID_OPENCL);

	// Test with a let variable that refers to a literal
	testMainIntegerArg("def main(int x) int : let v = [10, 11, 12, 13]va in v[0]", 0, 10, INVALID_OPENCL);

	// Test with a let variable that refers to a named constant
	testMainIntegerArg("V = [10, 11, 13, 13]va    def main(int x) int : V[0]", 0, 10, INVALID_OPENCL);

	testMainIntegerArgInvalidProgram("def main(int x) int : [10, 11, 12, 13]va[-1]");
	testMainIntegerArgInvalidProgram("def main(int x) int : [10, 11, 12, 13]va[-10]");
	testMainIntegerArgInvalidProgram("def main(int x) int : [10, 11, 12, 13]va[4]");
	testMainIntegerArgInvalidProgram("def main(int x) int : [10, 11, 12, 13]va[10]");

	// Test with runtime indices
	testMainIntegerArg("def main(int x) int : if x >= 0 && x < 4 then [10, 11, 12, 13]va[x] else 0", 0, 10, INVALID_OPENCL);
	//testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[if x >= 0 && x < 4 then x else 0]", 0, 10);
	testMainIntegerArg("def main(int x) int : if x >= 0 && x < 4 then [10, 11, 12, 13]va[x] else 0", 2, 12, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[2]", 2, 12, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [10, 11, 12, 13]va[3]", 3, 13, INVALID_OPENCL);

	testMainIntegerArgInvalidProgram("def main(int x) int : [10, 11, 12, 13]va[x]");
	testMainIntegerArgInvalidProgram("def main(int x) int : if x >= -1 && x < 4 then [10, 11, 12, 13]va[x] else 0"); // Test with some incorrect bounding expressions.
	testMainIntegerArgInvalidProgram("def main(int x) int : if x >= 0 && x <= 4 then [10, 11, 12, 13]va[x] else 0"); // Test with some incorrect bounding expressions.
	testMainIntegerArgInvalidProgram("def main(int x) int : if x >= 0 && x < 5 then [10, 11, 12, 13]va[x] else 0"); // Test with some incorrect bounding expressions.


	// ===================================================================
	// Test VArrays
	// ===================================================================
	
	// Test VArray let variables.
	testMainFloatArg("def main(float x) float : let v = [99]va in x", 1.f, 1.0f, INVALID_OPENCL);
	testMainFloatArg("def main(float x) float : let v = [1.0, 2.0, 3.0]va in x", 1.f, 1.0f, INVALID_OPENCL);
	testMainFloatArg("def main(float x) float : let v = [x + 1.0, x + 2.0, x + 3.0]va in x", 1.f, 1.0f, INVALID_OPENCL);

	// Two VArray let variables
	testMainFloatArg("def main(float x) float : let a = [1]va  b = [2]va   in x", 1.f, 1.0f, INVALID_OPENCL);

	// Back-references in let variables
	testMainFloatArg("def main(float x) float : let a = [1]va  b = a   in x", 1.f, 1.0f, INVALID_OPENCL);

	// Test a reference to a let var from another let block
	testMainFloatArg("def main(float x) float :			\n\
					 let								\n\
						a = [1]va						\n\
					in									\n\
						let								\n\
							b = a						\n\
						in								\n\
							x", 1.f, 1.0f, INVALID_OPENCL);


	// Return a varray from a function
	testMainFloatArg("def f() varray<int> : [3]va      def main(float x) float : let v = f() in x", 1.f, 1.0f, INVALID_OPENCL);

	testMainFloatArg("def f(float x) !noinline varray<float> : [x, x, x, x]va      def main(float x) float : (f(x))[2]", 1.f, 1.0f, INVALID_OPENCL | ALLOW_UNSAFE);
	testMainFloatArg("def f(float x)           varray<float> : [x, x, x, x]va      def main(float x) float : (f(x))[2]", 1.f, 1.0f, INVALID_OPENCL | ALLOW_UNSAFE);
	
	// Test a varray with access
	//testMainFloatArgAllowUnsafe("def main(float x) float : [x + 1.0, x + 2.0, x + 3.0]va[1]", 1.f, 3.0f);

	// Test a varray returned from a function with access
	testMainFloatArgAllowUnsafe("def f(float x) varray<float> : [x]va    def main(float x) float : elem(f(x), 0)", 1.f, 1.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(float x) varray<float> : [x + 1.0, x + 2.0, x + 3.0]va    def main(float x) float : f(x)[0]", 1.f, 2.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(float x) varray<float> : [x + 1.0, x + 2.0, x + 3.0]va    def main(float x) float : f(x)[2]", 1.f, 4.0f, INVALID_OPENCL);
	
	// Test varray returned from an if expression
	testMainFloatArgAllowUnsafe("def main(float x) float : let v = (if x < 2.0 then [x + 1.0]va else [x + 2.0]va) in elem(v, 0)", 1.f, 2.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def main(float x) float : (if x < 2.0 then [x + 1.0]va else [x + 2.0]va)[0]", 1.f, 2.0f, INVALID_OPENCL);

	// Test varray returned from an if expression in a function
	testMainFloatArgAllowUnsafe("def f(float x) varray<float> : if x < 2.0 then [x + 1.0]va else [x + 2.0]va      def main(float x) float : f(x)[0]", 1.f, 2.0f, INVALID_OPENCL);



	testMainFloatArgAllowUnsafe("struct S { float x }     def f(float x) S : S(x)        def main(float x) float : f(x).x", 1.f, 1.0f, INVALID_OPENCL);

	

	// Test varray in struct (just assigned to let var but not used)
	testMainFloatArgAllowUnsafe("struct S { varray<float> v }     def main(float x) float : let s = S([x]va) in x", 1.f, 1.0f, INVALID_OPENCL);
	
	// Test varray in struct
	testMainFloatArgAllowUnsafe("struct S { varray<float> v }     def main(float x) float : S([x]va).v[0]", 1.f, 1.0f, INVALID_OPENCL);

	// Test two varrays in struct
	testMainFloatArgAllowUnsafe("struct S { varray<float> a, varray<float> b }  def main(float x) float : S([x]va, [x]va).a[0]", 1.f, 1.0f, INVALID_OPENCL);

	// Test varray in struct returned from function
	testMainFloatArgAllowUnsafe("struct S { varray<float> v }     def f(float x) S : S([x + 1.0]va)        def main(float x) float : f(x).v[0]", 1.f, 2.0f, INVALID_OPENCL);

	// Test two varrays in struct returned from function
	testMainFloatArgAllowUnsafe("struct S { varray<float> a, varray<float> b }     def f(float x) S : S([x + 1.0]va, [x + 2.0]va)        def main(float x) float : f(x).a[0]", 1.f, 2.0f, INVALID_OPENCL);

	//------------------------ VArrays as function arguments --------------------------
	// Test a VArray returned from identity function
	testMainFloatArgAllowUnsafe("def f(varray<float> v) varray<float> : v      def main(float x) float : f([99.0]va)[0]", 1.f, 99.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(varray<float> v) varray<float> : v      def main(float x) float : let v = f([99.0]va) in x", 99.f, 99.0f, INVALID_OPENCL);

	// Test a VArray returned from 'identity' function with two args
	testMainFloatArgAllowUnsafe("def f(varray<float> v, varray<float> v2) varray<float> : v      def main(float x) float : f([88.0]va, [99.0]va)[0]", 1.f, 88.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(varray<float> v, varray<float> v2) varray<float> : v2      def main(float x) float : f([88.0]va, [99.0]va)[0]", 1.f, 99.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("def f(varray<float> v, varray<float> v2) varray<float> : v2      def main(float x) float : let v = [99.0]va in f(v, v)[0]", 1.f, 99.0f, INVALID_OPENCL);


	// Test a VArray passed as a function argument (but not used)
	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : 1.0      def main(float x) float : f([99.0]va)", 1.f, 1.0f, INVALID_OPENCL);

	// Test a VArray passed as a function argument, then indexed into.
	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : v[0]      def main(float x) float : f([x]va)", 10.f, 10.0f, INVALID_OPENCL);

	// Test two VArrays passed as function arguments, then indexed into.
	testMainFloatArgAllowUnsafe("def f(varray<float> v_a, varray<float> v_b) float : v_a[0] + v_b[0]      def main(float x) float : f([x]va, [1.0]va)", 10.f, 11.0f, INVALID_OPENCL);


	// Test nested function call with varray
	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : v[0]      def g(varray<float> v) float : f(v)      def main(float x) float : g([x]va)", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : v[0]      def g(varray<float> v) float : f(v) + 1.0     def main(float x) float : g([x]va)", 10.f, 11.0f, INVALID_OPENCL);

	// Test varray function argument being referenced by let
	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : let v_2 = v in v_2[0]      def main(float x) float : f([x]va)", 10.f, 10.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(varray<float> v) float : let v_2 = v in v[0]      def main(float x) float : f([x]va)", 10.f, 10.0f, INVALID_OPENCL);

	// Test varray passed as function argument but returned in struct (tests return of argument via enclosing type)
	testMainFloatArgAllowUnsafe("struct S { varray<float> v }     def f(varray<float> arg) S : S(arg)      def main(float x) float : f([x]va).v[0]", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("struct S { varray<float> a, varray<float> b }     def f(varray<float> arg) S : S(arg, arg)      def main(float x) float : f([x]va).a[0]", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("struct S { varray<float> a, varray<float> b }     def f(varray<float> arg, varray<float> arg2) S : S(arg, arg2)      def main(float x) float : f([x]va, [x]va).a[0]", 10.f, 10.0f, INVALID_OPENCL);
	

	//------------------------ VArrays in tuples --------------------------
	testMainFloatArgAllowUnsafe("def f(varray<float> arg) tuple<varray<float> > : [arg]t      def main(float x) float : f([x]va)[0][0]", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("def f(varray<float> arg) tuple<varray<float>, varray<float> > : (arg, arg)      def main(float x) float : f([x]va)[0][0]", 10.f, 10.0f, INVALID_OPENCL);


	//------------------------ Test a VArray in a VArray --------------------------
	testMainFloatArg("def main(float x) float : let a = [[99]va]va in x", 10.f, 10.0f, INVALID_OPENCL);

	
	//------------------------ Test a string in a VArray --------------------------
	testMainFloatArg("def main(float x) float : let a = [\"hello\"]va in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("def main(float x) float : let a = [[\"hello\"]va]va in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainInt64Arg("def main(int64 x) int64 : let a = [\"hello\"]va in length(a[0])", 0, 5, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	//------------------------ Test a struct in a VArray, with access --------------------------
	testMainFloatArg("struct s { float x }		def main(float x) float : [s(4.0)]va[0].x", 0, 4.0f, INVALID_OPENCL);
	testMainFloatArg("struct s { float x }		def main(float x) float : [s(10.0), s(11.0), s(12.0), s(13.0)]va[2].x", 0, 12.0f, INVALID_OPENCL);
	
	//------------------------ Test a struct, with a ref counted field, in a VArray --------------------------
	testMainFloatArg("struct S { string str }      def main(float x) float : let a = S(\"hello\") in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("struct S { string str }      def main(float x) float : let a = [S(\"hello\")]va in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("struct S { string str }      def main(float x) float : let a = [S(\"hello\"), S(\"world\")]va in x", 10.f, 10.0f, INVALID_OPENCL);

	// Test a struct with two ref counted fields
	testMainFloatArg("struct S { string s_a, string s_b }      def main(float x) float : let a = S(\"hello\", \"world\") in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("struct S { string s_a, string s_b }      def main(float x) float : let a = [S(\"hello\", \"world\")]va in x", 10.f, 10.0f, INVALID_OPENCL);
	


	//------------------------ Test a tuple, with a ref counted field, in a VArray --------------------------
	testMainFloatArg("struct S { string str }      def main(float x) float : let a = S(\"hello\") in x", 10.f, 10.0f, INVALID_OPENCL);
	testMainFloatArg("def main(float x) float : let a = [\"hello\"]t in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("def main(float x) float : let a = [[\"hello\"]t]va in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("def main(float x) float : let a = [[\"hello\"]t, [\"world\"]t]va in x", 10.f, 10.0f, INVALID_OPENCL);

	// Test a tuple with two ref counted fields
	testMainFloatArg("def main(float x) float : let a = (\"hello\", \"world\") in x", 10.f, 10.0f, INVALID_OPENCL);

	testMainFloatArg("def main(float x) float : let a = [(\"hello\", \"world\")]va in x", 10.f, 10.0f, INVALID_OPENCL);

	
	//------------------------ Test a VArray in a named constant --------------------------
	testMainFloatArg("TEST = [99.0]va     def main(float x) float : TEST[0]", 10.f, 99.0f, INVALID_OPENCL);
	testMainFloatArg("TEST = [99.0]va     def main(float x) float : TEST[0] + TEST[0]", 10.f, 198.0f, INVALID_OPENCL);

	testMainFloatArg("TEST = [1.0 + 2.0]va     def main(float x) float : TEST[0] + TEST[0]", 10.f, 6.0f, INVALID_OPENCL);

	testMainFloatArgAllowUnsafe("def f(varray<float> v) : v     TEST = f([3.0]va)     def main(float x) float : TEST[0]", 10.f, 3.0f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def f(varray<float> v) : v     TEST = f([3.0]va)     def main(float x) float : TEST[0] + TEST[0]", 10.f, 6.0f, INVALID_OPENCL);

	// ===================================================================
	// Test compare-equal operators
	// ===================================================================

	// Test == with !noline to test LLVM codgen
	testMainFloat("def eq(varray<float> a, varray<float> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]va, [1.0, 3.0]va) ? 1.0 : 0.0", 
				  0.0f);

	testMainFloat("def eq(varray<float> a, varray<float> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]va, [1.0, 2.0, 3.0]va) ? 1.0 : 0.0", 
				  0.0f);

	testMainFloat("def eq(varray<float> a, varray<float> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]va, [1.0, 2.0]va) ? 1.0 : 0.0", 
				  1.0f);

	// Test with a heap-allocated type as the varray elem type
	testMainFloat("def eq(varray<string> a, varray<string> b) !noinline bool : a == b     \n\
				  def main() float : eq([\"a\", \"b\"]va, [\"a\", \"b\"]va) ? 1.0 : 0.0", 
				  1.0f);

	testMainFloat("def eq(varray<string> a, varray<string> b) !noinline bool : a == b     \n\
				  def main() float : eq([\"a\", \"b\"]va, [\"a\", \"c\"]va) ? 1.0 : 0.0", 
				  0.0f);


	// Test == without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : ([1.0, 2.0]va == [1.0, 3.0]va) ? 1.0 : 0.0", 0.0f);
	testMainFloat("def main() float : ([1.0, 2.0]va == [3.0, 2.0]va) ? 1.0 : 0.0", 0.0f);
	testMainFloat("def main() float : ([1.0, 2.0]va == [1.0, 2.0]va) ? 1.0 : 0.0", 1.0f);


	// Test !=
	testMainFloat("def neq(varray<float> a, varray<float> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]va, [1.0, 3.0]va) ? 1.0 : 0.0", 
				  1.0f);

	testMainFloat("def neq(varray<float> a, varray<float> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]va, [1.0, 2.0, 3.0]va) ? 1.0 : 0.0", 
				  1.0f);

	testMainFloat("def neq(varray<float> a, varray<float> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]va, [1.0, 2.0]va) ? 1.0 : 0.0", 
				  0.0f);

	// Test != without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : ([1.0, 2.0]va != [1.0, 3.0]va) ? 1.0 : 0.0", 1.0f);
	testMainFloat("def main() float : ([1.0, 2.0]va != [3.0, 2.0]va) ? 1.0 : 0.0", 1.0f);
	testMainFloat("def main() float : ([1.0, 2.0]va != [1.0, 2.0]va) ? 1.0 : 0.0", 0.0f);
	testMainFloat("def main() float : ([1.0, 2.0]va != [1.0]va) ? 1.0 : 0.0", 1.0f);

	// ===================================================================
	// Test allocation of large varray that is not captured or returned.
	// ===================================================================
	// Because this varray is not returned or captured from f(), it might have been allocated on the stack.
	// We want to make sure it isn't because it is too large.
	testMainInt64Arg("def f(int64 index) !noinline int64 : ([123i64]va1000000)[index]    \n\
		def main(int64 x) int64 : f(x)", 1, 123, ALLOW_UNSAFE);

	// Test with smaller int suffix, should not emit a call to makeVArray()
	testMainInt64Arg("def f(int64 index) !noinline int64 : ([123i64]va5)[index]    \n\
		def main(int64 x) int64 : f(x)", 1, 123, ALLOW_UNSAFE);

	// Test the same as above, but with a structure in the varray
	testMainInt64Arg("struct S { int64 x }    \n\
		def f(int64 index) !noinline int64 : ([S(123i64)]va1000000)[index].x    \n\
		def main(int64 x) int64 : f(x)", 1, 123, ALLOW_UNSAFE);

	// Test with smaller int suffix, should not emit a call to makeVArray()
	testMainInt64Arg("struct S { int64 x }    \n\
		def f(int64 index) !noinline int64 : ([S(123i64)]va5)[index].x    \n\
		def main(int64 x) int64 : f(x)", 1, 123, ALLOW_UNSAFE);

	// TODO: test with refcounted heap-allocated types like string.
}


static void testToIntFunctions()
{
	// ===================================================================
	// Test toInt32
	// ===================================================================
	testMainIntegerArg("def main(int x) int : toInt32(0i64)", 0, 0);
	testMainIntegerArg("def main(int x) int : toInt32(1i64)", 0, 1);
	testMainIntegerArg("def main(int x) int : toInt32(-1i64)", 0, -1);
	testMainIntegerArg("def main(int x) int : toInt32(12345i64)", 0, 12345);
	testMainIntegerArg("def main(int x) int : toInt32(-12345i64)", 0, -12345);
	testMainIntegerArg("def main(int x) int : toInt32(-2147483648i64)", 0, -2147483647 - 1);
	testMainIntegerArg("def main(int x) int : toInt32(2147483647i64)", 0, 2147483647);

	// Test some out-of domain values
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(-2147483649i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(-2147483650i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(-300147483650i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(-9223372036854775808i64)"); // -2^63
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(2147483648i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(2147483649i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(300147483650i64)");
	testMainIntegerArgInvalidProgram("def main(int x) int : toInt32(9223372036854775807i64)"); // 2^63-1

	// TODO: should be able to remove ALLOW_UNSAFE at some point.
	testMainIntegerArg("def main(int x) int : toInt32(toInt64(x))", 0, 0, ALLOW_UNSAFE);
	testMainIntegerArg("def main(int x) int : toInt32(toInt64(x))", 1, 1, ALLOW_UNSAFE);
	testMainIntegerArg("def main(int x) int : toInt32(toInt64(x))", -1, -1, ALLOW_UNSAFE);
	testMainIntegerArg("def main(int x) int : toInt32(toInt64(x))", -2147483647 - 1, -2147483647 - 1, ALLOW_UNSAFE);
	testMainIntegerArg("def main(int x) int : toInt32(toInt64(x))", 2147483647, 2147483647, ALLOW_UNSAFE);

	// ===================================================================
	// Test toInt64
	// ===================================================================
	testMainInt64Arg("def main(int64 x) int64 : toInt64(0)", 0, 0);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(1)", 0, 1);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(-1)", 0, -1);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(-2147483648)", 0, -2147483648LL);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(2147483647)", 0, 2147483647);
}


static void testStringFunctions()
{
	// ===================================================================
	// Test string elem function (elem(string s, uint64 index) char)
	// ===================================================================
	// TODO: should be able to remove ALLOW_UNSAFE at some point.
	testMainIntegerArg("def main(int x) int : codePoint(elem(\"a\", 0i64))", 0, (int)'a', ALLOW_UNSAFE | INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint(elem(\"hello\", 0i64))", 0, (int)'h', ALLOW_UNSAFE | INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint(elem(\"hello\", 1i64))", 0, (int)'e', ALLOW_UNSAFE | INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint(elem(\"hello\", 4i64))", 0, (int)'o', ALLOW_UNSAFE | INVALID_OPENCL);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"abc\", x)))", 0, (int)'a', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"abc\", x)))", 1, (int)'b', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"abc\", x)))", 2, (int)'c', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// with multi-byte chars:
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"\\u{393}\", x)))", 0, 0x393, ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"\\u{393}bc\", x)))", 0, 0x393, ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"\\u{393}bc\", x)))", 1, (int)'b', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"\\u{393}bc\", x)))", 2, (int)'c', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"a\\u{393}c\", x)))", 0, (int)'a', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"a\\u{393}c\", x)))", 1, 0x393, ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"a\\u{393}c\", x)))", 2, (int)'c', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"ab\\u{393}\", x)))", 0, (int)'a', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"ab\\u{393}\", x)))", 1, (int)'b', ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : toInt64(codePoint(elem(\"ab\\u{393}\", x)))", 2, 0x393, ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// ===================================================================
	// Test Escaped characters in string literals
	// ===================================================================
	const std::string SINGLE_QUOTE = "'";
	const std::string DBL_QUOTE = "\"";
	const std::string NEWLINE = "\n";
	const std::string CRG_RTN = "\r";
	const std::string TAB = "\t";
	const std::string BACKSLASH = "\\";
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + NEWLINE + DBL_QUOTE, "hello", "\n"); // \n
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + CRG_RTN + DBL_QUOTE, "hello", "\r"); // \r
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + TAB + DBL_QUOTE, "hello", "\t"); // \t
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + BACKSLASH + BACKSLASH + DBL_QUOTE, "hello", "\\"); // test backslash escape
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + BACKSLASH + DBL_QUOTE + DBL_QUOTE, "hello", "\""); // test double quote escape

	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + BACKSLASH + "a" + DBL_QUOTE + ")");
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + BACKSLASH + "a"); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + BACKSLASH + "n"); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + BACKSLASH); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + "\\u{0}"); // EOF immediately after literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + "\\u{0"); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + "\\u{"); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + "\\u"); // EOF in middle of literal
	testMainIntegerArgInvalidProgram("def main(int x) int : length(" + DBL_QUOTE + "\\"); // EOF in middle of literal

	// Test Unicode code-point escape sequence
	testMainInt64Arg("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{0}" + DBL_QUOTE + ")", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{7FFF}" + DBL_QUOTE + ")", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{10FFFF}" + DBL_QUOTE + ")", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainStringArg("def main(string s) string : " + DBL_QUOTE + "\\u{393}" + DBL_QUOTE, "hello", "\xCE\x93"); // Greek capital letter gamma, U+393, http://unicodelookup.com/#gamma/1, 
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + "\\u{20AC}" + DBL_QUOTE, "hello", "\xE2\x82\xAC"); // Euro sign, U+20AC
	testMainStringArg("def main(string s) string : " + DBL_QUOTE + "\\u{2005A}" + DBL_QUOTE, "hello", "\xF0\xA0\x81\x9A"); // A vietnamese character: U+2005A  http://www.isthisthingon.org/unicode/index.php?page=20&subpage=0&glyph=2005A


	// Test some invalid Unicode code-point escape sequences
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{-0}" + DBL_QUOTE + ")");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{q}" + DBL_QUOTE + ")");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{7Fq}" + DBL_QUOTE + ")");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{11FFFF}" + DBL_QUOTE + ")");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{11FFFF" + DBL_QUOTE + ")");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(" + DBL_QUOTE + "\\u{" + DBL_QUOTE + ")");


	// ===================================================================
	// Test escaped characters in char literals
	// ===================================================================
	testMainStringArg("def main(string s) string : toString('" + NEWLINE + "')", "hello", "\n"); // \n
	testMainStringArg("def main(string s) string : toString('" + CRG_RTN + "')", "hello", "\r"); // \r
	testMainStringArg("def main(string s) string : toString('" + TAB + "')", "hello", "\t"); // \t
	testMainStringArg("def main(string s) string : toString('" + BACKSLASH + BACKSLASH + "')", "hello", "\\"); // test backslash escape
	testMainStringArg("def main(string s) string : toString('" + BACKSLASH + SINGLE_QUOTE + "')", "hello", "'"); // test single quote escape


	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\a'))");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\a"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{0}"); // EOF immediately after literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{0"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\"); // EOF in middle of literal

	// Test Unicode code-point escape sequence
	testMainInt64Arg("def main(int64 x) int64 : length(toString('\\u{0}'))", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(toString('\\u{7FFF}'))", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	testMainInt64Arg("def main(int64 x) int64 : length(toString('\\u{10FFFF}'))", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainStringArg("def main(string s) string : toString('\\u{393}')", "hello", "\xCE\x93"); // Greek capital letter gamma, U+393, http://unicodelookup.com/#gamma/1, 
	testMainStringArg("def main(string s) string : toString('\\u{20AC}')", "hello", "\xE2\x82\xAC"); // Euro sign, U+20AC
	testMainStringArg("def main(string s) string : toString('\\u{2005A}')", "hello", "\xF0\xA0\x81\x9A"); // A vietnamese character: U+2005A  http://www.isthisthingon.org/unicode/index.php?page=20&subpage=0&glyph=2005A


	// Test some invalid Unicode code-point escape sequences
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{-0}'))");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{q}'))");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{7Fq}'))");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{11FFFF}'))");
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{11FFFF'))"); // EOF in middle of literal
	testMainInt64ArgInvalidProgram("def main(int64 x) int64 : length(toString('\\u{'))"); // EOF in middle of literal


	// ===================================================================
	// Test codePoint()
	// ===================================================================
	testMainIntegerArg("def main(int x) int : codePoint('\\u{0}')", 2, 0, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint('a')", 2, 0x61, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint('\\u{393}')", 2, 0x393, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint('\\u{20AC}')", 2, 0x20AC, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : codePoint('\\u{2005A}')", 2, 0x2005A, INVALID_OPENCL);

	// ===================================================================
	// Test character literals
	// ===================================================================
	testMainIntegerArg("def main(int x) int : let c = 'a' in 10 ", 2, 10, INVALID_OPENCL);


	// Return a character from a function
	testMainIntegerArg("def f() char : 'a'    def main(int x) int : let c = f() in 10 ", 2, 10, INVALID_OPENCL);


	// ===================================================================
	// Test toString(char)
	// ===================================================================
	testMainStringArg("def main(string s) string : toString('a')", "", "a");

	testMainStringArg("def main(string s) string : toString(length(s) < 2 ? 'a' : 'b')", "", "a", ALLOW_TIME_BOUND_FAILURE); // Test with runtime character choice
	testMainStringArg("def main(string s) string : toString(length(s) < 2 ? 'a' : 'b')", "hello", "b", ALLOW_TIME_BOUND_FAILURE); // Test with runtime character choice

	testMainInt64Arg("def main(int64 x) int64 : length(toString('a'))", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	testMainInt64Arg("def main(int64 x) int64 : length(toString(x < 5 ? 'a' : 'b'))", 2, 1, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE); // Test with runtime character


	testMainStringArg("def main(string s) string : concatStrings(toString('a'), toString('b'))", "", "ab", ALLOW_TIME_BOUND_FAILURE | ALLOW_SPACE_BOUND_FAILURE);

	// ===================================================================
	// Test == and !=
	// ===================================================================

	// Test string ==
	testMainFloat("def eq(string a, string b) !noinline bool : a == b     \n\
				  def main() float : eq(\"abc\", \"def\") ? 1.0 : 2.0",
				  2.0f);

	// Test string ==
	testMainFloat("def eq(string a, string b) !noinline bool : a == b     \n\
				  def main() float : eq(\"abc\", \"abc\") ? 1.0 : 2.0",
				  1.0f);

	// Test string == without !noinline
	testMainFloat("def main() float : (\"abc\" == \"def\") ? 1.0 : 2.0", 2.0f);
	testMainFloat("def main() float : (\"abc\" == \"abc\") ? 1.0 : 2.0", 1.0f);
	testMainFloat("def main() float : (\"\" == \"\") ? 1.0 : 2.0", 1.0f);
	testMainFloat("def main() float : (\"\" == \"a\") ? 1.0 : 2.0", 2.0f);

	// Test string !=
	testMainFloat("def eq(string a, string b) !noinline bool : a != b     \n\
				  def main() float : eq(\"abc\", \"def\") ? 1.0 : 2.0",
				  1.0f);

	// Test string !=
	testMainFloat("def eq(string a, string b) !noinline bool : a != b     \n\
				  def main() float : eq(\"abc\", \"abc\") ? 1.0 : 2.0",
				  2.0f);

	// Test string != without !noinline
	testMainFloat("def main() float : (\"abc\" != \"def\") ? 1.0 : 2.0", 1.0f);
	testMainFloat("def main() float : (\"abc\" != \"abc\") ? 1.0 : 2.0", 2.0f);
	testMainFloat("def main() float : (\"\" != \"\") ? 1.0 : 2.0", 2.0f);
	testMainFloat("def main() float : (\"\" != \"a\") ? 1.0 : 2.0", 1.0f);
}


static void testDestructuringAssignment()
{
	// ===================================================================
	// Test destructuring assignment
	// ===================================================================
	testMainFloatArg("def main(float x) float :	 let a, b = (x + 1.0, x + 2.0) in a", 4.f, 5.f);

	testMainFloatArg("def main(float x) float :	 let a, b = (3, 4) in a", 1.f, 3.f);
	testMainFloatArg("def main(float x) float :	 let a, b = (3, 4) in b", 1.f, 4.f);

	// Test on a tuple returned from a function
	testMainFloatArg("def f(float x) : (x + 1.0, x + 2.0)           def main(float x) float :	 let a, b = f(x) in a", 1.f, 2.f);
	testMainFloatArg("def f(float x) : (x + 1.0, x + 2.0)           def main(float x) float :	 let a, b = f(x) in b", 1.f, 3.f);

	// Not enough vars
	testMainFloatArgInvalidProgram("def main(float x) float :	 let a, b = (1, 2, 3) in a");
	testMainFloatArgInvalidProgram("def main(float x) float :	 let a, b, c = (1, 2, 3, 4) in a");

	// Too many vars
	testMainFloatArgInvalidProgram("def main(float x) float :	 let a, b = [1]t in a");
	testMainFloatArgInvalidProgram("def main(float x) float :	 let a, b, c = (1, 2) in a");
	testMainFloatArgInvalidProgram("def main(float x) float :	 let a, b, c, d = (1, 2, 3) in a");

	// Test type declarations on let vars
	testMainFloatArg("def main(float x) float :	 let float a, b = (3.0, 4.0) in a", 1.f, 3.f);
	testMainFloatArg("def main(float x) float :	 let float a, float b = (3.0, 4.0f) in a", 1.f, 3.f);
	testMainFloatArg("def main(float x) float :	 let a, float b = (3.0, 4.0f) in a", 1.f, 3.f);
	testMainFloatArg("def main(float x) float :	 let float a, b = (3.0, 4.0) in b", 1.f, 4.f);
	testMainFloatArg("def main(float x) float :	 let float a, float b = (3.0, 4.0f) in a", 1.f, 3.f);
	testMainFloatArg("def main(float x) float :	 let a, float b = (3.0, 4.0f) in a", 1.f, 3.f);

	// TODO: test type coercion

	// Test ref-counted types
	testMainFloatArgAllowUnsafe("def main(float x) float :	 let a, b = ([3.0, 4.0, 5.0]va, 10.0) in a[0]", 1.f, 3.f, INVALID_OPENCL);
	testMainFloatArgAllowUnsafe("def main(float x) float :	 let a, b = ([3.0, 4.0, 5.0]va, 10.0) in a[1]", 1.f, 4.f, INVALID_OPENCL);

	// Test with structs
	testMainFloatArg("struct S { float s }     def main(float x) float :	 let a, b = (S(3.0), 10.0) in a.s", 1.f, 3.f);


	// Test on a tuple returned from a function with a struct
	testMainFloatArg("struct S { float s }     def f(float x) : (S(x + 1.0), x + 2.0)           def main(float x) float :	 let a, b = f(x) in a.s", 1.f, 2.f);
	
	// Test on a tuple returned from a function with a varray
	testMainFloatArgAllowUnsafe("def f(float x) : ([x + 1.0]va, x + 2.0)           def main(float x) float :	 let a, b = f(x) in a[0]", 1.f, 2.f, INVALID_OPENCL);

	// Test with an incorrect number of variable names (caused a crash at some point)
	testMainIntegerArgInvalidProgram("def main(float x) float :	 let float, a, b = (3.0, 4.0) in b");
}


static void testTernaryConditionalOperator()
{
	// ===================================================================
	// Test ternary conditional operator (?:)
	// ===================================================================
	testMainIntegerArg("def main(int x) int : x < 5 ? 10 : 5 ", 2, 10);
	testMainIntegerArg("def main(int x) int : x < 5 ? 10 : 5 ", 6, 5);

	testMainIntegerArg("def main(int x) int : (x < 5) ? 10 : 5 ", 6, 5);
	testMainIntegerArg("def main(int x) int : (x * 2) < 5 ? 10 : 5 ", 6, 5);

	// Test nested ?:
	testMainIntegerArg("def main(int x) int : x < 5 ? (x < 2 ? 1 : 2) : 5 ", 1, 1);
	testMainIntegerArg("def main(int x) int : x < 5 ? x < 2 ? 1 : 2 : 5 ", 1, 1);

	testMainIntegerArg("def main(int x) int : x < 5 ? x < 2 ? 1 : 2 : 5 ", 2, 2);
	testMainIntegerArg("def main(int x) int : x < 5 ? x < 2 ? 1 : 2 : 5 ", 10, 5);
	testMainIntegerArg("def main(int x) int : (x < 5) ? x < 2 ? 1 : 2 : 5 ", 10, 5); // Test with parens
	testMainIntegerArg("def main(int x) int : x < 5 ? (x < 2) ? 1 : 2 : 5 ", 10, 5); // Test with parens
}


static void testLengthFunc()
{
	// ===================================================================
	// Test length function
	// ===================================================================
	// Test length() function on varrays
	testMainInt64Arg("def main(int64 x) int64 : length([1.0]va)", 1, 1);
	testMainInt64Arg("def main(int64 x) int64 : length([1.0, 2.0, 3.0]va)", 1, 3);

	// Test length() function on tuples
	testMainInt64Arg("def main(int64 x) int64 : length([1.0]t)", 1, 1);
	testMainInt64Arg("def main(int64 x) int64 : length([1.0, 2.0, 3.0]t)", 1, 3);

	// Test length() function on arrays
	testMainInt64Arg("def main(int64 x) int64 : length([1.0]a)", 1, 1);
	testMainInt64Arg("def main(int64 x) int64 : length([1.0, 2.0, 3.0]a)", 1, 3);

	// Test length() function on vectors
	testMainInt64Arg("def main(int64 x) int64 : length([1.0]v)", 1, 1);
	testMainInt64Arg("def main(int64 x) int64 : length([1.0, 2.0, 3.0]v)", 1, 3);
}


static void testFuzzingIssues()
{
	testFuzzProgram("def main(float x) float : x!");

	testFuzzProgram("struct vec3 { float x, float y, float z }	 		def vec3(float v) vec3 : vec3(v, v, v)		 		def op_add(vec3 a, vec3 b) vec3 : vec3(a.x+b.x, a.y+b.y, a.z+b.z)	 		def eval(vec3 pos) vec3 :					 			let											 				scale = 2/0.0							 			in											 				vec3(scale) + vec3(0.2)					 		def main(float x) float: x(eval(vec3(x, x, x)))");

	testFuzzProgram("def main(float x) float : (1 * (1 * ((1 + 1) * (1 - 1 + 1)) - 1 - ((1 + (((((1 + 		 1 / (1 + (((1 + 1 / (1 * (1 - 1 + 1 / 1 / 1))) - 1 - 1 - (1 + 1 / 1) + (((1 * 1 		) * 1) * 1) / 1) - 1 + 1)) - 1 / 1) + (1 * 1)) + 1 - 1) + 1) * 1)) + 1) / (1 + 1 		) / 1 - ((1 + 1 / 1 - (((1 * ((1 - 1 - 1 / 1 - 1 * 1) - (1 / 1 - 1 * 1) + ((1 +	 		1) - 1 * 1) - 1 / 1)) + (1 / 1 / 1 - 1 - (1 + 1) / (1 + 1) + 1)) * (1 * ((((1 +	 		1) * 1) / 1 - 1 - 1 - (((1 + 1) * 1 - (((1 / 1 * 1) * (1 + 1) - 1) / 1 / 1 / 1 - 		 (1 + (((1 + 1) + 1) * 1)) - 1 / ((1 + 1) / (1 * 1) / (1 + (1 / 1 * 1 / 1 - 1 -	 		(1 * 1) / (1 + 1) - 1 - 1 - 1 - 1 - 1 / 1)) / (1 / 1 * 1) + (((1 + 1) * ((1 + 1) 		 * 1)) / 1 + ((1 - ((1 * 1) + 1) + 1) + 1 - 1 / ((1 / (1 - ((1 + 1) - ((((1 - (( 		1 * ((1 * ((1 + 1) + ((1 - (1 * 1) + (1 - (1 - 1 - (1 * 1) / (1 + 1) / 1 * (1 -	 		1 + (1 + 1) - ((1 / 1 / 1 + 1) * 1) - (1 - 1 * 1))) * 1) - 1 - 1 - 1 - (1 * (((1 		 * 1) * ((1 + (1 / (1 * 1) * 1 - 1)) * (1 + (((1 + 1) + 1) + 1)))) + 1) / 1)) +	 		1 / 1) / 1 - 1)) * 1 - 1)) / 1 * (1 * 1) - 1 - 1 / (1 * ((((1 + 1) + 1) * 1 - 1	 		- 1 / ((1 + 1) / (1 * 1) - ((1 / 1 + 1) * 1) + (1 + (1 * 1) - 1 - 1 - 1 - (1 + ( 		1 + 1))) / 1)) - 1 * (1 / ((1 * 1) + ((1 - 1 + (1 + 1)) + ((1 int  + 1) * 1))) / 1 *	 		1)) - 1) / 1 / 1 / 1) / 1 + 1) / (1 / (1 + (1 + (1 * 1 - 1)) - 1) / 1 - ((1 * 1	 		/ 1 / (1 / (1 * 1 / 1 - 1) / 1 * 1) / 1) - 1 * ((1 + (((1 + 1) + 1) - ((1 * ((1	 		+ 1) + 1)) * ((1 * 1) * 1) - 1) / 1 / (1 - 1 / (1 - 1 * 1) / 1 * 1) / ((1 - (1 + 		 1) / 1 * 1) + 1 - (1 + (1 * 1)) - 1) - 1 * 1) - 1 / 1) * 1)) + (((1 - (1 / 1 -	 		1 + 1 - (1 * (1 + 1)) / (1 + (1 + 1))) + 1) - ((1 + 1) * (1 * 1 / 1)) * 1 - 1) + 		 1) - 1 / 1 - (((1 * 1 - 1) + 1) - (1 + 1) - 1 - 1 * 1)) / (1 + 1) + (((1 * ((1	 		/ (1 * 1) * 1) * 1)) * 1) + 1 - (1 * 1)) / 1) * 1) + (1 / 1 * 1 / (1 * (1 + (1 / 		 1 / 1 - ((1 + ((1 - 1 / 1 + 1 / 1 - 1) * 1)) * 1) - 1 / 1 / 1 / (1 + 1) - (1 +	 		((1 + (((1 + 1 - 1 / ((1 + 1) * (1 + (1 / 1 + (1 + 1))))) * 1) * ((1 - 1 / (1 +	 		1 - (((1 / 1 - 1 / 1 * (1 + (((1 * 1) * (1 - 1 * (1 * 1))) - 1 + ((1 * 1) * ((1	 		- 1 * 1) / 1 + (1 + 1)))))) * 1) + 1 - 1 - 1 / (1 + ((1 * (1 + 1)) * 1)))) / (1	 		+ 1 - 1) + 1) - 1 * 1) / 1 - 1 / 1 - ((((1 + (1 * 1 - 1)) * ((1 + (1 * 1)) * 1)) 		 * ((1 + 1) + 1)) + 1) / 1 / 1 - ((1 / 1 - (((1 + (1 * 1) - 1) - 1 * 1 - (1 + 1) 		 - 1) + 1) + (1 / (1 + (1 * 1)) / 1 * 1 / 1) / (1 + 1)) * 1 / 1 - 1) - 1 - 1 - 1 		) / 1) * 1) / 1) + 1)) / 1 / 1)) / 1) * ((((1 / 1 + 1) - 1 * 1 / (1 + 1)) * 1 /	 		1) / 1 * 1) / 1 / 1 / 1 - ((1 * (((1 / 1 * (1 * (1 / ((1 - 1 * (1 * 1 / 1)) + (( 		1 * (1 + 1)) * 1)) * (1 + (1 + (1 * (1 + 1 / 1 - (1 - 1 * ((1 + 1) + 1 - (1 * 1) 		) / ((1 - 1 + 1) - 1 + 1)))) - 1 / (1 + 1) / 1 / 1 / 1 / (1 + (1 * (1 - (1 * 1 / 		 1) * 1))) / 1 - 1 - 1))) / ((1 + 1) + 1 /  + (1 * 1) / 1 / (1 * 1 / 1) - 1 - 1 - 1 		 / (1 * ((1 / 1 - 1 / 1 - 1 / 1 / 1 / 1 / 1 * 1) - 1 / (1 - ((1 / 1 + 1) + 1) -	 		1 / (1 * (1 + 1)) / 1 * (1 * 1) / 1) / 1 * 1)) / (1 + 1) / (((1 - 1 * 1) + (1 -	 		(1 + 1) + 1 / 1)) * 1) - (1 / 1 / 1 * 1) - (1 - ((1 * (1 - (1 + 1) + 1)) * (1 +	 		1)) / 1 / 1 + (1 - 1 - (1 + (1 * 1)) - ((1 + 1 - ((1 + ((1 * 1) * 1 - 1 / 1)) +	 		1) - 1) / 1 - 1 + 1) / (((1 * 1) + 1) + 1) * (1 * 1)))) / 1 / ((1 * 1) + 1 - 1)) 		 / 1) * 1) * 1)) * ((1 - (1 * 1 / 1) + 1) * 1 / 1)) / (1 + 1) / 1) + (1 / 1 * (1 		 + 1 / 1 - (1 / (1 - 1 + 1) + 1) / (1 + 1) - (1 * 1)) - (((1 / 1 + 1 - 1 - 1) /	 		1 * 1) + (1 * 1) / (1 / 1 * 1) - 1 - (1 + 1))) / 1) + 1) + 1)) / 1 / 1)) + 1) -	 		1 - (1 * 1 - 1)) * ((1 + 1) * 1)) - 1 * (1 + ((1 * 1) + (1 * 1)))) - 1 * 1)))) * 		 1) - 1 - 1 - 1))");


	// NaN
	testFuzzProgram("def g(float x) float :  pow( -2 + x, -(1.0 / 8.0))         def main(float x) float : g(0.5)");

	//testMainFloatArgReturnsInt("def main(float x) : if(x < 10.0, 3, 4)", 1.f, 3);
	testMainFloatArg("def main(float x) float : if(x < 10.0, 3, 4)", 1.f, 3.f);

	// Main function that is generic.
	testMainFloatArgInvalidProgram("struct Float4Struct { vector<float, 4> v }  		\n\
								   def pow(Float4Struct a, Float4Struct b) : Float4Struct(pow(a.v, b.v))		 	\n\
								   def main<T>(T x) T : x*x           \n\
								   def m(Float4Struct a, Float4Struct b) Float4Struct : pow(a, b)");

	testMainFloatArg("def f(int x) int : x*x	      def main(float x) float : f(2)", 2.0f, 4.0f);

	// Some constant folding tests
	testMainIntegerArgInvalidProgram("def main(int x) int : ((1 + 2) + (3 + true)) + (5 + 6)");
	testMainIntegerArg("def main(int x) int : ((1 + 2) + (3 + 4)) + (5 + 6)", 1, 21);

	testMainIntegerArgInvalidProgram("def g(function<float, float> f, float x) : f(xow(x, 3))");

	Timer timer;
	testMainFloatArgInvalidProgram("def main(float x) float : (1 * (1 * ((1 + 1) * (1 - 1 + 1)) - 1 - ((1 + (((((1 +\n\
		 1 / (1 + (((1 + 1 / (1 * (1 - 1 + 1 / 1 / 1))) - 1 - 1 - (1 + 1 / 1) + (((1 * 1\n\
		) * 1) * 1) / 1) - 1 + 1)) - 1 / 1) + (1 * 1)) + 1 - 1) + 1) * 1)) + 1) / (1 + 1\n\
		) / 1 - ((1 + 1 / 1 - (((1 * ((1 - 1 - 1 / 1 - 1 * 1) - (1 / 1 - 1 * 1) + ((1 +	\n\
		1) - 1 * 1) - 1 / 1)) + (1 / 1 / 1 - 1 - (1 + 1) / (1 + 1) + 1)) * (1 * ((((1 +	\n\
		1) * 1) / 1 - 1 - 1 - (((1 + 1) * 1 - (((1 / 1 * 1) * (1 + 1) - 1) / 1 / 1 / 1 -\n\
		 (1 + (((1 + 1) + 1) * 1)) - 1 / ((1 + 1) / (1 * 1) / (1 + (1 / 1 * 1 / 1 - 1 -	\n\
		(1 * 1) / (1 + 1) - 1 - 1 - 1 - 1 - 1 / 1)) / (1 / 1 * 1) + (((1 + 1) * ((1 + 1)\n\
		 * 1)) / 1 + ((1 - ((1 * 1) + 1) + 1) + 1 - 1 / ((1 / (1 - ((1 + 1) - ((((1 - ((\n\
		1 * ((1 * ((1 + 1) + ((1 - (1 * 1) + (1 - (1 - 1 - (1 * 1) / (1 + 1) / 1 * (1 -	\n\
		1 + (1 + 1) - ((1 / 1 / 1 + 1) * 1) - (1 - 1 * 1))) * 1) - 1 - 1 - 1 - (1 * (((1\n\
		 * 1) * ((1 + (1 / (1 * 1) * 1 - 1)) * (1 + (((1 + 1) + 1) + 1)))) + 1) / 1)) +	\n\
		1 / 1) / 1 - 1)) * 1 - 1)) / 1 * (1 * 1) - 1 - 1 / (1 * ((((1 + 1) + 1) * 1 - 1	\n\
		- 1 / ((1 + 1) / (1 * 1) - ((1 / 1 + 1) * 1) + (1 + (1 * 1) - 1 - 1 - 1 - (1 + (\n\
		1 + 1))) / 1)) - 1 * (1 / ((1 * 1) + ((1 - 1 + (1 + 1)) + ((1 + 1) * 1))) / 1 *	\n\
		1)) - 1) / 1 / 1 / 1) / 1 + 1) / (1 / (1 + (1 + (1 * 1 - 1)) - 1) / 1 - ((1 * 1	\n\
		/ 1 / (1 / (1 * 1 / 1 - 1) / 1 * 1) / 1) - 1 * ((1 + (((1 + 1) + 1) - ((1 * ((1	\n\
		+ 1) + 1)) * ((1 * 1) * 1) - 1) / 1 / (1 - 1 / (1 - 1 * 1) / 1 * 1) / ((1 - (1 +\n\
		 1) / 1 * 1) + 1 - (1 + (1 * 1)) - 1) - 1 * 1) - 1 / 1) * 1)) + (((1 - (1 / 1 -	\n\
		1 + 1 - (1 * (1 + 1)) / (1 + (1 + 1))) + 1) - ((1 + 1) * (1 * 1 / 1)) * 1 - 1) +\n\
		 1) - 1 / 1 - (((1 * 1 - 1) + 1) - (1 + 1) - 1 - 1 * 1)) / (1 + 1) + (((1 * ((1	\n\
		/ (1 * 1) * 1) * 1)) * 1) + 1 - (1 * 1)) / 1) * 1) + (1 / 1 * 1 / (1 * (1 + (1 /\n\
		 1 / 1 - ((1 + ((1 - 1 / 1 + 1 / 1 - 1) * 1)) * 1) - 1 / 1 / 1 / (1 + 1) - (1 +	\n\
		((1 + (((1 + 1 - 1 / ((1 + 1) * (1 + (1 / 1 + (1 + 1))))) * 1) * ((1 - 1 / (1 +	\n\
		1 - (((1 / 1 - 1 / 1 * (1 + (((1 * 1) * (1 - 1 * (1 * 1))) - 1 + ((1 * 1) * ((1	\n\
		- 1 * 1) / 1 + (1 + 1)))))) * 1) + 1 - 1 - 1 / (1 + ((1 * (1 + 1)) * 1)))) / (1	\n\
		+ 1 - 1) + 1) - 1 * 1) / 1 - 1 / 1 - ((((1 + (1 * 1 - 1)) * ((1 + (1 * 1)) * 1))\n\
		 * ((1 + 1) + 1)) + 1) / 1 / 1 - ((1 / 1 - (((1 + (1 * 1) - 1) - 1 * 1 - (1 + 1)\n\
		 - 1) + 1) + (1 / (1 + (1 * 1)) / 1 * 1 / 1) / (1 + 1)) * 1 / 1 - 1) - 1 - 1 - 1\n\
		) / 1) * 1) / 1) + 1)) / 1 / 1)) / 1) * ((((1 / 1 + 1) - 1 * 1 / (1 + 1)) * 1 /	\n\
		1) / 1 * 1) / 1 / 1 / 1 - ((1 * (((1 / 1 * (1 * (1 / ((1 - 1 * (1 * 1 / 1)) + ((\n\
		1 * (1 + 1)) * 1)) * (1 + (1 + (1 * (1 + 1 / 1 - (1 - 1 * ((1 + 1) + 1 - (1 * 1)\n\
		) / ((1 - 1 + 1) - 1 + 1)))) - 1 / (1 + 1) / 1 / 1 / 1 / (1 + (1 * (1 - (1 * 1 /\n\
		 1) * 1))) / 1 - 1 - 1))) / ((1 + 1) + 1 / (1 * 1) / 1 / (1 * 1 / 1) - 1 - 1 - 1\n\
		 / (1 * ((1 / 1 - 1 / 1 - 1 / 1 / 1 / 1 / 1 * 1) - 1 / (1 - ((1 / 1 + 1) + 1) -	\n\
		1 / (1 * (1 + 1)) / 1 * (1 * 1) / 1) / 1 * 1)) / (1 + 1) / (((1 - 1 * 1) + (1 -	\n\
		(1 + 1) + 1 / 1)) * 1) - (1 / 1 / 1 * 1) - (1 - ((1 * (1 - (1 + 1) + 1)) * (1 +	\n\
		1)) / 1 / 1 + (1 - 1 - (1 + (1 * 1)) - ((1 + 1 - ((1 + ((1 * 1) * 1 - 1 / 1)) +	\n\
		1) - 1) / 1 - 1 + 1) / (((1 * 1) + 1) + 1) * (1 * 1)))) / 1 / ((1 * 1) + 1 - 1))\n\
		 / 1) * 1) * 1)) * ((1 - (1 * 1 / 1) + 1) * 1 / 1)) / (1 + 1) / 1) + (1 / 1 * (1\n\
		 + 1 / 1 - (1 / (1 - 1 + 1) + 1) / (1 + 1) - (1 * 1)) - (((1 / 1 + 1 - 1 - 1) /	\n\
		1 * 1) + (1 * 1) / (1 / 1 * 1) - 1 - (1 + 1))) / 1) + 1) + 1)) / 1 / 1)) + 1) -	\n\
		1 - (1 * 1 - 1)) * ((1 + 1) * 1)) - 1 * (1 + ((1 * 1) + (1 * 1)))) - 1 * 1)))) *\n\
		 1) - 1 - 1 - 1))");

	//std::cout << timer.elapsedString() << std::endl;


	testMainIntegerArgInvalidProgram("def f<T>(T x) : x ()        def main() float : 0  +  f(2.0)");
	testMainIntegerArgInvalidProgram("def f<T>(T x) : x ()        def main() float : 0  -  f(2.0)");
	testMainIntegerArgInvalidProgram("def f<T>(T x) : x ()        def main() float : 0  *  f(2.0)");
	testMainIntegerArgInvalidProgram("def f<T>(T x) : x ()        def main() float : 0  /  f(2.0)");
	testMainIntegerArgInvalidProgram("def f<T>(T x) : x ()        def main() float : 0  <  f(2.0)");

	testMainIntegerArg("def main(int x) int : elem( [1, 1, 1, 1]v + [x, x, x, x]v, 2)", 1, 2);
	testMainIntegerArg("def main(int x) int : elem( [1, 1, 1, 1]v - [x, x, x, x]v, 2)", 1, 0);
	testMainIntegerArg("def main(int x) int : elem( [1, 1, 1, 1]v * [x, x, x, x]v, 2)", 1, 1);
	testMainIntegerArgInvalidProgram("def main(int x) int : elem( [1, 1, 1, 1]v / [x, x, x, x]v, 2)"); // Vector divides are not allowed for now


	//fuzzTests();

	testMainFloatArg(
		"def main(float x) float: elem(   2.0 * [1.0, 2.0, 3, 4]v, 1)",
		1.0f, 4.0f);

	
	testMainFloatArgInvalidProgram("def main(float x) float : [1.f, elem([1, 1.f]a, elem([1, 1.f]a, 1))]t");
	testMainFloatArgInvalidProgram("def main(float x) float : [false, [[1, 1]t, ((true - false) / (1.f / 1))]a]a");

	testMainFloatArgInvalidProgram("def main(float x) float : ([[false, (1.f && 1)]a, elem([1.f, false]v, - (1))]v / elem([1.f, (1 && 1)]v, - (- (1) ) ))");

	testMainFloatArgInvalidProgram("def main(float x) float : ([[[true, 1.f]a, !false]a, 1]v * [([false, 1]a * true), [-true, (false * true)]v]t)");

	testMainFloatArgInvalidProgram("def main(float x) float : ([[1.f, true]v / if 1 then true else 1, (-1 + (1 - 1.f))]a - ([[false, 1.f]t, (false + 1.f)]a - !-1.f))");

	testMainIntegerArg("def main(int x) int : (1 + 2) + (3 + 4)", 1, 10);



	testMainFloatArg("def main(float x) float : (1 * (1 + ((1 / (1 - 1 / 1 - 1 / ((1 / 1 / 1 + (1 + (1\n\
		 * 1))) * 1) - 1 - (1 - 1 / 1 - 1 - (1 * 1) * (1 + 1 - 1 / 1)) - (1 + 1) - 1 / (\n\
		(1 * 1) - ((1 - 1 * 1) + 1 - ((1 + 1) * (1 * 1) - 1) - 1 / 1 - (1 + (((1 + 1) +\n\
		(1 - 1 - 1 / 1 + 1)) - 1 + 1))) + 1) + ((((1 * (1 + (((1 * ((1 + 1) * 1)) + 1) /\n\
		 1 * 1) - 1 - 1 / ((1 + 1 - 1) + 1) - 1 - 1)) * (1 + (1 + (1 / (1 * (1 * 1)) - (\n\
		1 + 1 - (1 * 1)) + 1) - 1)) / 1) * ((1 + 1) + 1) / 1 / 1) / 1 * 1)) + 1) * (1 *\n\
		1)) - 1 / 1)) / (1 * 1)",
	1.f, 1.f);

	//fuzzTests();

	// Out of bounds elem()
	testMainFloatArgInvalidProgram("def main(int x) int :  elem(update([0]a, -1, 1), 0)");

	// Test coercion of int to float, after it is used to bind to a given function (toFloat).
	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(3*x)");
	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(truncateToInt(3.1)*x)");
	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(truncateToInt(3.1)+x)");
	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(truncateToInt(3.1)-x)");
	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(truncateToInt(3.1)/x)");


	// Test circular reference between a named constant and function.
	testMainIntegerArgInvalidProgram("TEN = main(0)		def main(int x) : TEN + x");




	// Test division is only valid for integers, floats
	testMainFloatArgInvalidProgram("def main(float x) float : [1, 2]a / [3, 4]a"); 	


	testMainIntegerArgInvalidProgram("def f<T>(T x) : x        def main() float : f + (2.0)");
	//	testFuzzProgram("def f(array<int, 16> current_state, int elem) array<int, 16> :				 			let																		 				old_count = elem(current_state, elem)								 				new_count = old_count + 1											 			in																		 				update(curreQnt_state, elem, new int _count)		");
//	testFuzzProgram("		def main() floa string t : 				 array 	 let a = [1.0[11.0, , 2.0, 3.0, 4.0]v 					 b = [11.0, 12.0, 13.0, 14.0]v in 					 e2(min(a, b)) 		def main() float : 				  let a = [1.0, 2.0, 3.0, 4.0]v 				  b = [11.0, 12.0, 13.0, 14.0]v in 				  e2( 1.0 min(b, a))");
	testMainIntegerArgInvalidProgram("	struct s { float x, float y }						 				  def op_adsd(s a, s b) : s(a.x + b.x, a.y + b.y)		 				  def f<T>(T a, T b) : a + b							 				  def main() float : x(!f(s(1, 2), s(3, 4)))");
//	testFuzzProgram("def main(floa string t x) float : 1.0   1  def f(int x) int : f(0)  ");


	//testFuzzProgram("def f(array<int, 4> a, int i) int :		 			if inBounds(a, i)						 				elem(a, i)			 										 			else						 				0									 		def main(int i) int :						 			f([1, 2, 3, 4]a, i)");
	//testFuzzProgram("#	def main(float x) float : 					 l main(float x) fet v = [x, x, x, x]v in\				 dot(v, v)");
//	testFuzzProgram("struct vec4 { vector<float, 4> v }					 				   struct vec16 { vector<float, 16> v }					 				   st ruct large_struct { vec4 a, vec16 b }				 				   def main(float x) float : large_struct(vec4([x, x, x, x]v), ve char c16([x, x, x, x, x int64 , x, x, x, x, x, x, x, x, x, x, x]v)).a.v.e0");
	testMainFloatArg("def overloadedFunc(int x) float : 4.0 	  def overloadedFunc(float x) float : 5.0 		def f<T>(T x) float: overloadedFunc(x) / 4  def main(float x) float : f(1.0)", 1.0f, 5.f/4);

	testMainIntegerArg("def main(int x) int : 1 * 2 + 3", 1, 5);
	testMainIntegerArg("def main(int x) int : 1 * (2 + 3)", 1, 5);
	testMainIntegerArg("def main(int x) int : [0, 1, 2, 3]a[0] + 1", 1, 1);

	testMainIntegerArg(
		"struct SpectralVector { vector<float, 8> v }			\n\
		def f(SpectralVector layer_contribution) float :			\n\
				layer_contribution.v[0] * 2.0f				\n\
																\n\
		def main(int x) int : 1", 1, 1);

	/*testMainIntegerArg(
		"struct SpectralVector { vector<float, 8> v }			\n\
		def splatsForLayerContribution(SpectralVector layer_contribution, SpectralVector wavelens) float :			\n\
			let													\n\
					XYZ_0 = wavelens.v[0]						\n\
				in												\n\
					layer_contribution.v[0] * XYZ_0				\n\
																\n\
		def main(int x) int : 1", 1, 1);*/

	// ===================================================================
	// Miscellaneous programs that caused crashes or other errors during fuzz testing
	// ===================================================================

	// max() with vector args with different width
 	testMainFloatArgInvalidProgram("def clamp(vector<float, 46> x, vector<float, 4> lowerbound, vector<float, 4> upperbound) vector<float, 4> : max(lowerbound, min(upperbound, x))    def main(float x) float : x");


	// Test invalidity of repeated struct definition
	testMainFloatArgInvalidProgram("struct Complex { float re, float im }  struct Complex { float rm }   def main(float x) float : x");
	testMainFloatArgInvalidProgram("struct Complex { float re, float im }  struct Complex { float re, float im }   def main(float x) float : x");
	testMainFloatArgInvalidProgram("struct Complex { float re, float im }  struct Complex { float rm }    def f() Complex : Complex(1.0, 2.0)    def main() float :   f().im");

	testMainFloatArgInvalidProgram("struct PolarisationVec { vector<float, 8> e }  		struct PolarisationVec {  string ve }");

//	testMainFloatArg("def main(float x)  : if(x < 10.0, 3, 4)", 1.0f, 3.0f);

	testMainFloatArg("def main(float x) float : let v = [x, x, x, x, x, x, x, x]v in dot(v, v)", 1.f, 8.f, INVALID_OPENCL); // dot() in OpenCL doesn't work with float8s.

	testMainFloatArgInvalidProgram("def g(function<float, float> f, float x) : f(x)");

	testMainFloatArgInvalidProgram("def overload(float x) float : 5.0 				  def overloadedFunc(float x) float : 5.0 	 def f<T>(T x) float: overloadedFunc       def main() float : f(1)");

	testMainFloatArgInvalidProgram("def expensiveA(float x) float : cos(x * 2.0)			 def expensiveA(float x) float : cos(x * 2.0)			 		def main(float x) float: e xpensiveA(x) 		def expensiveA(float x) float : cos(x * 2.0)			 		def main(float x) float: x + expensiveA(x) 		def expensiveA(float x) float : cos(x * 0.456 + cos(x))			 		def expensiveB(float x) float : sin(x * 0.345 + sin(x)).			 		def main(float x) float: if(x < 0.5, expensiveA(x + 0.145), expensiveB(x + 0.2435)) 	def g(float x) float : 8.f             def main(float x) float :  pow(g(x), 2.0) 	def main(float x) float :  let y = (1.0 + 1.0) in pow(y, 3.0) 	def g(float x) float :  pow(2 + x, -(1.0 / 8.0))         def main(float x) float : g(0.5)");

	// Disallow definition of same function (with same signature) twice.
	testMainFloatArgInvalidProgram("def f(float x) : x   def f(float x) : x     def main(float x) float : f(2.0)");

	testMainFloatArgInvalidProgram("def f<T>(T x) : x ()        def main() float : f(2.0)");

	testMainFloatArgInvalidProgram("struct s { float x, float y }						\n\
				  def f<T>(T a, T b) : a + b							\n\
				  def main() float : x(f(s(1, 2), s(3, 4)))");

	//testMainFloatArgInvalidProgram("struct s { float x, float y }	def op_fadd(s a, s b) : s(a.x + b.x, a.y + b.y)		 def f<T>(T a, T b) : a + b		 def main() float : x(f(s(1, 2), s(3, 4)))");

//	testMainFloatArgInvalidProgram("struct s { float x, float y }						 	struct s { float x, float y }						 				  def op_fadd(s a, s b) : s(a.x + b.x, a.y + b.y)		 				  def f<T>(T a, T b) : a + b							 				  def main() float : x(f(s(1, 2), s(3, 4)))");

	testMainFloatArgInvalidProgram("struct s { float x, float y }  	struct s { float x, float y }  				  def op_mul(s a, s b) : s(a.x * b.x, a().y * b.y)  				  def main() float : x(s(2, 3) * s(3, 4))");
	testMainFloatArgInvalidProgram("struct s { float x, float y }   def fop_add(s a, s b) : s(a.x + b.x, a.y + b.y)	 ");

	testMainFloatArgInvalidProgram("#e71 def main(float x) float : x");
	testMainFloatArgInvalidProgram("     n=\\():\"\"     ");
	//testMainFloatArgInvalidProgram("E=\\():4.    def main(float x) float : x");

	// struct s { float x, float y }  	struct s { float x, float y }  				  def op_mul(s a, s b) : s(a.x * b.x, a().y * b.y)  				  def main() float : x(s(2, 3) * s(3, 4))

	testMainFloatArgInvalidProgram("struct s { float x, float y }   def fop_add(s a, s b) : s(a.x + b.x, a.y + b.y)	 ");
	
	testMainFloatArgInvalidProgram(" def f<T>(T a, T b) : a + b	def main() float : x(f(s(1, 2), s(3, 4)))");

	testMainFloatArgInvalidProgram("def main(float x) float : elem(  elem([1.0, 2.0, 3.0, 4.0]a, [2, 3.]v)   , 0)");

	testMainFloatArgInvalidProgram("struct Float4Struct { vector<float, 4> v }  			def sin(Float4Struct f) : Float4Struct(sin < (f.v))		");

	testMainFloatArgInvalidProgram("def f<T>(T x) T : x  ( )   def main() float : f(2.0)");

	testMainFloatArgInvalidProgram("def main() float : let f = makeFunc(2.0, 3.0) in f()");

	testMainFloatArgInvalidProgram("\t\t\t\t\tdef main() float :                            0 \t\t\t\t\tdef main() float :                           \t\t\t\t\tlet f = makeFunc(2.0, 3.0) in                     \t\t\t\t\tf()");

	testMainFloatArgInvalidProgram("\t\t\t\t  def main() float : x(s(2, 3) / s(3, 4)) \t\t\t\t  def main() float : x(s(2, 3) / s(3, 4))");

	testMainFloatArgInvalidProgram("def main(float x) float : b(elem([Pair(1.0, 2.0), Pair(3.0, 4.0)]a, 1))  ");
	
	testMainFloatArgInvalidProgram("def main(float x) float : b(elem([Pair(1.0, 2.0), Pair(3.0, 4.0)]a, 1))  		def main(float x) float : b(elem([Pair(1.0, 2.0), Pair(3.0, 4.0)]a, 1)) ");

	testMainFloatArgInvalidProgram("\t\t\tin\t\t\t\t\t\t\t\t\t\t \t\t\tin\t\t\t\t\t\t\t\t\t\t \t\t\t\telem(a, -1)\t\t\t\t\t\t\t\" \t ");
}


static void testLogicalNegation()
{
	// ===================================================================
	// Test logical negation operator (!)
	// ===================================================================
	testMainIntegerArg("def main(int x) int : if !true then 10 else 20", 1, 20);
	testMainIntegerArg("def main(int x) int : if !false then 10 else 20", 1, 10);
	testMainIntegerArg("def main(int x) int : if !(x < 5) then 10 else 20", 1, 20);
	testMainIntegerArg("def main(int x) int : if !(x >= 5) then 10 else 20", 1, 10);

	testMainIntegerArg("def main(int x) int : if !true then 10 else 20", 1, 20);
	testMainIntegerArg("def main(int x) int : if !!true then 10 else 20", 1, 10);
	testMainIntegerArg("def main(int x) int : if !!!true then 10 else 20", 1, 20);

	testMainIntegerArgInvalidProgram("def main(int x) int : if !1 then 10 else 20");
	testMainIntegerArgInvalidProgram("def main(int x) int : if ![1, 2]t then 10 else 20");
}


static void testNamedConstants()
{
	// ===================================================================
	// Test named constants
	// ===================================================================
	testMainIntegerArg("TEN = 10			\n\
					   def main(int x) int : TEN + x", 3, 13);

	// Test an expression
	testMainIntegerArg("TEN = 5 * 2			\n\
					   def main(int x) int : TEN + x", 3, 13);

	// Test an expression involving another constant
	testMainIntegerArg("FIVE = 5		TEN = FIVE * 2			\n\
					   def main(int x) int : TEN + x", 3, 13);

	// Make sure constant folding works with named constants
	testMainFloatArgCheckConstantFolded("FIVE = 5.0		TEN = FIVE * 2.0			\n\
					   def main(float x) float : TEN", 3.0f, 10.0f);

	// Test an expression involving a function call
	testMainIntegerArg("def five() int : 5		TEN = five() * 2			\n\
					   def main(int x) int : TEN + x", 3, 13);

	// Test named constants with optional declared type.
	/*testMainIntegerArg("int TEN = 10			\n\
					   def main(int x) int : TEN + x", 3, 13);
	*/

	// Test int->float type coercion to match declared type for named constants with optional declared type.
	testMainFloatArg("float TEN = 10			\n\
					   def main(float x) float : TEN + x", 1.f, 11.f);


	// Test with a function call on a structure
	// TODO: make this work
	/*testMainFloatArgCheckConstantFolded("struct S { float x }		\n\
					 def f(S s) S : S(s.x + 1.0)		\n\
					 float X = f(S(1)).x			\n\
					 def main(float x) float : X", 1.f, 2.f);
	*/

	// test invalidity of two named constants with same name.
	testMainFloatArgInvalidProgram("z = 1     z = 2               def main(float x) float : x");


	// Test named constants that aren't constant
	testMainIntegerArgInvalidProgram("TEN = x			\n\
					   def main(int x) int : TEN + x");

	testMainIntegerArgInvalidProgram("TEN = main			\n\
					   def main(int x) int : TEN + x");
					   
	testMainIntegerArgInvalidProgram("TEN = main()			\n\
					   def main(int x) int : TEN + x");

	// self-reference
	testMainIntegerArgInvalidProgram("TEN = TEN			\n\
					   def main(int x) int : TEN + x");

	// Test invalidity of mutual references between named contants.
	testMainFloatArgInvalidProgram("z = y	y = z      def main(float x) float : x");
}


static void testFold()
{
	// ===================================================================
	// Test fold built-in function
	// fold(function<State, T, State> f, array<T> array, State initial_state) State
	// ===================================================================

	testMainIntegerArg("															\n\
		def f(int current_state, int elem) int :									\n\
			current_state + elem													\n\
		def main(int x) int :  fold(f, [0, 1, 2, 3, 4, 5]a, x)", 10, 25, INVALID_OPENCL);
		

	// Test fold built-in function with update
	testMainIntegerArg("															\n\
		def f(array<int, 16> current_state, int elem) array<int, 16> :				\n\
			let																		\n\
				old_count = elem(current_state, elem)								\n\
				new_count = old_count + 1											\n\
			in																		\n\
				update(current_state, elem, new_count)								\n\
		def main(int x) int :  elem(fold(f, [0, 0, 1, 2]a, [0]a16), 1)", 10, 1,
		ALLOW_UNSAFE | INVALID_OPENCL // allow_unsafe_operations
	);


	// If first arg to fold() is bound to global def (f), and bound function f has update() as 'last' function call,
	// and first arg to update() is first arg to f, then can transform to just setting element on running state.

	// Test fold built-in function with update
	{
		const int len = 256;
		js::Vector<int, 32> vals(len, 0);
		js::Vector<int, 32> b(len, 0);
		js::Vector<int, 32> target_results(len, 0);
		target_results[0] = len;

		testIntArray("																\n\
		def f(array<int, 256> counts, int x) array<int, 256> :			\n\
				update(counts, x, elem(counts, x) + 1)			\n\
		def main(array<int, 256> vals, array<int, 256> initial_counts) array<int, 256>:  fold(f, vals, initial_counts)",
			&vals[0], &b[0], &target_results[0], vals.size(),
			ALLOW_UNSAFE | INVALID_OPENCL // allow_unsafe_operations
		);
	}

	
	// Test fold built-in function with update
	{
		const int len = 256;
		js::Vector<int, 32> vals(len, 0);
		js::Vector<int, 32> b(len, 0);
		js::Vector<int, 32> target_results(len, 0);
		target_results[0] = len;

		testIntArray("																\n\
		def f(array<int, 256> current_state, int elem) array<int, 256> :			\n\
			let																		\n\
				old_count = elem(current_state, elem)								\n\
				new_count = old_count + 1											\n\
			in																		\n\
				update(current_state, elem, new_count)								\n\
		def main(array<int, 256> vals, array<int, 256> b) array<int, 256>:  fold(f, vals, b)",
			&vals[0], &b[0], &target_results[0], vals.size(),
			ALLOW_UNSAFE | INVALID_OPENCL // allow_unsafe_operations
		);
	}
}


static void testIterate()
{
	// ===================================================================
	// Test iterate built-in function
	// ===================================================================

	testMainIntegerArg("															\n\
		def f(int current_state, int iteration) tuple<int, bool> :					\n\
			if iteration >= 100														\n\
				[current_state, false]t # break										\n\
			else																	\n\
				[current_state + 1, true]t											\n\
		def main(int x) int :  iterate(f, 0)", 17, 100);

	testMainIntegerArg("struct State { int bound, int i }							\n\
		def f(State current_state, int iteration) tuple<State, bool> :				\n\
			if iteration >= current_state.bound 									\n\
				[State(current_state.bound, current_state.i), false]t # break		\n\
			else																	\n\
				[State(current_state.bound, current_state.i + 1), true]t			\n\
		def main(int x) int :  iterate(f, State(x, 0)).i", 17, 17);

	testMainFloatArg("struct State { float i }					\n\
		def f(State current_state, int iteration) tuple<State, bool> :	\n\
			if iteration >= 100									\n\
				[State(current_state.i), false]t # break		\n\
			else												\n\
				[State(current_state.i + 1.0), true]t			\n\
		def main(float x) float :  iterate(f, State(0.0)).i", 1.0f, 100.0f, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);// NOTE: this was actually working with OpenCL until struct pass by ptr.

	// Test iterate with optional invariant data:

	// Test with pass-by-value data (integer 2)
	testMainIntegerArg("															\n\
		def f(int current_state, int iteration, int invariant_data) tuple<int, bool> :					\n\
			if iteration >= 100														\n\
				[current_state, false]t # break										\n\
			else																	\n\
				[current_state + invariant_data, true]t											\n\
		def main(int x) int :  iterate(f, 0, 2)", 17, 200, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	// Test with pass-by-reference data (struct)
	testMainIntegerArg("															\n\
		struct s { int x, int y }													\n\
		def f(int current_state, int iteration, s invariant_data) tuple<int, bool> :					\n\
			if iteration >= 100														\n\
				[current_state, false]t # break										\n\
			else																	\n\
				[current_state + invariant_data.x, true]t											\n\
		def main(int x) int :  iterate(f, 0, s(3, 4))", 17, 300, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	// Test with two invariant data args - pass-by-reference data (struct) and an int.
	testMainIntegerArg("															\n\
		struct s { int x, int y }													\n\
		def f(int current_state, int iteration, s invariant_data, int m) tuple<int, bool> :					\n\
			if iteration >= m														\n\
				[current_state, false]t # break										\n\
			else																	\n\
				[current_state + invariant_data.x, true]t											\n\
		def main(int x) int :  iterate(f, 0, s(3, 4), x)", 17, 3 * 17, ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	// Test iterate with a lambda expression
	testMainIntegerArg("																	\n\
		def main(int x) int :																\n\
			let 																			\n\
				f = \\(int current_state, int iteration) ->									\n\
					if iteration >= 100														\n\
						[current_state, false]t # break										\n\
					else																	\n\
						[current_state + 1, true]t											\n\
			in																				\n\
				iterate(f, 0)", 17, 100, INVALID_OPENCL);

	// Test iterate with a lambda expression used directly as the first arg: This one is supported for OpenCL
	testMainIntegerArg("																	\n\
		def main(int x) int :																\n\
			iterate(																		\n\
				\\(int current_state, int iteration) ->										\n\
					if iteration >= 100														\n\
						[current_state, false]t # break										\n\
					else																	\n\
						[current_state + 1, true]t											\n\
				,																			\n\
				0																			\n\
			)", 17, 100);


	// Test where the lambda captures a variable.
	testMainIntegerArg("																	\n\
		def main(int x) int :																\n\
		let																					\n\
			z = 2																			\n\
		in																					\n\
			iterate(																		\n\
				\\(int current_state, int iteration) ->										\n\
					if iteration >= 100														\n\
						[current_state, false]t # break										\n\
					else																	\n\
						[current_state + z, true]t											\n\
				,																			\n\
				0																			\n\
			)", 17, 200);

	// Test where the lambda captures a struct variable.
	testMainIntegerArg("struct S { int s }													\n\
		def main(int x) int :																\n\
		let																					\n\
			s = S(2)																		\n\
		in																					\n\
			iterate(																		\n\
				\\(int current_state, int iteration) ->										\n\
					if iteration >= 100														\n\
						[current_state, false]t # break										\n\
					else																	\n\
						[current_state + s.s, true]t										\n\
				,																			\n\
				0																			\n\
			)", 17, 200);

	// Test where the lambda captures a struct variable that is passed in as an arg.
	testMainIntegerArg("struct S { int s }													\n\
		def f(int x, S s) !noinline int :																\n\
			iterate(																		\n\
				\\(int current_state, int iteration) ->										\n\
					if iteration >= 100														\n\
						[current_state, false]t # break										\n\
					else																	\n\
						[current_state + s.s, true]t										\n\
				,																			\n\
				0																			\n\
			)																				\n\
		def main(int x) int : f(x, S(2))													\n\
		", 17, 200);
	

	testMainDoubleArg("																	\n\
		def test(vector<double, 2> p)  double  :										\n\
			let 																		\n\
				f = \\(vector<double, 2> v, int i) ->									\n\
					if i >= 10		then												\n\
						(v, false) # break												\n\
					else																\n\
						([v[0], v[1] + 1.0]v, true)										\n\
			in																			\n\
				iterate(f, p)[1]														\n\
		def main(double x) : test([x, x]v)												\n\
		", 17, 27, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE | ALLOW_SPACE_BOUND_FAILURE);

	testFloat4Struct("struct Float4Struct { vector<float, 4> v } 						\n\
		def main(Float4Struct a, Float4Struct b) Float4Struct  :						\n\
			let 																		\n\
				x = b.v[0]																\n\
			in																			\n\
				iterate(\\(Float4Struct v, int i) ->									\n\
					if i >= 10		then												\n\
						(v, false) # break												\n\
					else																\n\
						(Float4Struct([v.v[0], v.v[1] * x, v.v[2], v.v[3]]v), true)							\n\
				, a)																	\n\
		", 
		Float4Struct(1.f, 1.f, 1.f, 1.f), Float4Struct(1.f, 1.f, 1.f, 1.f), Float4Struct(1.f, 1.f, 1.f, 1.f));

	testMainDoubleArg("																	\n\
		def test(vector<double, 2> p)  double  :										\n\
			let 																		\n\
				incr = 1.0																\n\
			in																			\n\
				let																		\n\
					f = \\(vector<double, 2> v, int i) ->								\n\
						if i >= 10		then											\n\
							(v, false) # break											\n\
						else															\n\
							([v[0], v[1] + incr]v, true)								\n\
				in																		\n\
					iterate(f, p)[1]													\n\
		def main(double x) : test([x, x]v)												\n\
		", 17, 27, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE | ALLOW_SPACE_BOUND_FAILURE);

}


static void testTuples()
{
	// ===================================================================
	// Test tuples
	// ===================================================================
	// Test tuple literals being used immediately with subscript operator.
	testMainFloatArg("def main(float x) float :  [x]t[0]", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  [x + 1.0, x + 2.0]t[1]", 1.0f, 3.0f);

	// Test tuples with a mix of types, and elem() calls on each type
	testMainFloatArg("def main(float x) float :  let t = [x + 1.0, 1]t in t[0] + toFloat(t[1])", 1.0f, 3.0f);

	// Test tuples being returned from a function, with subscript operator.
	testMainFloatArg("def f(float x) tuple<float> : [x]t   \n\
		def main(float x) float :  f(x)[0]", 1.0f, 1.0f);


	// Test tuple literals being used immediately
	testMainFloatArg("def main(float x) float :  elem([x]t, 0)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  elem([x + 1.0, x + 2.0]t, 1)", 1.0f, 3.0f);

	// Test tuples being returned from a function
	testMainFloatArg("def f(float x) tuple<float> : [x]t   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);
	testMainFloatArg("def f(float x) tuple<float, float> : [x, x]t   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);
	testMainFloatArg("def f(float x) tuple<float, float> : [x, x + 1.0]t   \n\
		def main(float x) float :  elem(f(x), 1)", 1.0f, 2.0f);

	// Test tuples being passed as a function argument
	testMainFloatArg("def f(tuple<float, float> t) float : elem(t, 1)   \n\
		def main(float x) float :  f([x + 1.0, x + 2.0]t)", 1.0f, 3.0f);


	// Test tuples being passed as a function argument and returned
	testMainFloatArg("def f(tuple<float, float> t) tuple<float, float> : t   \n\
		def main(float x) float :  elem(f([x + 1.0, x + 2.0]t), 1)", 1.0f, 3.0f);

	// Test a tuple with a mixture of types
	testMainFloatArg("def f(float x) tuple<float, int> : [x, 2]t   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);

	testMainFloatArg("def f(float x) tuple<float, int, bool> : [x, 2, true]t   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);

	// Test nested tuples
	testMainFloatArg("def f(float x) tuple<tuple<float, float>, tuple<float, float> > : [[x, x + 1.0]t, [x + 2.0, x + 3.0]t]t   \n\
		def main(float x) float :  elem(elem(f(x), 1), 0)", 1.0f, 3.0f);

	// Test a structure in a tuple
	testMainFloatArg("struct S { float a, int b }		\n\
		def f(float x) tuple<S, float> : [S(x + 2.0, 1), x]t   \n\
		def main(float x) float :  elem(f(x), 0).a", 1.0f, 3.0f);

	// Test a tuple in a stucture
	testMainFloatArg("struct S { tuple<float, float> a, int b }		\n\
		def f(float x) S : S([x + 2.0, x]t, 1)   \n\
		def main(float x) float :  elem(f(x).a, 0)", 1.0f, 3.0f);


	// Test empty tumple - not allowed.
	testMainFloatArgInvalidProgram("def f(float x) tuple<> : []t   \n\
		def main(float x) float :  elem(f(x), 0)");

	// Test tuple index out of bounds
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : [x, x]t   \n\
		def main(float x) float :  elem(f(x), -1)");
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : [x, x]t   \n\
		def main(float x) float :  elem(f(x), 2)");

	// Test varying index (invalid)
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : [x, x]t   \n\
		def main(float x) float :  elem(f(x), truncateToInt(x))");

	// ===================================================================
	// Test tuples with new parenthesis syntax, e.g. (1, 2, 3)
	// ===================================================================
	// Test tuple literals being used immediately with subscript operator.
	testMainFloatArg("def main(float x) float :  [x]t[0]", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  (x + 1.0, x + 2.0)[1]", 1.0f, 3.0f);

	// Test tuples with a mix of types, and elem() calls on each type
	testMainFloatArg("def main(float x) float :  let t = (x + 1.0, 1) in t[0] + toFloat(t[1])", 1.0f, 3.0f);

	// Test tuples being returned from a function, with subscript operator.
	testMainFloatArg("def f(float x) tuple<float> : [x]t   \n\
		def main(float x) float :  f(x)[0]", 1.0f, 1.0f);


	// Test tuple literals being used immediately
	testMainFloatArg("def main(float x) float :  elem([x]t, 0)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  elem((x + 1.0, x + 2.0), 1)", 1.0f, 3.0f);

	// Test tuples being returned from a function
	testMainFloatArg("def f(float x) tuple<float> : [x]t   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);
	testMainFloatArg("def f(float x) tuple<float, float> : (x, x)   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);
	testMainFloatArg("def f(float x) tuple<float, float> : (x, x + 1.0)   \n\
		def main(float x) float :  elem(f(x), 1)", 1.0f, 2.0f);

	// Test tuples being passed as a function argument
	testMainFloatArg("def f(tuple<float, float> t) float : elem(t, 1)   \n\
		def main(float x) float :  f((x + 1.0, x + 2.0))", 1.0f, 3.0f);

	// Test tuples being passed as a function argument and returned
	testMainFloatArg("def f(tuple<float, float> t) tuple<float, float> : t   \n\
		def main(float x) float :  elem(f((x + 1.0, x + 2.0)), 1)", 1.0f, 3.0f);

	// Test a tuple with a mixture of types
	testMainFloatArg("def f(float x) tuple<float, int> : (x, 2)   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);

	testMainFloatArg("def f(float x) tuple<float, int, bool> : (x, 2, true)   \n\
		def main(float x) float :  elem(f(x), 0)", 1.0f, 1.0f);

	// Test nested tuples
	testMainFloatArg("def f(float x) tuple<tuple<float, float>, tuple<float, float> > : ((x, x + 1.0), (x + 2.0, x + 3.0))   \n\
		def main(float x) float :  elem(elem(f(x), 1), 0)", 1.0f, 3.0f);

	// Test a structure in a tuple
	testMainFloatArg("struct S { float a, int b }		\n\
		def f(float x) tuple<S, float> : (S(x + 2.0, 1), x)   \n\
		def main(float x) float :  elem(f(x), 0).a", 1.0f, 3.0f);

	// Test a tuple in a stucture
	testMainFloatArg("struct S { tuple<float, float> a, int b }		\n\
		def f(float x) S : S((x + 2.0, x), 1)   \n\
		def main(float x) float :  elem(f(x).a, 0)", 1.0f, 3.0f);


	// Test empty tumple - not allowed.
	testMainFloatArgInvalidProgram("def f(float x) tuple<> : ()   \n\
		def main(float x) float :  elem(f(x), 0)");

	// Test tuple index out of bounds
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : (x, x)   \n\
		def main(float x) float :  elem(f(x), -1)");
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : (x, x)   \n\
		def main(float x) float :  elem(f(x), 2)");

	// Test varying index (invalid)
	testMainFloatArgInvalidProgram("def f(float x) tuple<float, float> : (x, x)   \n\
		def main(float x) float :  elem(f(x), truncateToInt(x))");

	// ===================================================================
	// Test comparison operators == and !=
	// ===================================================================
	
	// Test ==
	testMainFloat("def eq(tuple<float, float> a, tuple<float, float> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]t, [1.0, 2.0]t) ? 1.0 : 0.0",
				  1.0f);

	testMainFloat("def eq(tuple<float, float> a, tuple<float, float> b) !noinline bool : a == b     \n\
				  def main() float : eq([1.0, 2.0]t, [1.0, 3.0]t) ? 1.0 : 0.0",
				  0.0f);

	// Test == without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : [1.0, 2.0]t == [1.0, 2.0]t ? 1.0 : 0.0", 1.0f);
	testMainFloat("def main() float : [1.0, 2.0]t == [1.0, 3.0]t ? 1.0 : 0.0", 0.0f);
	
	// Test !=
	testMainFloat("def neq(tuple<float, float> a, tuple<float, float> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]t, [1.0, 3.0]t) ? 1.0 : 0.0",
				  1.0f);
				  
	testMainFloat("def neq(tuple<float, float> a, tuple<float, float> b) !noinline bool : a != b     \n\
				  def main() float : neq([1.0, 2.0]t, [1.0, 2.0]t) ? 1.0 : 0.0",
				  0.0f);

	// Test != without !noline to test winter interpreted execution/constant folding.
	testMainFloat("def main() float : [1.0, 2.0]t != [1.0, 3.0]t ? 1.0 : 0.0", 1.0f);
	testMainFloat("def main() float : [1.0, 2.0]t != [1.0, 2.0]t ? 1.0 : 0.0", 0.0f);

	
	// Test with a tuple in a tuple
	testMainFloat("def eq(tuple<tuple<int>, tuple<int> > a, tuple<tuple<int>, tuple<int> > b) !noinline bool : a == b     \n\
				  def main() float : eq([[1]t, [2]t]t, [[1]t, [2]t]t) ? 1.0 : 0.0",
				  1.0f);

	testMainFloat("def eq(tuple<tuple<int>, tuple<int> > a, tuple<tuple<int>, tuple<int> > b) !noinline bool : a == b     \n\
				  def main() float : eq([[1]t, [2]t]t, [[1]t, [3]t]t) ? 1.0 : 0.0",
				  0.0f);

	// Test with a string in a tuple
	testMainFloat("def eq(tuple<string> a, tuple<string> b) !noinline bool : a == b     \n\
				  def main() float : eq([\"a\"]t, [\"a\"]t) ? 1.0 : 0.0",
				  1.0f);

	testMainFloat("def eq(tuple<string> a, tuple<string> b) !noinline bool : a == b     \n\
				  def main() float : eq([\"a\"]t, [\"b\"]t) ? 1.0 : 0.0",
				  0.0f);
}


static void testTypeCoercion()
{
	// Test int->float type coercion in various ways	

	// Test int->float coercion in an if statement as required for an argument to another function. (truncateToInt in this case).
	// NOTE: this seems to be too hard to do.
	
	// testMainIntegerArg("def main(int x) int : truncateToInt(if(true, 3, 4))", 2, 3);

	// Test int->float coercion in an if statement as required for an argument to another function. ('if' in this case).
	// testMainFloatArg("def main(float x) float : if(false, if(false, 1, 2), 3)", 2.0f, 3.0f);


	// Test int->float coercion for an argument to a built-in function like sqrt
	testMainFloatArg("def main(float x) float : sqrt(4)", 2.0f, 2.0f);
	testMainFloatArg("def main(float x) float : cos(4)", 2.0f, std::cos(4.f));
	testMainFloatArg("def main(float x) float : pow(2, 3)", 2.0f, 8.f);
	testMainFloatArg("def main(float x) float : pow(x, 3)", 2.0f, 8.f);
	testMainFloatArg("def main(float x) float : pow(2, x)", 2.0f, 4.f);

	// Test double->float coercion for an argument to a built-in function like sqrt
	testMainDoubleArg("def main(double x) double : sqrt(4)",   2.0, 2.0);
	testMainDoubleArg("def main(double x) double : pow(2, 3)", 2.0, 8.0);
	testMainDoubleArg("def main(double x) double : pow(x, 3)", 2.0, 8.0);
	testMainDoubleArg("def main(double x) double : pow(2, x)", 2.0, 4.0);

	// Test int->float coercion for an argument to a function
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(3)", 2.0f, 9.f);
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(x + 1)", 2.0f, 9.f);
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(1 + x)", 2.0f, 9.f);
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(2 * x)", 2.0f, 16.f);
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(x * 2)", 2.0f, 16.f);
	testMainFloatArg("def f(float x) float : x*x                   def main(float x) float : f(1 / x)", 2.0f, 0.25f);
	testMainFloatArg("def f(float x) float : x + 2.0               def main(float x) float : f(1)", 2.0f, 3.f);
	testMainFloatArg("def f(float x, float y) float : x + y        def main(float x) float : f(1, x)", 2.0f, 3.f);
	testMainFloatArg("def f(float x, float y) float : x + y        def main(float x) float : f(x, 1)", 2.0f, 3.f);

	testMainDoubleArg("def f(double x) double : x*x                   def main(double x) double : f(3)", 2.0f, 9.f);
	testMainDoubleArg("def f(double x) double : x*x                   def main(double x) double : f(x + 1)", 2.0f, 9.f);

	testMainFloatArg("def main(float x) float : sqrt(2 + x)", 2.0f, 2.0f);

	// Test int->float coercion in an if statement as the function body.
	testMainFloatArg("def main(float x) float : if(true, 3, 4)", 2.0f, 3.0f);
	testMainFloatArg("def main(float x) float : if(x < 10.0, 3, 4)", 2.0f, 3.0f);

	testMainDoubleArg("def main(double x) double : if(true, 3, 4)", 2.0f, 3.0f);
	testMainDoubleArg("def main(double x) double : if(x < 10.0, 3, 4)", 2.0f, 3.0f);
	
	testMainFloatArg("def main(float x) float : 3", 2.0f, 3.0f);
	testMainDoubleArg("def main(double x) double : 3", 2.0f, 3.0f);

	testMainFloatArg("def main(float x) float : x + 1", 2.0f, 3.0f);
	testMainFloatArg("def main(float x) float : 1 + x", 2.0f, 3.0f);
	testMainFloatArg("def main(float x) float : 2 * (x + 1)", 2.0f, 6.0f);
	testMainFloatArg("def main(float x) float : 2 * (1 + x)", 2.0f, 6.0f);

	testMainDoubleArg("def main(double x) double : x + 1", 2.0f, 3.0f);
	testMainDoubleArg("def main(double x) double : 1 + x", 2.0f, 3.0f);
	testMainDoubleArg("def main(double x) double : 2 * (x + 1)", 2.0f, 6.0f);
	testMainDoubleArg("def main(double x) double : 2 * (1 + x)", 2.0f, 6.0f);

	testMainFloatArg("def main(float x) float : (1 + x) + (2 + x) * 3", 2.0f, 15.0f);

	// Test type checking for if() statements:
	testMainFloatArgInvalidProgram("def main(float x) float : if(x < 1.0, 3.0, true)");
	testMainFloatArgInvalidProgram("def main(float x) float : if(x < 1.0, true, 3.0)");
	testMainFloatArgInvalidProgram("def main(float x) float : if(3.0, 2.0, 3.0)");

	// Test wrong number of args to if
	testMainFloatArgInvalidProgram("def main(float x) float : if(x < 1.0)");
	testMainFloatArgInvalidProgram("def main(float x) float : if(x < 1.0, 2.0)");
	testMainFloatArgInvalidProgram("def main(float x) float : if(x < 1.0, 2.0, 3.0, 4.0)");

	// Test int->float type coercion for addition operands changing return type of tuple elem()
	testMainFloatArg("def main(float x) float :  elem([0  + 2.0]t, 0)", 1.0f, 2.0f);
	testMainFloatArg("def main(float x) float :  elem([x + 1.0, x, 0  + 2.0]t, 1)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  elem([x + 1.0, x, 0  - 2.0]t, 1)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  elem([x + 1.0, x, 0  * 2.0]t, 1)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float :  elem([x + 1.0, x, 0  / 2.0]t, 1)", 1.0f, 1.0f);

	// Test function binding based on int->float type coercion
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1 + 2.0)", 1.0f, 20.0f); // 1 op 2.0 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1 - 2.0)", 1.0f, 20.0f); // 1 op 2.0 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1 * 2.0)", 1.0f, 20.0f); // 1 op 2.0 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1 / 2.0)", 1.0f, 20.0f); // 1 op 2.0 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1.0 + 2)", 1.0f, 20.0f); // 1.0 op 2 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1.0 - 2)", 1.0f, 20.0f); // 1.0 op 2 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1.0 * 2)", 1.0f, 20.0f); // 1.0 op 2 should be coerced to float
	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1.0 / 2)", 1.0f, 20.0f); // 1.0 op 2 should be coerced to float

	testMainFloatArg("def f(int i) float : 10.0     def f(float x) float : 20.0     def main(float x) float : f(1 + x)", 1.0f, 20.0f); // 1 op x should be coerced to float


	
	// ===================================================================
	// Test implicit type conversions from int to float
	// ===================================================================

	// For numeric literals
	testMainFloat("def main() float : 3.0 + 4", 7.0);
	testMainFloat("def main() float : 3 + 4.0", 7.0);

	testMainFloat("def main() float : 3.0 - 4", -1.0);
	testMainFloat("def main() float : 3 - 4.0", -1.0);

	testMainFloat("def main() float : 3.0 * 4", 12.0);
	testMainFloat("def main() float : 3 * 4.0", 12.0);

	testMainFloat("def main() float : 12.0 / 4", 3.0);
	testMainFloat("def main() float : 12 / 4.0", 3.0);
	
	// Test with nodes that can't be constant-folded.
	// Addition
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) + 3", 2.0f, 7.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 3 + f(x)", 2.0f, 7.0f);

	// Subtraction
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) - 3", 2.0f, 1.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 3 - f(x)", 2.0f, -1.0f);

	// Multiplication
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) * 3", 2.0f, 12.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 3 * f(x)", 2.0f, 12.0f);

	// Division
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) / 3", 3.0f, 3.0f);
	// Can't be done as f(x) might be zero:
	// testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 3 / f(x)", 2.0f, 12.0f);

	// NOTE: this one can't convert because f(x) + 3 might be zero.
	//testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 14 / (f(x) + 3)", 2.0f, 2.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 14 / (f(2) + 3)", 2.0f, 2.0f);
	testMainFloatArg("def f(int x) int : x*x	      def main(float x) float : 14 / (f(2) + 3)", 2.0f, 2.0f);
	testMainFloatArg("def f<T>(T x) T : x*x           def main(float x) float : 14 / (f(2) + 3)", 2.0, 2.0f);

	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) * 2 + 1", 2.0f, 9.0f);
	
	testMainFloatArg("def main(float x) float : ( x + x) * (-0.4)", 2.0f, -1.6f);

	testMainFloatArg("def main(float x) float : (x + x) - 1.5", 2.0f, 2.5f);
	//testMainFloatArg("def main(float x) float : (x + x) -1.5", 2.0f, 2.0); // NOTE: this works in C++. (e.g. the -1.5 is interpreted as a binary subtraction)
	
	// Division
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) / 3", 3.0f, 3.0f);


	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) * (1.0 / 3.0)", 3.0f, 3.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) * (1 / 3.0)", 3.0f, 3.0f);
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : f(x) * (1.0 / 3)", 3.0f, 3.0f);

	// Division
	testMainFloatArg("def f(float x) float : x*x      def main(float x) float : 1.0 / 3.0", 3.0f, 1.0f / 3.0f);



	// Test promotion to match function return type:
	testMainFloat("def main() float : 3", 3.0);

	testMainFloat("def main() float : 1.0 + (2 + 3)", 6.0);
	
	testMainFloat("def main() float : 1.0 + 2 + 3", 6.0);
	
	// Test implicit conversion from int to float in addition operation with a function call
	//testMainFloat("def f(int x) : x*x    def main() float : 1.0 + f(2)", 5.0f);

	testMainFloat("def main() float : (1.0 + 2.0) + (3 + 4)", 10.0);
	testMainFloat("def main() float : (1.0 + 2) + (3 + 4)", 10.0);
}


static void testElem()
{
	// Test integer in-bounds runtime index access to array
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]a, i) else 0", 
		2, 3);


	// Test integer in-bounds runtime index access to vector
	/*	
	TEMP NOT SUPPORTED IN OPENCL YET
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]v, i) else 0", 
		2, 3);
		*/
	// ===================================================================
	// Test array access with elem()
	// ===================================================================
	// Test integer in-bounds constant index access to array
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]a					\n\
			in										\n\
				elem(a, 1)							",
		1, 2);

	// Test integer out-of-bounds constant index access to array
	testMainIntegerArgInvalidProgram(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]a					\n\
			in										\n\
				elem(a, -1)							"
	);

	// Test integer in-bounds runtime index access to array  (let clause)
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]a					\n\
			in										\n\
				if inBounds(a, i)					\n\
					elem(a, i)						\n\
				else								\n\
					0								",
		1, 2);

	// Test integer in-bounds runtime index access to array (function arg var)
	testMainIntegerArg(
		"def f(array<int, 4> a, int i) int :		\n\
			if inBounds(a, i)						\n\
				elem(a, i)							\n\
			else									\n\
				0									\n\
		def main(int i) int :						\n\
			f([1, 2, 3, 4]a, i)",
		1, 2);

	// Test integer out-of-bounds runtime index access to array
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]a					\n\
			in										\n\
				if inBounds(a, i)					\n\
					elem(a, i)						\n\
				else								\n\
					0								",
		-1, 0);

	// Test integer out-of-bounds runtime index access to array (function arg var)
	testMainIntegerArg(
		"def f(array<int, 4> a, int i) int :		\n\
			if inBounds(a, i)						\n\
				elem(a, i)							\n\
			else									\n\
				0									\n\
		def main(int i) int :						\n\
			f([1, 2, 3, 4]a, i)",
		-1, 0);


	// ===================================================================
	// Test vector access with elem()
	// ===================================================================
	// Test integer in-bounds constant index access to vector
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]v					\n\
			in										\n\
				elem(a, 1)							",
		1, 2);

	// Test integer out-of-bounds constant index access to vector
	testMainIntegerArgInvalidProgram(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]v					\n\
			in										\n\
				elem(a, -1)							"
		);

/*	
	TEMP NOT SUPPORTED IN OPENCL YET
	// Test integer in-bounds runtime index access to vector
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]v					\n\
			in										\n\
				if inBounds(a, i)					\n\
					elem(a, i)						\n\
				else								\n\
					0								",
		1, 2);
		
	// Test integer out-of-bounds runtime index access to vector
	testMainIntegerArg(
		"def main(int i) int :						\n\
			let										\n\
				a = [1, 2, 3, 4]v					\n\
			in										\n\
				if inBounds(a, i)					\n\
					elem(a, i)						\n\
				else								\n\
					0								",
		-1, 0);
*/
	/*testMainIntegerArg(
		"def main(int i) int :						\n\
			match x = elem([1, 2, 3, 4]a, i)		\n\
				int: x								\n\
				error: 0							",
		1, 2);*/

	// Test integer out-of-bounds runtime index access
	/*testMainIntegerArg(
		"def main(int i) int :						\n\
			match x = elem([1, 2, 3, 4]a, i)		\n\
				int: x								\n\
				error: 0							",
		100, 0);*/
}


static void testIfThenElse()
{
	// ===================================================================
	// Test if-then-else
	// ===================================================================
	testMainIntegerArg("def main(int x) int : if x < 5 then 10 else 5 ", 2, 10);
	testMainIntegerArg("def main(int x) int : if x < 5 then 10 else 5 ", 6, 5);

	testMainIntegerArg("def main(int x) int : if (x < 5) then 10 else 5 ", 6, 5);
	testMainIntegerArg("def main(int x) int : if (x * 2) < 5 then 10 else 5 ", 6, 5);

	// Test without optional 'then'
	testMainIntegerArg("def main(int x) int : if x < 5 10 else 5 ", 2, 10);
	testMainIntegerArg("def main(int x) int : if x < 5 10 else 5 ", 6, 5);
	testMainIntegerArg("def main(int x) int : if (x < 5) 10 else 5 ", 6, 5);
	testMainIntegerArg("def main(int x) int : if (x * 2) < 5 10 else 5 ", 6, 5);

	// Test nested if-then-else
	testMainIntegerArg("def main(int x) int : if x < 5 then if x < 2 then 1 else 2 else 5 ", 1, 1);
	testMainIntegerArg("def main(int x) int : if x < 5 then if x < 2 then 1 else 2 else 5 ", 2, 2);
	testMainIntegerArg("def main(int x) int : if x < 5 then if x < 2 then 1 else 2 else 5 ", 10, 5);
	testMainIntegerArg("def main(int x) int : if (x < 5) then if x < 2 then 1 else 2 else 5 ", 10, 5); // Test with parens
	testMainIntegerArg("def main(int x) int : if x < 5 then if (x < 2) then 1 else 2 else 5 ", 10, 5); // Test with parens

	// Test nested if-then-else without the 'then'
	testMainIntegerArg("def main(int x) int : if x < 5 if x < 2 1 else 2 else 5 ", 1, 1);
	testMainIntegerArg("def main(int x) int : if x < 5 if x < 2 1 else 2 else 5 ", 2, 2);
	testMainIntegerArg("def main(int x) int : if x < 5 if x < 2 1 else 2 else 5 ", 10, 5);

	testMainIntegerArg("def main(int x) int :			\n\
								if x < 5				\n\
									if x < 3			\n\
										1				\n\
									else				\n\
										2				\n\
								else					\n\
									if x < 7			\n\
										6				\n\
									else				\n\
										7				\n\
		", 10, 7);

	// Test nested if-then-else with parentheses
	testMainIntegerArg("def main(int x) int : if x < 5 then (if x < 2 then 1 else 2) else 5 ", 1, 1);
	testMainIntegerArg("def main(int x) int : if x < 5 then (if x < 2 then 1 else 2) else 5 ", 2, 2);
	testMainIntegerArg("def main(int x) int : if x < 5 then (if x < 2 then 1 else 2) else 5 ", 10, 5);



	// Test if expression with a structure
	testMainIntegerArg("struct S { int a }     def main(int x) int : (if x < 4 then S(x + 1) else S(x + 2)).a ", 10, 12);

	// Test if expression with a structure passed as an argument
	testMainIntegerArg("struct S { int a }     def f(int x, S a, S b) S : if x < 4 then a else b      def main(int x) int : f(x, S(x + 1), S(x + 2)).a ", 10, 12);
}


static void stringTests()
{
	// String test - string literal in let statement, with assignment.
	testMainInt64Arg(
		"def main(int64 x) int64 : length(concatStrings(\"hello\", \"world\"))",
		2, 10, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	// Double return of a string
	testMainInt64Arg(
		"def f() string : \"hello world\"	\n\
		def g() string : f()				\n\
		def main(int64 x) int64 : length(g())",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);
	

	

	// String test - string literal in let statement, with assignment.
	testMainInt64Arg(
		"def main(int64 x) int64 :			\n\
			let								\n\
				s = \"hello world\"			\n\
				s2 = s						\n\
			in								\n\
				length(s) + length(s2)",
		2, 22, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// String test - string literal in let statement, with assignment.
	testMainInt64Arg(
		"def main(int64 x) int64 :			\n\
			let								\n\
				s = \"hello world\"			\n\
				s2 = \"hallo thar\"			\n\
			in								\n\
				length(s) + length(s2)",
		2, 21, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// a function returning a string
	testMainInt64Arg(
		"def f() string : \"hello world\"	\n\
		def main(int64 x) int64 : length(f())",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// a function returning a string in a let block
	testMainInt64Arg(
		"def f() string : \"hello world\"	\n\
		def main(int64 x) int64 :			\n\
			let								\n\
				s = f()						\n\
			in								\n\
				length(s)",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// String test - string literal in let statement
	testMainInt64Arg(
		"def main(int64 x) int64 :			\n\
			let								\n\
				s = \"hello world\"			\n\
			in								\n\
				length(s)",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// String test - string literal as argument
	testMainInt64Arg(
		"def main(int64 x) int64 :			\n\
				length(\"hello world\")",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// String test - string literal in let statement, with assignment.
	testMainInt64Arg(
		"def main(int64 x) int64 :			\n\
			let								\n\
				s = \"hello world\"			\n\
				s2 = s						\n\
			in								\n\
				length(s2)",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// Char test
	testMainIntegerArg(
		"def main(int x) int :				\n\
			let								\n\
				c = 'a'						\n\
			in								\n\
				10",
		2, 10);


	// ===================================================================
	// Test comparisons
	// ===================================================================

	// Test ==
	testMainIntegerArg("def f(string a, string b) !noinline bool : a == b     \n\
		def main(int x) int : f(\"hello\", \"world\") ? 1 : 0",
		0, /*target=*/0, INVALID_OPENCL);

	testMainIntegerArg("def f(string a, string b) !noinline bool : a == b     \n\
		def main(int x) int : f(\"hello\", \"hello\") ? 1 : 0",
		0, /*target=*/1, INVALID_OPENCL);

	// Test == with empty string
	testMainIntegerArg("def f(string a, string b) !noinline bool : a == b     \n\
		def main(int x) int : f(\"\", \"\") ? 1 : 0",
		0, /*target=*/1, INVALID_OPENCL);

	// Test !=
	testMainIntegerArg("def f(string a, string b) !noinline bool : a != b     \n\
		def main(int x) int : f(\"hello\", \"hello\") ? 1 : 0",
		0, /*target=*/0, INVALID_OPENCL);

	testMainIntegerArg("def f(string a, string b) !noinline bool : a != b     \n\
		def main(int x) int : f(\"hello\", \"world\") ? 1 : 0",
		0, /*target=*/1, INVALID_OPENCL);

	// Test != with empty string
	testMainIntegerArg("def f(string a, string b) !noinline bool : a != b     \n\
		def main(int x) int : f(\"\", \"\") ? 1 : 0",
		0, /*target=*/0, INVALID_OPENCL);


	// Test == without noinline function, to test interpretation
	testMainIntegerArg("def main(int x) int : \"hello\" == \"world\" ? 1 : 0",
		0, /*target=*/0, INVALID_OPENCL);

	testMainIntegerArg("def main(int x) int : \"hello\" == \"hello\" ? 1 : 0",
		0, /*target=*/1, INVALID_OPENCL);

	// Test != without noinline function, to test interpretation
	testMainIntegerArg("def main(int x) int : \"hello\" != \"world\" ? 1 : 0",
		0, /*target=*/1, INVALID_OPENCL);

	testMainIntegerArg("def main(int x) int : \"hello\" != \"hello\" ? 1 : 0",
		0, /*target=*/0, INVALID_OPENCL);
}


static void testMathsFunctions()
{
	// Test sin and cos together, which may generate a call to ___sincosf_stret. (On Mac)
	testMainFloatArg("def main(float x) float : sin(x) + cos(x)", 9.0f, sin(9.0f) + cos(9.0f));
	testMainDoubleArg("def main(double x) double : sin(x) + cos(x)", 9.0, sin(9.0) + cos(9.0));

	// abs
	testMainFloatArg("def main(float x) float : abs(x)", 9.0f, 9.0f);
	testMainFloatArg("def main(float x) float : abs(x)", -9.0f, 9.0f);

	testMainDoubleArg("def main(double x) double : abs(x)", 9.0, 9.0);
	testMainDoubleArg("def main(double x) double : abs(x)", -9.0, 9.0);


	// Test abs on vector
	{
		Float4Struct a(1.0f, -2.0f, 3.0f, -4.0f);
		Float4Struct target_result(1.0f, 2.0f, 3.0f, 4.0f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def abs(Float4Struct f) : Float4Struct(abs(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				abs(a)", 
			a, a, target_result
		);
	}

	// TODO: abs for integers


	//testMainFloatArg("def main(float x) float : someFuncBleh(x)", 9.0f, 10.0f);

	// truncateToInt
	//testMainIntegerArg("def main(int x) int : truncateToInt(toFloat(x) + 0.2)", 3, 3);
	//testMainIntegerArg("def main(int x) int : truncateToInt(toFloat(x) + 0.2)", -3, -2);

	// Test truncateToInt on vector
	/*	
	TODO: FIXME: needs truncateToInt in bounds proof.
	{
		Float4Struct a(-2.2f, -1.2f, 0.2f, 1.2f);
		Float4Struct target_result(-2.f, -1.f, 0.f, 1.f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def sqrt(Float4Struct f) : Float4Struct(sqrt(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				let													\n\
					vec_int = truncateToInt(a.v)					\n\
				in													\n\
					Float4Struct(toFloat(vec_int))",
			a, a, target_result
		);
	}
	*/

	// sqrt
	testMainFloatArg("def main(float x) float : sqrt(x)", 9.0f, std::sqrt(9.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : sqrt(9)", 9.0f, std::sqrt(9.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : sqrt(9.0)", 9.0f, std::sqrt(9.0f));

	testMainDoubleArg("def main(double x) double : sqrt(x)", 9.0, 3.0);

	// Test sqrt on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct target_result(std::sqrt(1.0f), std::sqrt(2.0f), std::sqrt(3.0f), std::sqrt(4.0f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def sqrt(Float4Struct f) : Float4Struct(sqrt(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				sqrt(a)", 
			a, a, target_result
		);
	}

	// pow
	testMainFloatArg("def main(float x) float : pow(2.4, x)", 3.0f, std::pow(2.4f, 3.0f));
	testMainFloatArg("def main(float x) float : pow(2.0, x)", 3.0f, std::pow(2.0f, 3.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : pow(2.0, 3.0)", 3.0f, std::pow(2.0f, 3.0f));

	testMainDoubleArg("def main(double x) double : pow(2.0, x)", 3.0, std::pow(2.0, 3.0));

	// Test pow on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct b(1.4f, 2.4f, 3.4f, 4.4f);
		Float4Struct target_result(std::pow(1.0f, 1.4f), std::pow(2.0f, 2.4f), std::pow(3.0f, 3.4f), std::pow(4.0f, 4.4f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def pow(Float4Struct a, Float4Struct b) : Float4Struct(pow(a.v, b.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				pow(a, b)", 
			a, b, target_result
		);
	}

	// sin
	testMainFloatArg("def main(float x) float : sin(x)", 1.0f, std::sin(1.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : sin(1.0)", 1.0f, std::sin(1.0f));

	testMainDoubleArg("def main(double x) double : sin(x)", 1.0, std::sin(1.0));

	// Test sin on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct target_result(std::sin(1.0f), std::sin(2.0f), std::sin(3.0f), std::sin(4.0f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def sin(Float4Struct f) : Float4Struct(sin(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				sin(a)", 
			a, a, target_result
		);
	}

	// exp
	testMainFloatArg("def main(float x) float : exp(x)", 3.0f, std::exp(3.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : exp(3.0)", 3.0f, std::exp(3.0f));

	testMainDoubleArg("def main(double x) double : exp(x)", 3.0, std::exp(3.0));

	// Test exp on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct target_result(std::exp(1.0f), std::exp(2.0f), std::exp(3.0f), std::exp(4.0f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def exp(Float4Struct f) : Float4Struct(exp(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				exp(a)", 
			a, a, target_result
		);
	}

	// log
	testMainFloatArg("def main(float x) float : log(x)", 3.0f, std::log(3.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : log(3.0)", 3.0f, std::log(3.0f));

	testMainDoubleArg("def main(double x) double : log(x)", 3.0, std::log(3.0));

	// Test exp on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct target_result(std::log(1.0f), std::log(2.0f), std::log(3.0f), std::log(4.0f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def log(Float4Struct f) : Float4Struct(log(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				log(a)", 
			a, a, target_result
		);
	}

	// cos
	testMainFloatArg("def main(float x) float : cos(x)", 1.0f, std::cos(1.0f));
	testMainFloatArgCheckConstantFolded("def main(float x) float : cos(1.0)", 1.0f, std::cos(1.0f));

	testMainDoubleArg("def main(double x) double : cos(x)", 1.0, std::cos(1.0));

	// Test cos on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct target_result(std::cos(1.0f), std::cos(2.0f), std::cos(3.0f), std::cos(4.0f));
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def cos(Float4Struct f) : Float4Struct(cos(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				cos(a)", 
			a, a, target_result
		);
	}



	// Test floor
	testMainFloatArg("def main(float x) float : floor(x)", 2.3f, 2.0f);
	testMainFloatArg("def main(float x) float : floor(x)", -2.3f, -3.0f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : floor(-2.3)", -2.3f, -3.0f);

	testMainDoubleArg("def main(double x) double : floor(x)", 2.3, 2.0);
	testMainDoubleArg("def main(double x) double : floor(x)", -2.3, -3.0);

	// Test floor on vector
	{
		Float4Struct a(-1.2f, -0.2f, 0.2f, 1.2f);
		Float4Struct target_result(-2, -1, 0, 1);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def floor(Float4Struct f) : Float4Struct(floor(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				floor(a)", 
			a, a, target_result
		);
	}

	// Test ceil
	testMainFloatArg("def main(float x) float : ceil(x)", 2.3f, 3.0f);
	testMainFloatArg("def main(float x) float : ceil(x)", -2.3f, -2.0f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : ceil(-2.3)", -2.3f, -2.0f);

	testMainDoubleArg("def main(double x) double : ceil(x)", 2.3, 3.0);
	testMainDoubleArg("def main(double x) double : ceil(x)", -2.3, -2.0);

	// Test ceil on vector
	{
		Float4Struct a(-1.2f, -0.2f, 0.2f, 1.2f);
		Float4Struct target_result(-1, 0, 1, 2);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def ceil(Float4Struct f) : Float4Struct(ceil(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				ceil(a)", 
			a, a, target_result
		);
	}

	// _frem_ (same as libc fmod)
	testMainFloatArg("def main(float x) float : _frem_(5.1, x)", 3.0f, std::fmod(5.1f, 3.0f));
	testMainFloatArg("def main(float x) float : _frem_(-5.1, x)", 3.0f, std::fmod(-5.1f, 3.0f));
	testMainFloatArg("def main(float x) float : _frem_(5.1, x)", -3.0f, std::fmod(5.1f, -3.0f));
	testMainFloatArg("def main(float x) float : _frem_(-5.1, x)", -3.0f, std::fmod(-5.1f, -3.0f));

	testMainFloatArgCheckConstantFolded("def main(float x) float : _frem_(5.1, 3.0)", 3.0f, std::fmod(5.1f, 3.0f));

	testMainDoubleArg("def main(double x) double : _frem_(5.1, x)", 3.0, std::fmod(5.1, 3.0));

	// Test _frem_ on vector
	{
		Float4Struct a(1.0f, 2.0f, 3.0f, 4.0f);
		Float4Struct b(1.4f, 2.4f, 3.4f, 4.4f);
		Float4Struct target_result(std::fmod(1.0f, 1.4f), std::fmod(2.0f, 2.4f), std::fmod(3.0f, 3.4f), std::fmod(4.0f, 4.4f));

		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def _frem_(Float4Struct a, Float4Struct b) : Float4Struct(_frem_(a.v, b.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				_frem_(a, b)",
			a, b, target_result
		);
	}

	// Test floatNaN
	{
		testMainFloatArg("def main(float x) float : isNaN((x != 0.f) ? floatNaN() : x) ? 1.0f : 2.0f",
			3.0f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);

		// Test with DISABLE_CONSTANT_FOLDING, otherwise the call to floatNaN() will be removed.
		testMainFloatArg("def main(float x) float : isNaN((x != 0.f) ? floatNaN() : x) ? 1.0f : 2.0f",
			3.0f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS | DISABLE_CONSTANT_FOLDING);

		testMainFloatArg("def f(float x) !noinline float : (x != 0.f) ? floatNaN() : x           \n"
			"def main(float x) float : isNaN(f(x)) ? 1.0f : 2.0f",
			3.0f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	}

	// Test doubleNaN
	{
		testMainDoubleArg("def main(double x) double : isNaN((x != 0.0) ? doubleNaN() : x) ? 1.0 : 2.0",
			3.0, 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS);

		testMainDoubleArg("def main(double x) double : isNaN((x != 0.0) ? doubleNaN() : x) ? 1.0 : 2.0",
			3.0, 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS | DISABLE_CONSTANT_FOLDING);
	}

	// Test realNaN
	{
		testMainFloatArg("def main(real x) real : isNaN((x != 0.0) ? realNaN() : x) ? 1.0 : 2.0",
			3.0f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);

		testMainFloatArg("def main(real x) real : isNaN((x != 0.0) ? realNaN() : x) ? 1.0 : 2.0",
			3.0f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS | DISABLE_CONSTANT_FOLDING);

		testMainDoubleArg("def main(real x) real : isNaN((x != 0.0) ? realNaN() : x) ? 1.0 : 2.0",
			3.0, 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS);

		testMainDoubleArg("def main(real x) real : isNaN((x != 0.0) ? realNaN() : x) ? 1.0 : 2.0",
			3.0, 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS | DISABLE_CONSTANT_FOLDING);
	}
}


static void testExternalMathsFunctions()
{
	// tan
	testMainFloatArg("def main(float x) float : tan(x)", 0.3f, std::tan(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : tan(x)", 0.3, std::tan(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);

	testMainFloatArg("def main(float x) float : tan(0.5f)", 0.3f, std::tan(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);

	// asin
	testMainFloatArg("def main(float x) float : asin(x)", 0.3f, std::asin(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : asin(x)", 0.3, std::asin(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	
	// acos
	testMainFloatArg("def main(float x) float : acos(x)", 0.3f, std::acos(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : acos(x)", 0.3, std::acos(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	
	// atan
	testMainFloatArg("def main(float x) float : atan(x)", 0.3f, std::atan(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : atan(x)", 0.3, std::atan(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);

	// sinh
	testMainFloatArg("def main(float x) float : sinh(x)", 0.3f, std::sinh(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : sinh(x)", 0.3, std::sinh(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : sinh(0.5f)", 0.3f, std::sinh(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);

#if !defined(_MSC_VER) || _MSC_VER >= 1800 // asinh etc.. are only available in Visual Studio 2013+
	// asinh
	testMainFloatArg("def main(float x) float : asinh(x)", 0.3f, std::asinh(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : asinh(x)", 0.3, std::asinh(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : asinh(0.5f)", 0.3f, std::asinh(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);
#endif

	// cosh
	testMainFloatArg("def main(float x) float : cosh(x)", 0.3f, std::cosh(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : cosh(x)", 0.3, std::cosh(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : cosh(0.5f)", 0.3f, std::cosh(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);

#if !defined(_MSC_VER) || _MSC_VER >= 1800
	// acosh
	testMainFloatArg("def main(float x) float : acosh(x)", 2.0f, std::acosh(2.0f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : acosh(x)", 2.0, std::acosh(2.0), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : acosh(2.0f)", 2.2f, std::acosh(2.0f), INCLUDE_EXTERNAL_MATHS_FUNCS);
#endif

	// tanh
	testMainFloatArg("def main(float x) float : tanh(x)", 0.3f, std::tanh(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : tanh(x)", 0.3, std::tanh(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : tanh(0.5f)", 0.3f, std::tanh(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);

#if !defined(_MSC_VER) || _MSC_VER >= 1800
	// atanh
	testMainFloatArg("def main(float x) float : atanh(x)", 0.3f, std::atanh(0.3f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : atanh(x)", 0.3, std::atanh(0.3), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : atanh(0.5f)", 0.3f, std::atanh(0.5f), INCLUDE_EXTERNAL_MATHS_FUNCS);
#endif

	// atan2
	testMainFloatArg("def main(float x) float : atan2(x, 0.4f)", 0.3f, std::atan2(0.3f, 0.4f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : atan2(x, 0.4)", 0.3, std::atan2(0.3, 0.4), INCLUDE_EXTERNAL_MATHS_FUNCS);
	
	// float mod
	testMainFloatArg("def main(float x) float : mod(1.7, 1.2)", -2.8f, Maths::floatMod(1.7f, 1.2f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : mod(1.7, x)", 1.1f, Maths::floatMod(1.7f, 1.1f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : mod(-1.7, x)", 1.1f, Maths::floatMod(-1.7f, 1.1f), INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : mod(-1.7, x)", 1.1, Maths::doubleMod(-1.7, 1.1), INCLUDE_EXTERNAL_MATHS_FUNCS);

	// int mod
	testMainIntegerArg("def main(int x) int : mod(-5, x)", 4, 3, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainIntegerArg("def main(int x) int : mod(5, x)", 4, 1, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainIntegerArg("def main(int x) int : mod(5, 4)", 4, 1, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainIntegerArg("def main(int x) int : mod(5, x)", 3, 2, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainIntegerArg("def main(int x) int : mod(-5, x)", 7, 2, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainIntegerArg("def main(int x) int : mod(4, x)", 7, 4, INCLUDE_EXTERNAL_MATHS_FUNCS);

	// isFinite
	testMainFloatArg("def main(float x) float : isFinite(x) ? 1.0 : 0.0", 10.f, 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isFinite(x) ? 1.0 : 0.0", 10, 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isFinite(x) ? 1.0 : 0.0", std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isFinite(x) ? 1.0 : 0.0", -std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isFinite(x) ? 1.0 : 0.0", std::numeric_limits<float>::quiet_NaN(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isFinite(x) ? 1.0 : 0.0", std::numeric_limits<double>::quiet_NaN(), 0.0, INCLUDE_EXTERNAL_MATHS_FUNCS);

	// isNaN (old spelling)
	testMainFloatArg("def main(float x) float : isNaN(x) ? 1.0 : 0.0", 10.f, 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isNaN(x) ? 1.0 : 0.0", 10, 0.0, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNaN(x) ? 1.0 : 0.0", std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNaN(x) ? 1.0 : 0.0", -std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNaN(x) ? 1.0 : 0.0", std::numeric_limits<float>::quiet_NaN(), 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isNaN(x) ? 1.0 : 0.0", std::numeric_limits<double>::quiet_NaN(), 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS);

	// isNAN (old spelling)
	testMainFloatArg("def main(float x) float : isNAN(x) ? 1.0 : 0.0", 10.f, 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isNAN(x) ? 1.0 : 0.0", 10, 0.0, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNAN(x) ? 1.0 : 0.0", std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNAN(x) ? 1.0 : 0.0", -std::numeric_limits<float>::infinity(), 0.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainFloatArg("def main(float x) float : isNAN(x) ? 1.0 : 0.0", std::numeric_limits<float>::quiet_NaN(), 1.0f, INCLUDE_EXTERNAL_MATHS_FUNCS);
	testMainDoubleArg("def main(double x) double : isNAN(x) ? 1.0 : 0.0", std::numeric_limits<double>::quiet_NaN(), 1.0, INCLUDE_EXTERNAL_MATHS_FUNCS);
}


static void testOperatorOverloading()
{
	// =================================================================== 
	// Test Operator Overloading 
	// ===================================================================

	// test op_add
	testMainFloat("struct s { float x, float y } \n\
				  def op_add(s a, s b) : s(a.x + b.x, a.y + b.y) \n\
				  def main() float : x(s(1, 2) + s(3, 4))", 4.0f);

	// test op_mul
	testMainFloat("struct s { float x, float y } \n\
				  def op_mul(s a, s b) : s(a.x * b.x, a.y * b.y) \n\
				  def main() float : x(s(2, 3) * s(3, 4))", 6.0f);

	// Test op_sub
	testMainFloat("struct s { float x, float y } \n\
				  def op_sub(s a, s b) : s(a.x - b.x, a.y - b.y) \n\
				  def main() float : x(s(2, 3) - s(3, 4))", -1.0f);

	// Test op_div
	testMainFloat("struct s { float x, float y } \n\
				  def op_div(s a, s b) : s(a.x / b.x, a.y / b.y) \n\
				  def main() float : x(s(2, 3) / s(3, 4))", 2.0f / 3.0f);

	// Test op_unary_minus
	testMainFloat("struct s { float x, float y } \n\
				  def op_unary_minus(s a) : s(-a.x, -a.y) \n\
				  def main() float : x(-s(2, 3))", -2.0f);

	
	// op_eq
	testMainFloatArg("struct s { float x }					\n\
					 def op_eq(s a, s b) : a.x == b.x		\n\
					 def main(float x) float : (s(4.0f) == s(x)) ? 1.0 : 0.0", 4.0f, 1.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_eq(s a, s b) : a.x == b.x		\n\
					 def main(float x) float : (s(4.0f) == s(x)) ? 1.0 : 0.0", 5.0f, 0.0f);

	// Test that operator overloading overrises the built-in == function - 
	// Make op_eq return false when the built-in == would return true.
	testMainFloatArg("struct s { float x }					\n\
					 def op_eq(s a, s b) : false			\n\
					 def main(float x) float : (s(4.0f) == s(x)) ? 1.0 : 0.0", 4.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_eq(s a, s b) : false			\n\
					 def call_op_eq(s a, s b) !noinline : a == b		\n\
					 def main(float x) float : call_op_eq(s(4.0f), s(x)) ? 1.0 : 0.0", 4.0f, 0.0f);


	// op_neq
	testMainFloatArg("struct s { float x }					\n\
					 def op_neq(s a, s b) : a.x != b.x		\n\
					 def main(float x) float : (s(4.0f) != s(x)) ? 1.0 : 0.0", 4.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_neq(s a, s b) : a.x != b.x		\n\
					 def main(float x) float : (s(4.0f) != s(x)) ? 1.0 : 0.0", 5.0f, 1.0f);


	// Test that operator overloading overrides the built-in != function - 
	// Make op_neq return false when the built-in != would return true.
	// in this test: s(4.0f) != s(5.0) should be true, but we use the overloading op_neq to return false.
	testMainFloatArg("struct s { float x }					\n\
					 def op_neq(s a, s b) : false			\n\
					 def main(float x) float : (s(4.0f) != s(x)) ? 1.0 : 0.0", 5.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_neq(s a, s b) : false			\n\
					 def call_op_neq(s a, s b) !noinline : a != b		\n\
					 def main(float x) float : call_op_neq(s(4.0f), s(x)) ? 1.0 : 0.0", 5.0f, 0.0f);


	// op_lt
	testMainFloatArg("struct s { float x }					\n\
					 def op_lt(s a, s b) : a.x < b.x		\n\
					 def main(float x) float : (s(4.0f) < s(x)) ? 1.0 : 0.0", 5.0f, 1.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_lt(s a, s b) : a.x < b.x		\n\
					 def main(float x) float : (s(4.0f) < s(x)) ? 1.0 : 0.0", 4.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_lt(s a, s b) : a.x < b.x		\n\
					 def main(float x) float : (s(4.0f) < s(x)) ? 1.0 : 0.0", 2.0f, 0.0f);

	// op_gt
	testMainFloatArg("struct s { float x }					\n\
					 def op_gt(s a, s b) : a.x > b.x		\n\
					 def main(float x) float : (s(4.0f) > s(x)) ? 1.0 : 0.0", 5.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_gt(s a, s b) : a.x > b.x		\n\
					 def main(float x) float : (s(4.0f) > s(x)) ? 1.0 : 0.0", 4.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_gt(s a, s b) : a.x > b.x		\n\
					 def main(float x) float : (s(4.0f) > s(x)) ? 1.0 : 0.0", 2.0f, 1.0f);


	// op_lte
	testMainFloatArg("struct s { float x }					\n\
					 def op_lte(s a, s b) : a.x <= b.x		\n\
					 def main(float x) float : (s(4.0f) <= s(x)) ? 1.0 : 0.0", 5.0f, 1.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_lte(s a, s b) : a.x <= b.x		\n\
					 def main(float x) float : (s(4.0f) <= s(x)) ? 1.0 : 0.0", 4.0f, 1.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_lte(s a, s b) : a.x <= b.x		\n\
					 def main(float x) float : (s(4.0f) <= s(x)) ? 1.0 : 0.0", 2.0f, 0.0f);

	// op_gte
	testMainFloatArg("struct s { float x }					\n\
					 def op_gte(s a, s b) : a.x >= b.x		\n\
					 def main(float x) float : (s(4.0f) >= s(x)) ? 1.0 : 0.0", 5.0f, 0.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_gte(s a, s b) : a.x >= b.x		\n\
					 def main(float x) float : (s(4.0f) >= s(x)) ? 1.0 : 0.0", 4.0f, 1.0f);

	testMainFloatArg("struct s { float x }					\n\
					 def op_gte(s a, s b) : a.x >= b.x		\n\
					 def main(float x) float : (s(4.0f) >= s(x)) ? 1.0 : 0.0", 2.0f, 1.0f);



	// ===================================================================
	// Test Operator Overloading with two different structures
	// ===================================================================
	
	// op_add returning S
	testMainFloat("struct S { float x, float y }					\n\
				  struct T { float z, float w }					\n\
				  def op_add(S a, T b) S : S(a.x + b.z, a.y + b.w)	\n\
				  def main() float : x(S(1, 2) + T(3, 4))", 4.0f);

	// op_add returning T
	testMainFloat("struct S { float x, float y }					\n\
				  struct T { float z, float w }					\n\
				  def op_add(S a, T b) T : T(a.x + b.z, a.y + b.w)	\n\
				  def main() float : z(S(1, 2) + T(3, 4))", 4.0f);

	// ===================================================================
	// Test Operator Overloading within a generic function
	// ===================================================================
	testMainFloat("struct s { float x, float y }						\n\
				  def op_add(s a, s b) : s(a.x + b.x, a.y + b.y)		\n\
				  def f<T>(T a, T b) : a + b							\n\
				  def main() float : x(f(s(1, 2), s(3, 4)))", 4.0f);

	// Test when op_add is not present

	testMainFloatArgInvalidProgram("struct s { float x, float y }						\n\
				  def f<T>(T a, T b) : a + b							\n\
				  def main() float : x(f(s(1, 2), s(3, 4)))");


	// ===================================================================
	// Test Operator Overloading in collection literals
	// ===================================================================

	// Without a collection
	testMainFloat("struct s { float x }									\n\
				  def op_add(s a, s b) : s(a.x + b.x)					\n\
				  def main() float : (s(1) + s(3)).x", 4.0f);

	// In Array literal
	testMainFloat("struct s { float x }									\n\
				  def op_add(s a, s b) : s(a.x + b.x)					\n\
				  def main() float : [s(1) + s(3)]a[0].x", 4.0f);

	// In VArray literal
	testMainFloatArgAllowUnsafe("struct s { float x }									\n\
				  def op_add(s a, s b) : s(a.x + b.x)					\n\
				  def main(float x) float : [s(1) + s(3)]va[0].x", 1.0f, 4.0f, INVALID_OPENCL);

	// In Vector literal
	testMainFloat("struct s { float x }									\n\
				  def op_add(s a, s b) : s(a.x + b.x)					\n\
				  def main() float : [(s(1) + s(3)).x]v[0]", 4.0f);


	testMainFloat("def f(float x) float : x*x         def main() float : f(10)", 100.0);
	testMainFloat("def f(float x, float y) float : 1.0f   \n\
				  def f(float x, int y) float : 2.0f   \n\
				  def main() float : f(1.0, 2)", 2.0);



	// Test operator overloading (op_add) in a let block.
	testMainFloatArg("								\n\
		struct vec3 { float x, float y, float z }	\n\
		def vec3(float v) vec3 : vec3(v, v, v)		\n\
		def op_add(vec3 a, vec3 b) vec3 : vec3(a.x+b.x, a.y+b.y, a.z+b.z)	\n\
		def eval(vec3 pos) vec3 :					\n\
			let											\n\
				scale = 20.0							\n\
			in											\n\
				vec3(scale) + vec3(0.2)					\n\
		def main(float x) float: x(eval(vec3(x, x, x)))",
		1.0f, 20.2f);	

	// Test operator overloading (op_mul) in a let block.
	testMainFloatArg("								\n\
		struct vec3 { float x, float y, float z }	\n\
		def vec3(float v) vec3 : vec3(v, v, v)		\n\
		def op_mul(vec3 a, float b) vec3 : vec3(a.x*b, a.y*b, a.z*b)	\n\
		def eval(vec3 pos) vec3 :					\n\
			let											\n\
				actualpos = vec3(pos.x, pos.y, pos.z + 10.0)				\n\
			in											\n\
				actualpos * 10000.0						\n\
		def main(float x) float: x(eval(vec3(x, x, x)))",
		1.0f, 10000.0f);	
}



static void testComparisons()
{
	// Integer comparisons:
	// Test <=
	testMainInteger("def main() int : if(1 <= 2, 10, 20)", 10);
	testMainInteger("def main() int : if(1 <= 1, 10, 20)", 10);
	testMainInteger("def main() int : if(3 <= 1, 10, 20)", 20);

	// Test >=
	testMainInteger("def main() int : if(1 >= 2, 10, 20)", 20);
	testMainInteger("def main() int : if(1 >= 1, 10, 20)", 10);
	testMainInteger("def main() int : if(3 >= 1, 10, 20)", 10);

	// Test <
	testMainInteger("def main() int : if(1 < 2, 10, 20)", 10);
	testMainInteger("def main() int : if(3 < 1, 10, 20)", 20);

	// Test >
	testMainInteger("def main() int : if(1 > 2, 10, 20)", 20);
	testMainInteger("def main() int : if(3 > 1, 10, 20)", 10);

	// Test ==
	testMainInteger("def main() int : if(1 == 1, 10, 20)", 10);
	testMainInteger("def main() int : if(1 == 2, 10, 20)", 20);

	// Test !=
	testMainInteger("def main() int : if(1 != 1, 10, 20)", 20);
	testMainInteger("def main() int : if(1 != 2, 10, 20)", 10);


	// Float comparisons:
	// Test <=
	testMainFloat("def main() float : if(1.0 <= 2.0, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(1.0 <= 1.0, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(3.0 <= 1.0, 10.0, 20.0)", 20.0);

	// Test >=
	testMainFloat("def main() float : if(1.0 >= 2.0, 10.0, 20.0)", 20.0);
	testMainFloat("def main() float : if(1.0 >= 1.0, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(3.0 >= 1.0, 10.0, 20.0)", 10.0);

	// Test <
	testMainFloat("def main() float : if(1.0 < 2.0, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(3.0 < 1.0, 10.0, 20.0)", 20.0);

	// Test >
	testMainFloat("def main() float : if(1.0 > 2.0, 10.0, 20.0)", 20.0);
	testMainFloat("def main() float : if(3.0 > 1.0, 10.0, 20.0)", 10.0);

	// Test ==
	testMainFloat("def main() float : if(1.0 == 1.0, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(1.0 == 2.0, 10.0, 20.0)", 20.0);

	// Test !=
	testMainFloat("def main() float : if(1.0 != 1.0, 10.0, 20.0)", 20.0);
	testMainFloat("def main() float : if(1.0 != 2.0, 10.0, 20.0)", 10.0);


	// ===================================================================
	// Test invalid types for comparisons
	// ===================================================================

	// comparing different types
	testMainFloatArgInvalidProgram("def main() float : if 1.0 == [1.0]a then 10.0 else 20.0");

	// Comparisons between vectors are not currently supported.
	testMainFloatArgInvalidProgram("def main() float : if [1.0, 2.0]v == [1.0, 2.0]v then 10.0 else 20.0");


	// ===================================================================
	// Test implicit conversions from int to float in comparisons
	// ===================================================================
	// Comparison between two literals:

	// Test a comparison that returns true
	testMainFloat("def main() float : if(1.0 <= 2, 10.0, 20.0)", 10.0);
	testMainFloat("def main() float : if(1 <= 2.0, 10.0, 20.0)", 10.0);

	// Test a comparison that returns false
	testMainFloat("def main() float : if(3 <= 1.0, 10.0, 20.0)", 20.0);
	testMainFloat("def main() float : if(3.0 <= 1, 10.0, 20.0)", 20.0);

	// Comparison between a variable and a literal:
	testMainFloatArg("def main(float x) float : if(x <= 2, 10.0, 20.0)", 1.0, 10.0);
	testMainFloatArg("def main(float x) float : if(x <= 2, 10.0, 20.0)", 3.0, 20.0);

	testMainFloatArg("def main(float x) float : if(2 <= x, 10.0, 20.0)", 3.0, 10.0);
	testMainFloatArg("def main(float x) float : if(2 <= x, 10.0, 20.0)", 1.0, 20.0);
}


static void testLetBlocks()
{
	// ===================================================================
	// Test let blocks
	// ===================================================================
	testMainFloat("def f(float x) float : \
				  let z = 2.0 \
				  in \
				  z \
				  def main() float : f(0.0)", 2.0);

	// Test two let clauses in a let block
	testMainFloat("def f(float x) float : \
				  let	\
					z = 2.0 \
					y = 3.0 \
				  in \
					y + z \
				  def main() float : f(0.0)", 5.0);

	// Test nested let blocks
	testMainFloat("	def f(float x) float : \
					let	\
						z = 2.0 \
					in \
						let		\
							y = 10.0  \
						in				\
							y + z			\
				  def main() float : f(0.0)", 12.0);

	// Test nested let blocks with multiple let clauses per block
	testMainFloat("	def f(float a) float : \
				  let	\
					x = 1.0  \
					y = 2.0 \
				  in \
					let		\
						z = 10.0  \
						w = 20.0  	\
					in				\
						x + y + z + w			\
				  def main() float : f(0.0)", 33.0);

	// Test two let clauses where one refers to the other.
	testMainFloat("def f() float : \
				  let	\
					z = 2.0 \
					y = z \
				  in \
					y \
				  def main() float : f()", 2.0);


	// Test optional types on let blocks
	testMainFloatArg("def f(float x) float :		\n\
				  let								\n\
					float z = 2.0					\n\
				  in								\n\
					x + z							\n\
				  def main(float x) float : f(x)", 2.0f, 4.0f);

	// Test optional types on let blocks
	testMainFloatArg("def f(float x) float :		\n\
				  let								\n\
					float y = 2.0					\n\
					float z = 3.0					\n\
				  in								\n\
					x + y + z							\n\
				  def main(float x) float : f(x)", 2.0f, 7.0f);

	// Test int->float type coercion with optional types on let blocks
	testMainFloatArg("def f(float x) float :		\n\
				  let								\n\
					float z = 2						\n\
				  in								\n\
					x + z							\n\
				  def main(float x) float : f(x)", 2.0f, 4.0f);


	// Test type mismatches between declared type and actual time.
	testMainFloatArgInvalidProgram("def f(float x) float :		\n\
				  let								\n\
					bool z = 2						\n\
				  in								\n\
					x + z							\n\
				  def main(float x) float : f(x)");

	testMainFloatArgInvalidProgram("def f(float x) float :		\n\
				  let								\n\
					int z = true					\n\
				  in								\n\
					x + z							\n\
				  def main(float x) float : f(x)");

	testMainFloatArgInvalidProgram("def f(float x) float :		\n\
				  let								\n\
					bool y = true					\n\
					int z = y						\n\
				  in								\n\
					x + z							\n\
				  def main(float x) float : f(x)");




	// Test two let clauses where one refers to the other (reverse order)
	#if 0
	testMainFloat("def f() float : \
				  let	\
					z = y \
					y = 2.0 \
				  in \
					y \
				  def main() float : f()", 2.0);

	testMainFloat("def f() float : \
				  let	\
					z = y \
					y = 2.0 \
				  in \
					y \
				  def main() float : f()", 2.0);
	#endif

	// Test nested let blocks
	testMainFloat("def f() float :				\n\
				let								\n\
					x = 2.0						\n\
				in								\n\
					let							\n\
						y = x					\n\
					in							\n\
						y						\n\
				def main() float : f()", 2.0);


	

	// Test addition expression in let
	testMainFloat("def f(float x) float : \
				  let z = 2.0 + 3.0 in\
				  z \
				  def main() float : f(0.0)", 5.0);

	// Test function expression in let
	testMainFloat("	def g(float x) float : x + 1.0 \
					def f(float x) float : \
					let z = g(1.0) in \
					z \
					def main() float : f(0.0)", 2.0);

	// Test function argument in let
	testMainFloat("	def f(float x) float : \
					let z = x + 1.0 in\
					z \
					def main() float : f(2.0)", 3.0);


	// Test use of let variable twice
	testMainFloat("	def f(float x) float : \
				  let z = x + 1.0 in\
				  z + z\
				  def main() float : f(2.0)", 6.0);

	// test multiple lets with the same name in the same let block aren't allowed
	testMainFloatArgInvalidProgram("def f(float x) float :		\n\
			  let								\n\
				float y = 2.0					\n\
				float y = 3.0					\n\
			  in								\n\
				x + y							\n\
			  def main(float x) float : f(x)");


	// Test a let var with structure type.
	testMainFloatArg("struct S { float a }							\n\
				def f(float x) float :			\n\
					let							\n\
						z = S(x)				\n\
					in								\n\
						z.a + 1.0						\n\
				def main(float x) float : f(x)", 4.0, 5.0, INVALID_OPENCL);


	// Test a let var set to a structure passed as an argument.
	testMainFloatArg("struct S { float a }							\n\
				def f(float x, S s) float :			\n\
					let							\n\
						z = s				\n\
					in								\n\
						z.a + 1.0						\n\
				def main(float x) float : f(x, S(x))", 4.0, 5.0, INVALID_OPENCL);


	

	// Test using a let variable (y) before it is defined:
	testMainFloatArgInvalidProgram("									\n\
		def main(float x) float :		\n\
			let							\n\
				z = y					\n\
				y = x					\n\
			in							\n\
				z						"
	);

	// Test using a let variable (y) before it is defined, that would have created a cycle:
	testMainFloatArgInvalidProgram("									\n\
		def main(float x) float :		\n\
			let							\n\
				z = y					\n\
				y = z					\n\
			in							\n\
				z						"
	);

	// Test avoidance of circular let definition: 
	testMainFloatArgInvalidProgram("									\n\
		def main(float y) float :		\n\
			let							\n\
				x = x					\n\
			in							\n\
				x						"
	);

	// Test avoidance of circular let definition
	testMainFloatArgInvalidProgram("									\n\
		def main(float y) float :		\n\
			let							\n\
				x = 1.0 + x				\n\
			in							\n\
				x						"
	);

	// Test avoidance of circular let definition for functions
	testMainFloatArg("						\n\
		def f(float x) float : x		\n\
		def main(float y) float :		\n\
			let							\n\
				f1 = f(1.0) + 1.0				\n\
				f = f(1.0) + 1.0				\n\
			in							\n\
				f						",
		1.0f,
		2.0f
	);


	//COPIED:
	// Test two let clauses where one refers to the other.
	testMainFloat("def f() float : \
				  let	\
					z = 2.0 \
					y = z \
				  in \
					y \
				  def main() float : f()", 2.0);

	// Test array subscripting in let clause
	testMainFloatArg("						\n\
		def f(array<float, 4> a) float : 	\n\
			let								\n\
				v = a[0]					\n\
				v1 = a[1]					\n\
				v2 = a[2]					\n\
			in								\n\
				v							\n\
		def main(float y) float	:			\n\
			f([y,y,y,y]a)",
		1.0f,
		1.0f,
		INVALID_OPENCL // array literals
	);

	// Test also that the second let clause with var name 'a' does not get interpreted as a suffix for an array literal on '[0]',
	testMainFloatArg("									\n\
		def f(array<float, 4> the_array) float : 		\n\
			let											\n\
				v = the_array[0]						\n\
				a = the_array[1]						\n\
			in											\n\
				v										\n\
		def main(float y) float	:						\n\
			f([y,y,y,y]a)",
		1.0f,
		1.0f,
		INVALID_OPENCL // array literals
	);
}


static void testLambdaExpressionsAndClosures()
{
	// ===================================================================
	// Test lambda expressions and closures
	// ===================================================================
	Winter::TestResults results;

	// Test lambda with arrow syntax:
	testMainFloatArg("def main(float x) float : (\\(float x) -> x*x) (x)", 4.0f, 16.0f, INVALID_OPENCL);

	// Test Lambda applied directly.  Function inlining (beta reduction) should be done.
	results = testMainFloatArg("def main(float x) float : (\\(float y) : y*y) (x)", 4.0f, 16.0f, INVALID_OPENCL);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType);

	// Test Lambda applied directly (with same variable names)
	testMainFloatArg("def main(float x) float : (\\(float x) : x*x) (x)", 4.0f, 16.0f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testMainFloatArg("def main(float x) float : (\\(float x) : x*x) (x + 1)", 4.0f, 25.0f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	

	// Test Lambda in let
	// f should be inlined if it's only called once.
	results = testMainFloat("def main() float :				\n\
				  let f = \\(float x) : x*x  in		\n\
				  f(2.0)", 4.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType);

	results = testMainFloat("def main() float :				\n\
				  let f = \\(float x) : x*x  in		\n\
					let g = f in					\n\
				  g(2.0)", 4.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType);

	results = testMainFloatArg("def main(float x) float :				\n\
				  let f = \\(float x) : x*x  in		\n\
					let g = f in					\n\
				  g(x)", 2.0f, 4.0f, INVALID_OPENCL);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType);

	testMainFloat("def main() float :				\n\
				  let f = \\(float x) : x*x  in		\n\
				  f(2.0) + f(3.0)", 13.0f);

	testMainFloat("	def f(float x) float : x + 1.0				\n\
					def main() float :							\n\
						let g = f								\n\
							in									\n\
						g(1.0)", 2.0f);

	// Test return of a lambda from a function
	results = testMainFloat("def makeLambda() : \\(float x) : x*x    \n\
					def main() float :           \n\
					let f = makeLambda()  in   \n\
				  f(2.0)", 4.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType);

	// Test variable capture: the returned lambda needs to capture the value of x.
	testMainFloat("	def makeFunc(float x) function<float> : \\() : x      \n\
					def main() float :                          \n\
					let f = makeFunc(2.0) in                    \n\
					f()", 2.0);

	// when makeFunc is inlined, gets transformed to:
	testMainFloat("	def main() float :                          \n\
					let f = \\() : 2.0 in                    \n\
					f()", 2.0);



	// Test generic lambda!!!
	/*testMainFloat("def makeLambda() : \\<T>(T x) : x*x    \n\
					def main() float :           \n\
					let f = makeLambda()  in   \n\
				  f(2.0)", 4.0f);*/

	testMainFloat("def g(function<float, float> f, float x) : f(x)			\n\
					def main() float :										\n\
					g(\\(float x) : x, 2.0f)", 2.0f);

	// Test Lambda passed as a function arg
	testMainFloat("def g(function<float, float> f, float x) : f(x)			\n\
					def main() float :										\n\
					g(\\(float x) : x*x*x, 2.0f)", 8.0f);


	// Test passing a normal function as an argument
	testMainFloat("def g(function<float, float> f, float x) : f(x)			\n\
					def square(float x) : x*x									\n\
					def main() float :										\n\
					g(square, 2.0f)", 4.0f);


	// Test 'compose' function: returns the composition of two functions
	// NOTE: this requires lexical closures to work :)
	results = testMainFloat("def compose(function<float, float> f) : f       \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main() float :                         \n\
						let z = compose(mulByTwo)  in \n\
						z(4.0)", 8.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType); // Function calls should be inlined.

	// Test lambda with function call in it.
	results = testMainFloatArg("def square(float z) float : z*z						\n\
					def main(float x) float :                         \n\
						let													  \n\
							f = \\(float y) : square(y)							\n\
						in													\n\
							f(x)", 4.0f, 16.0f, INVALID_OPENCL);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType); // Function calls should be inlined.

	// Test return of a lambda with a function call in it
	results = testMainFloatArg("def square(float z) float : z*z						\n\
					def makeLambda() function<float, float> : \\(float y) : square(y)		\n\
					def main(float x) float :                         \n\
						let													  \n\
							f = makeLambda()							\n\
						in													\n\
							f(x)", 4.0f, 16.0f, INVALID_OPENCL);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType || results.maindef->body->nodeType() == ASTNode::FloatLiteralType); // Function calls should be inlined.

	// Test lambda being return from a function, where the lambda has a call to a function argument in it.
	testMainFloat("def compose(function<float, float> f) : \\(float x) : f(x)       \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main() float :                         \n\
						let z = compose(mulByTwo)  in \n\
						8.f", 8.0f);

	/*

	def compose(function<float, float> f) : \\(float x) : f(x)   
	def mulByTwo(float x) : x * 2.0            
	def main() float :                        
		let z = \\(float x) : mulByTwo(x)  in 

		*/

	// Test lambda being return from a function, where the lambda has a call to a function argument in it.
	testMainFloat("def compose(function<float, float> f) : \\(float x) : f(x)       \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main() float :                         \n\
						let z = compose(mulByTwo)  in \n\
						z(4.0)", 8.0f);



	results = testMainFloatArg("def compose(function<float, float> f, function<float, float> g) : \\(float x) : f(g(x))       \n\
					def addOne(float x) : x + 1.0                \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main(float x) float :                         \n\
						let z = compose(addOne, mulByTwo)  in \n\
						z(x)", 10.0, 21.0f, INVALID_OPENCL);
	//TEMP isn't being inlined right now: testAssert(results.maindef->body->nodeType() == ASTNode::AdditionExpressionType); // Function calls should be inlined.

	//inlining compose(), goes to:
	testMainFloatArg("def compose(function<float, float> f, function<float, float> g) : \\(float x) : f(g(x))       \n\
					def addOne(float x) : x + 1.0                \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main(float x) float :                         \n\
						let z = \\(float x) : addOne(mulByTwo(x))  in \n\
						z(x)", 10.0, 21.0f, INVALID_OPENCL);
	// Test map with lambda expression
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {1.0f, 4.0f, 9.0f, 16.0f};

		testFloatArray(
			"def main(array<float, 4> a, array<float, 4> b) array<float, 4> : map(\\(float x) : x*x, a)",
			a, b, target_results, 4);
	}


	testMainFloat("def compose(function<float, float> f, function<float, float> g) : \\(float x) : f(g(x))       \n\
					def addOne(float x) : x + 1.0                \n\
					def mulByTwo(float x) : x * 2.0                \n\
					def main() float :                         \n\
						let z = compose(addOne, mulByTwo)  in \n\
						z(10.0)", 21.0f);
	// Test capturing a heap-allocated type (varray in this case)
	testMainIntegerArg("def main(int x) int :                          \n\
						let					                    \n\
							a = [10, 11, 12, 13]va				\n\
							f = \\(int x) int : a[x]			\n\
						in					                    \n\
							f(x)", 2, 12, ALLOW_UNSAFE | INVALID_OPENCL);

	// Test capturing a heap-allocated type in a closure that is returned from a function
	results = testMainIntegerArg("def makeFunc() function<int, int> :		\n\
						let					                    \n\
							a = [10, 11, 12, 13]va				\n\
						in										\n\
							\\(int x) int : a[x]				\n\
																\n\
						def main(int x) int :                   \n\
						let										\n\
							f = makeFunc()						\n\
						in					                    \n\
							f(x)", 2, 12, ALLOW_UNSAFE | INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls <= 2); // Should be one alloc call for the varray, and one for the closure returned from makeFunc()

	/*
	def main(int x) int :              
		let								
			f = let					                    \n\
					a = [10, 11, 12, 13]va				\n\
				in										\n\
					\\(int x) int : a[x]		
		in					            
			f(x)


	def main(int x) int :              
		let								
			f = ..
		in					            
			(let					                    \n\
				a = [10, 11, 12, 13]va				\n\
			in										\n\
				\\(int x) int : a[x])(x)


	=> goes to?

	def main(int x) int :              
		let								
			f = ..
		in					            
			let					                    \n\
				a = [10, 11, 12, 13]va				\n\
			in										\n\
				a[x]
	*/

	// Test capturing two varrays
	results = testMainIntegerArg("def makeFunc() function<int, int> :		\n\
						let					                    \n\
							a = [10, 11, 12, 13]va				\n\
							b = [20, 21, 22, 23]va				\n\
						in										\n\
							\\(int x) int : a[x] + b[x]			\n\
																\n\
						def main(int x) int :                   \n\
						let										\n\
							f = makeFunc()						\n\
						in					                    \n\
							f(x)", 2, 34, ALLOW_UNSAFE | INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls <= 3); // Should be one alloc call for each varray, and one for the closure returned from makeFunc()

	// Test capturing a string
	results = testMainIntegerArg("def makeFunc() function<int, int> :		\n\
						let					                    \n\
							s = \"hello\"						\n\
						in										\n\
							\\(int x) int : codePoint(elem(s, toInt64(x)))			\n\
																\n\
						def main(int x) int :                   \n\
						let										\n\
							f = makeFunc()						\n\
						in					                    \n\
							f(x)", 1, (int)'e', ALLOW_UNSAFE | INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls <= 2); // Should be one alloc call for the string, and one for the closure returned from makeFunc()


	// Test capturing a struct value (pass-by-pointer but stored in the captured var struct)
	results = testMainIntegerArg("struct S { int s }						\n\
					def makeFunc() !noinline function<int, int> :			\n\
						let													\n\
							a = S(10)										\n\
						in													\n\
							\\(int x) int : x + a.s							\n\
																			\n\
					def main(int x) int :									\n\
						let													\n\
							f = makeFunc()									\n\
						in													\n\
							f(x)", 1, 11, ALLOW_UNSAFE | INVALID_OPENCL);

	// Test capturing an array value (pass-by-pointer but stored in the captured var struct)
	results = testMainIntegerArg("											\n\
					def makeFunc() !noinline function<int, int> :			\n\
						let													\n\
							ar = [1, 2, 3]a									\n\
						in													\n\
							\\(int x) int : x + ar[1]						\n\
																			\n\
					def main(int x) int :									\n\
						let													\n\
							f = makeFunc()									\n\
						in													\n\
							f(x)", 1, 3, ALLOW_UNSAFE | INVALID_OPENCL);



	// Test escape analyis allowing a closure to be stack-allocated.
	results = testMainIntegerArg("def main(int x) int :			\n\
						let										\n\
							f = \\(int x) : x + 10				\n\
						in					                    \n\
							f(x)", 2, 12, ALLOW_UNSAFE | INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	// Test closures

	//// Test variable capture: the returned lambda needs to capture the value of x.
	testMainFloat("	def makeFunc(float x) function<float> : \\() : x      \n\
					def main() float :                          \n\
					let f = makeFunc(2.0) in                    \n\
					f()", 2.0);


	//// Test variable capture with two captured variables.
	testMainFloat("	def makeFunc(float x, float y) function<float> : \\() : x + y     \n\
					def main() float :                          \n\
					let f = makeFunc(2.0, 3.0) in                    \n\
					f()", 5.0);
	
	// Test capture of one variable and one usual argument
	testMainFloat("	def makeFunc(float x) function<float, float> : \\(float y) : x + y     \n\
					def main() float :                          \n\
					let f = makeFunc(2.0) in                    \n\
					f(3.0)", 5.0);

	// Test capture of let variable.
	testMainFloat("	def main() float :					\n\
					let z = 3.0 in						\n\
					let f = \\() : z  in				\n\
					f()", 3.0);


	// Test a function capturing a variable, then being passed to a higher order function which calls it.
	results = testMainIntegerArg("															\n\
		def f(function<int> arg_func) !noinline : arg_func()								\n\
		def main(int x) int :																\n\
			let																				\n\
				z = 2																		\n\
			in																				\n\
				f(\\() -> z)																\n\
		", 17, 2, INVALID_OPENCL);
	testAssert(results.stats.num_free_vars_stored == 1); // z is free and should be captured from lexical env.

	testMainIntegerArg("																	\n\
		def f(function<int> arg_func) !noinline : arg_func()								\n\
		def main(int x) int :																\n\
			let																				\n\
				z = x + 1																	\n\
			in																				\n\
				f(\\() -> z)																\n\
		", 17, 18, INVALID_OPENCL);

	testMainIntegerArg("																	\n\
		def f(function<int> arg_func) : arg_func()								\n\
		def ident(int x) : x																\n\
		def main(int x) int :																\n\
			let																				\n\
				z = 2																		\n\
			in																				\n\
				f(\\() -> 																	\n\
					let																			\n\
						y = 3																	\n\
					in																			\n\
						ident(z) + y																	\n\
				)																			 \n\
		", 17, 5, INVALID_OPENCL);

	
		

	// TODO: test two lets variables at same level

	// Test capture of let variable up one level.
	testMainFloat("	def main() float :                          \n\
					let x = 3.0 in                         \n\
					let z = 4.0 in                         \n\
					let f = \\() : x  in                    \n\
					f()", 3.0);

	testMainFloat("	def main() float :                          \n\
					let x = 3.0 in                         \n\
					let z = 4.0 in                         \n\
					let f = \\() : z  in                    \n\
					f()", 4.0);


	
	// ===================================================================
	// Test type-checking for calls to function variables.
	// ===================================================================
	testMainFloatArg("def main(float x) float :							\n\
					let f = \\(float y) : y + 10.0  in					\n\
					f(x)", 3.0, 13.0, ALLOW_UNSAFE | INVALID_OPENCL);

	// Test with invalid number of args:
	// zero args:
	testMainFloatArgInvalidProgram("def main(float x) float :							\n\
					let f = \\(float y) : y + 10.0  in					\n\
					f()");

	// too many args:
	testMainFloatArgInvalidProgram("def main(float x) float :							\n\
					let f = \\(float y) : y + 10.0  in					\n\
					f(x, x)");

	// incorrect type:
	testMainFloatArgInvalidProgram("def main(float x) float :							\n\
					let f = \\(float y) : y + 10.0  in					\n\
					f(true)");

	// incorrect type:
	testMainFloatArgInvalidProgram("def main(float x) float :							\n\
					let f = \\(float a, float b) : a + b  in							\n\
					f(x, true)");


	// ===================================================================
	// Test correct captures with let blocks
	// ===================================================================

	// Test that let blocks in the lambda expression function are ignored when computing the let block offset.
	testMainFloat("def main() float :						\n\
							let 							\n\
								a = 3.0						\n\
							in 								\n\
								let 						\n\
									f =\\() : 				\n\
										a 					\n\
								in f()						\n\
								", 3.0f);

	testMainFloat("def main() float :						\n\
							let 							\n\
								a = 3.0						\n\
							in 								\n\
								let 						\n\
									f =\\() : 				\n\
										let 				\n\
										in 					\n\
											a 				\n\
								in f()						\n\
								", 3.f);

	testMainFloat("def main() float :						\n\
							let 							\n\
								a = 3.0						\n\
							in 								\n\
								let 						\n\
									f =\\() : 				\n\
										let 				\n\
										in 					\n\
											let				\n\
											in				\n\
												a 			\n\
								in f()						\n\
								", 3.f);

	// Test correct capture of destructured let var
	testMainFloat("def main() float :						\n\
							let 							\n\
								a, b = (3.0, 4.0)			\n\
							in 								\n\
								let 						\n\
									f =\\() : 				\n\
										a 					\n\
								in f()						\n\
								", 3.0f);

	// Test correct capture of destructured let var
	testMainFloat("def main() float :						\n\
							let 							\n\
								a, b = (3.0, 4.0)			\n\
							in 								\n\
								let 						\n\
									f =\\() : 				\n\
										b 					\n\
								in f()						\n\
								", 4.0f);

	testMainFloatArg("def main(float x) float :								\n\
						let 												\n\
							a = x											\n\
							f = \\() !noinline : a		# captures 'a'		\n\
						in													\n\
							f()												\n\
								", 10.f, 10.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	testMainFloatArg("def main(float x) float :								\n\
						let 												\n\
							a = x											\n\
							b = x + 1.0f									\n\
							f = \\() !noinline : a + b		# captures 'a'	\n\
						in													\n\
							f()												\n\
								", 10.f, 21.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	

	// ===================================================================
	// Test capturing vars with nested lambdas
	// In this case 'a' in the inner lambda is lexically bound to the a let node
	// in main.  'a' is free in the inner lambda.  'a' is also free in the outer lambda.
	// When the outer lambda is capturing values (at runtime), it will capture the value of 'a', which it can get from
	// the let node.
	// When the inner lambda is capturing values (at runtime), it will need to capture the value of 'a' from the captured-vars struct
	// of the outer lambda!
	// ===================================================================
	results = testMainFloatArg("def main(float x) float :													\n\
								let 															\n\
									a = x														\n\
									f = \\() !noinline : 				# captures 'a'			\n\
										let 													\n\
											g = \\() !noinline : a		# captures 'a' as well	\n\
										in														\n\
											g()													\n\
								in																\n\
									f()															\n\
								", 10.f, 10.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testAssert(results.stats.num_closure_allocations == 2);
	testAssert(results.stats.num_free_vars_stored == 2);

	// Test nested lambdas with multiple variable capture
	results = testMainFloatArg("def main(float x) float :													\n\
								let 															\n\
									a = x														\n\
									b = x + 1.0f												\n\
									f = \\() !noinline :										\n\
										let 													\n\
											g = \\() !noinline : a + b							\n\
										in														\n\
											g()													\n\
								in																\n\
									f()															\n\
								", 
		10.f, 21.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testAssert(results.stats.num_closure_allocations == 2);
	testAssert(results.stats.num_free_vars_stored == 4);

	// Test nested lambdas with multiple variable capture - in this case only the inner lambda needs to 
	// capture 2 variables, the outer lambda only needs to capture 1.
	results = testMainFloatArg("def main(float x) float :													\n\
								let 															\n\
									a = x														\n\
									f = \\() !noinline :										\n\
										let 													\n\
											b = a + 1.0f										\n\
											g = \\() !noinline : a + b							\n\
										in														\n\
											g()													\n\
								in																\n\
									f()															\n\
								",
		10.f, 21.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testAssert(results.stats.num_closure_allocations == 2);
	testAssert(results.stats.num_free_vars_stored == 3);


	// Test a free variable that is used multiple times.
	// This should be captured just once, and the captured var referenced multiple times.
	results = testMainFloatArg("def main(float x) float :										\n\
								let 															\n\
									f = \\() !noinline :										\n\
										x + x + x												\n\
								in																\n\
									f()															\n\
								",
		10.f, 30.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testAssert(results.stats.num_closure_allocations == 1);
	testAssert(results.stats.num_free_vars_stored == 1);

	// Test multiple free variables that are used multiple times.
	results = testMainFloatArg("def main(float x) float :										\n\
								let 															\n\
									y = x + 1.0f												\n\
									f = \\() !noinline :										\n\
										x + x + x + y + y + y + y								\n\
								in																\n\
									f()															\n\
								",
		10.f, 74.f, INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);
	testAssert(results.stats.num_closure_allocations == 1);
	testAssert(results.stats.num_free_vars_stored == 2);


	// transforms to:

	testMainFloatArg("def main(float x) float :							\n\
								let 									\n\
									a = x								\n\
									f = \\() : 		# captures 'a'		\n\
										let 							\n\
											g = \\() : a	# captures 'a' as well	\n\
										in								\n\
											a							\n\
								in										\n\
									f()									\n\
								", 10.f, 10.f, INVALID_OPENCL);

	//then:

	testMainFloatArg("def main(float x) float :							\n\
								let 									\n\
									a = x								\n\
									f = \\() : 		# captures 'a'		\n\
										let 							\n\
											g = \\() : a	# captures 'a' as well	\n\
										in								\n\
											a							\n\
								in										\n\
									let 								\n\
										g = \\() : a	# captures 'a' as well	\n\
									in									\n\
										a								\n\
								", 10.f, 10.f, INVALID_OPENCL);





	testMainFloatArg("def main(float x) float :		\n\
								let 						\n\
									f = \\() : 				\n\
										let 				\n\
											g = \\() : x	\n\
										in					\n\
											g()				\n\
								in							\n\
									f()						\n\
								", 10.f, 10.f, INVALID_OPENCL);

	//inlining g:
	testMainFloatArg("def main(float x) float :		\n\
								let 						\n\
									f = \\() : 				\n\
										let 				\n\
											g = \\() : x	\n\
										in					\n\
											x				\n\
								in							\n\
									f()						\n\
								", 10.f, 10.f, INVALID_OPENCL);

	// inlining f:
	testMainFloatArg("def main(float x) float :		\n\
								let 						\n\
									f = \\() : 				\n\
										let 				\n\
											g = \\() : x	\n\
										in					\n\
											x				\n\
								in							\n\
									let 					\n\
										g = \\() : x	\n\
									in					\n\
										x				\n\
								", 10.f, 10.f, INVALID_OPENCL);

	// Test with ref-counted values:
	testMainFloatArg("def main(float x) float :							\n\
								let 									\n\
									a = [x + 1.0]va						\n\
									f = \\() : 		# captures 'a'		\n\
										let 							\n\
										g = \\() : a[0]		# captures 'a' as well	\n\
										in								\n\
											g()							\n\
								in										\n\
									f()									\n\
								", 10.f, 11.f, INVALID_OPENCL);

	// Test with ref-counted values:
	testMainFloatArg("def somefunc(float x) varray<float> :					\n\
								[x + 1.0]va									\n\
							def main(float x) float : somefunc(x)[0] + somefunc(x)[0]							\n\
							", 10.f, 22.f,  ALLOW_UNSAFE | INVALID_OPENCL);

	// Test with ref-counted values:
	testMainFloatArg("def somefunc(float x) varray<float> :							\n\
								let 									\n\
									a = [x + 1.0]va						\n\
								in										\n\
									a									\n\
							def main(float x) float : somefunc(x)[0] + somefunc(x)[0]							\n\
							", 10.f, 22.f,  ALLOW_UNSAFE | INVALID_OPENCL);

	// Test capture of ref-counted values.
	testMainFloatArg("def somefunc(float x) varray<float> :							\n\
								let 									\n\
									a = [x + 1.0]va						\n\
									f = \\() : 	a						\n\
								in										\n\
									f()									\n\
							def main(float x) float : somefunc(x)[0] + somefunc(x)[0]							\n\
							", 10.f, 22.f,  ALLOW_UNSAFE | INVALID_OPENCL);

	// Test capture of ref-counted values, captured twice:
	testMainFloatArg("def somefunc(float x) varray<float> :							\n\
								let 									\n\
									a = [x + 1.0]va						\n\
									f = \\() : 		# captures 'a'		\n\
										let 							\n\
										g = \\() : a		# captures 'a' as well	\n\
										in								\n\
											g()							\n\
								in										\n\
									f()									\n\
							def main(float x) float : somefunc(x)[0] + somefunc(x)[0]							\n\
							", 10.f, 22.f,  ALLOW_UNSAFE | INVALID_OPENCL);


	// ===================================================================
	// Test binding with let blocks and nested lambdas
	// ===================================================================
	testMainFloat("def main() float :		\n\
							let 							\n\
								a = 3.0						\n\
							in 								\n\
								let 						\n\
									f = \\() : 				\n\
										let 				\n\
											g = \\() :		\n\
												let			\n\
												in 			\n\
													a 		\n\
										in					\n\
											g()				\n\
								in f()						\n\
								", 3.f);

	// Test capturing of multiple variables in a lambda that is used more than once.
	// Note that this doesn't quite test what I wanted to test, which is the lambda being compiled twice
	// and the captured var struct coming out different due to std::set usage.
	testMainFloatArg("def func1(float x) !noinline float :		\n\
		let 										\n\
			float a = x								\n\
			int b = truncateToInt(x) + 1			\n\
		in 											\n\
			let 									\n\
				f = \\() !noinline : a + toFloat(b)			\n\
			in 										\n\
				f()									\n\
													\n\
		def g1(float x) !noinline : func1(x)					\n\
		def g2(float x) !noinline : func1(x)					\n\
		def main(float x) float : g1(x) + g2(x)			",
		1.0f, 6.0f, DISABLE_CONSTANT_FOLDING | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE | INVALID_OPENCL);
}


static void testStructs()
{
	// Test construction of a structure (ComplexPair) using a structure passed as an arg (Complex).
	// Exposed an issue with OpenCL and arg dereferencing.
	testMainFloatArg("struct Complex { float re, float im }		\n\
		struct ComplexPair { Complex a, Complex b }		\n\
		def f(Complex c) !noinline ComplexPair : ComplexPair(c, c)		\n\
 		def main(float x) float : f(Complex(x, 3.0)).a.im", 1.0f, 3.0f);


	// Test creation of struct
	{
		Float4Struct a(1.0f, -2.0f, 3.0f, -4.0f);
		Float4Struct target_result(std::sqrt(1.0f), std::sqrt(2.0f), std::sqrt(3.0f), std::sqrt(4.0f));
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def sqrt(Float4Struct f) : Float4Struct(sqrt(f.v))		\n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				let													\n\
					a = Float4Struct([1.0, 2.0, 3.0, 4.0]v)			\n\
				in													\n\
					sqrt(a)",
			a, a, target_result
		);
	}


	// Test parsing of empty structs
	testMainFloat("struct EmptyStruct { } \
				  def main() float : 2.0f", 2.0f);

	// Test struct
	testMainFloatArg("struct Complex { float re, float im } \
				  def main(float x) float : re(Complex(x, 3.0))", 2.0f, 2.0f);
	
	testMainFloat("struct Complex { float re, float im } \
 				  def main() float : im(Complex(2.0, 3.0))", 3.0f);

	// Test struct in struct
	testMainFloat("struct Complex { float re, float im } \
				  struct ComplexPair { Complex a, Complex b } \
				  def main() float : im(a(ComplexPair(Complex(2.0, 3.0), Complex(4.0, 5.0))))",
				  3.0f);

	// Test field access with '.' applied to a variable.
	testMainFloat("struct Complex { float re, float im } \n\
 				  def main() float : \n\
					let z = Complex(2.0, 3.0) in \n\
					z.im", 3.0f);

	// Test field access with '.' applied to a structure literal.
	testMainFloat("struct Complex { float re, float im } \n\
 				  def main() float : \n\
					Complex(2.0, 3.0).im", 3.0f);

	// Test field access with '.' applied to a function call expression.
	testMainFloat("struct Complex { float re, float im } \n\
				  def f() Complex : Complex(1.0, 2.0) \n\
 				  def main() float : \n\
					f().im", 2.0f);


	// Test field access for nested structures.
	testMainFloat("struct Complex { float re, float im } \n\
				  struct ComplexPair { Complex a, Complex b } \n\
				  def main() float : ComplexPair(Complex(2.0, 3.0), Complex(4.0, 5.0)).a.im",
				  3.0f);
	
	
	// ===================================================================
	// Test equality comparison
	// ===================================================================
	
	// Test ==
	testMainFloat("struct Complex { float re, float im } \n\
				  def eq(Complex a, Complex b) !noinline bool : a == b     \n\
				  def main() float : eq(Complex(1.0, 2.0), Complex(1.0, 3.0)) ? 1.0 : 2.0",
				  2.0f);

	testMainFloat("struct Complex { float re, float im } \n\
				  def eq(Complex a, Complex b) !noinline bool : a == b     \n\
				  def main() float : eq(Complex(1.0, 2.0), Complex(1.0, 2.0)) ? 1.0 : 2.0",
				  1.0f);

	// Test == without !noline to test winter interpreted execution/constant folding.
	testMainFloat("struct Complex { float re, float im }  \n\
				  def main() float : (Complex(1.0, 2.0) == Complex(1.0, 3.0)) ? 1.0 : 2.0", 2.0f);

	testMainFloat("struct Complex { float re, float im }  \n\
				  def main() float : (Complex(1.0, 2.0) == Complex(1.0, 2.0)) ? 1.0 : 2.0", 1.0f);
	
	// Test !=
	testMainFloat("struct Complex { float re, float im } \n\
				  def neq(Complex a, Complex b) !noinline bool : a != b     \n\
				  def main() float : neq(Complex(1.0, 2.0), Complex(1.0, 3.0)) ? 1.0 : 2.0",
				  1.0f);
				  
	testMainFloat("struct Complex { float re, float im } \n\
				  def neq(Complex a, Complex b) !noinline bool : a != b     \n\
				  def main() float : neq(Complex(1.0, 2.0), Complex(1.0, 2.0)) ? 1.0 : 2.0",
				  2.0f);

	// Test != without !noline to test winter interpreted execution/constant folding.
	testMainFloat("struct Complex { float re, float im }  \n\
				  def main() float : (Complex(1.0, 2.0) != Complex(1.0, 3.0)) ? 1.0 : 2.0", 1.0f);

	testMainFloat("struct Complex { float re, float im }  \n\
				  def main() float : (Complex(1.0, 2.0) != Complex(1.0, 2.0)) ? 1.0 : 2.0", 2.0f);

	
	// Test with a struct in a struct
	testMainFloat("struct A { int x }             \n\
				   struct B { int x }             \n\
				   struct S { A a, B b }          \n\
				  def eq(S a, S b) !noinline bool : a == b     \n\
				  def main() float : eq(S(A(1), B(2)), S(A(1), B(3))) ? 1.0 : 2.0",
				  2.0f);
	
	// Test != with a struct in a struct
	testMainFloat("struct A { int x }             \n\
				   struct B { int x }             \n\
				   struct S { A a, B b }          \n\
				  def neq(S a, S b) !noinline bool : a != b     \n\
				  def main() float : neq(S(A(1), B(2)), S(A(1), B(3))) ? 1.0 : 2.0",
				  1.0f);

	// Test with an array in a struct
	testMainFloat("struct S { array<float, 2> a }          \n\
				  def eq(S a, S b) !noinline bool : a == b     \n\
				  def main() float : eq(S([1.0, 2.0]a), S([1.0, 3.0]a)) ? 1.0 : 2.0",
				  2.0f);

	// Test with a string in a struct
	testMainFloat("struct S { string str } \n\
				  def eq(S a, S b) !noinline bool : a == b     \n\
				  def main() float : eq(S(\"abc\"), S(\"def\")) ? 1.0 : 2.0",
				  2.0f);

	testMainFloat("struct S { string str } \n\
				  def eq(S a, S b) !noinline bool : a == b     \n\
				  def main() float : eq(S(\"abc\"), S(\"abc\")) ? 1.0 : 2.0",
				  1.0f);
}


static void testVectors()
{	
	// Test vector
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v in\
					e0(x)", 1.0f);
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v in \
					e1(x)", 2.0f);

	// Test vector being returned from a function
	testMainFloat("	def f() vector<float, 4> : [1.0, 2.0, 3.0, 4.0]v \
					def main() float : e2(f())", 3.0f);

	testMainDoubleArg("def f() vector<double, 4> : [1.0, 2.0, 3.0, 4.0]v \
					def main(double x) double : e2(f())", 3.0f, 3.0f);

	testMainDoubleArg("def f(double x) vector<double, 4> : [x + 1.0, x + 2.0, x + 3.0, x + 4.0]v \
					def main(double x) double : e2(f(x))", 3.0f, 6.0f);

	// Test vector addition
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					y = [10.0, 20.0, 30.0, 40.0]v in\
					e1(x + y)", 22.0f);

	testMainFloatArg("def main(float x) float :  ([1.0, 2.0, 3.0, 4.0]v + [x, x, x, x]v)[1]", 20.0, 22.0);
	testMainDoubleArg("def main(double x) double :  ([1.0, 2.0, 3.0, 4.0]v + [x, x, x, x]v)[1]", 20.0, 22.0);


	// Test vector subtraction
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					y = [10.0, 20.0, 30.0, 40.0]v in \
					e1(x - y)", -18.0f);
	
	testMainFloatArg("def main(float x) float :  ([1.0, 2.0, 3.0, 4.0]v - [x, x, x, x]v)[1]", 20.0, -18);
	testMainDoubleArg("def main(double x) double :  ([1.0, 2.0, 3.0, 4.0]v - [x, x, x, x]v)[1]", 20.0, -18);


	// Test vector * float multiplication
	testMainFloat("	def main() float : \
				  let x = [1.0, 2.0, 3.0, 4.0]v in \
			  e1(x * 10.0)", 2.0f * 10.0f);

	testMainFloatArg("def main(float x) float :  ([1.0, 2.0, 3.0, 4.0]v * x)[1]", 10.0, 20.0);
	testMainDoubleArg("def main(double x) double :  ([1.0, 2.0, 3.0, 4.0]v * x)[1]", 10.0, 20.0);


	// Test vector * vector multiplication
	testMainFloat("	def main() float : \
				  let x = [1.0, 2.0, 3.0, 4.0]v \
				  y = [10.0, 20.0, 30.0, 40.0]v in\
				e1(x * y)", 2.0f * 20.0f);

	testMainFloatArg("def main(float x) float :  ([1.0, 2.0, 3.0, 4.0]v * [x, x, x, x]v)[1]", 10.0, 20.0);
	testMainDoubleArg("def main(double x) double :  ([1.0, 2.0, 3.0, 4.0]v * [x, x, x, x]v)[1]", 10.0, 20.0);


	// Test vector<int> * vector<int> multiplication
	testMainInteger("	def main() int : \
				  let x = [1, 2, 3, 4]v \
				  y = [10, 20, 30, 40]v in\
				e1(x * y)", 2 * 20);

	// Test vector * scalar multiplication
	testMainFloat("	def mul(vector<float, 4> v, float x) vector<float, 4> : v * [x, x, x, x]v \n\
					def main() float : \
						let x = [1.0, 2.0, 3.0, 4.0]v \
						y = 10.0 in \
						mul(x, y).e1", 2.0f * 10.0f);

	testMainFloatArg("	def mul(vector<float, 4> v, float x) vector<float, 4> : v * [x, x, x, x]v \n\
				  def main(float x) float : \
				  let v = [1.0, 2.0, 3.0, 4.0]v in\
				  e1(mul(v, x))", 10.0f, 2.0f * 10.0f);


	// Test vector<float, x> / float division
	testMainFloatArg("def main(float x) float : \
				let v = [1.0, 2.0, 3.0, 4.0]v in \
				e1(v / x)", 10.0, 2.0f / 10.0f);

	// Test vector<double, x> / double division
	testMainDoubleArg("def main(double x) double : \
				let v = [1.0, 2.0, 3.0, 4.0]v in \
				e1(v / x)", 10.0, 2.0f / 10.0f);


	testMainFloatArg("def main(float x) float :  ([1.0, 2.0, 3.0, 4.0]v * x)[1]", 10.0, 20.0);
	testMainDoubleArg("def main(double x) double :  ([1.0, 2.0, 3.0, 4.0]v * x)[1]", 10.0, 20.0);

	// Try dot product
	testMainFloatArg("	def main(float x) float : \
					 let v = [x, x, x, x]v in\
					 dot(v, v)", 2.0f, 16.0f);

	// OpenCL dot not supported for > 4 elems in vector
	testMainFloatArg("	def main(float x) float : \
					 let v = [x, x, x, x, x, x, x, x]v in\
					 dot(v, v)", 4.0f, 128.0f, INVALID_OPENCL);

	// Test dot product variants that use a variable number of components
	testMainFloatArg("def main(float x) float : dot1([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0f, 2.0f);
	testMainFloatArg("def main(float x) float : dot2([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0f, 6.0f);
	testMainFloatArg("def main(float x) float : dot3([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0f, 12.0f);
	testMainFloatArg("def main(float x) float : dot4([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0f, 20.0f);


	// Test dot product variants that use a variable number of components
	testMainDoubleArg("def main(double x) double : dot1([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0, 2.0);
	testMainDoubleArg("def main(double x) double : dot2([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0, 6.0);
	testMainDoubleArg("def main(double x) double : dot3([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0, 12.0);
	testMainDoubleArg("def main(double x) double : dot4([1.0, 2.0, 3.0, 4.0]v, [x, x, x, x]v)", 2.0, 20.0);

	// Test dppd instruction emission
	testMainDoubleArg("def main(double x) double : dot1([1.0, 2.0]v, [x, x]v)", 2.0, 2.0);
	testMainDoubleArg("def main(double x) double : dot2([1.0, 2.0]v, [x, x]v)", 2.0, 6.0);


	// Test vector min
	testMainFloat("	def main() float : \
					 let a = [1.0, 2.0, 3.0, 4.0]v \
					 b = [11.0, 12.0, 13.0, 14.0]v in\
					 e2(min(a, b))", 3.0);
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  b = [11.0, 12.0, 13.0, 14.0]v in \
				  e2(min(b, a))", 3.0);

	testMainFloatArg("def main(float x) float : min([1.0, 2.0]v, [3.0, x]v)[1]", 1.5, 1.5);
	testMainDoubleArg("def main(double x) double : min([1.0, 2.0]v, [3.0, x]v)[1]", 1.5, 1.5);

	// Test vector max
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  b = [11.0, 12.0, 13.0, 14.0]v in \
				  e2(max(a, b))", 13.0);
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  b = [11.0, 12.0, 13.0, 14.0]v in \
				  e2(max(b, a))", 13.0);
	
	testMainFloatArg("def main(float x) float : max([1.0, 2.0]v, [3.0, x]v)[1]", 1.5, 2.0);
	testMainDoubleArg("def main(double x) double : max([1.0, 2.0]v, [3.0, x]v)[1]", 1.5, 2.0);


	testMainFloat("	def clamp(vector<float, 4> x, vector<float, 4> lowerbound, vector<float, 4> upperbound) vector<float, 4> : max(lowerbound, min(upperbound, x))  \n\
					def make_v4f(float x) vector<float, 4> : [x, x, x, x]v  \n\
					def main() float : \
					let a = [1.0, 2.0, 3.0, 4.0]v in\
					e2(clamp(a, make_v4f(2.0), make_v4f(2.5)))", 2.5);

	testMainFloat("	struct PolarisationVec { vector<float, 8> e } \n\
																	\n\
								def make_v4f(float x) vector<float, 4> : [x, x, x, x]v  \n\
																		\n\
					def clamp(vector<float, 4> x, vector<float, 4> lowerbound, vector<float, 4> upperbound) vector<float, 4> : max(lowerbound, min(upperbound, x))  \n\
																																					\n\
					def clamp(PolarisationVec x, float lowerbound, float upperbound) PolarisationVec : \n\
						let lo = [e0(e(x)), e1(e(x)), e2(e(x)), e3(e(x))]v   \n\
						hi = [e4(e(x)), e5(e(x)), e6(e(x)), e7(e(x))]v   \n\
						clamped_lo = clamp(lo, make_v4f(lowerbound), make_v4f(upperbound))   \n\
						clamped_hi = clamp(hi, make_v4f(lowerbound), make_v4f(upperbound))  in \n\
						PolarisationVec([e0(clamped_lo), e1(clamped_lo), e2(clamped_lo), e3(clamped_lo), e0(clamped_hi), e1(clamped_hi), e2(clamped_hi), e3(clamped_hi)]v)   \n\
																																												\n\
				  def main() float : \
					let a = PolarisationVec([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v) in \
					e5(e(clamp(a, 2.0, 2.5)))", 2.5);

	

	// test type coercion on vectors: vector<float> initialisation with some int elems
	testMainFloatArg(
		"def main(float x) float: elem(   2.0 * [1.0, 2.0, 3, 4]v, 1)",
		1.0f, 4.0f);

	// float * Vector<float> multiplication
	testMainFloatArg(
		"def main(float x) float: elem(   2.0 * [1.0, 2.0, 3.0, 4.0]v, 1)",
		1.0f, 4.0f);

	// Vector<float> * float multiplication
	testMainFloatArg(
		"def main(float x) float: elem(   [1.0, 2.0, 3.0, 4.0]v * 2.0, 1)",
		1.0f, 4.0f);

	// int * Vector<int> multiplication
	testMainIntegerArg(
		"def main(int x) int: elem(   2 * [1, 2, 3, 4]v, 1)",
		1, 4);

	// Vector<int> * int multiplication
	testMainIntegerArg(
		"def main(int x) int: elem(   [1, 2, 3, 4]v * 2, 1)",
		1, 4);


	// float * Vector<float> multiplication
	testMainFloatArg(
		"def main(float x) float: elem(   x * [x, 2.0, 3.0, 4.0]v, 1)",
		2.0f, 4.0f);

	// Vector<float> * float multiplication
	testMainFloatArg(
		"def main(float x) float: elem(   [1.0, 2.0, 3.0, 4.0]v * x, 1)",
		2.0f, 4.0f);

	// int * Vector<int> multiplication
	testMainIntegerArg(
		"def main(int x) int: elem(   x * [1, 2, 3, 4]v, 1)",
		2, 4);

	// Vector<int> * int multiplication
	testMainIntegerArg(
		"def main(int x) int: elem(   [1, 2, 3, 4]v * x, 1)",
		2, 4);

	//================== Test '==' ============================

	// Test for vector<float, 2>
	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b   \n\
		def main(float x) float : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.0f, 1.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b   \n\
		def main(float x) float : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 0.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b   \n\
		def main(float x) float : f([1.0, x]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		2.0f, 1.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b   \n\
		def main(float x) float : f([1.0, x]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 0.0f);

	// Test for vector<float, 4>
	testMainFloatArg(
		"def f(vector<float, 4> a, vector<float, 4> b) !noinline bool : a == b   \n\
		def main(float x) float : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.0f, 1.0f);

	testMainFloatArg(
		"def f(vector<float, 4> a, vector<float, 4> b) !noinline bool : a == b   \n\
		def main(float x) float : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.5f, 0.0f);

	// Test for vector<float, 8>
	testMainFloatArg(
		"def f(vector<float, 8> a, vector<float, 8> b) !noinline bool : a == b   \n\
		def main(float x) float : f([x, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v) ? 1.0 : 0.0",
		1.0f, 1.0f);

	// Test for vector<double, 2>
	testMainDoubleArg(
		"def f(vector<double, 2> a, vector<double, 2> b) !noinline bool : a == b   \n\
		def main(double x) double : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.0f, 1.0f);

	testMainDoubleArg(
		"def f(vector<double, 2> a, vector<double, 2> b) !noinline bool : a == b   \n\
		def main(double x) double : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 0.0f);

	// Test for vector<double, 4>
	testMainDoubleArg(
		"def f(vector<double, 4> a, vector<double, 4> b) !noinline bool : a == b   \n\
		def main(double x) double : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.0f, 1.0f);

	testMainDoubleArg(
		"def f(vector<double, 4> a, vector<double, 4> b) !noinline bool : a == b   \n\
		def main(double x) double : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.5f, 0.0f);

	//================== Test '!=' ============================

	// Test for vector<float, 2>
	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a != b   \n\
		def main(float x) float : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.0f, 0.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a != b   \n\
		def main(float x) float : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 1.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a != b   \n\
		def main(float x) float : f([1.0, x]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		2.0f, 0.0f);

	testMainFloatArg(
		"def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a != b   \n\
		def main(float x) float : f([1.0, x]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 1.0f);

	// Test for vector<float, 4>
	testMainFloatArg(
		"def f(vector<float, 4> a, vector<float, 4> b) !noinline bool : a != b   \n\
		def main(float x) float : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.0f, 0.0f);

	testMainFloatArg(
		"def f(vector<float, 4> a, vector<float, 4> b) !noinline bool : a != b   \n\
		def main(float x) float : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.5f, 1.0f);

	// Test for vector<double, 2>
	testMainDoubleArg(
		"def f(vector<double, 2> a, vector<double, 2> b) !noinline bool : a != b   \n\
		def main(double x) double : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.0f, 0.0f);

	testMainDoubleArg(
		"def f(vector<double, 2> a, vector<double, 2> b) !noinline bool : a != b   \n\
		def main(double x) double : f([x, 2.0]v, [1.0, 2.0]v) ? 1.0 : 0.0",
		1.5f, 1.0f);

	// Test for vector<double, 4>
	testMainDoubleArg(
		"def f(vector<double, 4> a, vector<double, 4> b) !noinline bool : a != b   \n\
		def main(double x) double : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.0f, 0.0f);

	testMainDoubleArg(
		"def f(vector<double, 4> a, vector<double, 4> b) !noinline bool : a != b   \n\
		def main(double x) double : f([x, 2.0, 3.0, 4.0]v, [1.0, 2.0, 3.0, 4.0]v) ? 1.0 : 0.0",
		1.5f, 1.0f);

	// Test integer in-bounds runtime index access to vector
	/*testMainIntegerArg(
		"def main(int i) int :								\n\
			let												\n\
				a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]v		\n\
				b = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ]v		\n\
			in												\n\
				elem(a + b, i)",
		2, 4);

	

	// Test integer in-bounds runtime index access to vector
	testMainIntegerArg(
		"def main(int i) int :								\n\
			let												\n\
				a = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v		\n\
				b = [1.0, 1.6, 1.0, 1.0, 1.3, 1.6, 1.8, 1.6]v		\n\
				c = [1.7, 2.8, 3.0, 4.7, 5.5, 6.7, 7.0, 8.4]v		\n\
			in												\n\
				truncateToInt(elem(a + if(i < 1, b, c), i))",
		2, 6);

	//exit(1);//TEMP

	// Test integer in-bounds runtime index access to vector
	testMainIntegerArg(
		"def main(int i) int :								\n\
			let												\n\
				a = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0]v		\n\
				b = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 ]v		\n\
				c = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0]v		\n\
			in												\n\
				truncateToInt(elem(a + if(i < 1, b, c), i))",
		2, 6);
		*/
	

	// Test integer in-bounds runtime index access to vector
	/*TEMP NO OPENCL
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]v, i) else 0", 
		2, 3);
		*/
}


static void testFunctionRedefinition()
{
	// Disallow definition of same function (with same signature) twice.
	testMainFloatArgInvalidProgram("def f(float x) : x   def f(float x) : x     def main(float x) float : f(2.0)");
}


static void testUseOfLaterDefinition()
{
	testMainFloatArgInvalidProgram(
		"def main(float x) float : f(2.0f)  \n"
		"def f(float x) : x"
	);
}


// Test interfacing with C++ with functions that take and return bools.
static void testBoolFunctions()
{
	testMainBoolArg("def main(bool x) bool : x", true, true);
	testMainBoolArg("def main(bool x) bool : x", false, false);
	testMainBoolArg("def main(bool x) bool : true", false, true);
	testMainBoolArg("def main(bool x) bool : false", true, false);

	// For vector equality, LLVM is generating code that is returning 255 in RAX, not 1.
	testMainBoolArg("def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b       \n\
		def main(bool x) bool : [1.0f, 2.0f]v == [1.0f, 2.0f]v", false, true);

	testMainBoolArg("def f(vector<float, 2> a, vector<float, 2> b) !noinline bool : a == b       \n\
		def main(bool x) bool : [1.0f, 2.0f]v == [1.0f, 3.0f]v", false, false);
}


// Tests
// if(true, a, b)  =>  a
// and
// if(false, a, b)  =>  b
static void testIfSimplification()
{
	TestResults results;
	results = testMainFloatArg("def main(float x) float : if true then x*3.1 else x+2.0", 1.f, 3.1f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType);
	
	results = testMainFloatArg("def main(float x) float : if (1 != 2) then x*3.1 else x+2.0", 1.f, 3.1f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType);
	
	results = testMainFloatArg("def main(float x) float : if false then x*3.1 else x+2.0", 1.f, 3.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::AdditionExpressionType);

	// Test an if expression inside some arithmetic expressions
	results = testMainFloatArg("def main(float x) float : 10.0 + (if true then x*3.1 else x+2.0)", 1.f, 13.1f);
	testAssert(results.maindef->body->nodeType() == ASTNode::AdditionExpressionType);
	testAssert(results.maindef->body.downcast<AdditionExpression>()->b->nodeType() == ASTNode::MulExpressionType);

	results = testMainFloatArg("def main(float x) float : 10.0 * (if true then x*3.1 else x+2.0)", 1.f, 31.0f);
	testAssert(results.maindef->body->nodeType() == ASTNode::MulExpressionType);
	testAssert(results.maindef->body.downcast<MulExpression>()->b->nodeType() == ASTNode::MulExpressionType);

	// Test in an array literal
	results = testMainFloatArg("def main(float x) float : [(if true then x*3.1 else x+2.0)]a[0]", 1.f, 3.1f, INVALID_OPENCL);
	testAssert(results.maindef->body->nodeType() == ASTNode::FunctionExpressionType); // elem()
	testAssert(results.maindef->body.downcast<FunctionExpression>()->argument_expressions[0]->nodeType() == ASTNode::ArrayLiteralType); // elem()
	ArrayLiteralRef array_lit = results.maindef->body.downcast<FunctionExpression>()->argument_expressions[0].downcast<ArrayLiteral>();
	testAssert(array_lit->getElements().size() == 1);
	testAssert(array_lit->getElements()[0]->nodeType() == ASTNode::MulExpressionType);

	// Test in let var expr
	results = testMainFloatArg("def main(float x) float : let a = (if true then x*3.1 else x+2.0) in a", 1.f, 3.1f);
	{
		testAssert(results.maindef->body->nodeType() == ASTNode::LetBlockType);
		LetBlockRef let_block = results.maindef->body.downcast<LetBlock>();
		testAssert(let_block->lets.size() == 1 && let_block->lets[0]->expr->nodeType() == ASTNode::MulExpressionType);
	}

	// Test in let block body
	results = testMainFloatArg("def main(float x) float : let a = x in (if true then a*3.1 else a+2.0)", 1.f, 3.1f);
	{
		testAssert(results.maindef->body->nodeType() == ASTNode::LetBlockType);
		LetBlockRef let_block = results.maindef->body.downcast<LetBlock>();
		testAssert(let_block->expr->nodeType() == ASTNode::MulExpressionType);
	}
}


static void testTimeBounds()
{
	const size_t sin_time = 30; // See SinBuiltInFunc::getTimeBound().

	const size_t single_sin_time = getTimeBoundForMainFloatArg("def main(float x) float : sin(x)");
	testAssert(single_sin_time >= sin_time);

	{
		const size_t recursive_sin_time = getTimeBoundForMainFloatArg(
			"def f(float x) float : sin(x)								\n\
			def f4(float x) float : f(f(f(f(x))))						\n\
			def f16(float x)float :  f4(f4(f4(f4(x))))					\n\
			def f64(float x)float :  f16(f16(f16(f16(x))))				\n\
			def f256(float x) float : f64(f64(f64(f64(x))))				\n\
			def f1024(float x)float :  f256(f256(f256(f256(x))))		\n\
			def f4096(float x)float :  f1024(f1024(f1024(f1024(x))))	\n\
			def main(float x) float : f4096(x)");
		testAssert(recursive_sin_time >= sin_time * 4096);
	}
	{
		const size_t recursive_sin_time = getTimeBoundForMainFloatArg(
			"def f(float x) float : sin(x)														\n\
			def f4(float x) float : f(x) + f(x + 0.01)	+ f(x + 0.02) + f(x + 0.03)				\n\
			def f16(float x) float : f4(x) + f4(x + 0.01)	+ f4(x + 0.02) + f4(x + 0.03)		\n\
			def f64(float x) float : f16(x) + f16(x + 0.01)	+ f16(x + 0.02) + f16(x + 0.03)		\n\
			def main(float x) float : f64(x)");
		testAssert(recursive_sin_time >= sin_time * 64);
	}


	// For the following code: fn ultimately calls sin(x) 4^n times.
	{
		// So f10 calls sin(x) 4^10 = 1048576 times.
		const size_t recursive_sin_time = getTimeBoundForMainFloatArg(
			"def f0(float x) float : sin(x)						\n\
			def f1 (float x) float : f0(f0(f0(f0(x))))			\n\
			def f2 (float x) float : f1(f1(f1(f1(x))))			\n\
			def f3 (float x) float : f2(f2(f2(f2(x))))			\n\
			def f4 (float x) float : f3(f3(f3(f3(x))))			\n\
			def f5 (float x) float : f4(f4(f4(f4(x))))			\n\
			def f6 (float x) float : f5(f5(f5(f5(x))))			\n\
			def f7 (float x) float : f6(f6(f6(f6(x))))			\n\
			def f8 (float x) float : f7(f7(f7(f7(x))))			\n\
			def f9 (float x) float : f8(f8(f8(f8(x))))			\n\
			def f10(float x) float : f9(f9(f9(f9(x))))			\n\
			def main(float x) float : f10(x)");
		testAssert(recursive_sin_time >= sin_time * 1048576);
	}

	// Test something that will fail getTimeBound() due to too many recursive calls.
	{
		// f12 calls sin(x) 4^12 = 16,777,216 times.
		testTimeBoundInvalidForMainFloatArg(
			"def f0(float x) float : sin(x)							\n\
			def f1 (float x) float : f0(f0(f0(f0(x))))				\n\
			def f2 (float x) float : f1(f1(f1(f1(x))))				\n\
			def f3 (float x) float : f2(f2(f2(f2(x))))				\n\
			def f4 (float x) float : f3(f3(f3(f3(x))))				\n\
			def f5 (float x) float : f4(f4(f4(f4(x))))				\n\
			def f6 (float x) float : f5(f5(f5(f5(x))))				\n\
			def f7 (float x) float : f6(f6(f6(f6(x))))				\n\
			def f8 (float x) float : f7(f7(f7(f7(x))))				\n\
			def f9 (float x) float : f8(f8(f8(f8(x))))				\n\
			def f10(float x) float : f9(f9(f9(f9(x))))				\n\
			def f11(float x) float : f10(f10(f10(f10(x))))			\n\
			def f12(float x) float : f11(f11(f11(f11(x))))			\n\
			def main(float x) float : f12(x)");
	}

	{
		// f20 calls sin(x) 4^20 = 1,099,511,627,776 times.
		testTimeBoundInvalidForMainFloatArg(
			"def f0(float x) float : sin(x)							\n\
			def f1 (float x) float : f0(f0(f0(f0(x))))				\n\
			def f2 (float x) float : f1(f1(f1(f1(x))))				\n\
			def f3 (float x) float : f2(f2(f2(f2(x))))				\n\
			def f4 (float x) float : f3(f3(f3(f3(x))))				\n\
			def f5 (float x) float : f4(f4(f4(f4(x))))				\n\
			def f6 (float x) float : f5(f5(f5(f5(x))))				\n\
			def f7 (float x) float : f6(f6(f6(f6(x))))				\n\
			def f8 (float x) float : f7(f7(f7(f7(x))))				\n\
			def f9 (float x) float : f8(f8(f8(f8(x))))				\n\
			def f10(float x) float : f9(f9(f9(f9(x))))				\n\
			def f11(float x) float : f10(f10(f10(f10(x))))			\n\
			def f12(float x) float : f11(f11(f11(f11(x))))			\n\
			def f13(float x) float : f12(f12(f12(f12(x))))			\n\
			def f14(float x) float : f13(f13(f13(f13(x))))			\n\
			def f15(float x) float : f14(f14(f14(f14(x))))			\n\
			def f16(float x) float : f15(f15(f15(f15(x))))			\n\
			def f17(float x) float : f16(f16(f16(f16(x))))			\n\
			def f18(float x) float : f17(f17(f17(f17(x))))			\n\
			def f19(float x) float : f18(f18(f18(f18(x))))			\n\
			def f20(float x) float : f19(f19(f19(f19(x))))			\n\
			def main(float x) float : f20(x)");
	}
}


static float testExternalFunc(float x)
{
	return x * x;
}


static ValueRef testExternalFuncInterpreted(const std::vector<ValueRef>& arg_values)
{
	testAssert(arg_values.size() == 1);
	testAssert(arg_values[0]->valueType() == Value::ValueType_Float);

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());

	return new FloatValue(testExternalFunc(float_val->value));
}


static void testExternalFuncVec2(float* ret, float x)
{
	ret[0] = x;
	ret[1] = x*x;
}


static ValueRef testExternalFuncVec2Interpreted(const std::vector<ValueRef>& arg_values)
{
	testAssert(arg_values.size() == 1);
	testAssert(arg_values[0]->valueType() == Value::ValueType_Float);

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());

	std::vector<ValueRef> elem_vals;
	elem_vals.push_back(new FloatValue(float_val->value));
	elem_vals.push_back(new FloatValue(float_val->value * float_val->value));
	return new StructureValue(std::vector<ValueRef>(1, new VectorValue(elem_vals)));
}


static void testExternalFuncs()
{
	TestResults results;

	// Test with testExternalFunc
	{
		ExternalFunctionRef f = new ExternalFunction(
			(void*)testExternalFunc,
			testExternalFuncInterpreted,
			FunctionSignature("testExternalFunc", std::vector<TypeVRef>(1, new Float())),
			new Float(), // ret type
			256, // time bound
			256, // stack size bound
			0 // heap size bound
		);
		const std::vector<ExternalFunctionRef> external_functions(1, f);


		// Test call to external function
		results = testMainFloatArg("def main(float x) float : testExternalFunc(x)", 5.0f, 25.0f, INVALID_OPENCL, &external_functions);
		testAssert(results.stats.final_num_llvm_function_calls == 1);

		// Check that we can do constant folding even with an external expression
		results = testMainFloatArgCheckConstantFolded("def main(float x) float : testExternalFunc(3.0)", 5.0f, 9.0f, INVALID_OPENCL, &external_functions);
		testAssert(results.stats.final_num_llvm_function_calls == 0);

		// Check that only a single testExternalFunc(x) call is made.
		// NOTE that because testExternalFunc does not have a SRET arg, the calls can be combined.
		results = testMainFloatArg("def main(float x) float : testExternalFunc(x) + testExternalFunc(x)", 5.0f, 50.0f, INVALID_OPENCL, &external_functions);
		testAssert(results.stats.final_num_llvm_function_calls == 1);
	}


	// Test with testExternalFuncVec2
	{
		const std::vector<std::string> vec_elem_names(1, "v");
		const StructureTypeVRef vec2_struct_type = new StructureType("vec2", std::vector<TypeVRef>(1, new VectorType(new Float(), 2)), vec_elem_names);

		ExternalFunctionRef f = new ExternalFunction(
			(void*)testExternalFuncVec2,
			testExternalFuncVec2Interpreted,
			FunctionSignature("testExternalFuncVec2", std::vector<TypeVRef>(1, new Float())),
			vec2_struct_type, // ret type
			256, // time bound
			256, // stack size bound
			0 // heap size bound
		);
		const std::vector<ExternalFunctionRef> external_functions(1, f);

		
		// Test call to external function
		//results = testMainFloatArg("struct vec2 { vector<float, 2> v }   def main(float x) float : testExternalFuncVec2(x).v[0]", 5.0f, 5.0f, INVALID_OPENCL, &external_functions);
		//testAssert(results.stats.num_llvm_function_calls == 1);
		//
		//results = testMainFloatArg("struct vec2 { vector<float, 2> v }   def main(float x) float : testExternalFuncVec2(x).v[1]", 5.0f, 25.0f, INVALID_OPENCL, &external_functions);

		// Check that we can do constant folding even with an external expression
		results = testMainFloatArg("struct vec2 { vector<float, 2> v }   def main(float x) float : testExternalFuncVec2(3.0f).v[1]", 5.0f, 9.0f, INVALID_OPENCL, &external_functions);
		testAssert(results.stats.final_num_llvm_function_calls == 0);

		// Test with the same call expression twice.
		results = testMainFloatArg("struct vec2 { vector<float, 2> v }   def main(float x) float : testExternalFuncVec2(x).v[0] + testExternalFuncVec2(x).v[0]", 5.0f, 10.0f, INVALID_OPENCL, &external_functions);
		
		// NOTE: Ideally this would only emit 1 call.  Due to the SRET arg of testExternalFuncVec2, they can't be combined currently.
		testAssert(results.stats.final_num_llvm_function_calls == 2);

	}
}


static void testConstantFolding()
{
	TestResults results;

	// Test constant folding of a function that returns a struct, to a struct constructor
	testMainFloatArgCheckConstantFolded(
		"struct S { float y }										\n\
		def f(float x) !noinline S : S(x + 1.0f)					\n\
		def main(float x) float : y(f(1.0f))", 1.f, 2.f);

	// Test constant folding of an expression that has as a child a struct constructor
	testMainFloatArgCheckConstantFolded(
		"struct S { float y }										\n\
		def main(float x) float : y(S(1.0f))", 1.f, 1.f);

	// Check basic constant folding, 1.0 + 2.0 should be replaced with 3.0.
	testMainFloatArgCheckConstantFolded("def main(float x) float : 1.0 + 2.0", 1.f, 3.f);

	// Test constant folding with a vector literal that is not well typed (has boolean element).
	// Make sure that constant folding doesn't allow it.
	testMainFloatArgInvalidProgram("def main(float x) float : elem(  [2.0, false]v   , 0)");

	// Check constant folding with elem() and collections
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]v, 1)", 1.f, 2.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]t, 1)", 1.f, 2.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]a, 1)", 1.f, 2.f);

	// Check constant folding with elem() and collections, and operations on collections
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]v + [3.0, 4.0]v, 1)", 1.f, 6.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]v - [3.0, 4.0]v, 1)", 1.f, -2.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float : elem([1.0, 2.0]v * [3.0, 4.0]v, 1)", 1.f, 8.f);


	// Check constant folding for expression involving a let variable
	testMainFloatArgCheckConstantFolded("def main(float x) float :  let y = 2.0 in y", 1.f, 2.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float :  let y = 1.0 + 1.0 in y", 1.f, 2.f);
	testMainFloatArgCheckConstantFolded("def main(float x) float :  let y = (1.0 + 1.0) in pow(y, 3.0)", 1.f, 8.f);


	// Check constant folding of a pow function that is inside another function
	testMainFloatArgCheckConstantFolded("def g(float x) float :  pow(2 + x, -(1.0 / 8.0))         def main(float x) float : g(0.5)", 1.f, std::pow(2.f + 0.5f, -(1.0f / 8.0f)));

	testMainFloatArgCheckConstantFolded("def g(float x, float y) float : pow(x, y)         def main(float x) float : g(2.0, 3.0)", 1.f, 8.0f);
}


void LanguageTests::run()
{
	Timer timer;
	TestResults results;

	Lexer::test();

	//useKnownReturnRefCountOptimsiation(3);

	//fuzzTests();
	//////////////////


	// ===================================================================
	// Check equality for types, and that they work in sets etc.. correctly.
	// ===================================================================
	const TypeRef ta = new Int();
	const TypeRef tb = new Int();

	testAssert(*ta == *tb);

	{
		std::set<TypeRef, TypeRefLessThan> type_set;
		type_set.insert(ta);
		type_set.insert(tb);
		testAssert(type_set.size() == 1);
	}

	//testMainFloatArgInvalidProgram("struct s { s a, s b } def main(float x) float : x");
	//testMainFloatArgInvalidProgram("def main(float x) float : let varray<T> v = [v]va in x");
	
	// ===================================================================
	// 
	// ===================================================================
	//testMainFloatArgAllowUnsafe("def main(float x) float : let A = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]a   i = truncateToInt(x) in A[i] + A[i+1]", 2.f, 7.0f);
	//testMainFloatArgAllowUnsafe("A = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]a       def main(float x) float : let i = truncateToInt(x) in A[i] + A[i+1] + A[i+2]", 2.f, 12.0f);

	// Test map with more elems
	/*{
		const size_t N = 1 << 28;
		js::Vector<float, 32> input(N, 4.0f);
		js::Vector<float, 32> target_results(N, 16.f);//std::pow(4.0f, 2.2f));//std::sqrt(std::sqrt(std::sqrt(4.0))));


		testFloatArray(
			"def f(float x) float : pow(x, 2.2)			\n\
			def main(array<float, 268435456> a, array<float, 268435456> b) array<float, 268435456> : map(f, a)",
			&input[0], &input[0], &target_results[0], N);

		// Reference c++ code:
		{
			js::Vector<float, 32> output(N);

			

			// Warm up cache, page table etc..
			std::memcpy(&output[0], &input[0], N * sizeof(float));

			double sum1 = 0;
			for(size_t i=0; i<N; ++i)
				sum1 += output[i];

			Timer timer;

			//float sum = 0;
			//for(size_t i=0; i<N; ++i)
			//	sum += input[i];
			for(size_t i=0; i<N; ++i)
				output[i] = std::pow(input[i], 2.2f);//std::sqrt(std::sqrt(std::sqrt(input[i])));
			//std::memcpy(&output[0], &input[0], N * sizeof(float));

			const double elapsed = timer.elapsed();

			double sum = 0;
			for(size_t i=0; i<N; ++i)
				sum += output[i];

			std::cout << "C++ ref elapsed: " << (elapsed * 1.0) << " s" << std::endl;
			const double bandwidth = N * sizeof(float) / elapsed;
			std::cout << "C++ ref bandwidth: " << (bandwidth * 1.0e-9) << " GiB/s" << std::endl;
			std::cout << "sum1: " << sum << std::endl;
			std::cout << "sum: " << sum << std::endl;
		}
	}*/

	//testMainStringArg("def main(string s) string : s", "hello", "hello");
	//testMainStringArg("def main(string s) string : \"hello\"", "bleh", "hello");


	/*

	want to prove:
	T: index >= 0 && index < len(f(x))

	substitute
	index <- 0

	get T_1:
	T_1: 0 >= 0 && 0 < len(f(x))
	=  true && 0 < len(f(x))
	=  0 < len(f(x))

	Use postcondition of f:  len(res) = 1

	T' becomes
	0 < 1
	true


	{ len(res) = 1 }

	----------------------------
	valid =
	0 >= 0 && 0 < len(f(x))
	true && 0 < len(f(x))
	0 < len(f(x))

	[use len(f(x)) = 1]
	0 < 1
	true



	----------------------------------
	From "def f(int i) varray<float> : [i, i, i, i]va			def main(int x) int : if i > 0 && i < 4 then f(x)[i] else 0", 4, 4);:

	Want to prove:

	i >= 0 && i < len(f(x))

	[since in true branch, we know i > 0 && i < 3, in other words i e [0, 3]]

	true && i < len(f(x))
	i < len(f(x))

	[use len(f(x)) = 4]

	i < 4

	true

	--------------------------------------------------
	*/

	testConstantFolding();

	testExternalFuncs();

	testIfSimplification();

	doCKeywordTests();
	
	doUnsignedIntegerTests();
	
	doBitWiseOpTests();

	doHexLiteralParsingTests();

	testVariableShadowing();

	testFunctionInlining();

	testDeadCodeElimination();

	testDeadFunctionElimination();
	
	testArrays();

	testVArrays();

	testToIntFunctions();

	testStringFunctions();
	
	testTernaryConditionalOperator();

	testDestructuringAssignment();

	testLengthFunc();

	testFuzzingIssues();

	testLogicalNegation();

	testNamedConstants();
	
	testFold();

	testIterate();

	testTuples();

	testTypeCoercion();

	testElem();

	testIfThenElse();

	stringTests();
	
	testMathsFunctions();
	
	testExternalMathsFunctions();

	testOperatorOverloading();

	testComparisons();

	testLetBlocks();
	
	testLambdaExpressionsAndClosures();
	
	testStructs();

	testBoolFunctions();

	testVectors();

	testFunctionRedefinition();

	testUseOfLaterDefinition();

	testTimeBounds();

	testMainFloatArgInvalidProgram("def g(float x) fl: 1 / x         def main(float x) float : g(2)");

	// TODO: why is this vector being parsed as doubles?
	//testMainFloatArgInvalidProgram("def main(float x): elem(-[1.0, 2.0, 3.0, 4.0]v, 2)");//, 1.0f, -3.0f, 0);

	testMainFloatArgInvalidProgram("def makeFunc(float x) function<float, float> :\\(float y) : x + y       def main() float :  let f = makeFunc(2.0) in f(true)");

	testMainFloatArgInvalidProgram("def main(int x) int : if x < 5 if x < 3 1 else 2 ( ) else if x < 7 6 else 7 ");
	testMainFloatArgInvalidProgram("def main(float x) float : (1 * (1 * ((1 + 1) * (1 - 1 + 1)) - 1 - ((1 + (((((1 + 		 1 / (1 + (((1 + 1 / (1 * (1 - 1 + 1 / 1 / 1))) - 1 - 1 - (1 + 1 / 1) + (((1 * 1 		) * 1) * 1) / 1) - 1 + 1)) - 1 / 1) + (1 * 1)) + 1 - 1) + 1) * 1)) + 1) / (1 + 1 		) / 1 - ((1 + 1 / 1 - (((1 * ((1 - 1 - 1 / 1 - 1 * 1) - (1 / 1 - 1 * 1) + ((1 +	 		1) - 1 * 1) - 1 / 1)) + (1 / 1 / 1 - 1 - (1 + 1) / (1 + 1) + 1)) * (1 * ((((1 +	 		1) * 1) / 1 - 1 - 1 - (((1 + 1) * 1 - (((1 / 1 * 1) * (1 + 1) - 1) / 1 / 1 / 1 - 		 (1 + (((1 + 1) + 1) * 1)) - 1 / ((1 + 1) / (1 * 1) / (1 + (1 / 1 * 1 / 1 - 1 -	 		(1 * 1) / (1 + 1) - 1 - 1 - 1 - 1 - 1 / 1)) / (1 / 1 * 1) + (((1 + 1) * ((1 + 1) 		 * 1)) / 1 + ((1 - ((1 * 1) + 1) + 1) + 1 - 1 / ((1 / (1 - ((1 + 1) - ((((1 - (( 		1 * ((1 * ((1 + 1) + ((1 - (1 * 1) + (1 - (1 - 1 - (1 * 1) / (1 + 1) / 1 * (1 -	 		1 + (1 + 1) - ((1 / 1 / 1 + 1) * 1) - (1 - 1 * 1))) * 1) - 1 - 1 - 1 - (1 * (((1 		 * 1) * ((1 + (1 / (1 * 1) * 1 - 1)) * (1 + (((1 + 1) + 1) + 1)))) + 1) / 1)) +	 		1 / 1) / 1 - 1)) * 1 - 1)) / 1 * (1 * 1) - 1 - 1 / (1 * ((((1 + 1) + 1) * 1 - 1	 		- 1 / ((1 + 1) / (1 * 1) - ((1 / 1 + 1) * 1) + (1 + (1 * 1) - 1 - 1 - 1 - (1 + ( 		1 + 1))) / 1)) - 1 * (1 / ((1 * 1) + ((1 - 1 + (1 + 1)) + ((1 + 1) * 1))) / 1 *	 		1)) - 1) / 1 / 1 / 1) / 1 + 1) / (1 / (1 + (1 + (1 * 1 - 1)) - 1) / 1 - ((1 * 1	 		/ 1 / (1 / (1 * 1 / 1 - 1) / 1 * 1) / 1) - 1 * ((1 + (((1 + 1) + 1) - ((1 * ((1	 		+ 1) + 1)) * ((1 * 1) * 1) - 1) / 1 / (1 - 1 / (1 - 1 * 1) / 1 * 1) / ((1 - (1 + 		 1) / 1 * 1) + 1 - (1 + (1 * 1)) - 1) - 1 * 1) - 1 / 1) * 1)) + (((1 - (1 / 1 -	 		1 + 1 - (1 * (1 + 1)) / (1 + (1 + 1))) + 1) - ((1 + 1) * (1 * 1 / 1)) * 1 - 1) + 		 1) - 1 / 1 - (((1 * 1 - 1) + 1) - (1 + 1) - 1 - 1 * 1)) / (1 + 1) + (((1 * ((1	 		/ (1 * 1) * 1) * 1)) * 1) + 1 - (1 * 1)) / 1) * 1) + (1 / 1 * 1 / (1 * (1 + (1 / 		 1 / 1 - ((1 + ((1 - 1 / 1 + 1 / 1 - 1) * 1)) * 1) - 1 / 1 / 1 / (1 + 1) - (1 +	 		((1 + (((1 + 1 - 1 / ((1 + 1) * (1 + (1 / 1 + (1 + 1))))) * 1) * ((1 - 1 / (1 +	 		1 - (((1 / 1 - 1 / 1 * (1 + (((1 * 1) * (1 - 1 * (1 * 1))) - 1 + ((1 * 1) * ((1	 		- 1 * 1) / 1 + (1 + 1)))))) * 1) + 1 - 1 - 1 / (1 + ((1 * (1 + 1)) * 1)))) / (1	 		+ 1 - 1) + 1) - 1 * 1) / 1 - 1 / 1 - ((((1 + (1 * 1 - 1)) * ((1 + (1 * 1)) * 1)) 		 * ((1 + 1) + 1)) + 1) / 1 / 1 - ((1 / 1 - (((1 + (1 * 1) - 1) - 1 * 1 - (1 + 1) 		 - 1) + 1) + (1 / (1 + (1 * 1)) / 1 * 1 / 1) / (1 + 1)) * 1 / 1 - 1) - 1 - 1 - 1 		) / 1) * 1) / 1) + 1)) / 1 / 1)) / 1) * ((((1 / 1 + 1) - 1 * 1 / (1 + 1)) * 1 /	 		1) / 1 * 1) / 1 / 1 / 1 - ((1 * (((1 / 1 * (1 * (1 / ((1 - 1 * (1 * 1 / 1)) + (( 		1 * (1 + 1)) * 1)) * (1 + (1 + (1 * (1 + 1 / 1 - (1 - 1 * ((1 + 1) + 1 - (1 * 1) 		) / ((1 - 1 + 1) - 1 + 1)))) - 1 / (1 + 1) / 1 / 1 / 1 / (1 + (1 * (1 - (1 * 1 / 		 1) * 1))) / 1 - 1 - 1))) / ((1 + 1) + 1 / (1 * 1) / 1 / (1 * 1 / 1) - 1 - 1 - 1 		 / (1 * ((1 / 1 - 1 / 1 - 1 / 1 / 1 / 1 / 1 * 1) - 1 / (1 - ((1 / 1 + 1) + 1) -	 		1 / (1 * (1 + 1)) / 1 * (1 * 1) / 1) / 1 * 1)) / (1 + 1) / (((1 - 1 * 1) + (1 -	 		(1 + 1) + 1 / 1)) * 1)(1 / 1 / 1 * 1) - (1 - ((1 * (1 - (1 + 1) + 1)) * (1 +	 		1)) / 1 / 1 + (1 - 1 - (1 + (1 * 1)) - ((1 + 1 - ((1 + ((1 * 1) * 1 - 1 / 1)) +	 		1) - 1) / 1 - 1 + 1) / (((1 * 1) + 1) + 1) * (1 * 1)))) / 1 / ((1 * 1) + 1 - 1)) 		 / 1) * 1) * 1)) * ((1 - (1 * 1 / 1) + 1) * 1 / 1)) / (1 + 1) / 1) + (1 / 1 * (1 		 + 1 / 1 - (1 / (1 - 1 + 1) + 1) / (1 + 1) - (1 * 1)) - (((1 / 1 + 1 - 1 - 1) /	 		1 * 1) + (1 * 1) / (1 / 1 * 1) - 1 - (1 + 1))) / 1) + 1) + 1)) / 1 / 1)) + 1) -	 		1 - (1 * 1 - 1)) * ((1 + 1) * 1)) - 1 * (1 + ((1 * 1) + (1 * 1)))) - 1 * 1)))) * 		 1) - 1 - 1 - 1))");
	testMainFloatArgInvalidProgram("def main(float x) float : (1 * (1 * ((1 + 1) * (1 - 1 + 1)) - 1 - ((1 + (((((1 + 		 1 / (1 + (((1 + 1 / (1 * (1 - 1 + 1 / 1 / 1))) - 1 - 1 - (1 + 1 / 1) + (((1 * 1 		) * 1) * 1) / 1) - 1 + 1)) - 1 / 1) + (1 * 1)) + 1 - 1) + 1) * 1)) + 1) / (1 + 1 		) / 1 - ((1 + 1 / 1 - (((1 * ((1 - 1 - 1 / 1 - 1 * 1) - (1 / 1 - 1 * 1) + ((1 +	 		1) - 1 * 1) - 1 / 1)) + (1 / 1 / 1 - 1 - (1 + 1) / (1 + 1) + 1)) * (1 * ((((1 +	 		1) * 1) / 1 - 1 - 1 - (((1 + 1) * 1 - (((1 / 1 * 1) * (1 + 1) - 1) / 1 / 1 / 1 - 		 (1 + (((1 + 1) + 1) * 1)) - 1 / ((1 + 1) / (1 * 1) / (1 + (1 / 1 * 1 / 1 - 1 -	 		(1 * 1) / (1 + 1) - 1 - 1 - 1 - 1 - 1 / 1)) / (1 / 1 * 1) + (((1 + 1) * ((1 + 1) 		 * 1)) / 1 + ((1 - ((1 * 1) + 1) + 1) + 1 - 1 / ((1 / (1 - ((1 + 1) - ((((1 - (( 		1 * ((1 * ((1 + 1) + ((1 - (1 * 1) + (1 - (1 - 1 - (1 * 1) / (1 + 1) / 1 * (1 -	 		1 + (1 + 1) - ((1 / 1 / 1 + 1) * 1) - (1 - 1 * 1))) * 1) - 1 - 1 - 1 - (1 * (((1 		 * 1) * ((1 + (1 / (1 * 1) * 1 - 1)) * (1 + (((1 + 1) + 1) + 1)))) + 1)) / 1)) +	 		1 / 1) / 1 - 1)) * 1 - 1)) / 1 * (1 * 1) - 1 - 1 / (1 * ((((1 + 1) + 1) * 1 - 1	 		- 1 / ((1 + 1) / (1 * 1) - ((1 / 1 + 1) * 1) + (1 + (1 * 1) - 1 - 1 - 1 - (1 + ( 		1 + 1))) / 1)) - 1 * (1 / ((1 * 1) + ((1 - 1 + (1 + 1)) + ((1 + 1) * 1))) / 1 *	 		1)) - 1) / 1 / 1 / 1) / 1 + 1) / (1 / (1 + (1 + (1 * 1 - 1)) - 1) / 1 - ((1 * 1	 		/ 1 / (1 / (1 * 1 / 1 - 1) / 1 * 1) / 1) - 1 * ((1 + (((1 + 1) + 1) - ((1 * ((1	 		+ 1) + 1)) * ((1 * 1) * 1) - 1) / 1 / (1 - 1 / (1 - 1 * 1) / 1 * 1) / ((1 - (1 + 		 1) / 1 * 1) + 1 - (1 + (1 * 1)) - 1) - 1 * 1) - 1 / 1) * 1)) + (((1 - (1 / 1 -	 		1 + 1 - (1 * (1 + 1)) / (1 + (1 + 1))) + 1) - ((1 + 1) * (1 * 1 / 1)) * 1 - 1) + 		 1) - 1 / 1 - (((1 * 1 - 1) + 1) - (1 + 1) - 1 - 1 * 1)) / (1 + 1) + (((1 * ((1	 		/ (1 * 1) * 1) * 1)) * 1) + 1 - (1 * 1)) / 1) * 1) + (1 / 1 * 1 / (1 * (1 + (1 / 		 1 / 1 - ((1 + ((1 - 1 / 1 + 1 / 1 - 1) * 1)) * 1) - 1 / 1 / 1 / (1 + 1) - (1 +	 		((1 + (((1 + 1 - 1 / ((1 + 1) * (1 + (1 / 1 + (1 + 1))))) * 1) * ((1 - 1 / (1 +	 		1 - (((1 / 1 - 1 / 1 * (1 + (((1 * 1) * (1 - 1 * (1 * 1))) - 1 + ((1 * 1) * ((1	 		- 1 * 1) / 1 + (1 + 1))()))) * 1) + 1 - 1 - 1 / (1 + ((1 * (1 + 1)) * 1)))) / (1	 		+ 1 - 1) + 1) - 1 * 1) / 1 - 1 / 1 - ((((1 + (1 * 1 - 1)) * ((1 + (1 * 1)) * 1)) 		 * ((1 + 1) + 1)) + 1) / 1 / 1 - ((1 / 1 - (((1 + (1 * 1) - 1) - 1 * 1 - (1 + 1) 		 - 1) + 1) + (1 / (1 + (1 * 1)) / 1 * 1 / 1) / (1 + 1)) * 1 / 1 - 1) - 1 - 1 - 1 		) / 1) * 1) / 1) + 1))) / 1 / 1)) / 1) * ((((1 / 1 + 1) - 1 * 1 / (1 + 1)) * 1 /	 		1) / 1 * 1) / 1 / 1 / 1 - ((1 * (((1 / 1 * (1 * (1 / ((1 - 1 * (1 * 1 / 1)) + (( 		1 * (1 + 1)) * 1)) * (1 + (1 + (1 * (1 + 1 / 1 - (1 - 1 * ((1 + 1) + 1 - (1 * 1) 		) / ((1 - 1 + 1) - 1 + 1)))) - 1 / (1 + 1) / 1 / 1 / 1 / (1 + (1 * (1 - (1 * 1 / 		 1) * 1))) / 1 - 1 - 1))) / ((1 + 1) + 1 / (1 * 1) / 1 / (1 * 1 / 1) - 1 - 1 - 1 		 / (1 * ((1 / 1 - 1 / 1 - 1 / 1 / 1 / 1 / 1 * 1) - 1 / (1 - ((1 / 1 + 1) + 1) -	 		1 / (1 * (1 + 1)) / 1 * (1 * 1) / 1) / 1 * 1)) / (1 + 1) / (((1 - 1 * 1) + (1 -	 		(1 + 1) + 1 / 1)) * 1) - (1 / 1 / 1 * 1) - (1 - ((1 * (1 - (1 + 1) + 1)) * (1 +	 		1)) / 1 / 1 + (1 - 1 - (1 + (1 * 1)) - ((1 + 1 - ((1 + ((1 * 1) * 1 - 1 / 1)) +	 		1) - 1) / 1 - 1 + 1) / (((1 * 1) + 1) + 1) * (1 * 1)))) / 1 / ((1 * 1) + 1 - 1)) 		 / 1) * 1) * 1)) * ((1 - (1 * 1 / 1) + 1) * 1 / 1)) / (1 + 1) / 1) + (1 / 1 * (1 		 + 1 / 1 - (1 / (1 - 1 + 1) + 1) / (1 + 1) - (1 * 1)) - (((1 / 1 + 1 - 1 - 1) /	 		1 * 1) + (1 * 1) / (1 / 1 * 1) - 1 - (1 + 1))) / 1) + 1) + 1)) / 1 / 1)) + 1) -	 		1( - (1 * 1 - 1)) * ((1 + 1) * 1)) - 1 * (1 + ((1 * 1) + (1 * 1)))) - 1 * 1)))) * 		 1) - 1 - 1 - 1))");
	testMainFloatArgInvalidProgram("def main(float x) float : (1 * (1 * ((1 + 1) * (1 - 1 + 1)) - 1 - ((1 + (((((1 + 		 1 / (1 + (((1 + 1 / (1 * (1 - 1 + 1 / 1 / 1))) - 1 - 1 - (1 + 1 / 1) + (((1 * 1 		) * 1) * 1) / 1) - 1 + 1)) - 1 / 1) + (1 * 1)) + 1 - 1) + 1) * 1)) + 1) / (1 + 1 		) / 1 - ((1 + 1 / 1 - (((1 * ((1 - 1 - 1 / 1 - 1 * 1) - (1 / 1 - 1 * 1) + ((1 +	 		1) - 1 * 1) - 1 / 1)) + (1 / 1 / 1 - 1 - (1 + 1) / (1 + 1) + 1)) * (1 * ((((1 +	 		1) * 1) / 1 - 1 - 1 - (((1 + 1) * 1 - (((1 / 1 * 1) * (1 + 1) - 1) / 1 / 1 / 1 - 		 (1 + (((1 + 1) + 1) * 1)) - 1 / ((1 + 1) / (1 * 1) / (1 + (1 / 1 * 1 / 1 - 1 -	 		(1 * 1) / (1 + 1) - 1 - 1 - 1 - 1 - 1 / 1)) / (1 / 1 * 1) + (((1 + 1) * ((1 + 1) 		 * 1)) / 1 + ((1 -  true ((1 * 1) + 1) + 1) + 1 - 1 / ((1 / (1 - ((1 + 1) - ((((1 - (( 		1 * ((1 * ((1 + 1) + ((1 - (1 * 1) + (1 - (1 - 1 - (1 * 1) / (1 + 1) / 1 * (1 -	 		1 + (1 + 1) - ((1 / 1 / 1 + 1) * 1) - (1 - 1 * 1))) * 1) - 1 - 1 - 1 - (1 * (((1 		 * 1) * ((1 + (1 / (1 * 1) * 1 - 1)) * (1 + (((1 + 1) + 1) + 1)))) + 1) / 1)) +	 		1 / 1) / 1 - 1)) * 1 - 1)) / 1 * (1 * 1) - 1 - 1 / (1 * ((((1 + 1) + 1) * 1 - 1	 		- 1 / ((1 + 1) / (1 * 1) - ((1 / 1 + 1) * 1) + (1 + (1 * 1) - 1 - 1 - 1 - (1 + ( 		1 + 1))) / 1)) - 1 * (1 / ((1 * 1) + ((1 - 1 + (1 + 1)) + ((1 + 1) * 1))) / 1 *	 		1)) - 1) / 1 / 1 / 1) / 1 + 1) / (1 / (1 + (1 + (1 * 1 - 1)) - 1) / 1 - ((1 * 1	 		/ 1 / (1 / (1 * 1 / 1 - 1) / 1 * 1) / 1) - 1 * ((1 + (((1 + 1) + 1) - ((1 * ((1	 		+ 1) + 1)) * ((1 * 1) * 1) - 1) / 1 / (1 - 1 / (1 - 1 * 1) / 1 * 1) / ((1 - (1 + 		 1) / 1 * 1) + 1 - (1 + (1 * 1)) - 1) - 1 * 1) - 1 / 1) * 1)) + (((1 - (1 / 1 -	 		1 + 1 - (1 * (1 + 1)) / (1 + (1 + 1))) + 1) - ((1 + 1) * (1 * 1 / 1)) * 1 - 1) + 		 1) - 1 / 1 - (((1 * 1 - 1) + 1) - (1 + 1) - 1 - 1 * 1)) / (1 + 1) + (((1 * ((1	 		/ (1 * 1) * 1) * 1)) * 1) + 1 - (1 * 1)) / 1) * 1) + (1 / 1 * 1 / (1 * (1 + (1 / 		 1 / 1 - ((1 + ((1 - 1 / 1 + 1 / 1 - 1) * 1)) * 1) - 1 / 1 / 1 / (1 + 1) - (1 +	 		((1 + (((1 + 1 - 1 / ((1 + 1) * (1 + (1 / 1 + (1 + 1))))) * 1) * ((1 - 1 / (1 +	 		1 - (((1 / 1 - 1 / 1 * (1 + (((1 * 1) * (1 - 1 * (1 * 1))) - 1 + ((1 * 1) * ((1	 		- 1 * 1) / 1 + (1 + 1)))))) * 1) + 1 - 1 - 1 / (1 + ((1 * (1 + 1)) * 1)))) / (1	 		+ 1 - 1) + 1) - 1 * 1) / 1 - 1 / 1 - ((((1 + (1 * 1 - 1)) * ((1 + (1 * 1)) * 1)) 		 * ((1 + 1) + 1)) + 1) / 1 / 1 - ((1 / 1 - (((1 + (1 * 1) - 1) - 1 * 1 - (1 + 1) 		 - 1) + 1) + (1 / (1 + (1 * 1)) / 1 * 1 / 1) / (1 + 1)) * 1 / 1 - 1) - 1 - 1 - 1 		) / 1) * 1) / 1) + 1)) / 1 / 1)) / 1) * ((((1 / 1 + 1) - 1 * 1 / (1 + 1)) * 1 /	 		1) / 1 * 1) / 1 / 1 / 1 - ((1 * (((1 / 1 * (1 * (1 / ((1 - 1 * (1 * 1 / 1)) + (( 		1 * (1 + 1)) * 1)) * (1 + (1 + (1 * (1 + 1 / 1 - (1 - 1 * ((1 + 1) + 1 - (1 * 1) 		) / ((1 - 1 + 1) - 1 + 1)))) - 1 / (1 + 1) / 1 / 1 / 1 / (1 + (1 * (1 - (1 * 1 / 		 1) * 1))) / 1 - 1 - 1))) / ((1 + 1) + 1 / (1 * 1) / 1 / (1 * 1 / 1) - 1 - 1 - 1 		 / (1 * ((1 / 1 - 1 / 1 - 1 / 1 / 1 / 1 / 1 * 1) - 1 / (1 - ((1 / 1 + 1) + 1) -	 		1 / (1 * (1 + 1)) / 1 * (1 * 1) / 1) / 1 * 1)) / (1 + 1) / (((1 - 1 * 1) + (1 -	 		(1 + 1) + 1 / 1)) * 1) - (1 / 1 / 1 * 1) - (1 - ((1 * (1 - (1 + 1) + 1)) * (1 +	 		1)) / 1 / 1 + (1 - 1 - (1 + (1 * 1)) - ((1 + 1 - ((1 + ((1 * 1) * 1 - 1 / 1)) +	 		1) - 1) / 1 - 1 + 1) / (((1 * 1) + 1) + 1) * (1 * 1)))) / 1 / ((1 * 1) + 1 - 1)) 		 / 1) * 1) * 1)) * ((1 - (1 * 1 / 1) + 1) * 1 / 1)) / (1 + 1) / 1) + (1 / 1 * (1 		 + 1 / 1 - (1 / (1 - 1 + 1) + 1) / (1 + 1) - (1 * 1)) - (((1 / 1 + 1 - 1 - 1) /	 		1 * 1) + (1 * 1) / (1 / 1 * 1) - 1 - (1 + 1))) / 1) + 1) + 1)) / 1 / 1)) + 1) -	 		1 - (1 * 1 - 1)) * ((1 + 1) * 1)) - 1 * (1 + ((1 * 1) + (1 * 1)))) - 1 * 1)))) * 		 1) - 1 - 1 - 1))");

	testMainFloatArg("def g(function<float> f) float : f()       def main(float x) float : g(\\() float : x)",  4.0f, 4.0f, ALLOW_UNSAFE | INVALID_OPENCL | ALLOW_SPACE_BOUND_FAILURE | ALLOW_TIME_BOUND_FAILURE);

	// Test first class function with OpenCL code-gen
	//testMainFloatArg("def f(array<float, 16> a, int i) float : a[i]    def g(function<array<float, 16>, int, float>      def main(float x) float : g(f, [truncateToInt(x)]", 2.1f, 3.0f, ALLOW_UNSAFE);


	//testMainFloatArg("def main(float x) float : let a = [1.0, 2.0, 3.0, 4.0]a in a[truncateToInt(x)]", 2.1f, 3.0f, ALLOW_UNSAFE);

	// ===================================================================
	// Test 16-bit integers
	// ===================================================================
	testMainInt16Arg("def main(int16 x) : x + 5i16", 1, 6);
	
	// TODO: need to add i32 -> i16 implicit conversions.

	testMainInt16Arg("def main(int16 x) : [10i16, 11i16]a[0]", 1, 10);
	//testMainInt16Arg("def main(int16 x) : [65535i16, 11i16]a[0]", 1, 65535);


	// Test toFloat(int16)
	//testMainInt16Arg("def main(int16 x) int16 : toInt16(1.0f + toFloat(x))", 2, 3); // TODO: add truncateToInt16
	testMainFloatArg("def main(float x) float : x + toFloat(4i16)", 2.0f, 6.0f);

	// Test toDouble(int)
	testMainDoubleArg("def main(double x) double : x + toDouble(8)", 2.0, 10.0);


	// ===================================================================
	// Test 'real' type.
	// Will be aliased to 'float' for float tests, and to 'double'
	// for double tests.
	// ===================================================================
	testMainFloatArg("def main(real x) real :  x", 1.0f, 1.0f);
	testMainDoubleArg("def main(real x) real :  x", 1.0f, 1.0f);

	// test toReal()
	testMainFloatArg("def main(float x) float :  x + toReal(10)", 1.0f, 11.0f);
	testMainFloatArg("def main(float x) float :  toReal(truncateToInt(x))", 3.1f, 3.0f);
	testMainDoubleArg("def main(double x) double :  x + toReal(10)", 1.0, 11.0);
	testMainDoubleArg("def main(double x) double :  toReal(truncateToInt(x))", 3.1, 3.0);



	// ===================================================================
	// Test sign()
	// ===================================================================
	testMainFloatArg("def main(float x) float : sign(x)", 4.f, 1.f);
	testMainFloatArg("def main(float x) float : sign(x)", -4.f, -1.f);
	testMainFloat("def main() float : sign( 4.0)", 1.f);
	testMainFloat("def main() float : sign(-4.0)", -1.f);

	// Test sign(+0) and sign(-0)
	testMainFloatArg("def main(float x) float :	sign(x)", 0.f, 0.f);
	testMainFloatArg("def main(float x) float :	sign(x)", -0.f, -0.f); // NOTE: This is not actually checking the sign bit
	testMainFloat("def main() float : sign( 0.0)",  0.f);
	testMainFloat("def main() float : sign(-0.0)", -0.f); // NOTE: This is not actually checking the sign bit

	// sign() on vectors:
	testMainFloatArg("def f(vector<float, 4> v) !noinline : sign(v)     \n\
		def main(float x) float :	f([x, x, x, x]v)[0]", -4.f, -1.f);
	testMainFloatArg("def f(vector<float, 4> v) !noinline : sign(v)     \n\
		def main(float x) float :	f([x, x, x, x]v)[0]", 4.f, 1.f);

	testMainDoubleArg("def f(vector<double, 4> v) !noinline : sign(v)     \n\
		def main(double x) double :	f([x, x, x, x]v)[0]", -4.f, -1.f);

	testMainFloatArg("def main(float x) float :	sign([x, x, x, x]v)[0]", -4.f, -1.f);
	testMainFloatArg("def main(float x) float :	sign([x, x, x, x]v)[3]", -4.f, -1.f);
	testMainFloatArg("def main(float x) float :	sign([4.0, 4.0, 4.0, 4.0]v)[3]", -4.f, 1.f);

	// with vector of length 3:
	testMainFloatArg("def main(float x) float :	sign([x, x, x]v)[0]", -4.f, -1.f);
	testMainFloatArg("def main(float x) float :	sign([x, x, x]v)[2]", -4.f, -1.f);
	testMainFloatArg("def main(float x) float :	sign([4.0, 4.0, 4.0]v)[2]", -4.f, 1.f);


	// Caused a crash earlier:
	testMainFloatArgInvalidProgram("struct Float4Struct { vector<float, 4> v }   def sin(Float4Struct f) : Float4Struct(sign(f.v))		def main(Float4Struct a, Float4Struct b) Float4Struct : sin(a)");

	testMainFloatArgAllowUnsafe("struct S { float x }   def f(S s) float : s.x   def main(float x) float :	f(S(x))", 1.f, 1.f);


	// ===================================================================
	// Test optimisation of varray from heap allocated to stack allocated
	// ===================================================================
	results = testMainFloatArg("def main(float x) float :	 [3.0, 4.0, 5.0]va[0]", 1.f, 3.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	results = testMainFloatArg("def main(float x) float :	 let v = [3.0, 4.0, 5.0]va in v[0]", 1.f, 3.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	results = testMainFloatArg("def main(float x) float :	 let v = [3.0, 4.0, 5.0]va in v[1]", 1.f, 4.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	results = testMainFloatArg("def main(float x) float :	 let v = [3.0, 4.0, 5.0]va in v[2]", 1.f, 5.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	results = testMainFloatArg("def main(float x) float :	 let v = [x + 3.0, x + 4.0, x + 5.0]va in v[0]", 10.f, 13.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls == 0);

	// Test varray being returned from function, has to be heap allocated.  (Assuming no inlining of f)
	results = testMainFloatArgAllowUnsafe("def f(float x) : [x + 3.0, x + 4.0, x + 5.0]va             def main(float x) float :	 let v = f(x) in v[0]", 10.f, 13.f, INVALID_OPENCL);
	testAssert(results.stats.num_heap_allocation_calls <= 1);

	
	
	// Test map
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {1.0f, 4.0f, 9.0f, 16.0f};

		testFloatArray(
			"def square(float x) float : x*x			\n\
			def main(array<float, 4> a, array<float, 4> b) array<float, 4> : map(square, a)",
			a, b, target_results, 4);
	}

	

	testMainFloatArg("struct S { int x }  def f(S s) S : let t = S(1) in s   def main(float x) float : let v = [99]va in x", 1.f, 1.0f, INVALID_OPENCL);


	// ===================================================================
	// Test Refcounting optimisations: test that a let variable used as a function argument is not incremented and decremented
	// ===================================================================

	testMainInt64Arg("def f(string s, int64 x) int64 : length(s) + x       def main(int64 x) int64 : let s = \"hello\" in f(s, x)", 1, 6, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);


	// ===================================================================
	// Test Refcounting optimisations: test that a argument variable used as a function argument is not incremented and decremented
	// ===================================================================

	testMainInt64Arg("def f(string s, int64 x) int64 : length(s) + x     def g(string s, int64 x) int64 : f(s, x)      def main(int64 x) int64 : g(\"hello\", x)", 1, 6, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// test that a argument variable used as a function argument is not incremented and decremented, even when the argument may be stored and returned in the result.
	testMainInt64Arg("struct S { string str }      def f(string str) S : S(str)     def g(string s) S : f(s)      def main(int64 x) int64 : length(g(\"hello\").str)", 1, 5, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);



	testMainInt64Arg("def f(string s, int64 x) int64 : length(s) + x       def main(int64 x) int64 : f(\"hello\", x)", 1, 6, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	
	// Test fold built-in function with update
	/*{
		const int len = 256;
		js::Vector<int, 32> vals(len, 0);
		js::Vector<int, 32> b(len, 0);
		js::Vector<int, 32> target_results(len, 1);

		testIntArray("																\n\
		def f(array<int, 256> state, int iteration) tuple<array<int, 256>, bool> :			\n\
				[update(state, iteration, elem(state, iteration) + 1),					\n\
				iteration < 255]t																\n\
																							\n\
		def main(array<int, 256> vals, array<int, 256> initial_counts) array<int, 256>:  iterate(f, vals)",
			&vals[0], &b[0], &target_results[0], vals.size(),
			true // allow_unsafe_operations
		);
	}

	std::cout << timer.elapsedString() << std::endl;
	int a = 9;
	return;*/

	// Test with pass-by-reference data (struct)
	testMainIntegerArg("															\n\
		struct s { int x, int y }													\n\
		def f(s current_state, int iteration) tuple<s, bool> :					\n\
			if iteration >= current_state.y												\n\
				[current_state, false]t # break										\n\
			else																	\n\
				[s(current_state.x*current_state.x, current_state.y), true]t					\n\
		def main(int x) int :  iterate(f, s(x, x)).x", 3, 6561, INVALID_OPENCL);
	

	// This test triggers a problem with __chkstk when >= 4K is allocated on stack
	/*{
		const int len = 1024;
		js::Vector<int, 32> vals(len, 0);
		js::Vector<int, 32> b(len, 0);
		js::Vector<int, 32> target_results(len, 0);
		target_results[0] = len;

		testIntArray("																\n\
		def f(array<int, 1024> counts, int x) array<int, 1024> :			\n\
				update(counts, x, elem(counts, x) + 1)			\n\
		def main(array<int, 1024> vals, array<int, 1024> initial_counts) array<int, 1024>:  fold(f, vals, initial_counts)",
			&vals[0], &b[0], &target_results[0], vals.size(),
			true // allow_unsafe_operations
		);
	}*/

	//testMainFloatArg("def f(array<float, 1024> a, float x) float : a[0]       def main(float x) float : f([x]a1024, x) ", 2.f, 2.f);

	// Test that binding to a let variable overrides (shadows) a global function definition
	testMainIntegerArg("def f(int x) int : 1		   def main(int x) int : let y = 1 f = 2 in f", 10, 2);

	testMainIntegerArg("def f(int x) tuple<int, int> : [1, 2]t		   def main(int x) int : f(x)[1 - 1]", 10, 1);

	testMainIntegerArg("def f() bool : 1 + 2 < 3         def main(int x) int : x", 1, 1);
	//testMainIntegerArg("def f() bool : 1 + 2 * 3         def main(int x) int : x", 1, 1);

	//TEMP:
	testMainFloatArgInvalidProgram("def main(float x) float :  x [bleh");

	testMainIntegerArg("def f(int x) tuple<int, int> : [1, 2]t		   def main(int x) int : f(x)[0]", 10, 1);
	/*testMainIntegerArg("def f(int x) tuple<int, int> : if x < 0 [1, 2]t else [3, 4]t			\n\
					   def main(int x) int : f(x)[0]", 10, 3);*/

	testMainIntegerArg("def f(int x) tuple<int> : if x < 0 [1]t else [3]t			\n\
					   def main(int x) int : f(x)[0]", 10, 3);



	


	//testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([1.0, 2.0]v)  in a.v[0]", 0.f, 1.0f);
	testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([1.0, 2.0]v) in a.v[0]", 0.f, 1.0f);

	testMainFloatArg("struct s { float y }     def main(float x) float : s(x).y", 3.f, 3.0f);

	testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([x, 2.0]v)  in (a.v)[0]", 3.f, 3.0f);
	testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([1.0, 2.0]v)  in a.v[0]", 0.f, 1.0f);
	testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([x, 2.0]v)  in a.v[0]", 3.f, 3.0f);
	testMainFloatArg("struct vec2 { vector<float, 2> v }     def main(float x) float : let a = vec2([1.0, 2.0]v)  in  elem(a.v, 0)", 0.f, 1.0f);





	testMainFloatArg("def main(float x) float : elem([x, 2.0]v, 1)", 1.f, 2.f);


	

	

	

	// int64 - int mixing
	testMainIntegerArgInvalidProgram("def main(int64 x) int :	x + 1");
	testMainIntegerArgInvalidProgram("def main(int64 x) int :	x - 1");
	testMainIntegerArgInvalidProgram("def main(int64 x) int :	x * 1");
	testMainIntegerArgInvalidProgram("def main(int64 x) int :	x / 1");
	testMainIntegerArgInvalidProgram("def main(int64 x) int :	if x < 5 1 else 2");

	testMainFloatArgInvalidProgram("	def f(float x) float if(x < 1.0) : 				  let	 					z = 2.0 					y = 3.0 				  in 					y + z 				  def main() float : f(0.0)");
	testMainFloatArgInvalidProgram("def main() float : 					 let a = [1.0, 2.0, 3.0, 4.0]v 					 b = [11.0, 12.0, 13.0, 14 else .0]v in 					 e2(min(a, b)) 		de if f main() float : 				  let a = [1.0, 2.0, 3.0, 4.0]v 				  b = [11.0, 12.0, 13.0, 14.0]v in 				  e2(min(b, a))");
	testMainFloatArgInvalidProgram("		main(int i) int :								 			let												 				a = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v		 				b = string  [1.0, 1.6, 1.0, 1.0, 1.3, 1.6, 1.8, 1.6]v		 				c = [1.7, 2.8, 3.0, 4 true .7, 5.5, 6.7, 7.0, 8.4] int64 v		 			in												 				truncateToInt(elem(a + if(i < 1, b, c), i))");
	testMainFloatArgInvalidProgram("		def main() float : 					let x = [1.0, 2.0, 3.0, 4.0]v 					y = [10.0, 20.0, 30.0, 40.0]v* sin(x + 0.03) in 					e1(x + y)");

	// Test that we can't put a structure in a vector
	testMainFloatArgInvalidProgram("struct teststruct { float y } \n\
        def main(float x) float : elem([teststruct(x)]v, 0).y");

	testMainFloatArgInvalidProgram("struct teststruct { int y } \n\
								   def f(teststruct a, teststruct b, bool condition) teststruct : let x = if(condition, a, b) in let v = [x, x, x, x, x, x, x, x]v in x \n\
        def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) )");

	testMainFloatArgInvalidProgram("struct s { float x, float yd }  	def op_sub(s a, s b) : s(a.x - b.x, a.y - b.y)  	def main() float : x( true  < s(2, 3) - s(3, 4))");

	testMainFloatArgInvalidProgram("def f(float x) float : x*x def main(int x) int :  elem(fold(f, [0, 0, 1, 2]a, [0]a16), 1)    def main(float x) float : f(x) - 3");

	
	// A vector of elements that contains one or more float-typed elements will be considered to be a float-typed vector.
	// Either all integer elements will be succesfully constant-folded and coerced to a float literal, or type checking will fail.

	// OLD: A vector of elements, where each element is either an integer literal, or has float type, and there is at least one element with float tyoe, 
	// and each integer literal can be converted to a float literal, will be converted to a vector of floats

	// Test incoercible integers
	testMainFloatArgInvalidProgram("def main(float x) float : elem(  [100000001, 2.0]v   , 0)");
	testMainFloatArgInvalidProgram("def main(float x) float : elem(  [2.0, 100000001]v   , 0)");

	testMainFloatArg("def main(float x) float : elem(  [1, 2.0]v   , 0)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float : elem(  [1.0, 2]v   , 0)", 1.0f, 1.0f);

	testMainFloatArg("def main(float x) float : elem(  [1, x]v   , 0)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float : elem(  [x, 2]v   , 0)", 1.0f, 1.0f);

	testMainFloatArg("def main(float x) float : elem(  [1, x, 3.0]v   , 0)", 1.0f, 1.0f);
	testMainFloatArg("def main(float x) float : elem(  [x, 2, 3.0]v   , 0)", 1.0f, 1.0f);

	testMainIntegerArgInvalidProgram("struct Float4Struct { vector<float, 4> v }  			def main(Float4Struct a, Float4Struct b) Float4Struct :  				Float4Struct(a.v + [eleFm(b.v, 0)]v4)");


	testMainIntegerArgInvalidProgram("def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5 + 1.0, 6, 7, 8, 9, 10]v, i) else 0");
		
	testMainIntegerArgInvalidProgram("def main(int x) int :  elem(update([0]a, 0, [  x ]t ), 0)");
	
	testMainIntegerArgInvalidProgram("def main(int i) int : if i >= 0 && i < 1i0 then elem([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]v, i) else 0");

	// Test circular definitions between named constants and function definitions
	testMainIntegerArgInvalidProgram("TEN = main			def main(int x) : TEN + x");
	
	testMainFloatArgInvalidProgram("struct S { tuple<float, float> a, int b }		 		def f(float x) S : S([x + 2.0, x(f(x), 0).a]t, 1)    		def main(float x) float :  elem(f(x).a, 0)");
	
	testMainFloatArgInvalidProgram("def f(float x) float :	let	float z = 2.0 in x + z       def main(int x) int :  iterate(f, 0)");
//	testMainFloatArgInvalidProgram("def f(float x) float :	let	float z = 2.0 in x + z       def main(int x) int :  iterate(f, 0)	  def main(float x) float : f(x)");
	
	
	testMainFloatArgInvalidProgram("def main(float x) float : elem(   shuffle([1.0, 20, 4.0]v, [2, 3]v)   , 1)");

	testMainIntegerArgInvalidProgram("def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8., 9, 10]t, i) else 0");
	testMainIntegerArgInvalidProgram("def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8., 9, 10]a, i) else 0");

	// Test type coercion in vector literal changing elem() type
	testMainFloatArgInvalidProgram("def main(int i) int : if i >= 0 && i < 10 then elem([1, 2, 3, 4, 5, 6, 7, 8., 9, 10]v, i) else 0");

	testMainFloatArgInvalidProgram("def mul(vector<float, 4> v, float x) vector<float, 4> : v * [x, x,]v  					def main() float : 	let x = [1.0, 2.0, 3.0, 4.0]v 		y = 10.0 in   mul(x, y).e1");
	
		

	testMainFloatArgInvalidProgram("def main() float : 					let x = [1.0, 2.0, 3.0, 4.0]v 					y = [10.0, 20.0, 30.0, 40.0]v  <1.0 in 					e1(x + y)");

	testMainFloatArgInvalidProgram("def f(array<int, 4> a, int i) int : if inBounds(a, i) elem(a, i) else 0       def main(int i) int : f([1, 2, 3]a, i)");


	// This function has special static global variable-creating mem-leaking powers.
	testMainFloatArg(
		"def expensiveA(float x) float : cos(x * 0.456 + cos(x))			\n\
		def expensiveB(float x) float : sin(x * 0.345 + sin(x))			\n\
		def main(float x) float: if(x < 0.5, expensiveA(x + 0.145), expensiveB(x + 0.2435))",
		0.2f, cos((0.2f + 0.145f) * 0.456f + cos((0.2f + 0.145f))));	



	



	// ===================================================================
	// Test that recursion is disallowed.
	// ===================================================================
	testMainFloatArgInvalidProgram("def main(float x) float :  main(x)  ");
	testMainFloatArgInvalidProgram("def main(float x) float :  1  def f(int x) int : f(0)  ");

	// Mutual recursion
	testMainFloatArgInvalidProgram("def g(float x) float : f(x)      def f(float x) float : g(x)              def main(float x) float : f(x)");
	
	
	// ===================================================================
	// Test that some programs with invalid syntax fail to compile.
	// ===================================================================
	testMainFloatArgInvalidProgram("def main(float x) float :  x [bleh");


	testMainIntegerArg("def main(int x) int : 10 + (if x < 10 then 1 else 2)", 5, 11);

	testMainIntegerArg("def main(int x) int : if x < 10 then 1 else 2", 5, 1);

	testMainIntegerArg("def main(int x) int : if x < 10 then (if x < 5 then 1 else 2) else (if x < 15 then 3 else 4)", 5, 2);



	


	// NOTE: if 'then' token is removed, we get a parse error below.

	testMainIntegerArg("def f(int x) tuple<int> : if x < 0 [1]t else [3]t			\n\
					   def main(int x) int : f(x)[0]", 10, 3);
	// ===================================================================
	// Test array subscript (indexing) operator []
	// ===================================================================

	// NOTE: if 'then' token is removed, we get a parse error below.

	testMainIntegerArg("def f(int x) tuple<int> : if x < 0 [1]t else [3]t			\n\
					   def main(int x) int : f(x)[0]", 10, 3);

	testMainIntegerArg("def f(int x) tuple<int, int> : if x < 0 [1, 2]t else [3, 4]t			\n\
					   def main(int x) int : f(x)[0]", 10, 3);

	// OpenCL doesn't support new array values at runtime, nor fold() currently.
	
	testMainIntegerArg("def main(int x) int : [x]a[0]", 10, 10, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [x, x + 1, x + 2]a[1]", 10, 11, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int : [x, x + 1, x + 2]a[1 + 1]", 10, 12, INVALID_OPENCL);

	// Test index out of bounds
	testMainIntegerArgInvalidProgram("def main(int x) int :  [x, x + 1, x + 2]a[-1])");
	testMainIntegerArgInvalidProgram("def main(int x) int :  [x, x + 1, x + 2]a[3])");
	
	// ===================================================================
	// Test update()
	// update(CollectionType c, int index, T newval) CollectionType
	// ===================================================================

	testMainIntegerArg("def main(int x) int :  elem(update([0]a, 0, x), 0)", 10, 10, INVALID_OPENCL);

	testMainIntegerArg("def main(int x) int :  elem(update([0, 0]a, 0, x), 0)", 10, 10, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int :  elem(update([0, 0]a, 0, x), 1)", 10, 0, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int :  elem(update([0, 0]a, 1, x), 0)", 10, 0, INVALID_OPENCL);
	testMainIntegerArg("def main(int x) int :  elem(update([0, 0]a, 1, x), 1)", 10, 10, INVALID_OPENCL);

	// Test update arg out of bounds
	testMainIntegerArgInvalidProgram("def main(int x) int :  elem(update([]a, 0, x), 0)");
	testMainIntegerArgInvalidProgram("def main(int x) int :  elem(update([0]a, -1, x), 0)");
	testMainIntegerArgInvalidProgram("def main(int x) int :  elem(update([0]a, 1, x), 0)");

	// Test failure to prove valid, since index arg cannot be proven in bounds:
	testMainIntegerArgInvalidProgram("def main(int x) int :  elem(update([0]a, x, x), 0)");


	

	testMainFloatArg("def main(float x) float :  elem([x, x, x, x]v, 0)", 1.0f, 1.0f);

	


	// Test if LLVM combined multiple sqrts into a sqrtps instruction (doesn't do this as of LLVM 3.4)
	testMainFloatArg("def main(float x) float : sqrt(x) + sqrt(x + 1.0) + sqrt(x + 2.0) + sqrt(x + 3.0)", 1.0f, sqrt(1.0f) + sqrt(1.0f + 1.0f) + sqrt(1.0f + 2.0f) + sqrt(1.0f + 3.0f));

	{
		Float4Struct a(1.0f, 2.0, 3.0, 4.0);
		Float4Struct target_result(3.f, 4.f, 5.f, 6.f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				Float4Struct(a.v + [2.0]v4)", 
			a, a, target_result
		);
	}

	{
		Float4Struct a(1.0f, 2.0, 3.0, 4.0);
		Float4Struct target_result(2.f, 3.f, 4.f, 5.f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				Float4Struct(a.v + [elem(b.v, 0)]v4)", 
			a, a, target_result
		);
	}

	// Check scalar initialisation of vector
	testMainFloatArg("def main(float x) float :  elem(    [2.0]v8    , 5)", 1.0f, 2.0f);

	/*testMainFloatArg(
		"def expensiveA(float x) float : cos(x * 2.0)			\n\
		def main(float x) float: expensiveA(x)",
		0.2f, cos(0.2 * 2.0f));

	testMainFloatArg(
		"def expensiveA(float x) float : cos(x * 2.0)			\n\
		def main(float x) float: x + expensiveA(x)",
		0.2f, 0.2f + cos(0.2 * 2.0f));
*/
	testMainFloatArg(
		//"def expensiveA(float x) float : pow(x, 0.1 + pow(0.2, x))			\n
		"def expensiveA(float x) float : cos(x * 0.456 + cos(x))			\n\
		def expensiveB(float x) float : sin(x * 0.345 + sin(x))			\n\
		def main(float x) float: if(x < 0.5, expensiveA(x + 0.145), expensiveB(x + 0.2435))",
		0.2f, cos((0.2f + 0.145f) * 0.456f + cos((0.2f + 0.145f))));	


	// Check constant folding for a function that is ostensibly a function of several arguments but does not actually depend on them.
	//testMainFloatArg("def g(float x) float : pow(2.0, 3.0)             def main(float x) float :  g(x)",    1.f, 8.f);

	//testMainFloatArg("def g(float x) float : pow(2.0, 3.0)             def main(float x) float :  pow(g(x), 2.0)",    1.f, 64.f);
	testMainFloatArg("def g(float x) float : 8.f             def main(float x) float :  pow(g(x), 2.0)",    1.f, 64.f);

	// Check constant folding for a let variable
	//testMainFloatArg("def main(float x) float :  let y = pow(2.0, 3.0) in y", 1.f, 8.f);



	// Test promotion to match function return type:  
	// Test in if statement
	// TODO: Get this working testMainFloatArg("def g(float x) float : if x > 1.0 then 2 else 3         def main(float x) float : g(x)", 1.f, 2.f);

	//testMainFloatArg("def g(float x) float : 2         def main(float x) float : g(2)", 1.f, 2.f);


	// Test calling a function that will only be finished / correct when type promotion is done, but will be called during constant folding phase.
	testMainFloatArgCheckConstantFolded("def g(float x) float : 1 / x         def main(float x) float : g(2)", 1.f, 0.5f);
	testMainFloatArg("def g(float x) float : 1 / x         def main(float x) float : g(x)", 4.f, 0.25f);


	// Test calling a function that will only be finished / correct when type promotion is done, but will be called during constant folding phase.
	testMainFloatArgCheckConstantFolded("def g(float x) float : 10 + x         def main(float x) float : g(1)", 2.f, 11.f);

	// Test promotion from int to float
	testMainFloatArg("def main(float x) float : 1 + x", 2.f, 3.f);

	testMainFloatArg("def g(float x) float : x*x         def main(float x) float : g(1 + x)", 2.f, 9.f);


	// Test FMA codegen.
	testMainFloatArg("def main(float x) float : sin(x) + sin(x + 0.02) * sin(x + 0.03)", 0.f, sin(0.f) + sin(0.02f) * sin(0.03f));

	// Unary minus applied to vector:
	// Test with no runtime values
	testMainFloatArg("def main(float x) float : elem(-[1.0, 2.0, 3.0, 4.0]v, 2)", 0.1f, -3.0f);
	testMainIntegerArg("def main(int x) int : elem(-[1, 2, 3, 4]v, 2)", 1, -3);

	// Test with runtime values
	testMainFloatArg("def main(float x) float : elem(-[x + 1.0, x + 2.0, x + 3.0, x + 4.0]v, 2)", 0.4f, -3.4f);
	testMainIntegerArg("def main(int x) int : elem(-[x + 1, x + 2, x + 3, x + 4]v, 2)", 1, -4);

	// Test runtime unary minus of 4-vector
	{
		Float4Struct a(1.0f, 2.0, 3.0, 4.0);
		Float4Struct target_result(-1.0f, -2.0f, -3.0f, -4.0f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				Float4Struct(-v(a))", 
			a, a, target_result
		);
	}


	// Vector divide (not supported currently)
	testMainIntegerArgInvalidProgram("def main() float : 					let x = [1.0, 2.0, 3.0, 4.0]v 					y = [10.0, 20.0, 30.0, 40.0]v in 					e1(x/ - y)");


	// Gather load:
	testMainFloatArg("def main(float x) float : elem(  elem([1.0, 2.0, 3.0, 4.0]a, [2, 3]v)   , 0)", 0.1f, 3.0f);

	// Shuffle
	testMainFloatArg("def main(float x) float : elem(   shuffle([1.0, 2.0, 3.0, 4.0]v, [2, 3]v)   , 1)", 0.1f, 4.0f);

	testMainFloatArg("def main(float x) float : elem(   shuffle([x + 1.0, x + 2.0, x + 3.0, x + 4.0]v, [2, 3]v)   , 1)", 0.1f, 4.1f);

	// Test shuffle mask index out of bounds.
	testMainFloatArgInvalidProgram("def main(float x) float : elem(   shuffle([x + 1.0, x + 2.0, x + 3.0, x + 4.0]v, [2, 33]v)   , 1)     def main(float x) float : x");
 

	// Test a shuffle where the mask is invalid - a float
	testMainFloatArgInvalidProgram("def main(float x) float : elem(   shuffle([1.0, 2.0, 3.0, 4.0]v, [2, 3.0]v)   , 1)");

	// Test a shuffle where the mask is invalid - not a constant
	testMainIntegerArgInvalidProgram("def main(int x) int : elem(   shuffle([1.0, 2.0, 3.0, 4.0]v, [2, x]v)   , 1)");

	

	// Test returning a structure from a function that is called inside an if expression
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f() teststruct : teststruct(1)						\n\
		def g() teststruct : teststruct(2)						\n\
		def main(int x) int : y( if x < 5 then f() else g() ) ",
		2, 1);

	// Test returning a structure from a function that takes a structure and that is called inside an if expression
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct s) teststruct : teststruct(1 + y(s))						\n\
		def g(teststruct s) teststruct : teststruct(2 + y(s))						\n\
		def main(int x) int : y( if x < 5 then f(teststruct(1)) else g(teststruct(2)) ) ",
		2, 2);



	// Test a function (f) that just returns an arg directly
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct a, teststruct b, bool condition) teststruct : a      \n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);

	// Test a function (f) that just returns a new struct
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct a, teststruct b, bool condition) teststruct : teststruct(1)      \n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);

	// Test a function (f) that is just an if expression that returns pass-by-reference arguments directly.
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct a, teststruct b, bool condition) teststruct : if(condition, a, b)      \n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);


	// Test a function (f) that is just an if expression that returns pass-by-reference arguments directly, in a let block
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct a, teststruct b, bool condition) teststruct :      \n\
			let																\n\
				x = if(condition, a, b)										\n\
			in																\n\
				x															\n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);

	// Test a function (f) that is just an if expression that returns pass-by-reference arguments directly, in a let block
	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def f(teststruct a, teststruct b, bool condition) teststruct :      \n\
			let																\n\
				x = teststruct( y(if(condition, a, b))	)									\n\
			in																\n\
				x															\n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);


	testMainIntegerArg(
		"struct teststruct { int y }										\n\
		def g(teststruct a, teststruct b, bool condition) teststruct : if(condition, a, b)      \n\
		def f(teststruct a, teststruct b, bool condition) teststruct : if(condition, g(a, b, condition), g(a, b, condition))      \n\
		def main(int x) int : y( f(teststruct(1), teststruct(2), x < 5) ) ",
		2, 1);

	// truncateToInt with runtime args, with bounds checking
	testMainFloatArg("def main(float x) float : toFloat(if x >= -2147483648.0 && x < 2147483647.0 then truncateToInt(x) else 0)", 3.1f, 3.0f);

	// truncateToInt with constant value
	testMainFloatArg("def main(float x) float : toFloat(truncateToInt(3.1))", 3.1f, 3.0f);

	// Test truncateToInt where we can't prove the arg is in-bounds.
//TEMP	testMainFloatArgInvalidProgram("def main(float x) float : toFloat(truncateToInt(x))", 3.9f, 3.0f);



	

	// Test division by -1 where we prove the numerator is not INT_MIN
	/*testMainIntegerArg(
		"def main(int i) int : if i > 1 then 2 else 0", 
		8, -8);*/


	testMainIntegerArg("def div(int x, int y) int : if(y != 0 && x != -2147483648, x / y, 0)	\n\
					   def main(int i) int : div(i, i)",    
		5, 1);
	testMainIntegerArg("def main(int i) int : if i != 0 && i != -1 then i / i else 0",    
		5, 1);


	// Do a test where a division by zero would be done while constant folding.
	testMainFloatArgInvalidProgram("def f(int x) int : x*x	      def main(float x) float : 14 / (f(2) - 4)");

	testMainFloatArg("def f(int x) int : x*x	      def main(float x) float : 14 / (f(2) + 2)", 2.0f, 2.0f);

	//testMainFloatArg("def f(int x) int : x*x	      def main(float x) float : f(2) + 3", 1.0f, 7.0f);
	testMainFloatArg("def f(int x) int : x*x	      def main(float x) float : 14 / (f(2) + 3)", 2.0f, 2.0f);

	// Test division by -1 where we prove the numerator is not INT_MIN
	testMainIntegerArg(
		"def main(int i) int : if i > -10000 then i / -1 else 0", 
		8, -8);

	// Test division where numerator = INT_MIN where we prove the denominator is not -1
	testMainIntegerArg(
		"def main(int i) int : if i >= 1 then -2147483648 / i else 0", 
		2, -1073741824);

	// Test division by -1 where we can't prove the numerator is not INT_MIN
	testMainIntegerArgInvalidProgram(
		"def main(int i) int : i / -1"
	);


	// Test division by a constant
	testMainIntegerArg(
		"def main(int i) int : i / 4", 
		8, 2);

	// Test division by zero
	testMainIntegerArgInvalidProgram(
		"def main(int i) int : i / 0"
	);


	// Test division by a runtime value
	testMainIntegerArg(
		"def main(int i) int : if i != 0 then 8 / i else 0", 
		4, 2);






	// Test array in array
/*	
	TEMP NOT SUPPORTED IN OPENCL YET
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 2 then elem(elem([[1, 2]a, [3, 4]a]a, i), i) else 0", 
		1, 4);
		*/




	/*float x = 2.0f;
	float y = (x + x) -1.5;*/

	// Test divide-by-zero
	//testMainIntegerArg("def main(int x) int : 10 / x ", 0, 10);

	

	// ===================================================================
	// Ref counting tests
	// ===================================================================
	// Test putting a string in a structure, then returning it.
	testMainInt64Arg(
		"struct teststruct { string str }										\n\
		def f() teststruct : teststruct(\"hello world\")						\n\
		def main(int64 x) int64 : length(str(f())) ",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);

	// Test a string in a structure
	testMainInt64Arg(
		"struct teststruct { string str }										\n\
		def main(int64 x) int64 : length(teststruct(\"hello world\").str) ",
		2, 11, INVALID_OPENCL | ALLOW_TIME_BOUND_FAILURE);



	
	

	testMainFloatArg(
		"def main(float x) float: elem( if(x < 0.5, [1.0, 2.0]a, [3.0, 4.0]a), 0)",
		1.0f, 3.0f, INVALID_OPENCL);
	
	// Test a structure composed of another structure, and taking the dot product of two vectors in the child structures, in an if statement
	{
		Float4StructPair a(
			Float4Struct(1, 1, 1, 1),
			Float4Struct(2, 2, 2, 2)
		);
		
		testFloat4StructPairRetFloat(
			"struct Float4Struct { vector<float, 4> v }			\n\
			struct Float4StructPair { Float4Struct a, Float4Struct b }			\n\
			def main(Float4StructPair pair1, Float4StructPair pair2) float :			\n\
				if(e0(pair1.a.v) < 0.5, dot(pair1.a.v, pair2.b.v), dot(pair1.b.v, pair2.a.v))",
			a, a, 8.0f);
	}

	// Test a structure composed of another structure, and taking the dot product of two vectors in the child structures.
	{
		Float4StructPair a(
			Float4Struct(1, 1, 1, 1),
			Float4Struct(2, 2, 2, 2)
		);
		
		testFloat4StructPairRetFloat(
			"struct Float4Struct { vector<float, 4> v }			\n\
			struct Float4StructPair { Float4Struct a, Float4Struct b }			\n\
			def main(Float4StructPair pair1, Float4StructPair pair2) float :			\n\
				dot(pair1.a.v, pair2.b.v)",
			a, a, 8.0f);
	}


	

	// Test if with expensive-to-eval args
	testMainFloatArg(
		//"def expensiveA(float x) float : pow(x, 0.1 + pow(0.2, x))			\n
		"def expensiveA(float x) float : cos(x * 0.456 + cos(x))			\n\
		def expensiveB(float x) float : sin(x * 0.345 + sin(x))			\n\
		def main(float x) float: if(x < 0.5, expensiveA(x + 0.145), expensiveB(x + 0.2435))",
		0.2f, cos((0.2f + 0.145f) * 0.456f + cos((0.2f + 0.145f))));	


/*
TEMP OPENCL
	// Test operator overloading (op_add) for an array
	testMainFloatArg(
		"def op_add(array<float, 2> a, array<float, 2> b) array<float, 2> : [elem(a, 0) + elem(b, 0), elem(a, 1) + elem(b, 1)]a		\n\
		def main(float x) float: elem([1.0, 2.0]a  + [3.0, 4.0]a, 1)",
		1.0f, 6.0f);	

	// Test operator overloading (op_mul) for an array
	testMainFloatArg(
		"def op_mul(array<float, 2> a, float x) array<float, 2> : [elem(a, 0) * x, elem(a, 1) * x]a		\n\
		def main(float x) float: elem([1.0, 2.0]a * 2.0, 1)",
		1.0f, 4.0f);	

	
	// Test array in array
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 2 then elem(elem([[1, 2]a, [3, 4]a]a, i), i) else 0", 
		1, 4);

	// Test struct in array
	testMainIntegerArg(
		"struct Pair { int a, int b }		\n\
		def main(int i) int : if i >= 0 && i < 2 then b(elem([Pair(1, 2), Pair(3, 4)]a, i)) else 0 ", 
		1, 4);
*/

	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 5 then elem([1, 2, 3, 4, 5]a, i) else 0", 
		1, 2);
		
	// Test integer in-bounds runtime index access
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 20 then elem([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]a, i) else 0", 
		10, 11);

	// Test integer in-bounds runtime index access
	for(int i=0; i<5; ++i)
	{
		testMainIntegerArg(
			"def main(int i) int : if i >= 0 && i < 5 then elem([1, 2, 3, 4, 5]a, i) else 0", 
			i, i+1);
	}

	// Test integer in-bounds runtime index access
	testMainIntegerArg(
		"def main(int i) int : if i >= 0 && i < 4 then elem([1, 2, 3, 4]a, i) else 0", 
		1, 2);

	// Test runtime out of bounds access.  Should return 0.
	if(false)
	{
		testMainIntegerArg(
			"def main(int i) int : elem([1, 2, 3, 4]a, i)", 
			-1, 0);

		testMainIntegerArg(
			"def main(int i) int : elem([1, 2, 3, 4]a, i)", 
			4, 0);

		// Test out of bounds array access
		testMainFloatArg(
			"def main(float x) float : elem([1.0, 2.0, 3.0, 4.0]a, 4)", 
			10.0, std::numeric_limits<float>::quiet_NaN());

		testMainFloatArg(
			"def main(float x) float : elem([1.0, 2.0, 3.0, 4.0]a, -1)", 
			10.0, std::numeric_limits<float>::quiet_NaN());
	}

	



	

	// Test float arrays getting passed in to and returned from main function
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {1.0f, 2.0f, 3.0f, 4.0f};

		testFloatArray("def main(array<float, 4> a, array<float, 4> b) array<float, 4> : a",
			a, b, target_results, 4);
	}

	// Test map
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {1.0f, 4.0f, 9.0f, 16.0f};

		testFloatArray(
			"def square(float x) float : x*x			\n\
			def main(array<float, 4> a, array<float, 4> b) array<float, 4> : map(square, a)",
			a, b, target_results, 4);
	}

	// Test map from one element type to another
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {1.0f, 2.0f, 3.0f, 4.0f};

		testFloatArray(
			"def squareToInt(float x) int : truncateToInt(x*x + 0.000001)			\n\
			def sqrtToFloat(int i) float : sqrt(toFloat(i))							\n\
			def main(array<float, 4> a, array<float, 4> b) array<float, 4> : map(sqrtToFloat, map(squareToInt, a))",
			a, b, target_results, 4);
	}

	// Test map with more elems
	{
		const size_t N = 256;
		std::vector<float> input(N, 2.0f);
		std::vector<float> target_results(N, 4.0f);


		testFloatArray(
			"def square(float x) float : x*x			\n\
			def main(array<float, 256> a, array<float, 256> b) array<float, 256> : map(square, a)",
			&input[0], &input[0], &target_results[0], N);
	}


	// Test float arrays getting passed in to and returned from main function
	{
		const float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
		const float b[] = {10.0f, 20.0f, 30.0f, 40.0f};
		float target_results[] = {11.f, 22.f, 33.f, 44.f};

		testFloatArray("def main(array<float, 4> a, array<float, 4> b) array<float, 4> : [elem(a,0) + elem(b,0), elem(a,1) + elem(b,1), elem(a,2) + elem(b,2), elem(a,3) + elem(b,3)]a",
			a, b, target_results, 4);
	}



	

	// Test structure getting returned directly.
	{
		Float4Struct a(1.0f, -2.0f, 3.0f, -4.0f);
		Float4Struct target_result(1.0f, -2.0f, 3.0f, -4.0f);
		
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				a", 
			a, a, target_result
		);
	}


	

	// Test Array of structs
	testMainFloatArg(
		"struct Pair { float a, float b }		\n\
		def main(float x) float : b(elem([Pair(1.0, 2.0), Pair(3.0, 4.0)]a, 1)) ", 
		10.0, 4.0f, INVALID_OPENCL);
	
	// Test mixing of int and float in an array - is invalid
	testMainFloatArgInvalidProgram("def main(float x) float : elem([1.0, 2, 3.0, 4.0]a, 1) + x");
	testMainFloatArgInvalidProgram("def main(float x) float : elem([1, 2.0, 3.0, 4.0]a, 1) + x");

	// Test Array Literal
	testMainFloatArg("def main(float x) float : elem([1.0, 2.0, 3.0, 4.0]a, 1) + x", 10.0, 12.f);

	// Test Array Literal with one element
	testMainFloatArg("def main(float x) float : elem([1.0]a, 0) + x", 10.0, 11.f);

	/*
TEMP OPENCL
	// Test Array Literal with non-const value
	testMainFloatArg("def main(float x) float : elem([x, x, x, x]a, 1) + x", 1.0, 2.f);
	testMainFloatArg("def main(float x) float : elem([x, x+1.0, x+2.0, x+3.0]a, 2)", 1.0, 3.f);
	*/
	
	// Test Array Literal of integers
	testMainIntegerArg("def main(int x) int : elem([1, 2, 3, 4]a, 1) + x", 10, 12);


	// Test array with let statement
	testMainFloatArg(
		"def main(float x) float :				\n\
			let									\n\
				a = [1.0, 2.0, 3.0, 4.0]a		\n\
			in									\n\
				elem(a, 0)",
		0.0f, 1.0f);

	// Passing array by argument
	testMainFloatArg(
		"def f(array<float, 4> a) float : elem(a, 1)		\n\
		def main(float x) float :						\n\
			f([1.0, 2.0, 3.0, 4.0]a) +  x",
		10.0f, 12.0f);



	// Test vector in structure
	{
		Float4Struct a(1, 2, 3, 4);
		Float4Struct b(1, 2, 3, 4);
		Float4Struct target_result(1, 2, 3, 4);
		
		 //Float8Struct([min(e0(a.v), e0(b.v)), min(e1(a.v), e1(b.v)),	min(e2(a.v), e2(b.v)),	min(e3(a.v), e3(b.v)), min(e4(a.v), e4(b.v)), min(e5(a.v), e5(b.v)), min(e6(a.v), e6(b.v)), min(e7(a.v), e7(b.v))]v ) 
		testFloat4Struct(
			"struct Float4Struct { vector<float, 4> v } \n\
			def min(Float4Struct a, Float4Struct b) Float4Struct : Float4Struct(min(a.v, b.v)) \n\
			def main(Float4Struct a, Float4Struct b) Float4Struct : \n\
				min(a, b)", 
			a, b, target_result
		);
	}


	// Test vector in structure
	{
		Float8Struct a;
		a.v.e[0] = 10;
		a.v.e[1] = 2;
		a.v.e[2] = 30;
		a.v.e[3] = 4;
		a.v.e[4] = 4;
		a.v.e[5] = 5;
		a.v.e[6] = 6;
		a.v.e[7] = 7;

		Float8Struct b;
		b.v.e[0] = 1;
		b.v.e[1] = 20;
		b.v.e[2] = 3;
		b.v.e[3] = 40;
		b.v.e[4] = 4;
		b.v.e[5] = 5;
		b.v.e[6] = 6;
		b.v.e[7] = 7;

		Float8Struct target_result;
		target_result.v.e[0] = 1;
		target_result.v.e[1] = 2;
		target_result.v.e[2] = 3;
		target_result.v.e[3] = 4;
		target_result.v.e[4] = 4;
		target_result.v.e[5] = 5;
		target_result.v.e[6] = 6;
		target_result.v.e[7] = 7;
		
		 //Float8Struct([min(e0(a.v), e0(b.v)), min(e1(a.v), e1(b.v)),	min(e2(a.v), e2(b.v)),	min(e3(a.v), e3(b.v)), min(e4(a.v), e4(b.v)), min(e5(a.v), e5(b.v)), min(e6(a.v), e6(b.v)), min(e7(a.v), e7(b.v))]v ) 
		testFloat8Struct(
			"struct Float8Struct { vector<float, 8> v } \n\
			def min(Float8Struct a, Float8Struct b) Float8Struct : Float8Struct(min(a.v, b.v)) \n\
			def main(Float8Struct a, Float8Struct b) Float8Struct : \n\
				min(a, b)", 
			a, b, target_result
		);
	}


	// Test alignment of structure elements when they are vectors
	testMainFloatArg("struct vec4 { vector<float, 4> v }					\n\
				   struct vec16 { vector<float, 16> v }					\n\
				   struct large_struct { vec4 a, vec16 b }				\n\
				   def main(float x) float : large_struct(vec4([x, x, x, x]v), vec16([x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x]v)).a.v.e0",
				  3.0f, 3.0f);


	//TEMP:
	// Try dot product
	testMainFloatArg("	def main(float x) float : \
					 let v = [x, x, x, x]v in\
					 dot(v, v)", 4.0f, 64.0f);

	testMainFloatArg("	def main(float x) float : \
					 let v = [x, x, x, x]v in\
					 dot(v, v)", 2.0f, 16.0f);

	testMainDoubleArg("	def main(double x) double : \
					 let v = [x, x, x, x]v in\
					 dot(v, v)", 4.0, 64.0);

	// 2-vector dot product
	testMainFloatArg("def main(float x) float : \
					 let v = [x, x]v in\
					 dot(v, v)", 4.0f, 32.0f);

	testMainDoubleArg("def main(double x) double : \
					 let v = [x, x]v in\
					 dot(v, v)", 4.0, 32.0);
	

	// Test avoidance of circular variable definition: 
	testMainFloatArg("									\n\
		def main(float x) float :		\n\
			let							\n\
				x = 2.0					\n\
			in							\n\
				x						",
		2.0f,
		2.0f
	);

	// Test avoidance of circular variable definition: the x(pos) expression was attempting to get the type of the 'x =' let node,
	// which was not known yet as was being computed.  The solution adopted is to not try to bind to let variables that are ancestors of the current variable.
	// Another solution could be to not try to bind to unbound variables.
	testMainFloatArg("									\n\
		struct vec3 { float x, float y, float z }		\n\
		def vec3(float v) vec3 : vec3(v, v, v)			\n\
		def eval(vec3 pos) vec3 :						\n\
			let											\n\
				x = sin(x(pos) * 1000.0)				\n\
			in											\n\
				vec3(0.1)								\n\
		def main(float t) float: x(eval(vec3(t, t, t)))",
		1.0f,
		0.1f
	);



	
	
	// Test comparison vs addition precedence: addition should have higher precedence.
	testMainFloatArg("def main(float x) float : if(x < 1.0 + 2.0, 5.0, 6.0)", 1.0f, 5.0f);




	// Test capture of let variable.
	
	//NOTE: Disabled, because these tests leak due to call to allocateRefCountedStructure().
	/*testMainFloat("	def main() float :                          \n\
				  let blerg = 3.0 in                     \n\
				  let f = \\() : blerg  in                    \n\
				  f()", 3.0);
	*/
	

	testMainFloatArg("def main(float x) float : sin(x)", 1.0f, std::sin(1.0f));


	// Test boolean logical expressions
	testMainFloat(" def main() float : if(true && true, 1.0, 2.0)", 1.0f);
	testMainFloat(" def main() float : if(true && false, 1.0, 2.0)", 2.0f);
	testMainFloat(" def main() float : if(true || false, 1.0, 2.0)", 1.0f);
	testMainFloat(" def main() float : if(false || false, 1.0, 2.0)", 2.0f);





	// Test 'if'
	testMainInteger("def main() int : if(true, 2, 3)", 2);
	testMainInteger("def main() int : if(false, 2, 3)", 3);

	// Test 'if' with structures as the return type
	testMainInteger(
		"struct s { int a, int b }   \n\
		def main() int : a(if(true, s(1, 2), s(3, 4)))", 
		1);
	testMainInteger(
		"struct s { int a, int b }   \n\
		def main() int : a(if(false, s(1, 2), s(3, 4)))", 
		3);

	// Test 'if' with vectors as the return type
	testMainFloat(
		"def main() float : e0(if(true, [1.0, 2.0, 3.0, 4.0]v, [10.0, 20.0, 30.0, 40.0]v))", 
		1.0f);
	testMainFloat(
		"def main() float : e0(if(false, [1.0, 2.0, 3.0, 4.0]v, [10.0, 20.0, 30.0, 40.0]v))", 
		10.0f);


	// Simple test
	testMainFloat("def main() float : 1.0", 1.0);

	// Test addition expression
	testMainFloat("def main() float : 1.0 + 2.0", 3.0);

	// Test integer addition
	testMainInteger("def main() int : 1 + 2", 3);

	// Test multiple integer additions
	testMainInteger("def main() int : 1 + 2 + 3", 6);
	testMainInteger("def main() int : 1 + 2 + 3 + 4", 10);

	// Test multiple integer subtractions
	testMainInteger("def main() int : 1 - 2 - 3 - 4", 1 - 2 - 3 - 4);

	// Test left-to-right associativity
	// Note that right-to-left associativity here would give 2 - (3 + 4) = -5
	assert(2 - 3 + 4 == (2 - 3) + 4);
	assert(2 - 3 + 4 == 3);
	testMainInteger("def main() int : 2 - 3 + 4", 3);


	// Test multiplication expression
	testMainFloat("def main() float : 3.0 * 2.0", 6.0);




	// Test integer multiplication
	testMainInteger("def main() int : 2 * 3", 6);

	// Test multiple integer multiplication
	testMainInteger("def main() int : 2 * 3 * 4 * 5", 2 * 3 * 4 * 5);

	// Test left-to-right associativity of division
	// 12 / 4 / 3 = (12 / 4) / 3 = 1
	// whereas
	// 12 / (4 / 3) = 12 / 1 = 12
	testMainInteger("def main() int : 12 / 4 / 3", 1);


	// Test float subtraction
	testMainFloat("def main() float : 3.0 - 2.0", 1.0);

	// Test integer subtraction
	testMainInteger("def main() int : 2 - 3", -1);

	// Test precedence
	testMainInteger("def main() int : 2 + 3 * 4", 14);
	testMainInteger("def main() int : 2 * 3 + 4", 10);

	// Test parentheses controlling order of operation
	testMainInteger("def main() int : (2 + 3) * 4", 20);
	testMainInteger("def main() int : 2 * (3 + 4)", 14);


	// Test unary minus in front of parenthesis
	testMainInteger("def main() int : -(1 + 2)", -3);

	// Test floating point unary minus
	testMainFloat("def main() float : -(1.0 + 2.0)", -3.0);

	// Test unary minus in front of var
	testMainInteger("def f(int x) int : -x        def main() int : f(3)", -3);

	// Test unary minus with space
	testMainFloatArg("def main(float x) : - x", 10.f, -10.f);
	// Test double unary minus
	testMainFloatArg("def main(float x) : - -x", 10.f, 10.f);
	testMainFloatArg("def main(float x) : --x", 10.f, 10.f);




	// Test simple function call
	testMainFloat("def f(float x) float : x        def main() float : f(3.0)", 3.0);

	// Test function call with two parameters
	testMainFloat("def f(float x, float y) float : x        def main() float : f(3.0, 4.0)", 3.0);
	testMainFloat("def f(float x, float y) float : y        def main() float : f(3.0, 4.0)", 4.0);

	// Test inferred return type (for f)
	testMainFloat("def f(float x) : x        def main() float : f(3.0)", 3.0);

	// Test two call levels of inferred return type (f, g)
	testMainFloat("def g(float x) : x    def f(float x) : g(x)       def main() float : f(3.0)", 3.0);
	testMainFloat("def f(float x) : x    def g(float x) : f(x)       def main() float : g(3.0)", 3.0);

	// Test generic function
	testMainFloat("def f<T>(T x) T : x        def main() float : f(2.0)", 2.0);

	// Test generic function with inferred return type (f)
	testMainFloat("def f<T>(T x) : x        def main() float : f(2.0)", 2.0);


	// Test function overloading - call with int param, should select 1st overload
	testMainFloat("def overloadedFunc(int x) float : 4.0 \
				  def overloadedFunc(float x) float : 5.0 \
				  def main() float: overloadedFunc(1)", 4.0f);

	// Call with float param, should select 2nd overload.
	testMainFloat("def overloadedFunc(int x) float : 4.0 \
				  def overloadedFunc(float x) float : 5.0 \
				  def main() float: overloadedFunc(1.0)", 5.0f);

	// Test binding to different overloaded functions based on type parameter to generic function
	testMainFloat("def overloadedFunc(int x) float : 4.0 \
				  def overloadedFunc(float x) float : 5.0 \
				  def f<T>(T x) float: overloadedFunc(x)\
				  def main() float : f(1)", 4.0f);


	// Test invalidity of recursive call in generic function.
	testMainFloatArgInvalidProgram("def f<T>(T x) f(x)   def main(float x) float : f(x)");
	testMainFloatArgInvalidProgram("def f<T>(T x) T : x*x / (f(2) + 3)   def main(float x) float : 1/ (f(2) + 3)");


	// Call f with float param
	testMainFloat("def overloadedFunc(int x) float : 4.0 \
				  def overloadedFunc(float x) float : 5.0 \
				  def f<T>(T x) float: overloadedFunc(x)\
				  def main() float : f(1.0)", 5.0f);

	
	// Test structure being returned from main function
	{
		TestStruct target_result;
		target_result.a = 1;
		target_result.b = 2;
		target_result.c = 3;
		target_result.d = 4;

		std::string test_func = "struct TestStruct { float a, float b, float c, float d } \
			def main() TestStruct : TestStruct(1.0, 2.0, 3.0, 4.0)";
		testMainStruct<TestStruct>(test_func, target_result);
	}
	{
		TestStruct target_result;
		target_result.a = 5;
		target_result.b = 6;
		target_result.c = 3;
		target_result.d = 4;

		TestStructIn in;
		in.x = 5;
		in.y = 6;

		std::string test_func = "struct TestStruct { float a, float b, float c, float d } \
			struct TestStructIn { float x, float y } \
			def main(TestStructIn in_s) TestStruct : TestStruct(x(in_s), y(in_s), 3.0, 4.0)";
		testMainStructInputAndOutput(test_func, in, target_result);
	}

	
	// Test vector in structure
	{
		StructWithVec in;
		in.a.e[0] = 1;
		in.a.e[1] = 2;
		in.a.e[2] = 3;
		in.a.e[3] = 4;

		in.b.e[0] = 4;
		in.b.e[1] = 5;
		in.b.e[2] = 6;
		in.b.e[3] = 7;

		in.data2 = 10;

		StructWithVec target_result;
		target_result.a.e[0] = 5;
		target_result.a.e[1] = 7;
		target_result.a.e[2] = 9;
		target_result.a.e[3] = 11;

		target_result.b.e[0] = 1;
		target_result.b.e[1] = 2;
		target_result.b.e[2] = 3;
		target_result.b.e[3] = 4;

		target_result.data2 = 10;

		testVectorInStruct(
			"struct StructWithVec { vector<float, 4> a, vector<float, 4> b, float data2 } \n\
			def main(StructWithVec in_s) StructWithVec : \n\
				StructWithVec(  \n\
				a(in_s) + b(in_s), #[e0(a(in_s)) + e0(b(in_s)), e1(a(in_s)) + e1(b(in_s)), e2(a(in_s)) + e2(b(in_s)), e3(a(in_s)) + e3(b(in_s))]v, \n\
				a(in_s), \n\
				data2(in_s))", 
			in, target_result);
	}

	conPrint("=================== All Winter tests passed.  Elapsed: " + timer.elapsedStringNPlaces(2) + " =============================");
}


} // end namespace Winter


#endif // BUILD_TESTS
