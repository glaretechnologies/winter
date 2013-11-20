#pragma once



//#include <maths/sse.h>
extern "C"
{
#include <xmmintrin.h> //SSE header file
};


#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/fileutils.h"
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





static bool epsEqual(float x, float y)
{
	return std::fabs(x - y) < 1.0e-5f;
}


struct TestEnv
{
	float val;
};


static float testFunc(float x, TestEnv* env)
{
	std::cout << "In test func!, " << x << std::endl;
	std::cout << "In test func!, env->val: " << env->val << std::endl;
	return env->val;
}


static ValueRef testFuncInterpreted(const vector<ValueRef>& arg_values)
{
	assert(arg_values.size() == 2);
	assert(dynamic_cast<const FloatValue*>(arg_values[0].getPointer()));
	assert(dynamic_cast<const VoidPtrValue*>(arg_values[1].getPointer()));

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());
	const VoidPtrValue* voidptr_val = static_cast<const VoidPtrValue*>(arg_values[1].getPointer());

	return ValueRef(new FloatValue(testFunc(float_val->value, (TestEnv*)voidptr_val->value)));
}


/*static float externalSin(float x, TestEnv* env)
{
	return std::sin(x);
}*/


static ValueRef externalSinInterpreted(const vector<ValueRef>& arg_values)
{
	assert(arg_values.size() == 1);
	assert(dynamic_cast<const FloatValue*>(arg_values[0].getPointer()));
	//assert(dynamic_cast<const VoidPtrValue*>(arg_values[1].getPointer()));

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());
	//const VoidPtrValue* voidptr_val = static_cast<const VoidPtrValue*>(arg_values[1].getPointer());

	return ValueRef(new FloatValue(std::sin(float_val->value/*, (TestEnv*)voidptr_val->value*/)));
}


typedef float(WINTER_JIT_CALLING_CONV * float_void_func)(void* env);


static void testMainFloat(const std::string& src, float target_return_val)
{
	std::cout << "===================== Winter testMainFloat() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testFunc;
			f->interpreted_func = testFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testFunc", vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}
		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)(float(*)(float))std::sin; //externalSin;
			f->interpreted_func = externalSinInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("sin", vector<TypeRef>(1, TypeRef(new Float())));
			f->takes_hidden_voidptr_arg = false;
			vm_args.external_functions.push_back(f);
		}

		const FunctionSignature mainsig("main", std::vector<TypeRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		void* f = vm.getJittedFunction(mainsig);

		//// cast to correct type
		float_void_func mainf = (float_void_func)f;


		//// Call the JIT'd function
		const float jitted_result = mainf(&test_env);


		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate(true);
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(&test_env)));

		ValueRef retval = maindef->invoke(vmstate);

		assert(vmstate.argument_stack.size() == 1);
		//delete vmstate.argument_stack[0];
		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval.getPointer());
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}

		if(!epsEqual(val->value, target_return_val))
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

	//	delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testMainFloatArgInvalidProgram(const std::string& src, float argument, float target_return_val)
{
	std::cout << "===================== Winter testMainFloatArgInvalidProgram() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		float(WINTER_JIT_CALLING_CONV*f)(float, void*) = (float(WINTER_JIT_CALLING_CONV*)(float, void*))vm.getJittedFunction(mainsig);


		std::cerr << "Test failed: Expected compilation failure." << std::endl;
		exit(1);
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		std::cout << "Expected exception occurred: " << e.what() << std::endl;
	}
}


static void testMainFloatArg(const std::string& src, float argument, float target_return_val)
{
	std::cout << "===================== Winter testMainFloatArg() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testFunc;
			f->interpreted_func = testFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testFunc", vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}
		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		float(WINTER_JIT_CALLING_CONV*f)(float, void*) = (float(WINTER_JIT_CALLING_CONV*)(float, void*))vm.getJittedFunction(mainsig);


		// Call the JIT'd function
		const float jitted_result = f(argument, &test_env);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate(true);
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(ValueRef(new FloatValue(argument)));
		vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(&test_env)));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval.getPointer());
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			exit(1);
		}

		if(!epsEqual(val->value, target_return_val))
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			exit(1);
		}

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static void testMainInteger(const std::string& src, int target_return_val)
{
	std::cout << "===================== Winter testMainInteger() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int (WINTER_JIT_CALLING_CONV *f)(void*) = (int (WINTER_JIT_CALLING_CONV *)(void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;

		// Call the JIT'd function
		const int jitted_result = f(&test_env);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate(true);
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(&test_env)));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		IntValue* val = dynamic_cast<IntValue*>(retval.getPointer());
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

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testMainIntegerArg(const std::string& src, int x, int target_return_val)
{
	std::cout << "===================== Winter testMainIntegerArg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, new Int()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int (WINTER_JIT_CALLING_CONV *f)(int, void*) = (int (WINTER_JIT_CALLING_CONV *)(int, void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;

		// Call the JIT'd function
		const int jitted_result = f(x, &test_env);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate(true);
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(ValueRef(new IntValue(x)));
		vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(&test_env)));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		IntValue* val = dynamic_cast<IntValue*>(retval.getPointer());
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
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static void testMainIntegerArgInvalidProgram(const std::string& src, int argument)
{
	std::cout << "===================== Winter testMainIntegerArgInvalidProgram() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Int())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int(WINTER_JIT_CALLING_CONV*f)(int, void*) = (int(WINTER_JIT_CALLING_CONV*)(int, void*))vm.getJittedFunction(mainsig);

		std::cerr << "Test failed: Expected compilation failure." << std::endl;
		exit(1);
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		std::cout << "Expected exception occurred: " << e.what() << std::endl;
	}
}


//typedef float(*float_void_func)();





template <class StructType>
static void bleh(StructType* s)
{
	s->a = 1;
}



#if defined(_WIN32) || defined(_WIN64)
#define SSE_ALIGN _MM_ALIGN16
#define SSE_CLASS_ALIGN _MM_ALIGN16 class
#else
#define SSE_ALIGN __attribute__ ((aligned (16)))
#define SSE_CLASS_ALIGN class __attribute__ ((aligned (16)))
#endif






template <class StructType>
static void testMainStruct(const std::string& src, const StructType& target_return_val)
{
	std::cout << "===================== Winter testMainStruct() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		SSE_ALIGN StructType jitted_result;
		
		void (WINTER_JIT_CALLING_CONV *f)(StructType*, void*) = (void (WINTER_JIT_CALLING_CONV *)(StructType*, void*))vm.getJittedFunction(mainsig);
		//StructType (WINTER_JIT_CALLING_CONV *f)() = (StructType (WINTER_JIT_CALLING_CONV *)())vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;


		// Call the JIT'd function
		f(&jitted_result, &test_env);
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
			assert(0);
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
	std::cout << "===================== Winter testMainStructInputAndOutput() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		vector<string> field_names;
		field_names.push_back("x");
		field_names.push_back("y");

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(1, TypeRef(new StructureType(
				"TestStructIn", 
				std::vector<TypeRef>(2, TypeRef(new Float)), 
				field_names
			)))
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(OutStructType*, InStructType*, void*) = (void (WINTER_JIT_CALLING_CONV *)(OutStructType*, InStructType*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN OutStructType jitted_result;

		SSE_ALIGN InStructType aligned_struct_in = struct_in;

		TestEnv test_env;
		test_env.val = 10;

		f(&jitted_result, &aligned_struct_in, &test_env);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
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


SSE_CLASS_ALIGN float8
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


SSE_CLASS_ALIGN Float8Struct
{
public:
	float8 v;

	inline bool operator == (const Float8Struct& other)
	{
		return v == other.v;
	}
};


static void testFloat4StructPairRetFloat(const std::string& src, const Float4StructPair& a, const Float4StructPair& b, float target_return_val)
{
	std::cout << "===================== Winter testFloat4StructPairRetFloat() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));


		// Create Float4Struct type
		TypeRef float_4_struct_type;
		{
			vector<string> field_names;
			field_names.push_back("v");

			vector<TypeRef> field_types;
			field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));

			float_4_struct_type = new StructureType(
				"Float4Struct", 
				field_types, 
				field_names
			);
		}

		// Create Float4StructPair type
		vector<string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");

		vector<TypeRef> field_types(2, float_4_struct_type);

		TypeRef Float4StructPair_type = new StructureType(
			"Float4StructPair", 
			field_types, 
			field_names
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, Float4StructPair_type)
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		float (WINTER_JIT_CALLING_CONV *f)(const Float4StructPair*, const Float4StructPair*, void*) = (float (WINTER_JIT_CALLING_CONV *)(const Float4StructPair*, const Float4StructPair*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const float jitted_result = f(&a, &b, NULL);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
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


static void testVectorInStruct(const std::string& src, const StructWithVec& struct_in, const StructWithVec& target_return_val)
{
	std::cout << "===================== Winter testVectorInStruct() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		vector<string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");
		field_names.push_back("data2");

		vector<TypeRef> field_types;
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));
		field_types.push_back(TypeRef(new Float));


		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(1, TypeRef(new StructureType(
				"StructWithVec", 
				field_types, 
				field_names
			)))
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(StructWithVec*, StructWithVec*, void*) = (void (WINTER_JIT_CALLING_CONV *)(StructWithVec*, StructWithVec*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN StructWithVec jitted_result;

		SSE_ALIGN StructWithVec aligned_struct_in = struct_in;

		TestEnv test_env;
		test_env.val = 10;

		f(&jitted_result, &aligned_struct_in, &test_env);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
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


static void testFloat4Struct(const std::string& src, const Float4Struct& a, const Float4Struct& b, const Float4Struct& target_return_val)
{
	std::cout << "===================== Winter testFloat4Struct() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		vector<string> field_names;
		field_names.push_back("v");

		vector<TypeRef> field_types;
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));


		TypeRef float_4_struct_type = new StructureType(
			"Float4Struct", 
			field_types, 
			field_names
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, float_4_struct_type)
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(Float4Struct*, const Float4Struct*, const Float4Struct*, void*) = 
			(void (WINTER_JIT_CALLING_CONV *)(Float4Struct*, const Float4Struct*, const Float4Struct*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		Float4Struct jitted_result;

		TestEnv test_env;
		test_env.val = 10;

		f(&jitted_result, &a, &b, &test_env);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testFloat8Struct(const std::string& src, const Float8Struct& a, const Float8Struct& b, const Float8Struct& target_return_val)
{
	std::cout << "===================== Winter testFloat8Struct() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		vector<string> field_names;
		field_names.push_back("v");

		vector<TypeRef> field_types;
		field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 8)));


		TypeRef float_8_struct_type = new StructureType(
			"Float8Struct", 
			field_types, 
			field_names
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, float_8_struct_type)
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(Float8Struct*, const Float8Struct*, const Float8Struct*, void*) = 
			(void (WINTER_JIT_CALLING_CONV *)(Float8Struct*, const Float8Struct*, const Float8Struct*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		Float8Struct jitted_result;

		TestEnv test_env;
		test_env.val = 10;

		f(&jitted_result, &a, &b, &test_env);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


static void testFloatArray(const std::string& src, const float* a, const float* b, const float* target_return_val, size_t len)
{
	std::cout << "===================== Winter testFloatArray() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, new ArrayType(new Float(), 4)) // 2 float arrays of 4 elems each
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(float*, const float*, const float*, void*) = 
			(void (WINTER_JIT_CALLING_CONV *)(float*, const float*, const float*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		std::vector<float> jitted_result(len);
		float* jitted_result_ptr = &jitted_result[0];

		TestEnv test_env;
		test_env.val = 10;

		f(jitted_result_ptr, a, b, &test_env);

		// Check JIT'd result.
		for(size_t i=0; i<len; ++i)
		{
			if(jitted_result[i] != target_return_val[i])
			{
				std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
				assert(0);
				exit(1);
			}
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		exit(1);
	}
}


float test()
{
	return 10;
}

//int test2()
//{
//	return 3.0f;
//}

SSE_CLASS_ALIGN test_vec4
{
	float x[4];
};

SSE_CLASS_ALIGN test_vec16
{
	float x[16];
};

SSE_CLASS_ALIGN large_struct
{
	test_vec4 a;
	test_vec16 b;
};


float someFuncBleh(float x)
{
	return x + 1; 
}


} // end namespace Winter
