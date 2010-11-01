//
// Generated by makeclass.rb on Sun Oct 04 15:08:43 +1300 2009.
// Copyright Nicholas Chapman.
//
#include "LanguageTests.h"


//#include <maths/sse.h>
extern "C"
{
#include <xmmintrin.h> //SSE header file
};


#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/FileUtils.h"
#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_ASTNode.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "VirtualMachine.h"


namespace Winter
{


LanguageTests::LanguageTests()
{}


LanguageTests::~LanguageTests()
{}


static float testFunc(float x)
{
	std::cout << "In test func!, " << x << std::endl;
	return x * x;
}


static Value* testFuncIntepreted(const vector<const Value*>& arg_values)
{
	assert(arg_values.size() == 1);
	assert(dynamic_cast<const FloatValue*>(arg_values[0]));

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0]);

	return new FloatValue(testFunc(float_val->value));
}


typedef float(WINTER_JIT_CALLING_CONV * float_void_func)();


static void testMainFloat(const std::string& src, float target_return_val)
{
	std::cout << "============================== testMainFloat() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = testFunc;
			f->interpreted_func = testFuncIntepreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testFunc", vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}

		VirtualMachine vm(vm_args);

		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>());
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		void* f = vm.getJittedFunction(mainsig);

		// cast to correct type
		float_void_func mainf = (float_void_func)f;

		// Call the JIT'd function
		const float jitted_result = mainf();


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval);
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			exit(1);
		}

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testMainFloatArg(const std::string& src, float argument, float target_return_val)
{
	std::cout << "============================== testMainFloat() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = testFunc;
			f->interpreted_func = testFuncIntepreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testFunc", vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}

		VirtualMachine vm(vm_args);

		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		float(WINTER_JIT_CALLING_CONV*f)(float) = (float(WINTER_JIT_CALLING_CONV*)(float))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const float jitted_result = f(argument);

		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new FloatValue(argument));

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval);
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			exit(1);
		}

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testMainInteger(const std::string& src, float target_return_val)
{
	std::cout << "============================== testMainInteger() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		VirtualMachine vm(vm_args);

		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>());
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int (WINTER_JIT_CALLING_CONV *f)() = (int (WINTER_JIT_CALLING_CONV *)()) vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const int jitted_result = f();


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		IntValue* val = dynamic_cast<IntValue*>(retval);
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			exit(1);
		}

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


//typedef float(*float_void_func)();





template <class StructType>
static void bleh(StructType* s)
{
	s->a = 1;
}



#if defined(WIN32) || defined(WIN64)
#define SSE_ALIGN _MM_ALIGN16
#define SSE_CLASS_ALIGN _MM_ALIGN16 class
#else
#define SSE_ALIGN __attribute__ ((aligned (16)))
#define SSE_CLASS_ALIGN class __attribute__ ((aligned (16)))
#endif






template <class StructType>
static void testMainStruct(const std::string& src, const StructType& target_return_val)
{
	std::cout << "============================== testMainStruct() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		VirtualMachine vm(vm_args);

		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>());
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		SSE_ALIGN StructType jitted_result;
		
		void (WINTER_JIT_CALLING_CONV *f)(StructType*) = (void (WINTER_JIT_CALLING_CONV *)(StructType*))vm.getJittedFunction(mainsig);
		//StructType (WINTER_JIT_CALLING_CONV *f)() = (StructType (WINTER_JIT_CALLING_CONV *)())vm.getJittedFunction(mainsig);


		// Call the JIT'd function
		f(&jitted_result);
		//jitted_result = f();

		/*std::cout << "============================" << std::endl;
		std::cout << jitted_result.a << std::endl;
		std::cout << jitted_result.b << std::endl;
		std::cout << jitted_result.c << std::endl;
		std::cout << jitted_result.d << std::endl;*/

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			exit(1);
		}

		/*VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		StructureValue* val = dynamic_cast<StructureValue*>(retval);
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			exit(1);
		}*/


		/*if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			exit(1);
		}*/

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


template <class InStructType, class OutStructType>
static void testMainStructInputAndOutput(const std::string& src, const InStructType& struct_in, const OutStructType& target_return_val)
{
	std::cout << "============================== testMainStructInputAndOutput() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		VirtualMachine vm(vm_args);

		vector<string> field_names;
		field_names.push_back("x");
		field_names.push_back("y");

		// Get main function
		FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(1, TypeRef(new StructureType(
				"TestStructIn", 
				std::vector<TypeRef>(2, TypeRef(new Float)), 
				field_names
			)))
		);
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(OutStructType*, InStructType*) = (void (WINTER_JIT_CALLING_CONV *)(OutStructType*, InStructType*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN OutStructType jitted_result;

		SSE_ALIGN InStructType aligned_struct_in = struct_in;

		f(&jitted_result, &aligned_struct_in);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			exit(1);
		}

		/*VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		StructureValue* val = dynamic_cast<StructureValue*>(retval);
		if(!val)
		{
		std::cerr << "main() Return value was of unexpected type." << std::endl;
		exit(1);
		}*/


		/*if(val->value != target_return_val)
		{
		std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
		exit(1);
		}*/

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


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


static void testVectorInStruct(const std::string& src, const StructWithVec& struct_in, const StructWithVec& target_return_val)
{
	std::cout << "============================== testVectorInStruct() ============================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(src);

		VirtualMachine vm(vm_args);

		vector<string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");
		field_names.push_back("data2");

		vector<TypeRef> field_types;
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));
		field_types.push_back(TypeRef(new Float));


		// Get main function
		FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(1, TypeRef(new StructureType(
				"StructWithVec", 
				field_types, 
				field_names
			)))
		);
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(StructWithVec*, StructWithVec*) = (void (WINTER_JIT_CALLING_CONV *)(StructWithVec*, StructWithVec*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN StructWithVec jitted_result;

		SSE_ALIGN StructWithVec aligned_struct_in = struct_in;

		f(&jitted_result, &aligned_struct_in);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			exit(1);
		}

		/*VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		StructureValue* val = dynamic_cast<StructureValue*>(retval);
		if(!val)
		{
		std::cerr << "main() Return value was of unexpected type." << std::endl;
		exit(1);
		}*/


		/*if(val->value != target_return_val)
		{
		std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
		exit(1);
		}*/

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


void LanguageTests::run()
{
/*	// Integer comparisons:
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



	// Test call to external function
	testMainFloat("def main() float : testFunc(3.0)", 9.0);


	
	// Simple test
	testMainFloat("def main() float : 1.0", 1.0);

	// Test addition expression
	testMainFloat("def main() float : 1.0 + 2.0", 3.0);

	// Test integer addition
	testMainInteger("def main() int : 1 + 2", 3);

	// Test multiple integer additions
	testMainInteger("def main() int : 1 + 2 + 3", 6);
	//testMainInteger("def main() int : 1 + 2 + 3 + 4", 10);

	// Test left-to-right associativity
	assert(2 - 3 + 4 == (2 - 3) + 4);
	assert(2 - 3 + 4 == 3);
	//testMainInteger("def main() int : 2 - 3 + 4", 3);


	// Test multiplication expression
	testMainFloat("def main() float : 3.0 * 2.0", 6.0);

	// Test integer multiplication
	testMainInteger("def main() int : 2 * 3", 6);

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




	// Test simple function call
	testMainFloat("def f(float x) float : x        def main() float : f(3.0)", 3.0);

	// Test function call with two parameters
	testMainFloat("def f(float x, float y) float : x        def main() float : f(3.0, 4.0)", 3.0);
	testMainFloat("def f(float x, float y) float : y        def main() float : f(3.0, 4.0)", 4.0);

	// Test inferred return type (for f)
	testMainFloat("def f(float x) : x        def main() float : f(3.0)", 3.0);

	// Test two call levels of inferred return type (f, g)
	testMainFloat("def f(float x) : g(x)    def g(float x) : x    def main() float : f(3.0)", 3.0);
	testMainFloat("def f(float x) : x    def g(float x) : f(x)    def main() float : g(3.0)", 3.0);

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
	// Call f with float param
	testMainFloat("def overloadedFunc(int x) float : 4.0 \
				  def overloadedFunc(float x) float : 5.0 \
				  def f<T>(T x) float: overloadedFunc(x)\
				  def main() float : f(1.0)", 5.0f);

	// Test let
	testMainFloat("def f(float x) float : \
				  let z = 2.0 \
				  z \
				  def main() float : f(0.0)", 2.0);

	// Test two let clauses
	testMainFloat("def f(float x) float : \
				  let z = 2.0 \
				  let y = 3.0 \
				  y \
				  def main() float : f(0.0)", 3.0);

	// Test Lambda in let
	//Doesn't work with LLVM
	//testMainFloat("def main() float : let f = \\(float x) : x        f(2.0)", 2.0f);

	// Test addition expression in let
	testMainFloat("def f(float x) float : \
				  let z = 2.0 + 3.0 \
				  z \
				  def main() float : f(0.0)", 5.0);

	// Test function expression in let
	testMainFloat("	def g(float x) float : x + 1.0 \
					def f(float x) float : \
					let z = g(1.0) \
					z \
					def main() float : f(0.0)", 2.0);

	// Test function argument in let
	testMainFloat("	def f(float x) float : \
					let z = x + 1.0 \
					z \
					def main() float : f(2.0)", 3.0);


	// Test use of let variable twice
	testMainFloat("	def f(float x) float : \
				  let z = x + 1.0 \
				  z + z\
				  def main() float : f(2.0)", 6.0);


	// Test struct
	testMainFloat("struct Complex { float re, float im } \
				  def main() float : re(Complex(2.0, 3.0))", 2.0f);
	
	testMainFloat("struct Complex { float re, float im } \
 				  def main() float : im(Complex(2.0, 3.0))", 3.0f);

	// Test struct in struct
	testMainFloat("struct Complex { float re, float im } \
				  struct ComplexPair { Complex a, Complex b } \
				  def main() float : im(a(ComplexPair(Complex(2.0, 3.0), Complex(4.0, 5.0))))",
				  3.0f);



	// Test vector
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					e0(x)", 1.0f);
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					e1(x)", 2.0f);

	// Test vector being returned from a function
	testMainFloat("	def f() vector<float, 4> : [1.0, 2.0, 3.0, 4.0]v \
					def main() float : e2(f())", 3.0f);

	// Test vector addition
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					let y = [10.0, 20.0, 30.0, 40.0]v \
					e1(x + y)", 22.0f);

	// Test vector subtraction
	testMainFloat("	def main() float : \
					let x = [1.0, 2.0, 3.0, 4.0]v \
					let y = [10.0, 20.0, 30.0, 40.0]v \
					e1(x - y)", -18.0f);

	// Test vector * float multiplication
	// NOTE: doesn't work yet.
	//testMainFloat("	def main() float : \
	//			  let x = [1.0, 2.0, 3.0, 4.0]v \
	///		  e1(x * 10.0)", 2.0f * 10.0f);

	// Test vector * vector multiplication
	testMainFloat("	def main() float : \
				  let x = [1.0, 2.0, 3.0, 4.0]v \
				  let y = [10.0, 20.0, 30.0, 40.0]v \
				e1(x * y)", 2.0f * 20.0f);

	// Test vector * scalar multiplication
	testMainFloat("	def mul(vector<float, 4> v, float x) vector<float, 4> : v * [x, x, x, x]v \n\
					def main() float : \
						let x = [1.0, 2.0, 3.0, 4.0]v \
						let y = 10.0 \
						e1(mul(x, y))", 2.0f * 10.0f);

	testMainFloatArg("	def mul(vector<float, 4> v, float x) vector<float, 4> : v * [x, x, x, x]v \n\
				  def main(float x) float : \
				  let v = [1.0, 2.0, 3.0, 4.0]v \
				  e1(mul(v, x))", 10.0f, 2.0f * 10.0f);

	// Try dot product
	testMainFloatArg("	def main(float x) float : \
					 let v = [x, x, x, x]v \
					 dot(v, v)", 2.0f, 16.0f);

	// Test vector min
	testMainFloat("	def main() float : \
					 let a = [1.0, 2.0, 3.0, 4.0]v \
					 let b = [11.0, 12.0, 13.0, 14.0]v \
					 e2(min(a, b))", 3.0);
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  let b = [11.0, 12.0, 13.0, 14.0]v \
				  e2(min(b, a))", 3.0);

	// Test vector max
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  let b = [11.0, 12.0, 13.0, 14.0]v \
				  e2(max(a, b))", 13.0);
	testMainFloat("	def main() float : \
				  let a = [1.0, 2.0, 3.0, 4.0]v \
				  let b = [11.0, 12.0, 13.0, 14.0]v \
				  e2(max(b, a))", 13.0);
				  */

	testMainFloat("	def clamp(vector<float, 4> x, vector<float, 4> lowerbound, vector<float, 4> upperbound) vector<float, 4> : max(lowerbound, min(upperbound, x))  \n\
					def make_v4f(float x) vector<float, 4> : [x, x, x, x]v  \n\
					def main() float : \
					let a = [1.0, 2.0, 3.0, 4.0]v \
					e2(clamp(a, make_v4f(2.0), make_v4f(2.5)))", 2.5);

	testMainFloat("	struct PolarisationVec { vector<float, 8> e } \n\
																	\n\
					def clamp(vector<float, 4> x, vector<float, 4> lowerbound, vector<float, 4> upperbound) vector<float, 4> : max(lowerbound, min(upperbound, x))  \n\
																																					\n\
					def clamp(PolarisationVec x, float lowerbound, float upperbound) PolarisationVec : \n\
						let lo = [e0(e(x)), e1(e(x)), e2(e(x)), e3(e(x))]v   \n\
						let hi = [e4(e(x)), e5(e(x)), e6(e(x)), e7(e(x))]v    \n\
						let clamped_lo = clamp(lo, make_v4f(lowerbound), make_v4f(upperbound))   \n\
						let clamped_hi = clamp(hi, make_v4f(lowerbound), make_v4f(upperbound))   \n\
						PolarisationVec([e0(clamped_lo), e1(clamped_lo), e2(clamped_lo), e3(clamped_lo), e0(clamped_hi), e1(clamped_hi), e2(clamped_hi), e3(clamped_hi)]v)   \n\
																																												\n\
				  def make_v4f(float x) vector<float, 4> : [x, x, x, x]v  \n\
																		\n\
				  def main() float : \
					let a = PolarisationVec([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]v) \
					e5(e(clamp(a, 2.0, 2.5)))", 2.5);

	//class PolarisationVec
	//{
	//public:
	//	__m128 e[2];


	/* Try matrix * vec4f mult

	*/
	/*testMainFloatArg("	struct Matrix { vector<float, 4> r0, vector<float, 4> r1, vector<float, 4> r2, vector<float, 4> r3 } \n\
						def mul(Matrix m, vector<float, 4> v) vector<float, 4> : [dot(r0(m), v), dot(r1(m), v), dot(r2(m), v), dot(r3(m), v)]v  \n\
						def main(float x) float : \
						let m = Matrix([x, x, x, x]v, [x, x, x, x]v, [x, x, x, x]v, [x, x, x, x]v) \
					 let v = [1.0, 2.0, 3.0, 4.0]v \
					 e0(mul(m, v))", 1.0f, 10.0f);*/


	// Test structure being returned from main function
	{
		struct TestStruct
		{
			float a;
			float b;
			float c;
			float d;

			bool operator == (const TestStruct& other) const { return (a == other.a) && (b == other.b); }
		};

		TestStruct target_result;
		target_result.a = 1;
		target_result.b = 2;
		target_result.c = 3;
		target_result.d = 4;
		testMainStruct<TestStruct>("struct TestStruct { float a, float b, float c, float d } \
								   def main() TestStruct : TestStruct(1.0, 2.0, 3.0, 4.0)", target_result);

		testMainStruct<TestStruct>("struct TestStruct { float a, float b, float c, float d } \
								   def main() TestStruct : TestStruct(1.0, 2.0, 3.0, 4.0)", target_result);

	}
	{
		struct TestStruct
		{
			float a;
			float b;
			float c;
			float d;

			bool operator == (const TestStruct& other) const { return (a == other.a) && (b == other.b); }
		};

		TestStruct target_result;
		target_result.a = 5;
		target_result.b = 6;
		target_result.c = 3;
		target_result.d = 4;

		struct TestStructIn
		{
			float x;
			float y;
		};

		TestStructIn in;
		in.x = 5;
		in.y = 6;

		testMainStructInputAndOutput("struct TestStruct { float a, float b, float c, float d } \
									 struct TestStructIn { float x, float y } \
									 def main(TestStructIn in) TestStruct : TestStruct(x(in), y(in), 3.0, 4.0)", in, target_result);
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
							def main(StructWithVec in) StructWithVec : \n\
								StructWithVec(  \n\
								a(in) + b(in), #[e0(a(in)) + e0(b(in)), e1(a(in)) + e1(b(in)), e2(a(in)) + e2(b(in)), e3(a(in)) + e3(b(in))]v, \n\
								a(in), \n\
								data2(in))", 
							in, target_result);
	}


	std::cout << "===================All LanguageTests passed.=============================" << std::endl;
}


}
