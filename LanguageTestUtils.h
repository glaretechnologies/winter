#pragma once



//#include <maths/sse.h>
extern "C"
{
#include <xmmintrin.h> //SSE header file
};


#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/Timer.h"
#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_ASTNode.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "VirtualMachine.h"
#include <StandardPrintOutput.h>
#include <Mutex.h>
#include <Lock.h>
#include <Exception.h>
#include <Vector.h>
#include <ConPrint.h>
#include <Platform.h>
//#include "utils/Obfuscator.h"


//#define WINTER_OPENCL_TESTS 1


// OpenCL:
#if WINTER_OPENCL_TESTS
#include "../../indigo/trunk/opencl/OpenCL.h"
#include "../../indigo/trunk/opencl/OpenCLBuffer.h"
#include "../../indigo/trunk/opencl/OpenCLKernel.h"
#endif

namespace Winter
{


const uint32 INVALID_OPENCL = 1; // Flag value
const uint32 ALLOW_UNSAFE = 2; // Flag value


struct TestResults
{
	ProgramStats stats;
	Reference<FunctionDefinition> maindef;
};


static bool epsEqual(float x, float y)
{
	return std::fabs(x - y) < 1.0e-5f;
}


static bool epsEqual(double x, double y)
{
	return std::fabs(x - y) < 1.0e-5;
}


struct TestEnv
{
	float val;
};


static float testExternalFunc(float x/*, TestEnv* env*/)
{
	//std::cout << "In test func!, " << x << std::endl;
	//std::cout << "In test func!, env->val: " << env->val << std::endl;
	//return env->val;
	return x * x;
}


static ValueRef testExternalFuncInterpreted(const std::vector<ValueRef>& arg_values)
{
	assert(arg_values.size() == 1);
	assert(arg_values[0]->valueType() == Value::ValueType_Float);
	//assert(dynamic_cast<const VoidPtrValue*>(arg_values[1].getPointer()));

	// Cast argument 0 to type FloatValue
	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());
	//const VoidPtrValue* voidptr_val = static_cast<const VoidPtrValue*>(arg_values[1].getPointer());

	return new FloatValue(testExternalFunc(float_val->value/*, (TestEnv*)voidptr_val->value*/));
}


/*static float externalSin(float x, TestEnv* env)
{
	return std::sin(x);
}*/


//static ValueRef externalSinInterpreted(const vector<ValueRef>& arg_values)
//{
//	assert(arg_values.size() == 1);
//	assert(dynamic_cast<const FloatValue*>(arg_values[0].getPointer()));
//	//assert(dynamic_cast<const VoidPtrValue*>(arg_values[1].getPointer()));
//
//	// Cast argument 0 to type FloatValue
//	const FloatValue* float_val = static_cast<const FloatValue*>(arg_values[0].getPointer());
//	//const VoidPtrValue* voidptr_val = static_cast<const VoidPtrValue*>(arg_values[1].getPointer());
//
//	return ValueRef(new FloatValue(std::sin(float_val->value/*, (TestEnv*)voidptr_val->value*/)));
//}


typedef float(WINTER_JIT_CALLING_CONV * float_void_func)(void* env);


static TestResults testMainFloat(const std::string& src, float target_return_val)
{
	std::cout << "===================== Winter testMainFloat() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.real_is_double = false;

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testExternalFunc;
			f->interpreted_func = testExternalFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testExternalFunc", std::vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}
		//{
		//	ExternalFunctionRef f(new ExternalFunction());
		//	f->func = (void*)(float(*)(float))std::sin; //externalSin;
		//	f->interpreted_func = externalSinInterpreted;
		//	f->return_type = TypeRef(new Float());
		//	f->sig = FunctionSignature("sin", vector<TypeRef>(1, TypeRef(new Float())));
		//	vm_args.external_functions.push_back(f);
		//}

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

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

//		assert(vmstate.argument_stack.size() == 1);
		//delete vmstate.argument_stack[0];
		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Float)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		FloatValue* val = static_cast<FloatValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		TestResults res;
		res.stats = vm.getProgramStats();
		res.maindef = maindef;
		return res;
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static void testMainFloatArgInvalidProgram(const std::string& src)
{
	std::cout << "===================== Winter testMainFloatArgInvalidProgram() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;
		vm_args.real_is_double = false;

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		//if(maindef.isNull())
		//	throw BaseException("Failed to find function " + mainsig.toString());
		//if(maindef->returnType()->getType() != Type::FloatType)
		//	throw BaseException("main did not return float.");


		vm.getJittedFunction(mainsig);

		std::cerr << "Test failed: Expected compilation failure." << std::endl;
		assert(0);
		exit(1);
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		std::cout << "Expected exception occurred: " << e.what() << std::endl;
	}
}


static TestResults doTestMainFloatArg(const std::string& src, float argument, float target_return_val, bool check_constant_folded_to_literal, uint32 test_flags)
{
	std::cout << "===================== Winter testMainFloatArg() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		/*Obfuscator obfusctor(
			true, // collapse_whitespace
			true, // remove_comments
			true, // change tokens
			Obfuscator::Lang_Winter
		);
		
		const std::string obfuscated_src = obfusctor.obfuscateWinterSource(src);

		std::cout << "==================== original src: =====================" << std::endl;
		std::cout << src << std::endl;
		std::cout << "==================== obfuscated_src: =====================" << std::endl;
		std::cout << obfuscated_src << std::endl;
		std::cout << "==========================================================" << std::endl;*/

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.try_coerce_int_to_double_first = false;
		vm_args.real_is_double = false;

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testExternalFunc;
			f->interpreted_func = testExternalFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testExternalFunc", std::vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}
		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull())
			throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::FloatType)
			throw BaseException("main did not return float.");

		if(check_constant_folded_to_literal)
			if(maindef->body.isNull() || maindef->body->nodeType() != ASTNode::FloatLiteralType) // body may be null if it is a built-in function (e.g. elem())
				throw BaseException("main was not folded to a float literal.");

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

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new FloatValue(argument));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Float)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		FloatValue* val = static_cast<FloatValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		//delete retval;




		//============================= New: test with OpenCL ==============================
		const bool TEST_OPENCL = false;
		if(!(test_flags & INVALID_OPENCL) && TEST_OPENCL)
		{
#if WINTER_OPENCL_TESTS
			OpenCL* opencl = getGlobalOpenCL();

			const gpuDeviceInfo& gpu_device = opencl->getDeviceInfo()[0];

			cl_context context;
			cl_command_queue command_queue;
			opencl->deviceInit(
				gpu_device,
				/*enable_profiling=*/false, 
				context,
				command_queue
			);

			Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
			std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

			// OpenCL keeps complaining about 'main must return type int', so rename main to main_.
			//opencl_code = StringUtils::replaceAll(opencl_code, "main", "main_"); // NOTE: slightly dodgy string-based renaming.

			const std::string extended_source = opencl_code + "\n" + "__kernel void main_kernel(float x, __global float * const restrict output_buffer) { \n" + 
				"	output_buffer[0] = main_float_(x);		\n" + 
				" }";

			//std::cout << extended_source << std::endl;
			{
				std::ofstream f("opencl_source.c");
				f << extended_source;
			}

			OpenCLBuffer output_buffer(context, sizeof(float), CL_MEM_READ_WRITE);

			std::string options;
			//std::string options = "-save-temps";
			//options += " -fbin-llvmir";//TEMP

			// Compile and build program.
			std::string build_log;
			cl_program program = opencl->buildProgram(
				extended_source,
				context,
				gpu_device.opencl_device,
				options,
				build_log
			);

			conPrint("build_log: \n" + build_log);
			//opencl->dumpBuildLog(program, gpu_device.opencl_device); 

			// Create kernel

			const bool profile = false;
			OpenCLKernelRef kernel = new OpenCLKernel(program, "main_kernel", gpu_device.opencl_device, profile);

			kernel->setKernelArgFloat(0, argument);
			kernel->setKernelArgBuffer(1, output_buffer.getDevicePtr());

			// Launch the kernel
			//const size_t block_size = 1;
			const size_t global_work_size = 1;

			kernel->launchKernel(command_queue, global_work_size);

			SSE_ALIGN float host_output_buffer[1];

			// Read back result
			cl_int result = opencl->clEnqueueReadBuffer(
				command_queue,
				output_buffer.getDevicePtr(), // buffer
				CL_TRUE, // blocking read
				0, // offset
				sizeof(float), // size in bytes
				host_output_buffer, // host buffer pointer
				0, // num events in wait list
				NULL, // wait list
				NULL //&readback_event // event
			);
			if(result != CL_SUCCESS)
				throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

			// Free the context and command queue for this device.
			opencl->deviceFree(context, command_queue);

			const float opencl_result = host_output_buffer[0];

			if(!epsEqual(opencl_result, target_return_val))
			{
				std::cerr << "Test failed: OpenCL returned " << opencl_result << ", target was " << target_return_val << std::endl;
				assert(0);
				exit(1);
			}
#endif // #if WINTER_OPENCL_TESTS
		}

		TestResults res;
		res.stats = vm.getProgramStats();
		res.maindef = maindef;
		return res;
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static TestResults doTestMainDoubleArg(const std::string& src, double argument, double target_return_val, bool check_constant_folded_to_literal, uint32 test_flags)
{
	std::cout << "===================== Winter doTestMainDoubleArg() =====================" << std::endl;
	try
	{
		TestEnv test_env;
		test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.env = &test_env;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = true;
		vm_args.real_is_double = true;

		{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testExternalFunc;
			f->interpreted_func = testExternalFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testExternalFunc", std::vector<TypeRef>(1, TypeRef(new Float())));
			vm_args.external_functions.push_back(f);
		}
		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Double())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull())
			throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::DoubleType)
			throw BaseException("main did not return double.");

		//if(check_constant_folded_to_literal)
		//	if(maindef->body.isNull() || maindef->body->nodeType() != ASTNode::DoubleLiteralType) // body may be null if it is a built-in function (e.g. elem())
		//		throw BaseException("main was not folded to a float literal.");

		double(WINTER_JIT_CALLING_CONV*f)(double, void*) = (double(WINTER_JIT_CALLING_CONV*)(double, void*))vm.getJittedFunction(mainsig);



		// Call the JIT'd function
		const double jitted_result = f(argument, &test_env);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new DoubleValue(argument));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Double)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		DoubleValue* val = static_cast<DoubleValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		//delete retval;




		//============================= New: test with OpenCL ==============================
		const bool TEST_OPENCL = false;
		if(!(test_flags & INVALID_OPENCL) && TEST_OPENCL)
		{
#if WINTER_OPENCL_TESTS
			OpenCL* opencl = getGlobalOpenCL();

			const gpuDeviceInfo& gpu_device = opencl->getDeviceInfo()[0];

			cl_context context;
			cl_command_queue command_queue;
			opencl->deviceInit(
				gpu_device,
				/*enable_profiling=*/false, 
				context,
				command_queue
			);

			Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
			std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

			// OpenCL keeps complaining about 'main must return type int', so rename main to main_.
			//opencl_code = StringUtils::replaceAll(opencl_code, "main", "main_"); // NOTE: slightly dodgy string-based renaming.

			const std::string extended_source = opencl_code + "\n" + "__kernel void main_kernel(double x, __global double * const restrict output_buffer) { \n" + 
				"	output_buffer[0] = main_double_(x);		\n" + 
				" }";

			//std::cout << extended_source << std::endl;
			{
				std::ofstream f("opencl_source.c");
				f << extended_source;
			}

			OpenCLBuffer output_buffer(context, sizeof(double), CL_MEM_READ_WRITE);

			std::string options;
			//std::string options = "-save-temps";
			//options += " -fbin-llvmir";//TEMP

			// Compile and build program.
			std::string build_log;
			cl_program program = opencl->buildProgram(
				extended_source,
				context,
				gpu_device.opencl_device,
				options,
				build_log
			);


			//opencl->dumpBuildLog(program, gpu_device.opencl_device); 

			// Create kernel
			cl_int result;
			cl_kernel kernel = opencl->clCreateKernel(program, "main_kernel", &result);

			if(!kernel)
				throw Indigo::Exception("clCreateKernel failed");


			if(opencl->clSetKernelArg(kernel, 0, sizeof(cl_double), &argument) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 0");
			if(opencl->clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_buffer.getDevicePtr()) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 1");

			// Launch the kernel
			const size_t block_size = 1;
			const size_t global_work_size = 1;

			result = opencl->clEnqueueNDRangeKernel(
				command_queue,
				kernel,
				1,					// dimension
				NULL,				// global_work_offset
				&global_work_size,	// global_work_size
				&block_size,		// local_work_size
				0,					// num_events_in_wait_list
				NULL,				// event_wait_list
				NULL				// event
			);
			if(result != CL_SUCCESS)
				throw Indigo::Exception("clEnqueueNDRangeKernel failed: " + OpenCL::errorString(result));


			SSE_ALIGN double host_output_buffer[1];

			// Read back result
			result = opencl->clEnqueueReadBuffer(
				command_queue,
				output_buffer.getDevicePtr(), // buffer
				CL_TRUE, // blocking read
				0, // offset
				sizeof(double), // size in bytes
				host_output_buffer, // host buffer pointer
				0, // num events in wait list
				NULL, // wait list
				NULL //&readback_event // event
			);
			if(result != CL_SUCCESS)
				throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

			// Free the context and command queue for this device.
			opencl->deviceFree(context, command_queue);

			const double opencl_result = host_output_buffer[0];

			if(!epsEqual(opencl_result, target_return_val))
			{
				std::cerr << "Test failed: OpenCL returned " << opencl_result << ", target was " << target_return_val << std::endl;
				assert(0);
				exit(1);
			}
#endif // #if WINTER_OPENCL_TESTS
		}

		TestResults res;
		res.stats = vm.getProgramStats();
		res.maindef = maindef;
		return res;
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static TestResults testMainFloatArg(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0)
{
	return doTestMainFloatArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags
	);
}


static TestResults testMainDoubleArg(const std::string& src, double argument, double target_return_val, uint32 test_flags = 0)
{
	return doTestMainDoubleArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags
	);
}


static TestResults testMainFloatArgAllowUnsafe(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0)
{
	return doTestMainFloatArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags | ALLOW_UNSAFE
	);
}


static void testMainFloatArgCheckConstantFolded(const std::string& src, float argument, float target_return_val, uint32 test_flags = 0)
{
	doTestMainFloatArg(src, argument, target_return_val,
		true, // check constant-folded to literal
		test_flags
	);
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

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
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


static void testMainStringArg(const std::string& src, const std::string& arg, const std::string& target_return_val, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testMainStringArg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, new String()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		StringRep* (WINTER_JIT_CALLING_CONV *f)(const StringRep*, void*) = (StringRep* (WINTER_JIT_CALLING_CONV *)(const StringRep*, void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;


		StringRep* arg_string_rep = (StringRep*)malloc(sizeof(StringRep) + arg.size());
		arg_string_rep->refcount = 1;
		arg_string_rep->len = arg.size();
		arg_string_rep->flags = 1; // heap allocated
		if(!arg.empty())
			std::memcpy((uint8*)arg_string_rep + sizeof(StringRep), &arg[0], arg.size()); // Copy data

		debugIncrStringCount();

		// Call the JIT'd function
		StringRep* jitted_result = f(arg_string_rep, &test_env);

		

		if(jitted_result->len != target_return_val.size())
		{
			std::cerr << "Test failed: JIT'd main returned string with length " << jitted_result->len << ", target was " << target_return_val.size() << std::endl;
			assert(0);
			exit(1);
		}

		std::string result_str;
		result_str.resize(jitted_result->len);
		std::memcpy(&result_str[0], (uint8*)jitted_result + sizeof(StringRep), jitted_result->len);


		// Check JIT'd result.
		if(result_str != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << result_str << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		arg_string_rep->refcount--;
		if(arg_string_rep->refcount == 0)
		{
			debugDecrStringCount();
			free(arg_string_rep);
		}

		jitted_result->refcount--;
		if(jitted_result->refcount == 0)
		{
			debugDecrStringCount();
			free(jitted_result);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new StringValue(arg));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_String)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		StringValue* val = static_cast<StringValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static TestResults testMainIntegerArg(const std::string& src, int x, int target_return_val, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testMainIntegerArg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
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

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		//============================= New: test with OpenCL ==============================
		const bool TEST_OPENCL = false;
		if(!(test_flags & INVALID_OPENCL) && TEST_OPENCL)
		{
#if WINTER_OPENCL_TESTS
			OpenCL* opencl = getGlobalOpenCL();

			cl_context context;
			cl_command_queue command_queue;
			opencl->deviceInit(
				opencl->getDeviceInfo()[0],
				/*enable_profiling=*/false, 
				context,
				command_queue
			);

			/*const std::string opencl_lib_code = 
				"float toFloat(int x) { return (float)x; } \n\
				int truncateToInt(float x) { return (int)x; } \n\
				";*/

			Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
			std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

			// OpenCL keeps complaining about 'main must return type int', so rename main to main_.
			//opencl_code = StringUtils::replaceAll(opencl_code, "main", "main_"); // NOTE: dodgy string-based renaming.

			const std::string extended_source = /*opencl_lib_code + "\n" +*/ opencl_code + "\n" + "__kernel void main_kernel(int x, __global int * const restrict output_buffer) { \n" + 
				"	output_buffer[0] = main_int_(x);		\n" + 
				" }";

			std::cout << extended_source << std::endl;
			{
				std::ofstream f("opencl_source.c");
				f << extended_source;
			}

			OpenCLBuffer output_buffer(context, sizeof(float), CL_MEM_READ_WRITE);

			std::string options = "";

			// Compile and build program.
			std::string build_log;
			cl_program program = opencl->buildProgram(
				extended_source,
				context,
				opencl->getDeviceInfo()[0].opencl_device,
				options,
				build_log
			);


			//opencl->dumpBuildLog(program, opencl->getDeviceInfo()[0].opencl_device, print_output); 

			// Create kernel
			cl_int result;
			cl_kernel kernel = opencl->clCreateKernel(program, "main_kernel", &result);

			if(!kernel)
				throw Indigo::Exception("clCreateKernel failed");


			if(opencl->clSetKernelArg(kernel, 0, sizeof(cl_int), &x) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 0");
			if(opencl->clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_buffer.getDevicePtr()) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 1");

			// Launch the kernel
			const size_t block_size = 1;
			const size_t global_work_size = 1;

			result = opencl->clEnqueueNDRangeKernel(
				command_queue,
				kernel,
				1,					// dimension
				NULL,				// global_work_offset
				&global_work_size,	// global_work_size
				&block_size,		// local_work_size
				0,					// num_events_in_wait_list
				NULL,				// event_wait_list
				NULL				// event
			);
			if(result != CL_SUCCESS)
				throw Indigo::Exception("clEnqueueNDRangeKernel failed: " + OpenCL::errorString(result));


			SSE_ALIGN int host_output_buffer[1];

			// Read back result
			result = opencl->clEnqueueReadBuffer(
				command_queue,
				output_buffer.getDevicePtr(), // buffer
				CL_TRUE, // blocking read
				0, // offset
				sizeof(int), // size in bytes
				host_output_buffer, // host buffer pointer
				0, // num events in wait list
				NULL, // wait list
				NULL //&readback_event // event
			);
			if(result != CL_SUCCESS)
				throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

			// Free the context and command queue for this device.
			opencl->deviceFree(context, command_queue);

			const int opencl_result = host_output_buffer[0];

			if(opencl_result != target_return_val)
			{
				std::cerr << "Test failed: OpenCL returned " << opencl_result << ", target was " << target_return_val << std::endl;
				assert(0);
				exit(1);
			}
#endif // #if WINTER_OPENCL_TESTS
		}

		TestResults res;
		res.stats = vm.getProgramStats();
		res.maindef = maindef;
		return res;
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}



static void testMainInt64Arg(const std::string& src, int64 x, int64 target_return_val, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testMainInt64Arg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, new Int(64)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int64 (WINTER_JIT_CALLING_CONV *f)(int64, void*) = (int64 (WINTER_JIT_CALLING_CONV *)(int64, void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;

		// Call the JIT'd function
		const int64 jitted_result = f(x, &test_env);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static void testMainInt16Arg(const std::string& src, int16 x, int16 target_return_val, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testMainInt16Arg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, new Int(16)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int16 (WINTER_JIT_CALLING_CONV *f)(int16, void*) = (int16 (WINTER_JIT_CALLING_CONV *)(int16, void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;

		// Call the JIT'd function
		const int16 jitted_result = f(x, &test_env);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


static void testMainUInt32Arg(const std::string& src, uint32 x, uint32 target_return_val, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testMainUInt32Arg() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, new Int(32, false)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		uint32 (WINTER_JIT_CALLING_CONV *f)(uint32, void*) = (uint32 (WINTER_JIT_CALLING_CONV *)(uint32, void*)) vm.getJittedFunction(mainsig);

		TestEnv test_env;
		test_env.val = 10;

		// Call the JIT'd function
		const uint32 jitted_result = f(x, &test_env);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
		{
			std::cerr << "Test failed: JIT'd main returned " << jitted_result << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, false));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
		{
			std::cerr << "Test failed: main returned " << val->value << ", target was " << target_return_val << std::endl;
			assert(0);
			exit(1);
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
	catch(Indigo::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}



static void testMainIntegerArgInvalidProgram(const std::string& src)
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

		vm.getJittedFunction(mainsig);

		std::cerr << "Test failed: Expected compilation failure." << std::endl;
		assert(0);
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


#if defined(_WIN32) || defined(_WIN64)
#define ALIGN_32 _CRT_ALIGN(32)
#define CLASS_ALIGN_32 _CRT_ALIGN(32) class
#else
#define ALIGN_32 __attribute__ ((aligned (32)))
#define CLASS_ALIGN_32 class __attribute__ ((aligned (32)))
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
		if(!epsEqual(jitted_result, target_return_val))
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
		assert(0);
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

		std::vector<std::string> field_names;
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
		if(!epsEqual(jitted_result, target_return_val))
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
		assert(0);
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

inline bool epsEqual(const float4& a, const float4& b)
{
	for(int i=0; i<4; ++i)
		if(!epsEqual(a.e[i], b.e[i]))
			return false;
	return true;
}


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


inline bool epsEqual(const StructWithVec& a, const StructWithVec& b)
{
	return epsEqual(a.a, b.a) && epsEqual(a.b, b.b) && epsEqual(a.data2, b.data2);
}


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


inline bool epsEqual(const Float4Struct& a, const Float4Struct& b)
{
	for(int i=0; i<4; ++i)
		if(!epsEqual(a.v.e[i], b.v.e[i]))
			return false;
	return true;
}


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


CLASS_ALIGN_32 float8
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


CLASS_ALIGN_32 Float8Struct
{
public:
	float8 v;

	inline bool operator == (const Float8Struct& other)
	{
		return v == other.v;
	}
};


inline bool epsEqual(const Float8Struct& a, const Float8Struct& b)
{
	for(int i=0; i<8; ++i)
		if(!epsEqual(a.v.e[i], b.v.e[i]))
			return false;
	return true;
}


static void testFloat4StructPairRetFloat(const std::string& src, const Float4StructPair& a, const Float4StructPair& b, float target_return_val)
{
	std::cout << "===================== Winter testFloat4StructPairRetFloat() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.real_is_double = false;


		// Create Float4Struct type
		TypeRef float_4_struct_type;
		{
			std::vector<std::string> field_names;
			field_names.push_back("v");

			std::vector<TypeRef> field_types;
			field_types.push_back(TypeRef(new VectorType(TypeRef(new Float), 4)));

			float_4_struct_type = new StructureType(
				"Float4Struct", 
				field_types, 
				field_names
			);
		}

		// Create Float4StructPair type
		std::vector<std::string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");

		std::vector<TypeRef> field_types(2, float_4_struct_type);

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
		if(!epsEqual(jitted_result, target_return_val))
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
		assert(0);
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

		std::vector<std::string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");
		field_names.push_back("data2");

		std::vector<TypeRef> field_types;
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
		if(!epsEqual(jitted_result, target_return_val))
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
		assert(0);
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
		vm_args.floating_point_literals_default_to_double = false;

		std::vector<std::string> field_names;
		field_names.push_back("v");

		std::vector<TypeRef> field_types;
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
		if(!epsEqual(jitted_result, target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
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


static void testFloat8Struct(const std::string& src, const Float8Struct& a, const Float8Struct& b, const Float8Struct& target_return_val)
{
	std::cout << "===================== Winter testFloat8Struct() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		std::vector<std::string> field_names;
		field_names.push_back("v");

		std::vector<TypeRef> field_types;
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
		if(!epsEqual(jitted_result, target_return_val))
		{
			std::cerr << "Test failed: jitted_result != target_return_val  " << std::endl;
			assert(0);
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


static void testIntArray(const std::string& src, const int* a, const int* b, const int* target_return_val, size_t len, uint32 test_flags = 0)
{
	std::cout << "===================== Winter testIntArray() =====================" << std::endl;
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, new ArrayType(new Int(), len)) // 2 float arrays of len elems each
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(int*, const int*, const int*, void*) = 
			(void (WINTER_JIT_CALLING_CONV *)(int*, const int*, const int*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		js::Vector<int, 32> jitted_result(len);
		int* jitted_result_ptr = &jitted_result[0];

		TestEnv test_env;
		test_env.val = 10;

		f(jitted_result_ptr, a, b, &test_env);

		// Check JIT'd result.
		for(size_t i=0; i<len; ++i)
		{
			if(jitted_result[i] != target_return_val[i])
			{
				std::cerr << "Test failed: jitted_result[i] != target_return_val[i]  " << std::endl;
				std::cerr << "i: " << i << std::endl;
				std::cerr << "jitted_result[i]: " << jitted_result[i] << std::endl;
				std::cerr << "target_return_val[i]: " << target_return_val[i] << std::endl;
				assert(0);
				exit(1);
			}
		}
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		assert(0);
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
		vm_args.floating_point_literals_default_to_double = false;

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeRef>(2, new ArrayType(new Float(), len)) // 2 float arrays of 4 elems each
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(float*, const float*, const float*, void*) = 
			(void (WINTER_JIT_CALLING_CONV *)(float*, const float*, const float*, void*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		js::Vector<float, 32> jitted_result(len);
		// Clear mem 
		for(size_t i=0; i<len; ++i)
			jitted_result[i] = 0.0f;

		float* jitted_result_ptr = &jitted_result[0];

		TestEnv test_env;
		test_env.val = 10;

		Timer timer;
		f(jitted_result_ptr, a, b, &test_env);
		const double elapsed = timer.elapsed();
		std::cout << "JITed code elapsed: " << elapsed << " s" << std::endl;
		const double bandwidth = len * sizeof(float) / elapsed;
		std::cout << "JITed bandwidth: " << (bandwidth * 1.0e-9) << " GiB/s" << std::endl;

		// Check JIT'd result.
		for(size_t i=0; i<len; ++i)
		{
			if(!epsEqual(jitted_result[i], target_return_val[i]))
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
		assert(0);
		exit(1);
	}
}


//float test()
//{
//	return 10;
//}
//
////int test2()
////{
////	return 3.0f;
////}
//
//SSE_CLASS_ALIGN test_vec4
//{
//	float x[4];
//};
//
//SSE_CLASS_ALIGN test_vec16
//{
//	float x[16];
//};
//
//SSE_CLASS_ALIGN large_struct
//{
//	test_vec4 a;
//	test_vec16 b;
//};


//float someFuncBleh(float x)
//{
//	return x + 1; 
//}


} // end namespace Winter
