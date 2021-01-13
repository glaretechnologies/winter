#include "LanguageTestUtils.h"


#if BUILD_TESTS


#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_ASTNode.h"
#include "wnt_MathsFuncs.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "CompiledValue.h"
#include <TestUtils.h>
#include <StandardPrintOutput.h>
#include <Mutex.h>
#include <Lock.h>
#include <Exception.h>
#include <Vector.h>
#include <ConPrint.h>
#include <Platform.h>
#include <FileUtils.h>
#include <StringUtils.h>
#include <FileUtils.h>
#include <ContainerUtils.h>
#include <PlatformUtils.h>
#include <Timer.h>
#include <cassert>
#include <fstream>
#ifdef _WIN32
#include <intrin.h>
#endif
//#include "utils/Obfuscator.h"

#define WINTER_OPENCL_TESTS 1
static const bool DUMP_OPENCL_C_SOURCE = false; // Dumps to opencl_source.c

static const bool PRINT_MEM_BOUNDS_AND_USAGE = false;


// Checking stack size usage doesn't seem to be working properly on macOS yet.
#if defined(OSX)
static const bool CHECK_STACK_USAGE = false;
#else
static const bool CHECK_STACK_USAGE = true;
#endif


// OpenCL:
#if WINTER_OPENCL_TESTS
#include <opencl/OpenCL.h>
#include <opencl/OpenCLBuffer.h>
#include <opencl/OpenCLKernel.h>
#include <opencl/OpenCLContext.h>
#include <opencl/OpenCLProgram.h>
#include <opencl/OpenCLCommandQueue.h>
#endif


namespace Winter
{


/*
Example of stack marked zone:
	
		stack info for current func
sp		---------------
		0x12
		0x45
		0x3a
		0x42
sp - 4	--------------------	
		0xab
		0xa6
sp - 6
		0xEE
		0xEE
sp - 8-------------------
		0xEE
		0xEE
		0xEE
		0xEE	
	
In this case, sp - 6 contains the lowest byte that is not 0xEE.
	
	*/


// Stack marking stuff is causing a crash on macOS currently, also in VS2019.  So just disable for now.
#if 1

#define MARK_STACK
#define GET_TOUCHED_STACK_SIZE(touched_stack_size_out) touched_stack_size_out = 0;

#else

static const int MARKED_ZONE_SIZE = 1000;

// Put some special byte patterns on the stack
// Store 0xEE for MARKED_ZONE_SIZE bytes below the current stack
#define MARK_STACK										\
uint8* _sp = (uint8*)getStackPointer();					\
for(uint8* p = _sp - MARKED_ZONE_SIZE; p < _sp; ++p)	\
	*p = 0xEE;


// Try and detect how much stack we actually used.
// We will do this by finding the lowest byte in the marked area that is not 0xEE.
#define GET_TOUCHED_STACK_SIZE(touched_stack_size_out)	\
touched_stack_size_out = 0;								\
for(uint8* p = _sp - MARKED_ZONE_SIZE; p < _sp; ++p)	\
{														\
	if(*p != 0xEE)										\
	{													\
		touched_stack_size_out = _sp - p;				\
		break;											\
	}													\
}


// Returns the stack pointer (value in RSP register) of the calling function.
static GLARE_NO_INLINE void* getStackPointer()
{
	// The return address of this function is stored immediately below where RSP for the calling function points to.
#ifdef _WIN32
	return (void*)((char*)_AddressOfReturnAddress() + sizeof(void*));
#else
	return (void*)((char*)__builtin_frame_address(0) + sizeof(void*));
#endif
}

#endif // !defined(OSX)


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


static void testPrint(const std::string& s)
{
	// Actually printing out stuff makes the tests dramatically slower to run (like 1.7s -> 6s)
	// conPrint(s);
}


typedef float(WINTER_JIT_CALLING_CONV * float_void_func)();


TestResults testMainFloat(const std::string& src, float target_return_val)
{
	testPrint("===================== Winter testMainFloat() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.real_is_double = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachineRef vm = new VirtualMachine(vm_args);

		// Get main function
		
		Reference<FunctionDefinition> maindef = vm->findMatchingFunction(mainsig);

		void* f = vm->getJittedFunction(mainsig);

		//// cast to correct type
		float_void_func mainf = (float_void_func)f;


		//// Call the JIT'd function
		const float jitted_result = mainf();


		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
		{
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));
		}

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		ValueRef retval = maindef->invoke(vmstate);

//		assert(vmstate.argument_stack.size() == 1);
		//delete vmstate.argument_stack[0];
		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Float)
			failTest("main() Return value was of unexpected type.");

		FloatValue* val = static_cast<FloatValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

		TestResults res;
		res.stats = vm->getProgramStats();
		res.maindef = maindef;
		return res;
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
		exit(1);
	}
}


void testMainFloatArgInvalidProgram(const std::string& src)
{
	testPrint("===================== Winter testMainFloatArgInvalidProgram() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.real_is_double = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		//if(maindef.isNull())
		//	throw BaseException("Failed to find function " + mainsig.toString());
		//if(maindef->returnType()->getType() != Type::FloatType)
		//	throw BaseException("main did not return float.");


		vm.getJittedFunction(mainsig);

		failTest("Test failed: Expected compilation failure.");
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		testPrint("Expected exception occurred: " + e.messageWithPosition());
	}
}


static TestResults doTestMainFloatArg(const std::string& src, float argument, float target_return_val, 
	bool check_constant_folded_to_literal, uint32 test_flags, const std::vector<ExternalFunctionRef>* external_funcs)
{
	testPrint("===================== Winter testMainFloatArg() =====================");
	try
	{
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
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.try_coerce_int_to_double_first = false;
		vm_args.real_is_double = false;
		vm_args.opencl_double_support = false; // This will allow this test to run on an OpenCL device without double fp support.
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		if(test_flags & INCLUDE_EXTERNAL_MATHS_FUNCS)
			MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		if(external_funcs)
			ContainerUtils::append(vm_args.external_functions, *external_funcs);

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));

		vm_args.entry_point_sigs.push_back(mainsig);

		const FunctionSignature entryPoint2sig("entryPoint2", std::vector<TypeVRef>(1, new Float()));
		vm_args.entry_point_sigs.push_back(entryPoint2sig);

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

		float(WINTER_JIT_CALLING_CONV*f)(float) = (float(WINTER_JIT_CALLING_CONV*)(float))vm.getJittedFunction(mainsig);

		resetMemUsageStats();

		// Put some special byte patterns on the stack
		MARK_STACK;

		//================= Call the JIT'd function ====================
		const float jitted_result = f(argument);

		// Try and detect how much stack we actually used.
		size_t touched_stack_size;
		GET_TOUCHED_STACK_SIZE(touched_stack_size);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		testAssert(getCurrentHeapMemUsage() == 0);

		try
		{
			GetSpaceBoundParams params;
			GetSpaceBoundResults space_bounds = maindef->getSpaceBound(params);

			// Compute space bounds for arg
			const size_t arg_heap_size = 0;
			const size_t total_allowed_heap = space_bounds.heap_space + arg_heap_size;

			if(PRINT_MEM_BOUNDS_AND_USAGE)
			{
				conPrint("stack space bound: " + toString(space_bounds.stack_space));
				conPrint("stack used:        " + toString(touched_stack_size));
				conPrint("heap space bound:  " + toString(total_allowed_heap));
				conPrint("heap used:         " + toString(getMaxHeapMemUsage()));
				conPrint("");
			}
			
			testAssert(getMaxHeapMemUsage() <= total_allowed_heap);
			if(CHECK_STACK_USAGE) testAssert(touched_stack_size <= space_bounds.stack_space);
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get space bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_SPACE_BOUND_FAILURE) == 0)
				failTest("Failed to get space bound: " + e.messageWithPosition());
		}

		try
		{
			GetTimeBoundParams params;
			const size_t time_bound = maindef->getTimeBound(params);
			if(time_bound > (1 << 22)) // Just do something with the time bound so it doesn't get compiled away.
				failTest("Time bound too large.");
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get time bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_TIME_BOUND_FAILURE) == 0)
				failTest("Failed to get time bound: " + e.messageWithPosition());
		}



		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new FloatValue(argument));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Float)
			failTest("main() Return value was of unexpected type.");
		FloatValue* val = static_cast<FloatValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

		//delete retval;




		//============================= New: test with OpenCL ==============================
		if(!(test_flags & INVALID_OPENCL))
		{
#if WINTER_OPENCL_TESTS
			if(::getGlobalOpenCL() && !::getGlobalOpenCL()->getOpenCLDevices().empty())
			{
				Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
				std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

				std::string extended_source = opencl_code + "\n" + "__kernel void main_kernel(float x, __global float * const restrict output_buffer) { \n" +
					"	output_buffer[0] = main_float_(x);		\n" +
					" }";

				if(false)
				{
					conPrint("!!!   USING SRC FROM test_opencl_source.c   !!!");
					extended_source = FileUtils::readEntireFileTextMode("test_opencl_source.c");
				}
				else
				{
					if(DUMP_OPENCL_C_SOURCE)
					{
						std::ofstream file("opencl_source.c");
						file << extended_source;
						conPrint("Dumped OpenCL C source to " + PlatformUtils::getCurrentWorkingDirPath() + "/opencl_source.c.");
					}
				}

				const OpenCLDeviceRef device = ::getGlobalOpenCL()->getOpenCLDevices()[0];
				std::vector<OpenCLDeviceRef> devices(1, device);
				OpenCLContextRef context = new OpenCLContext(device->opencl_platform_id);
				OpenCLCommandQueueRef command_queue = new OpenCLCommandQueue(context, device->opencl_device_id, /*profiling=*/false);
				std::string build_log;
				OpenCLProgramRef program;
				try
				{
					program = ::getGlobalOpenCL()->buildProgram(
						extended_source,
						context,
						devices,
						"", //"-cl-nv-verbose", // options
						build_log
					);
				}
				catch(Indigo::Exception& e)
				{
					failTest("Build failed: " + e.what() + "\nOpenCL device: " + device->description() + "\nbuild_log:\n" + build_log);
				}

				
				if(DUMP_OPENCL_C_SOURCE)
				{
					// Print build log:
					const std::string log = ::getGlobalOpenCL()->getBuildLog(program->getProgram(), device->opencl_device_id);
					conPrint("build_log: \n" + log);

					// Dump program binary:
					std::vector<OpenCLProgram::Binary> binaries;
					program->getProgramBinaries(binaries);

					for(size_t i=0; i<binaries.size(); ++i)
					{
						const std::string path = "binary_" + toString(i) + ".txt";
						FileUtils::writeEntireFile(path, (const char*)binaries[i].data.data(), binaries[i].data.size());
						conPrint("Wrote program binary to '" + path + "'.");
					}
				}

				OpenCLKernelRef kernel = new OpenCLKernel(program, "main_kernel", device->opencl_device_id, /*profile=*/false);

				OpenCLBuffer output_buffer(context, sizeof(float), CL_MEM_READ_WRITE);

				kernel->setKernelArgFloat(0, argument);
				kernel->setKernelArgBuffer(1, output_buffer.getDevicePtr());

				// Launch the kernel
				const size_t global_work_size = 1;
				kernel->launchKernel(command_queue->getCommandQueue(), global_work_size);

				SSE_ALIGN float host_output_buffer[1];

				// Read back result
				cl_int result = ::getGlobalOpenCL()->clEnqueueReadBuffer(
					command_queue->getCommandQueue(),
					output_buffer.getDevicePtr(), // buffer
					CL_TRUE, // blocking read
					0, // offset
					sizeof(float), // size in bytes
					host_output_buffer, // host buffer pointer
					0, // num events in wait list
					NULL, // wait list
					NULL // event
				);
				if(result != CL_SUCCESS)
					throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

				const float opencl_result = host_output_buffer[0];

				if(!epsEqual(opencl_result, target_return_val))
					failTest("Test failed: OpenCL returned " + toString(opencl_result) + ", target was " + toString(target_return_val));
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
		failTest(e.messageWithPosition());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	exit(1);
}


size_t getTimeBoundForMainFloatArg(const std::string& src, uint32 test_flags)
{
	testPrint("===================== Winter getTimeBoundForMainFloatArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.try_coerce_int_to_double_first = false;
		vm_args.real_is_double = false;
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;
		if(test_flags & INCLUDE_EXTERNAL_MATHS_FUNCS)
			MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));
		vm_args.entry_point_sigs.push_back(mainsig);

		Timer timer;
		VirtualMachine vm(vm_args);

		conPrint("Virtual machine build time: " + timer.elapsedString());

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull()) throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::FloatType) throw BaseException("main did not return float.");

		timer.reset();
		GetTimeBoundParams params;
		const size_t bound = maindef->getTimeBound(params);
		conPrint("Computing time bound took " + timer.elapsedString());
		return bound;
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
		exit(1);
	}
}


void testTimeBoundInvalidForMainFloatArg(const std::string& src, uint32 test_flags)
{
	testPrint("===================== Winter testTimeBoundInvalidForMainFloatArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.try_coerce_int_to_double_first = false;
		vm_args.real_is_double = false;
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		if(test_flags & INCLUDE_EXTERNAL_MATHS_FUNCS)
			MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));
		vm_args.entry_point_sigs.push_back(mainsig);

		Timer timer;
		VirtualMachine vm(vm_args);

		conPrint("Virtual machine build time: " + timer.elapsedString());

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull()) throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::FloatType) throw BaseException("main did not return float.");

		timer.reset();
		try
		{
			GetTimeBoundParams params;
			maindef->getTimeBound(params);

			failTest("Error: expected getTimeBound() to throw an exception.");
		}
		catch(Winter::BaseException& e)
		{
			conPrint("Caught expected exception from getTimeBound(): " + e.messageWithPosition());
			conPrint("Computing time bound took " + timer.elapsedString());
		}
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
}


static TestResults doTestMainDoubleArg(const std::string& src, double argument, double target_return_val, bool check_constant_folded_to_literal, uint32 test_flags)
{
	testPrint("===================== Winter doTestMainDoubleArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.floating_point_literals_default_to_double = true;
		vm_args.real_is_double = true;
		vm_args.opencl_double_support = true;
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		if(test_flags & INCLUDE_EXTERNAL_MATHS_FUNCS)
			MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);


		/*{
			ExternalFunctionRef f(new ExternalFunction());
			f->func = (void*)testExternalFunc;
			f->interpreted_func = testExternalFuncInterpreted;
			f->return_type = TypeRef(new Float());
			f->sig = FunctionSignature("testExternalFunc", std::vector<TypeVRef>(1, new Float()));
			vm_args.external_functions.push_back(f);
		}*/
		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Double()));

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

		double(WINTER_JIT_CALLING_CONV*f)(double) = (double(WINTER_JIT_CALLING_CONV*)(double))vm.getJittedFunction(mainsig);



		// Call the JIT'd function
		const double jitted_result = f(argument);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new DoubleValue(argument));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Double)
			failTest("main() Return value was of unexpected type.");

		DoubleValue* val = static_cast<DoubleValue*>(retval.getPointer());

		if(!epsEqual(val->value, target_return_val))
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

		//delete retval;




		//============================= New: test with OpenCL ==============================
		if(!(test_flags & INVALID_OPENCL))
		{
#if WINTER_OPENCL_TESTS
			if(::getGlobalOpenCL() && !::getGlobalOpenCL()->getOpenCLDevices().empty())
			{
				// Look through the OpenCL devices for a device that supports doubles.
				int device_index = -1;
				for(size_t z=0; z<::getGlobalOpenCL()->getOpenCLDevices().size(); ++z)
					if(::getGlobalOpenCL()->getOpenCLDevices()[z]->supports_doubles)
					{
						device_index = (int)z;
						break;
					}
				
				if(device_index == -1)
				{
					conPrint("Skipping OpenCL tests in doTestMainDoubleArg() as could not find OpenCL device with double floating-point support.");
				}
				else
				{
					Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
					std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

					const std::string extended_source = opencl_code + "\n" + "__kernel void main_kernel(double x, __global double * const restrict output_buffer) { \n" +
						"	output_buffer[0] = main_double_(x);		\n" +
						" }";

					if(DUMP_OPENCL_C_SOURCE)
					{
						std::ofstream file("opencl_source.c");
						file << extended_source;
						conPrint("Dumped OpenCL C source to opencl_source.c.");
					}

					const OpenCLDeviceRef& device = ::getGlobalOpenCL()->getOpenCLDevices()[device_index];
					std::vector<OpenCLDeviceRef> devices(1, device);
					OpenCLContextRef context = new OpenCLContext(device->opencl_platform_id);
					OpenCLCommandQueueRef command_queue = new OpenCLCommandQueue(context, device->opencl_device_id, /*profile=*/false);
					std::string build_log;
					OpenCLProgramRef program;
					try
					{
						program = ::getGlobalOpenCL()->buildProgram(
							extended_source,
							context,
							devices,
							"", // options
							build_log
						);
					}
					catch(Indigo::Exception& e)
					{
						failTest("Build failed: " + e.what() + "\nOpenCL device: " + device->description() + "\nbuild_log:\n" + build_log + "\nextended_source: \n" + extended_source);
					}
					//conPrint("build_log: \n" + build_log);

					OpenCLKernelRef kernel = new OpenCLKernel(program, "main_kernel", device->opencl_device_id, /*profile=*/false);


					OpenCLBuffer output_buffer(context, sizeof(double), CL_MEM_READ_WRITE);

					kernel->setKernelArgDouble(0, argument);
					kernel->setKernelArgBuffer(1, output_buffer.getDevicePtr());

					const size_t global_work_size = 1;
					kernel->launchKernel(command_queue->getCommandQueue(), global_work_size);



					SSE_ALIGN double host_output_buffer[1];

					// Read back result
					cl_int result = ::getGlobalOpenCL()->clEnqueueReadBuffer(
						command_queue->getCommandQueue(),
						output_buffer.getDevicePtr(), // buffer
						CL_TRUE, // blocking read
						0, // offset
						sizeof(double), // size in bytes
						host_output_buffer, // host buffer pointer
						0, // num events in wait list
						NULL, // wait list
						NULL // event
					);
					if(result != CL_SUCCESS)
						throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

					const double opencl_result = host_output_buffer[0];

					if(!epsEqual(opencl_result, target_return_val))
					{
						failTest("Test failed: OpenCL returned " + toString(opencl_result) + ", target was " + toString(target_return_val));
					}
				}
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
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	exit(1);
}


TestResults testMainFloatArg(const std::string& src, float argument, float target_return_val, uint32 test_flags, const std::vector<ExternalFunctionRef>* external_funcs)
{
	return doTestMainFloatArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags,
		external_funcs
	);
}


TestResults testMainDoubleArg(const std::string& src, double argument, double target_return_val, uint32 test_flags)
{
	return doTestMainDoubleArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags
	);
}


TestResults testMainFloatArgAllowUnsafe(const std::string& src, float argument, float target_return_val, uint32 test_flags)
{
	return doTestMainFloatArg(src, argument, target_return_val,
		false, // check constant-folded to literal
		test_flags | ALLOW_UNSAFE,
		NULL // external funcs
	);
}


TestResults testMainFloatArgCheckConstantFolded(const std::string& src, float argument, float target_return_val, uint32 test_flags, 
	const std::vector<ExternalFunctionRef>* external_funcs)
{
	return doTestMainFloatArg(src, argument, target_return_val,
		true, // check constant-folded to literal
		test_flags,
		external_funcs
	);
}

void testMainInteger(const std::string& src, int target_return_val)
{
	testPrint("===================== Winter testMainInteger() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeVRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int (WINTER_JIT_CALLING_CONV *f)() = (int (WINTER_JIT_CALLING_CONV *)()) vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const int jitted_result = f();


		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
			failTest("main() Return value was of unexpected type.");
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

		//delete retval;

	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
}


void testMainStringArg(const std::string& src, const std::string& arg, const std::string& target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainStringArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new String()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		StringRep* (WINTER_JIT_CALLING_CONV *f)(const StringRep*) = (StringRep* (WINTER_JIT_CALLING_CONV *)(const StringRep*)) vm.getJittedFunction(mainsig);

		testAssert(getCurrentHeapMemUsage() == 0);
		resetMemUsageStats();
		size_t touched_stack_size = 0;

		{ // Scope for CompiledValRefs
			CompiledValRef<StringRep> arg_string_ref = allocateString(arg.c_str());

			// Call the JIT'd function
			
			MARK_STACK; // Put some special byte patterns on the stack

			CompiledValRef<StringRep> jitted_result = f(arg_string_ref.getPointer());

			// Try and detect how much stack we actually used.
			GET_TOUCHED_STACK_SIZE(touched_stack_size);

			jitted_result->refcount--; // The jitted func will return a result with refcount 1

			// Check JIT'd result.
			if(jitted_result->toStdString() != target_return_val)
				failTest("Test failed: JIT'd main returned " + jitted_result->toStdString() + ", target was " + target_return_val);

			testAssert(arg_string_ref->getRefCount() == 1);
			testAssert(jitted_result->getRefCount() == 1);
		}

		

		testAssert(getCurrentHeapMemUsage() == 0);

		try
		{
			GetSpaceBoundParams params;
			GetSpaceBoundResults space_bounds = maindef->getSpaceBound(params);

			// Compute space bounds for arg
			const size_t arg_heap_size = sizeof(StringRep) + arg.size();

			const size_t total_allowed_heap = space_bounds.heap_space + arg_heap_size;

			if(PRINT_MEM_BOUNDS_AND_USAGE)
			{
				conPrint("stack space bound: " + toString(space_bounds.stack_space));
				conPrint("stack used:        " + toString(touched_stack_size));
				conPrint("heap space bound:  " + toString(total_allowed_heap));
				conPrint("heap used:         " + toString(getMaxHeapMemUsage()));
				conPrint("");
			}
			
			testAssert(getMaxHeapMemUsage() <= total_allowed_heap);
			if(CHECK_STACK_USAGE) testAssert(touched_stack_size <= space_bounds.stack_space);
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get space bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_SPACE_BOUND_FAILURE) == 0)
				failTest("Failed to get space bound: " + e.messageWithPosition());
		}

		try
		{
			GetTimeBoundParams params;
			const size_t time_bound = maindef->getTimeBound(params);
			if(time_bound > (1 << 22)) // Just do something with the time bound so it doesn't get compiled away.
				failTest("Time bound too large.");
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get time bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_TIME_BOUND_FAILURE) == 0)
				failTest("Failed to get time bound: " + e.messageWithPosition());
		}


		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new StringValue(arg));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_String)
			failTest("main() Return value was of unexpected type.");

		StringValue* val = static_cast<StringValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + val->value + ", target was " + target_return_val);
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());

	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


TestResults testMainIntegerArg(const std::string& src, int x, int target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainIntegerArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.opencl_double_support = false; // This will allow this test to run on an OpenCL device without double fp support.
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		if(test_flags & INCLUDE_EXTERNAL_MATHS_FUNCS)
			MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);


		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int (WINTER_JIT_CALLING_CONV *f)(int) = (int (WINTER_JIT_CALLING_CONV *)(int)) vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const int jitted_result = f(x);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
			failTest("main() Return value was of unexpected type.");

		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

		//============================= New: test with OpenCL ==============================
		if(!(test_flags & INVALID_OPENCL))
		{
#if WINTER_OPENCL_TESTS
			if(::getGlobalOpenCL() && !::getGlobalOpenCL()->getOpenCLDevices().empty())
			{
				Winter::VirtualMachine::BuildOpenCLCodeArgs opencl_args;
				std::string opencl_code = vm.buildOpenCLCodeCombined(opencl_args);

				const std::string extended_source = opencl_code + "\n" + "__kernel void main_kernel(int x, __global int * const restrict output_buffer) { \n" +
					"	output_buffer[0] = main_int_(x);		\n" +
					" }";

				if(DUMP_OPENCL_C_SOURCE)
				{
					std::ofstream file("opencl_source.c");
					file << extended_source;
					conPrint("Dumped OpenCL C source to opencl_source.c.");
				}

				const OpenCLDeviceRef& device = ::getGlobalOpenCL()->getOpenCLDevices()[0];
				std::vector<OpenCLDeviceRef> devices(1, device);
				OpenCLContextRef context = new OpenCLContext(device->opencl_platform_id);
				OpenCLCommandQueueRef command_queue = new OpenCLCommandQueue(context, device->opencl_device_id, false);
				std::string build_log;
				OpenCLProgramRef program;
				try
				{
					program = ::getGlobalOpenCL()->buildProgram(
						extended_source,
						context,
						devices,
						"", // options
						build_log
					);
				}
				catch(Indigo::Exception& e)
				{
					failTest("Build failed: " + e.what() + "\nOpenCL device: " + device->description() + "\nbuild_log:\n" + build_log);
				}
				//conPrint("build_log: \n" + build_log);

				OpenCLBuffer output_buffer(context, sizeof(int), CL_MEM_READ_WRITE);


				OpenCLKernelRef kernel = new OpenCLKernel(program, "main_kernel", device->opencl_device_id, /*profile=*/false);

				kernel->setKernelArgInt(0, x);
				kernel->setKernelArgBuffer(1, output_buffer.getDevicePtr());

				// Launch the kernel
				const size_t global_work_size = 1;
				kernel->launchKernel(command_queue->getCommandQueue(), global_work_size);

				SSE_ALIGN int host_output_buffer[1];

				// Read back result
				cl_int result = ::getGlobalOpenCL()->clEnqueueReadBuffer(
					command_queue->getCommandQueue(),
					output_buffer.getDevicePtr(), // buffer
					CL_TRUE, // blocking read
					0, // offset
					sizeof(int), // size in bytes
					host_output_buffer, // host buffer pointer
					0, // num events in wait list
					NULL, // wait list
					NULL // event
				);
				if(result != CL_SUCCESS)
					throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

				const int opencl_result = host_output_buffer[0];

				if(opencl_result != target_return_val)
					failTest("Test failed: OpenCL returned " + toString(opencl_result) + ", target was " + toString(target_return_val));
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
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	exit(1);
}


void testMainInt64Arg(const std::string& src, int64 x, int64 target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainInt64Arg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int(64)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int64 (WINTER_JIT_CALLING_CONV *f)(int64) = (int64 (WINTER_JIT_CALLING_CONV *)(int64)) vm.getJittedFunction(mainsig);

		resetMemUsageStats();

		MARK_STACK; // Put some special byte patterns on the stack

		// Call the JIT'd function
		const int64 jitted_result = f(x);

		// Try and detect how much stack we actually used.
		size_t touched_stack_size;
		GET_TOUCHED_STACK_SIZE(touched_stack_size); // Sets touched_stack_size var.

		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));


		testAssert(getCurrentHeapMemUsage() == 0);

		try
		{
			GetSpaceBoundParams params;
			GetSpaceBoundResults space_bounds = maindef->getSpaceBound(params);

			// Compute space bounds for arg
			const size_t arg_heap_size = 0;
			const size_t total_allowed_heap = space_bounds.heap_space + arg_heap_size;

			if(PRINT_MEM_BOUNDS_AND_USAGE)
			{
				conPrint("stack space bound: " + toString(space_bounds.stack_space));
				conPrint("stack used:        " + toString(touched_stack_size));
				conPrint("heap space bound:  " + toString(total_allowed_heap));
				conPrint("heap used:         " + toString(getMaxHeapMemUsage()));
				conPrint("");
			}
			
			testAssert(getMaxHeapMemUsage() <= total_allowed_heap);
			if(CHECK_STACK_USAGE) testAssert(touched_stack_size <= space_bounds.stack_space);
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get space bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_SPACE_BOUND_FAILURE) == 0)
				failTest("Failed to get space bound: " + e.messageWithPosition());
		}

		try
		{
			GetTimeBoundParams params;
			const size_t time_bound = maindef->getTimeBound(params);
			if(time_bound > (1 << 22)) // Just do something with the time bound so it doesn't get compiled away.
				failTest("Time bound too large.");
		}
		catch(BaseException& e)
		{
			conPrint("Failed to get time bound: " + e.messageWithPosition());
			if((test_flags & ALLOW_TIME_BOUND_FAILURE) == 0)
				failTest("Failed to get time bound: " + e.messageWithPosition());
		}


		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
			failTest("main() Return value was of unexpected type.");
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


void testMainInt16Arg(const std::string& src, int16 x, int16 target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainInt16Arg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int(16)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		int16 (WINTER_JIT_CALLING_CONV *f)(int16) = (int16 (WINTER_JIT_CALLING_CONV *)(int16)) vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const int16 jitted_result = f(x);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, true));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
			failTest("main() Return value was of unexpected type.");
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


void testMainUInt32Arg(const std::string& src, uint32 x, uint32 target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainUInt32Arg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int(32, false)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		uint32 (WINTER_JIT_CALLING_CONV *f)(uint32) = (uint32 (WINTER_JIT_CALLING_CONV *)(uint32)) vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		const uint32 jitted_result = f(x);


		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new IntValue(x, false));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Int)
			failTest("main() Return value was of unexpected type.");
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


void testMainBoolArg(const std::string& src, bool x, bool target_return_val, uint32 test_flags)
{
	testPrint("===================== Winter testMainBoolArg() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Bool()));
		vm_args.entry_point_sigs.push_back(mainsig);
		VirtualMachine vm(vm_args);
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig); // Get main function

		bool (WINTER_JIT_CALLING_CONV *f)(bool, void*) = (bool (WINTER_JIT_CALLING_CONV *)(bool, void*)) vm.getJittedFunction(mainsig);

		const bool jitted_result = f(x, NULL); // Call the JIT'd function

		printVar(jitted_result);

		// Check JIT'd result.
		if(jitted_result != target_return_val)
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new BoolValue(x));

		ValueRef retval = maindef->invoke(vmstate);
		vmstate.func_args_start.pop_back();
		
		if(retval->valueType() != Value::ValueType_Bool)
			failTest("main() Return value was of unexpected type.");

		BoolValue* val = static_cast<BoolValue*>(retval.getPointer());

		if(val->value != target_return_val)
			failTest("Test failed: main returned " + toString(val->value) + ", target was " + toString(target_return_val));

	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


void testMainIntegerArgInvalidProgram(const std::string& src)
{
	testPrint("===================== Winter testMainIntegerArgInvalidProgram() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int()));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		vm.getJittedFunction(mainsig);

		failTest("Test failed: Expected compilation failure.");
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		testPrint("Expected exception occurred: " + e.messageWithPosition());
	}
}


void testMainInt64ArgInvalidProgram(const std::string& src)
{
	testPrint("===================== Winter testMainInt64ArgInvalidProgram() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Int(64)));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		vm.getJittedFunction(mainsig);

		failTest("Test failed: Expected compilation failure.");
	}
	catch(Winter::BaseException& e)
	{
		// Expected.
		testPrint("Expected exception occurred: " + e.messageWithPosition());
	}
}


//typedef float(*float_void_func)();


template <class StructType>
static void bleh(StructType* s)
{
	s->a = 1;
}


//#if defined(_WIN32) || defined(_WIN64)
//#define SSE_ALIGN _MM_ALIGN16
//#define SSE_CLASS_ALIGN _MM_ALIGN16 class
//#else
//#define SSE_ALIGN __attribute__ ((aligned (16)))
//#define SSE_CLASS_ALIGN class __attribute__ ((aligned (16)))
//#endif
//
//
//#if defined(_WIN32) || defined(_WIN64)
//#define ALIGN_32 _CRT_ALIGN(32)
//#define CLASS_ALIGN_32 _CRT_ALIGN(32) class
//#else
//#define ALIGN_32 __attribute__ ((aligned (32)))
//#define CLASS_ALIGN_32 class __attribute__ ((aligned (32)))
//#endif

// Do explicit template instantiation
template void testMainStruct(const std::string& src, const TestStruct& target_return_val);

template <class StructType>
void testMainStruct(const std::string& src, const StructType& target_return_val)
{
	testPrint("===================== Winter testMainStruct() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));

		const FunctionSignature mainsig("main", std::vector<TypeVRef>());

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Get main function
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
		if(!(jitted_result == target_return_val)) //!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: jitted_result != target_return_val");


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
		failTest(e.what());
	}
}


// Do explicit template instantiation
template void testMainStructInputAndOutput(const std::string& src, const TestStructIn& struct_in, const TestStruct& target_return_val);


template <class InStructType, class OutStructType>
void testMainStructInputAndOutput(const std::string& src, const InStructType& struct_in, const OutStructType& target_return_val)
{
	testPrint("===================== Winter testMainStructInputAndOutput() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));

		std::vector<std::string> field_names;
		field_names.push_back("x");
		field_names.push_back("y");

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(1, TypeVRef(new StructureType(
				"TestStructIn", 
				std::vector<TypeVRef>(2, TypeVRef(new Float)), 
				field_names
			)))
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(OutStructType*, InStructType*) = (void (WINTER_JIT_CALLING_CONV *)(OutStructType*, InStructType*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN OutStructType jitted_result;

		SSE_ALIGN InStructType aligned_struct_in = struct_in;

		f(&jitted_result, &aligned_struct_in);

		// Check JIT'd result.
		if(!(jitted_result == target_return_val))
			failTest("Test failed: jitted_result != target_return_val");

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
		failTest(e.what());
	}
}


bool epsEqual(const float4& a, const float4& b)
{
	for(int i=0; i<4; ++i)
		if(!epsEqual(a.e[i], b.e[i]))
			return false;
	return true;
}


bool epsEqual(const StructWithVec& a, const StructWithVec& b)
{
	return epsEqual(a.a, b.a) && epsEqual(a.b, b.b) && epsEqual(a.data2, b.data2);
}


bool epsEqual(const Float4Struct& a, const Float4Struct& b)
{
	for(int i=0; i<4; ++i)
		if(!epsEqual(a.v.e[i], b.v.e[i]))
			return false;
	return true;
}


bool epsEqual(const Float8Struct& a, const Float8Struct& b)
{
	for(int i=0; i<8; ++i)
		if(!epsEqual(a.v.e[i], b.v.e[i]))
			return false;
	return true;
}


void testFloat4StructPairRetFloat(const std::string& src, const Float4StructPair& a, const Float4StructPair& b, float target_return_val)
{
	testPrint("===================== Winter testFloat4StructPairRetFloat() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.real_is_double = false;


		// Create Float4Struct type
	
		std::vector<std::string> field_names;
		field_names.push_back("v");

		std::vector<TypeVRef> field_types;
		field_types.push_back(new VectorType(new Float, 4));

		TypeVRef float_4_struct_type = new StructureType(
			"Float4Struct", 
			field_types, 
			field_names
		);
		

		// Create Float4StructPair type
		std::vector<std::string> field_names2;
		field_names2.push_back("a");
		field_names2.push_back("b");

		std::vector<TypeVRef> field_types2(2, float_4_struct_type);

		TypeVRef Float4StructPair_type = new StructureType(
			"Float4StructPair", 
			field_types2, 
			field_names2
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(2, Float4StructPair_type)
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
			failTest("Test failed: JIT'd main returned " + toString(jitted_result) + ", target was " + toString(target_return_val));
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
		failTest(e.what());
	}
}


void testVectorInStruct(const std::string& src, const StructWithVec& struct_in, const StructWithVec& target_return_val)
{
	testPrint("===================== Winter testVectorInStruct() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.floating_point_literals_default_to_double = false;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));

		std::vector<std::string> field_names;
		field_names.push_back("a");
		field_names.push_back("b");
		field_names.push_back("data2");

		std::vector<TypeVRef> field_types;
		field_types.push_back(new VectorType(new Float, 4));
		field_types.push_back(new VectorType(new Float, 4));
		field_types.push_back(new Float);


		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(1, new StructureType(
				"StructWithVec", 
				field_types, 
				field_names
			))
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(StructWithVec*, StructWithVec*) = (void (WINTER_JIT_CALLING_CONV *)(StructWithVec*, StructWithVec*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		SSE_ALIGN StructWithVec jitted_result;

		SSE_ALIGN StructWithVec aligned_struct_in = struct_in;

		f(&jitted_result, &aligned_struct_in);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: JIT'd main returned value != expected.");

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
		failTest(e.what());
	}
}


void testFloat4Struct(const std::string& src, const Float4Struct& a, const Float4Struct& b, const Float4Struct& target_return_val)
{
	testPrint("===================== Winter testFloat4Struct() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.floating_point_literals_default_to_double = false;

		std::vector<std::string> field_names;
		field_names.push_back("v");

		std::vector<TypeVRef> field_types;
		field_types.push_back(new VectorType(new Float, 4));


		TypeVRef float_4_struct_type = new StructureType(
			"Float4Struct", 
			field_types, 
			field_names
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(2, float_4_struct_type)
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(Float4Struct*, const Float4Struct*, const Float4Struct*) = 
			(void (WINTER_JIT_CALLING_CONV *)(Float4Struct*, const Float4Struct*, const Float4Struct*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		Float4Struct jitted_result;

		f(&jitted_result, &a, &b);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: JIT'd main returned different value than target.");
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
}


void testFloat8Struct(const std::string& src, const Float8Struct& a, const Float8Struct& b, const Float8Struct& target_return_val)
{
	testPrint("===================== Winter testFloat8Struct() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));

		std::vector<std::string> field_names;
		field_names.push_back("v");

		std::vector<TypeVRef> field_types;
		field_types.push_back(new VectorType(new Float, 8));


		TypeVRef float_8_struct_type = new StructureType(
			"Float8Struct", 
			field_types, 
			field_names
		);

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(2, float_8_struct_type)
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(Float8Struct*, const Float8Struct*, const Float8Struct*) = 
			(void (WINTER_JIT_CALLING_CONV *)(Float8Struct*, const Float8Struct*, const Float8Struct*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		Float8Struct jitted_result;

		f(&jitted_result, &a, &b);

		// Check JIT'd result.
		if(!epsEqual(jitted_result, target_return_val))
			failTest("Test failed: jitted_result != target_return_val");
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
}


void testIntArray(const std::string& src, const int* a, const int* b, const int* target_return_val, size_t len, uint32 test_flags)
{
	testPrint("===================== Winter testIntArray() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.allow_unsafe_operations = (test_flags & ALLOW_UNSAFE) != 0;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		if(test_flags & DISABLE_CONSTANT_FOLDING)
			vm_args.do_constant_folding = false;

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(2, new ArrayType(new Int(), len)) // 2 float arrays of len elems each
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(int*, const int*, const int*) = 
			(void (WINTER_JIT_CALLING_CONV *)(int*, const int*, const int*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		js::Vector<int, 32> jitted_result(len);
		int* jitted_result_ptr = &jitted_result[0];

		f(jitted_result_ptr, a, b);

		// Check JIT'd result.
		for(size_t i=0; i<len; ++i)
		{
			if(jitted_result[i] != target_return_val[i])
			{
				stdErrPrint("Test failed: jitted_result[i] != target_return_val[i]");
				stdErrPrint("i: " + toString(i));
				stdErrPrint("jitted_result[i]: " + toString(jitted_result[i]));
				failTest("target_return_val[i]: " + toString(target_return_val[i]));
			}
		}
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
}


void testFloatArray(const std::string& src, const float* a, const float* b, const float* target_return_val, size_t len)
{
	testPrint("===================== Winter testFloatArray() =====================");
	try
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		vm_args.floating_point_literals_default_to_double = false;

		// Get main function
		const FunctionSignature mainsig(
			"main", 
			std::vector<TypeVRef>(2, new ArrayType(new Float(), len)) // 2 float arrays of 4 elems each
		);

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		// __cdecl
		void (WINTER_JIT_CALLING_CONV *f)(float*, const float*, const float*) = 
			(void (WINTER_JIT_CALLING_CONV *)(float*, const float*, const float*))vm.getJittedFunction(mainsig);

		// Call the JIT'd function
		js::Vector<float, 32> jitted_result(len);
		// Clear mem 
		for(size_t i=0; i<len; ++i)
			jitted_result[i] = 0.0f;

		float* jitted_result_ptr = &jitted_result[0];

		Timer timer;
		f(jitted_result_ptr, a, b);
		const double elapsed = timer.elapsed();
		testPrint("JITed code elapsed: " + toString(elapsed) + " s");
		const double bandwidth = len * sizeof(float) / elapsed;
		testPrint("JITed bandwidth: " + toString(bandwidth * 1.0e-9) + " GiB/s");

		// Check JIT'd result.
		for(size_t i=0; i<len; ++i)
		{
			if(!epsEqual(jitted_result[i], target_return_val[i]))
				failTest("Test failed: jitted_result != target_return_val  ");
		}
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
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


#endif // BUILD_TESTS
