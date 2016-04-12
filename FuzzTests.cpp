/*=====================================================================
wnt_TupleLiteral.cpp
--------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_TupleLiteral.h"


#include "VMState.h"
#include "VirtualMachine.h"
#include "wnt_ArrayLiteral.h"
#include "wnt_VectorLiteral.h"
#include "wnt_TupleLiteral.h"
#include "wnt_IfExpression.h"
#include "wnt_FunctionExpression.h"
#include "wnt_Lexer.h"
#include "wnt_LangParser.h"
#include "wnt_LetASTNode.h"
#include "wnt_LetBlock.h"
#include <utils/Timer.h>
#include <utils/MTwister.h>
#include <utils/Task.h>
#include <utils/TaskManager.h>
#include <utils/MemMappedFile.h>
#include <utils/FileUtils.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#include "../indigo/StandardPrintOutput.h"
#include <Mutex.h>
#include <Lock.h>
#include <Exception.h>
#include <Vector.h>
#include <unordered_set>
#include <fstream>
#include <xxhash.h>

#if FUZZING_USE_OPENCL
#include "../../indigo/trunk/opencl/OpenCL.h"
#include "../../indigo/trunk/opencl/OpenCLBuffer.h"
#endif

#if BUILD_TESTS


#include <iostream>


namespace Winter
{


// Returns true if valid program, false otherwise.
bool testFuzzProgram(const std::string& src)
{
	try
	{
		//TestEnv test_env;
		//test_env.val = 10;

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		//vm_args.env = &test_env;
		
		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Remove non-printable chars so console doesn't make bell sounds while printing.
		//std::cout << ("\nCompiled OK:\n" + StringUtils::removeNonPrintableChars(src) + "\n");

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		float(WINTER_JIT_CALLING_CONV*f)(float, void*) = (float(WINTER_JIT_CALLING_CONV*)(float, void*))vm.getJittedFunction(mainsig);

		// Check it has return type float
		if(maindef->returnType()->getType() != Type::FloatType)
			throw Winter::BaseException("main did not have return type float.");


		// Call the JIT'd function
		const float argument = 1.0f;
		const float jitted_result = f(argument, NULL);//&test_env);

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new FloatValue(argument));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval.getPointer());
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}

		if(isNAN(val->value) && isNAN(jitted_result))
		{
			std::cout << "both values are NaN" << std::endl;
		}
		else if(val->value == jitted_result)
		{
		}
		else
		{
			if(!epsEqual(val->value, jitted_result))
			{
				std::cerr << "Test failed: main returned " << val->value << ", jitted_result was " << jitted_result << std::endl;
				assert(0);
				exit(1);
			}
		}


		//============================= New: test with OpenCL ==============================
		const bool TEST_OPENCL = true;
		if(TEST_OPENCL)
		{
#if FUZZING_USE_OPENCL
			OpenCL* opencl = getGlobalOpenCL();

			const int device_index = 0;

			cl_context context;
			cl_command_queue command_queue;
			opencl->deviceInit(
				opencl->getDeviceInfo()[device_index],
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

			std::cout << extended_source << std::endl;

			OpenCLBuffer output_buffer(context, sizeof(float), CL_MEM_READ_WRITE);

			std::string options = "-cl-opt-disable";//"-save-temps";

			// Compile and build program.
			cl_program program = opencl->buildProgram(
				extended_source,
				context,
				opencl->getDeviceInfo()[device_index].opencl_device,
				options
			);


			opencl->dumpBuildLog(program, opencl->getDeviceInfo()[device_index].opencl_device); 

			// Create kernel
			cl_int result;
			cl_kernel kernel = opencl->clCreateKernel(program, "main_kernel", &result);

			if(!kernel)
				throw Indigo::Exception("clCreateKernel failed");


			if(opencl->clSetKernelArg(kernel, 0, sizeof(cl_float), &argument) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 0");
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


			SSE_ALIGN float host_output_buffer[1];

			// Read back result
			result = opencl->clEnqueueReadBuffer(
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

			if(!((opencl_result == jitted_result) || epsEqual(opencl_result, jitted_result))) // opencl_result == jitted_result handles Inf == Inf case.
			{
				std::cerr << "Test failed: OpenCL returned " << opencl_result << ", jitted_result was " << jitted_result << std::endl;
				assert(0);
				exit(1);
			}
#endif // #if FUZZING_USE_OPENCL
		}
	}
	catch(Winter::BaseException& e)
	{
		if(e.what() == "Module verification errors.")
		{
			std::cerr << "Module verification errors while compiling " << src << std::endl;
			assert(0);
			exit(1);
		}
		// Compile failure when fuzzing is alright.
		//std::cerr << e.what() << std::endl;
		return false;
	}
	catch(Indigo::Exception& )
	{
		//std::cerr << e.what() << std::endl;
		return false;
	}

	return true;
}



// Returns true if valid program, false otherwise.
static bool testFuzzASTProgram(Reference<BufferRoot>& root)//const std::vector<FunctionDefinitionRef>& funcs)
{
	try
	{
		//TestEnv test_env;
		//test_env.val = 10;

		//const std::string src_string = func->sourceString();

		VMConstructionArgs vm_args;
		//vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer("buffer", src)));
		//vm_args.env = &test_env;

		vm_args.comments_in_opencl_output = false;
		for(size_t i=0; i<root->top_level_defs.size(); ++i)
			if(root->top_level_defs[i]->nodeType() == ASTNode::FunctionDefinitionType)
				vm_args.preconstructed_func_defs.push_back(root->top_level_defs[i].downcast<FunctionDefinition>());

		
		const FunctionSignature mainsig("main", std::vector<TypeRef>(1, TypeRef(new Float())));

		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		// Remove non-printable chars so console doesn't make bell sounds while printing.
		//std::cout << ("\nCompiled OK:\n" + StringUtils::removeNonPrintableChars(src) + "\n");

		// Get main function
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		float(WINTER_JIT_CALLING_CONV*f)(float, void*) = (float(WINTER_JIT_CALLING_CONV*)(float, void*))vm.getJittedFunction(mainsig);

		// Check it has return type float
		if(maindef->returnType()->getType() != Type::FloatType)
			throw Winter::BaseException("main did not have return type float.");


		// Call the JIT'd function
		const float argument = 1.0f;
		const float jitted_result = f(argument, NULL);//&test_env);

		VMState vmstate;
		vmstate.func_args_start.push_back(0);
		vmstate.argument_stack.push_back(new FloatValue(argument));
		//vmstate.argument_stack.push_back(new VoidPtrValue(&test_env));

		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();
		FloatValue* val = dynamic_cast<FloatValue*>(retval.getPointer());
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
			assert(0);
			exit(1);
		}

		if(!((val->value == jitted_result) || epsEqual(val->value, jitted_result))) // val->value == jitted_result handles Inf == Inf case.
		{
			std::cerr << "Test failed: main returned " << val->value << ", jitted_result was " << jitted_result << std::endl;
			assert(0);
			exit(1);
		}


		//============================= New: test with OpenCL ==============================
		const bool TEST_OPENCL = false;
		if(TEST_OPENCL)
		{
#if FUZZING_USE_OPENCL
			conPrint("Program was valid, testing in OpenCL.");
			//std::cout << "testing opencl prog: " + src_string << std::endl;
			OpenCL* opencl = getGlobalOpenCL();

			cl_context context;
			cl_command_queue command_queue;
			opencl->deviceInit(
				opencl->getDeviceInfo()[0],
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

			//std::cout << "---------------------\n" << extended_source << "\n-------------------" << std::endl;

			OpenCLBuffer output_buffer(context, sizeof(float), CL_MEM_READ_WRITE);

			std::string options = "";//"-save-temps";

			StandardPrintOutput print_output;

			// Compile and build program.
			cl_program program = opencl->buildProgram(
				extended_source,
				context,
				opencl->getDeviceInfo()[0].opencl_device,
				options
			);


			//opencl->dumpBuildLog(program, opencl->getDeviceInfo()[0].opencl_device); 

			// Create kernel
			cl_int result;
			cl_kernel kernel = opencl->clCreateKernel(program, "main_kernel", &result);

			if(!kernel)
				throw Indigo::Exception("clCreateKernel failed");


			if(opencl->clSetKernelArg(kernel, 0, sizeof(cl_float), &argument) != CL_SUCCESS) throw Indigo::Exception("clSetKernelArg failed 0");
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


			SSE_ALIGN float host_output_buffer[1];

			// Read back result
			result = opencl->clEnqueueReadBuffer(
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

			if(!((opencl_result == jitted_result) || epsEqual(opencl_result, jitted_result))) // opencl_result == jitted_result handles Inf == Inf case.
			{
				std::cerr << "Test failed: OpenCL returned " << val->value << ", jitted_result was " << jitted_result << std::endl;
				assert(0);
				exit(1);
			}
#endif // #if FUZZING_USE_OPENCL
		}
	}
	catch(Winter::BaseException& e)
	{
		if(e.what() == "Module verification errors.")
		{
			std::cerr << "Module verification errors while compiling AST program." << std::endl;
			assert(0);
			exit(1);
		}
		// Compile failure when fuzzing is alright.
		std::cerr << e.what() << std::endl;
		return false;
	}
	catch(Indigo::Exception& )
	{
		//std::cerr << e.what() << std::endl;
		return false;
	}

	return true;
}


// Fuzz testing choice
struct Choice
{
	enum Action
	{
		Action_Break,
		Action_Insert,
		Action_InsertRandomChar,
		Action_Copy,
		Action_CopyFromOtherProgram,
		Action_Remove,

		Action_AddASTNode,
		Action_AddStructConstructorCall
	};

	Choice() {}
	Choice(Action action_, float probability_) : action(action_), probability(probability_) {}
	Choice(Action action_, std::string left_, float probability_) : action(action_), probability(probability_), left(left_) {}
	Choice(Action action_, std::string left_, std::string right_, float probability_) : action(action_), probability(probability_), left(left_), right(right_) {}
	Choice(Action action_, ASTNode::ASTNodeType ast_node_type_, float probability_) : action(action_), probability(probability_), ast_node_type(ast_node_type_) {}

	Action action;
	float probability;
	std::string left;
	std::string right;
	ASTNode::ASTNodeType ast_node_type;
};


static std::string readRandomProgramFromFuzzerInput(const std::vector<std::string>* fuzzer_input, MTwister& rng)
{
	// Pick a random input line to get started
	std::string start_string;
	while(start_string.empty())
	{
		// Pick a line to start at
		size_t linenum = myMin(fuzzer_input->size()-1, (size_t)(fuzzer_input->size() * rng.unitRandom()));

		// If we are in whitespace, pick another line.
		if(::isAllWhitespace((*fuzzer_input)[linenum]))
			continue;

		// Go up until we are below a whitespace line
		while(linenum >= 1 && !::isAllWhitespace((*fuzzer_input)[linenum - 1]))
			linenum--;
				
		start_string = (*fuzzer_input)[linenum]; // Get line

		linenum++;
		// While there are more lines below that aren't just whitespace, then the program continues, so append.
		for(; linenum<fuzzer_input->size() && !::isAllWhitespace((*fuzzer_input)[linenum]); ++linenum)
			start_string += " " + (*fuzzer_input)[linenum];
	}
	return start_string;
}


struct BuildRandomSubTreeArgs
{
	bool allow_func_calls;
};


// Build random abstract-syntax sub-tree
static ASTNodeRef buildRandomASSubTree(BuildRandomSubTreeArgs& args, MTwister& rng, const std::vector<Choice>& choices, int depth)
{
	const float r = rng.unitRandom();

	if(depth > 3)
	{
		if(rng.unitRandom() < 0.3f)
			return new Variable("x", SrcLocation::invalidLocation());
		else
			return new FloatLiteral(rng.unitRandom() < 0.25f ? 0.f : (int)(-10.f + rng.unitRandom() * 20.f), SrcLocation::invalidLocation());
		//return new FloatLiteral(1.0f, SrcLocation::invalidLocation());

		/*if(r < 0.33f)
			return new FloatLiteral(1.0f, SrcLocation::invalidLocation());
		else if(r < 0.66f)
			return new IntLiteral(1, 32, SrcLocation::invalidLocation());
		else
			return new BoolLiteral(rng.unitRandom() < 0.5f, SrcLocation::invalidLocation());*/
	}

	float probability_sum = 0;
	for(size_t z=0; z<choices.size(); ++z)
	{
		probability_sum += choices[z].probability;
		if(r < probability_sum)
		{
			if(choices[z].action == Choice::Action_AddASTNode)
			{
				const ASTNode::ASTNodeType node_type = choices[z].ast_node_type;
				switch(node_type)
				{
				case ASTNode::VariableASTNodeType:
					return new Variable("x", SrcLocation::invalidLocation());
				case ASTNode::FunctionExpressionType:
					{
						if(args.allow_func_calls)
							return new FunctionExpression(SrcLocation::invalidLocation(), "f", buildRandomASSubTree(args, rng, choices, depth + 1));
						else
							return new FloatLiteral(1.f, SrcLocation::invalidLocation());
					}
				case ASTNode::FloatLiteralType:
					{
						return new FloatLiteral(rng.unitRandom() < 0.25f ? 0.f : (int)(-10.f + rng.unitRandom() * 20.f), SrcLocation::invalidLocation());
						//return new FloatLiteral(2.0f, SrcLocation::invalidLocation());//rng.unitRandom() < 0.25f ? 0.f : (-10.f + rng.unitRandom() * 20.f), SrcLocation::invalidLocation());
					}
				case ASTNode::IntLiteralType:
					return new IntLiteral(-2 + (int)(rng.unitRandom() * 4.0f), 32, SrcLocation::invalidLocation());
				case ASTNode::BoolLiteralType:
					return new BoolLiteral(rng.unitRandom() < 0.5f, SrcLocation::invalidLocation());

				case ASTNode::ArrayLiteralType:
					{
						std::vector<ASTNodeRef> elems;
						do 
						{
							elems.push_back(buildRandomASSubTree(args, rng, choices, depth + 1));
						}
						while(rng.unitRandom() < 0.5f);
						return new ArrayLiteral(elems, SrcLocation::invalidLocation(), false, 0);
					}
				case ASTNode::VectorLiteralType:
					{
						std::vector<ASTNodeRef> elems;
						do 
						{
							elems.push_back(buildRandomASSubTree(args, rng, choices, depth + 1));
						}
						while(rng.unitRandom() < 0.5f);
						return new VectorLiteral(elems, SrcLocation::invalidLocation(), false, 0);
					}
				case ASTNode::TupleLiteralType:
					{
						std::vector<ASTNodeRef> elems;
						do 
						{
							elems.push_back(buildRandomASSubTree(args, rng, choices, depth + 1));
						}
						while(rng.unitRandom() < 0.5f);
						return new TupleLiteral(elems, SrcLocation::invalidLocation());
					}

				case ASTNode::AdditionExpressionType:
					return new AdditionExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));
				case ASTNode::SubtractionExpressionType:
					return new SubtractionExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));
				case ASTNode::MulExpressionType:
					return new MulExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));
				case ASTNode::DivExpressionType:
					return new DivExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));

				case ASTNode::BinaryBooleanType:
					return new BinaryBooleanExpr(rng.unitRandom() < 0.5f ? BinaryBooleanExpr::AND : BinaryBooleanExpr::OR, buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1), SrcLocation::invalidLocation());
				case ASTNode::UnaryMinusExpressionType:
					return new UnaryMinusExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1));
		
				case ASTNode::ComparisonExpressionType:
					{
						Reference<TokenBase> token;
						const float z = rng.unitRandom();
						if(z < 1.f/6)
							token = new LEFT_ANGLE_BRACKET_Token(0);
						else if(z < 2.f/6)
							token = new RIGHT_ANGLE_BRACKET_Token(0);
						else if(z < 3.f/6)
							token = new DOUBLE_EQUALS_Token(0);
						else if(z < 4.f/6)
							token = new NOT_EQUALS_Token(0);
						else if(z < 5.f/6)
							token = new LESS_EQUAL_Token(0);
						else
							token = new GREATER_EQUAL_Token(0);
						return new ComparisonExpression(token, buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1), SrcLocation::invalidLocation());
					}
				case ASTNode::ArraySubscriptType:
					{
						//return new ArrayS(token, buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1), SrcLocation::invalidLocation());
						return new FunctionExpression(SrcLocation::invalidLocation(), "elem", buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));
					}
				case ASTNode::IfExpressionType:
						return new IfExpression(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1), buildRandomASSubTree(args, rng, choices, depth + 1));
				case ASTNode::LogicalNegationExprType:
					return new LogicalNegationExpr(SrcLocation::invalidLocation(), buildRandomASSubTree(args, rng, choices, depth + 1));
				default:
					assert(0);
					return NULL;
				}
			}
			else if(choices[z].action == Choice::Action_AddStructConstructorCall)
			{
				// returns some code like, for struct s { float x, float y }
				/*
				let
					s_1 = s(EXPR, EXPR)
				in
					s_1.x

				*/
				//return new FunctionExpression(SrcLocation::invalidLocation(), "s", buildRandomASSubTree(rng, choices, depth + 1), buildRandomASSubTree(rng, choices, depth + 1));
				/*ASTNodeRef constructor_call = new FunctionExpression(SrcLocation::invalidLocation(), "s", buildRandomASSubTree(rng, choices, depth + 1), buildRandomASSubTree(rng, choices, depth + 1));
				ASTNodeRef member_access = new FunctionExpression(SrcLocation::invalidLocation(), "x", new Variable("s_1", SrcLocation::invalidLocation()));
				Reference<LetASTNode> let_node = new LetASTNode(std::vector<LetNodeVar>(1, LetNodeVar("s_1")), constructor_call, SrcLocation::invalidLocation());
				ASTNodeRef let_block = new LetBlock(
					member_access, // expr
					std::vector<Reference<LetASTNode> >(1, let_node), // lets
					SrcLocation::invalidLocation()
				);*/


				/*
				let
					t = [EXPR, EXPR]t
				in
					elem(t, 0)
				*/
				const int tuple_size = 1 + (int)(rng.unitRandom() * 4.0);
				std::vector<ASTNodeRef> tuple_elems;
				for(int i=0; i<tuple_size; ++i)
					tuple_elems.push_back(buildRandomASSubTree(args, rng, choices, depth + 1));
				ASTNodeRef tuple_literal = new TupleLiteral(tuple_elems, SrcLocation::invalidLocation());

				ASTNodeRef field_access = new FunctionExpression(SrcLocation::invalidLocation(), "elem", 
					new Variable("t", SrcLocation::invalidLocation()), 
					new IntLiteral(myClamp((int)(rng.unitRandom() * tuple_size), 0, tuple_size-1), 32, SrcLocation::invalidLocation()));

				Reference<LetASTNode> let_node = new LetASTNode(std::vector<LetNodeVar>(1, LetNodeVar("t")), tuple_literal, SrcLocation::invalidLocation());

				ASTNodeRef let_block = new LetBlock(
					field_access, // expr
					std::vector<Reference<LetASTNode> >(1, let_node), // lets
					SrcLocation::invalidLocation()
				);

				return let_block;
			}
		}
	}

	return new FloatLiteral(1.0f, SrcLocation::invalidLocation());
}


class ASTFuzzTask : public Indigo::Task
{
public:
	void run(size_t thread_index)
	{
		Timer timer;
		Timer print_timer;
		MTwister rng(rng_seed);

		std::ofstream outfile("D:/fuzz_output/fuzz_thread_" + toString(thread_index) + ".txt"); // TEMP HACK HARD CODED PATH
		
		// See comment in FuzzTask below about this number.
		const int N = 134217728;
		int num_valid_programs = 0;
		for(int i=0; i<N; ++i)
		{
			// Pick a random program to get started
			SourceBufferRef source_buffer;
			FunctionDefinitionRef main_def;
			Reference<BufferRoot> root;// = new BufferRoot(SrcLocation::invalidLocation());
			if(true)
			{
				try
				{
					const std::string start_string = readRandomProgramFromFuzzerInput(fuzzer_input, rng);

					source_buffer = new SourceBuffer("source", start_string);
					std::vector<Reference<TokenBase> > tokens;
					Lexer::process(source_buffer, tokens);
				
					LangParser lang_parser(
						false, // floating_point_literals_default_to_double
						false // real_is_double
					);

					std::map<std::string, TypeRef> named_types;
					std::vector<TypeRef> named_types_ordered;
					int function_order_num = 0;

					root = lang_parser.parseBuffer(tokens,
						source_buffer,
						named_types,
						named_types_ordered,
						function_order_num
					);

					// Try and find function 'main'
					for(size_t i=0; i<root->top_level_defs.size(); ++i)
					{
						if(root->top_level_defs[i]->nodeType() == ASTNode::FunctionDefinitionType)
						{
							if(root->top_level_defs[i].downcastToPtr<FunctionDefinition>()->sig.name == "main")
								main_def = root->top_level_defs[i].downcast<FunctionDefinition>();
						}
					}
				}
				catch(BaseException& )
				{
				}
			}
			//TEMP


			//Reference<BufferRoot> root = new BufferRoot(SrcLocation::invalidLocation());

			/*{
				BuildRandomSubTreeArgs args;
				args.allow_func_calls = false;

				// Define function 'f'
				FunctionDefinitionRef def = new FunctionDefinition(SrcLocation::invalidLocation(), 0, "f",
					std::vector<FunctionDefinition::FunctionArg>(1, FunctionDefinition::FunctionArg(new Float(), "x")),
					buildRandomASSubTree(args, rng, choices, 0),// body ast node
					new Float(), // declared ret type
					NULL // built in func-impl
				);
				root->top_level_defs.push_back(def);
			}

			{
				BuildRandomSubTreeArgs args;
				args.allow_func_calls = true;

				FunctionDefinitionRef def = new FunctionDefinition(SrcLocation::invalidLocation(), 1, "main",
					std::vector<FunctionDefinition::FunctionArg>(1, FunctionDefinition::FunctionArg(new Float(), "x")),
					buildRandomASSubTree(args, rng, choices, 0),// body ast node
					new Float(), // declared ret type
					NULL // built in func-impl
				);
				root->top_level_defs.push_back(def);
			}*/
			

			const std::string src = root.nonNull() ? root->sourceString() : "";

			conPrint("-------------------------------\n" + src + "\n-------------------------------");//TEMP

			const uint64 src_hash = XXH64(src.data(), src.size(), 0);
		
			bool already_tested;
			size_t num_tested;
			{
				Lock lock(*tested_programs_mutex);

				already_tested = tested_program_hashes->find(src_hash) != tested_program_hashes->end();

				if(!already_tested)
					tested_program_hashes->insert(src_hash);

				num_tested = tested_program_hashes->size();
			}

			if(!already_tested)
			{
				// Write program source to disk, so if testFuzzProgram crashes we will have a record of the program.
				//def->print(0, outfile);
				outfile << src << std::endl;

				//TEMP
				//def->print(0, std::cout);
				//std::cout << src << std::endl;

				const bool valid_program =  root.nonNull() ? testFuzzASTProgram(root) : false;
				if(valid_program) num_valid_programs++;
			}
			

			if(print_timer.elapsed() > 2.0)
			{
				const double tests_per_sec = i / timer.elapsed();
				std::cout << (std::string("Iterations: ") + toString(i) + ", num_tested: " + toString(num_tested) + ", num valid: " + toString(num_valid_programs) + ", Test speed: " + doubleToStringNDecimalPlaces(tests_per_sec, 1) + " tests/s\n");
				print_timer.reset();
			}
		}
	}

	int rng_seed;
	std::vector<Choice> choices;
	Mutex* tested_programs_mutex;
	std::unordered_set<uint64>* tested_program_hashes;
	std::vector<std::string>* fuzzer_input;
};


class FuzzTask : public Indigo::Task
{
public:
	void run(size_t thread_index)
	{
		Timer timer;
		Timer print_timer;
		MTwister rng(rng_seed);

		std::ofstream outfile("D:/fuzz_output/fuzz_thread_" + toString(thread_index) + ".txt"); // TEMP HACK HARD CODED PATH
		
		// We want to N to be quite large, but not so large that the tested_program_hashes set uses up all our RAM.
		// Lets say each of T threads generates N strings, and they happen to be disjoint.
		// Then we have N * T * sizeof(uint64) direct item mem usage in tested_program_hashes.
		// Lets say we want to use 4 GB for this with 4 threads.
		// This gives N * 4 * 8 = 4 GB, so N = 134217728.
		const int N = 134217728;
		for(int i=0; i<N; ++i)
		{
			// Pick a random program to get started
			const std::string start_string = readRandomProgramFromFuzzerInput(fuzzer_input, rng);

			std::string s = start_string;

			// Insert tokens
			while(1)
			{
				const float r = rng.unitRandom();

				float probability_sum = 0;
				for(size_t z=0; z<choices.size(); ++z)
				{
					probability_sum += choices[z].probability;
					if(r < probability_sum)
					{
						if(choices[z].action == Choice::Action_Break) // If this is the 'stop-appending tokens' token:
							goto done;
						else if(choices[z].action == Choice::Action_Insert)
						{
							// Insert choices[z].left randomly in the existing string
							const size_t startpos = 0; // start_string.size();

							const size_t insert_pos = myMin((size_t)(startpos + rng.unitRandom() * (s.size()-startpos)), s.size() - 1);
							s.insert(insert_pos, choices[z].left); 

							// Insert choice right
							if(!choices[z].right.empty() && rng.unitRandom() < 0.8f)
							{
								const size_t right_insert_pos = myMin((size_t)(startpos + rng.unitRandom() * (s.size()-startpos)), s.size() - 1);
								s.insert(right_insert_pos, choices[z].right); 
							}
						}
						else if(choices[z].action == Choice::Action_InsertRandomChar)
						{
							const size_t insert_pos = myMin((size_t)(rng.unitRandom() * s.size()), s.size() - 1);
							s.insert(insert_pos, 1, (char)(rng.unitRandom() * 128)); 
						}
						else if(choices[z].action == Choice::Action_Remove)
						{
							// Remove random chunk of string
							if(!s.empty())
							{
								const size_t pos = myMin((size_t)(rng.unitRandom() * s.size()), s.size() - 1);
					
								const size_t chunk_len = 1 + (size_t)(20.f * rng.unitRandom() * rng.unitRandom());

								s.erase(pos, chunk_len);
							}
						}
						else if(choices[z].action == Choice::Action_Copy)
						{
							// Copy random chunk of text and insert somewhere else in string.
							if(!s.empty())
							{
								const size_t src_pos = myMin((size_t)(rng.unitRandom() * s.size()), s.size() - 1);
								const size_t chunk_len = 1 + (size_t)(20.f * rng.unitRandom() * rng.unitRandom());
								const std::string chunk = s.substr(src_pos, chunk_len);

								const size_t insert_pos = myMin((size_t)(rng.unitRandom() * s.size()), s.size());
								s.insert(insert_pos, chunk); 
							}
						}
						else if(choices[z].action == Choice::Action_CopyFromOtherProgram)
						{
							// Copy random chunk of text from some other program and insert somewhere in string.
							const std::string src_string = readRandomProgramFromFuzzerInput(fuzzer_input, rng);

							const size_t src_pos = myMin((size_t)(rng.unitRandom() * src_string.size()), src_string.size() - 1);
							const size_t chunk_len = 1 + (size_t)(src_string.size() * 2 * rng.unitRandom() * rng.unitRandom());
							const std::string chunk = src_string.substr(src_pos, chunk_len);

							const size_t insert_pos = myMin((size_t)(rng.unitRandom() * s.size()), s.size());
							s.insert(insert_pos, chunk); 
						}


						break;
					}
				}
			}
done:
			//std::cout << "\n------------------------------------------------------\n" << s << 
			//	"\n------------------------------------------------------\n" << std::endl;

			//s = "def main(float x) float : x + 1.0";

			const uint64 src_hash = XXH64(s.data(), s.size(), 0);

			bool already_tested;
			size_t num_tested;
			{
				Lock lock(*tested_programs_mutex);
				num_tested = tested_program_hashes->size();
				already_tested = tested_program_hashes->find(src_hash) != tested_program_hashes->end();

				if(!already_tested)
					tested_program_hashes->insert(src_hash);
			}

			if(!already_tested)
			{
				// Write program source to disk, so if testFuzzProgram crashes we will have a record of the program.
				outfile << s << std::endl;

				testFuzzProgram(s);
			}
			

			if(print_timer.elapsed() > 2.0)
			{
				const double tests_per_sec = i / timer.elapsed();
				std::cout << (std::string("Iterations: ") + toString(i) + ", Num tested: " + toString(num_tested) + ", Test speed: " + doubleToStringNDecimalPlaces(tests_per_sec, 1) + " tests/s\n");
				print_timer.reset();
			}
		}
	}

	int rng_seed;
	std::vector<Choice> choices;
	Mutex* tested_programs_mutex;
	std::unordered_set<uint64>* tested_program_hashes;
	std::vector<std::string>* fuzzer_input;
};


static void doASTFuzzTests()
{
	/*
		ASTNode::FunctionExpressionType,
		ASTNode::VariableASTNodeType,
		ASTNode::FloatLiteralType,
		ASTNode::IntLiteralType,
		ASTNode::BoolLiteralType,
		ASTNode::StringLiteralType,
		ASTNode::CharLiteralType,
		ASTNode::MapLiteralType,
		ASTNode::ArrayLiteralType,
		ASTNode::VectorLiteralType,
		ASTNode::TupleLiteralType,

		ASTNode::AdditionExpressionType,
		ASTNode::SubtractionExpressionType,
		ASTNode::MulExpressionType,
		ASTNode::DivExpressionType,
		ASTNode::BinaryBooleanType,

		ASTNode::UnaryMinusExpressionType,
		ASTNode::LetType,
		ASTNode::ComparisonExpressionType,
		ASTNode::AnonFunctionType,
		ASTNode::LetBlockType,
		ASTNode::ArraySubscriptType,
		ASTNode::IfExpressionType,
		ASTNode::NamedConstantType,
		ASTNode::LogicalNegationExprType
	*/
	try
	{
		std::vector<Choice> choices;
		
		choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::VariableASTNodeType, 2.0f));

		choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::FunctionExpressionType, 2.0f));

		choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::FloatLiteralType, 2.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::IntLiteralType, 2.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::BoolLiteralType, 2.0f));

	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::ArrayLiteralType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::VectorLiteralType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::TupleLiteralType, 1.0f));

		choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::AdditionExpressionType, 1.0f));
		choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::MulExpressionType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::SubtractionExpressionType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::DivExpressionType, 1.0f));

	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::BinaryBooleanType, 1.0f));

	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::UnaryMinusExpressionType, 1.0f));
		//choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::LetType, 1.0f));
		
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::ComparisonExpressionType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::ArraySubscriptType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::IfExpressionType, 1.0f));
	//	choices.push_back(Choice(Choice::Action_AddASTNode, ASTNode::LogicalNegationExprType, 1.0f));

		choices.push_back(Choice(Choice::Action_AddStructConstructorCall, 1.0f));

		

		// Normalise probabilities.
		float sum = 0;
		for(size_t i=0; i<choices.size(); ++i)
			sum += choices[i].probability;
		for(size_t i=0; i<choices.size(); ++i)
			choices[i].probability /= sum;

		std::vector<std::string> fuzzer_input;
		std::string filecontent;
		FileUtils::readEntireFileTextMode("N:/winter/trunk/fuzzer_input.txt", filecontent); // TEMP HACK hardcoded path
		fuzzer_input = ::split(filecontent, '\n');


		// Each stage has different random number seeds, and after each stage tested_programs will be cleared, otherwise it gets too large and uses up too much RAM.
		int rng_seed = 10;
		for(int stage=0; stage<1000000; ++stage)
		{
			std::cout << "=========================== Stage " << stage << "===========================================" << std::endl;

			Mutex tested_programs_mutex;
			std::unordered_set<uint64> tested_program_hashes;

			const int NUM_THREADS = 1;
			Indigo::TaskManager manager(NUM_THREADS);
			for(int i=0; i<NUM_THREADS; ++i)
			{
				Reference<ASTFuzzTask> t = new ASTFuzzTask();
				t->rng_seed = rng_seed;
				t->choices = choices;
				t->tested_programs_mutex = &tested_programs_mutex;
				t->tested_program_hashes = &tested_program_hashes;
				t->fuzzer_input = &fuzzer_input;
				manager.addTask(t);

				rng_seed++;
			}
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		std::cerr << "Test failed: " << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


void fuzzTests()
{
	doASTFuzzTests();

	try
	{
		std::vector<Choice> choices;
		choices.push_back(Choice(Choice::Action_Break, 20.f)); // Break loop choice.  Should be reasonably high probability so we don't make too many random changes each test.
		choices.push_back(Choice(Choice::Action_InsertRandomChar, 5.f));
		choices.push_back(Choice(Choice::Action_Remove, 5.f));
		choices.push_back(Choice(Choice::Action_Copy, 5.f));
		choices.push_back(Choice(Choice::Action_CopyFromOtherProgram, 5));
		
		choices.push_back(Choice(Choice::Action_Insert, " 1.0 ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " 0 ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " 1 ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " x ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, "(", ")", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " if ", " else ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " then ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " + ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " * ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " < ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " ? ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " : ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " true ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " < ", " > ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " [ ", " ]t ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " [ ", " ] ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " { ", " } ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " struct ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, ",", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, ".", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, "'", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, "\"", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, "\\", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " let ", " in ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " = ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " def ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " f ", " def f(int x) int : ", 1.0f));

		choices.push_back(Choice(Choice::Action_Insert, " int ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " int64 ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " string ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " char ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " opaque ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " float ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " bool ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " map ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " array ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " function ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " vector ", 1.0f));
		choices.push_back(Choice(Choice::Action_Insert, " tuple ", 1.0f));
		

		// Normalise probabilities.
		float sum = 0;
		for(size_t i=0; i<choices.size(); ++i)
			sum += choices[i].probability;
		for(size_t i=0; i<choices.size(); ++i)
			choices[i].probability /= sum;

		std::vector<std::string> fuzzer_input;
		std::string filecontent;
		FileUtils::readEntireFileTextMode("N:/winter/trunk/fuzzer_input.txt", filecontent); // TEMP HACK hardcoded path
		fuzzer_input = ::split(filecontent, '\n');


		// Each stage has different random number seeds, and after each stage tested_programs will be cleared, otherwise it gets too large and uses up too much RAM.
		int rng_seed = 290;
		for(int stage=0; stage<1000000; ++stage)
		{
			std::cout << "=========================== Stage " << stage << "===========================================" << std::endl;

			Mutex tested_programs_mutex;
			std::unordered_set<uint64> tested_program_hashes;

			const int NUM_THREADS = 4;
			Indigo::TaskManager manager(NUM_THREADS);
			for(int i=0; i<NUM_THREADS; ++i)
			{
				Reference<FuzzTask> t = new FuzzTask();
				t->rng_seed = rng_seed;
				t->choices = choices;
				t->tested_programs_mutex = &tested_programs_mutex;
				t->tested_program_hashes = &tested_program_hashes;
				t->fuzzer_input = &fuzzer_input;
				manager.addTask(t);

				rng_seed++;
			}
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		std::cerr << "Test failed: " << e.what() << std::endl;
		assert(0);
		exit(1);
	}
}


} // end namespace Winter


#endif // BUILD_TESTS
