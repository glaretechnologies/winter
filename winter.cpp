/*=====================================================================
winter.cpp
----------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/


#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_ASTNode.h"
#include "wnt_MathsFuncs.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "LanguageTests.h"
#include "VirtualMachine.h"
#include "FuzzTests.h"
#include "PerfTests.h"
#include "utils/FileUtils.h"
#include "utils/Clock.h"
#include "utils/ArgumentParser.h"
#include <iostream>
#include <cassert>
#include <fstream>
using namespace Winter;


typedef float(*float_void_func)();


int main(int argc, char** argv)
{
	Clock::init();

	if(argc < 2)
	{
		std::cerr << "Usage: winter program.win" << std::endl;
		return 1;
	}

	VirtualMachine::init();

	std::vector<std::string> arg_vec;
	for(int i=0; i<argc; ++i)
		arg_vec.push_back(argv[i]);

	std::map<std::string, std::vector<ArgumentParser::ArgumentType> > args_syntax;
	args_syntax["--test"] = std::vector<ArgumentParser::ArgumentType>(0);
	args_syntax["--fuzz"] = std::vector<ArgumentParser::ArgumentType>(2, ArgumentParser::ArgumentType_string); // two string args
	args_syntax["--astfuzz"] = std::vector<ArgumentParser::ArgumentType>(2, ArgumentParser::ArgumentType_string); // two string args
	args_syntax["--perftest"] = std::vector<ArgumentParser::ArgumentType>(0);

	ArgumentParser args(arg_vec, args_syntax);

	if(args.isArgPresent("--test"))
	{
#if BUILD_TESTS
		LanguageTests::run();
		return 0;
#else
		std::cerr << "BUILD_TESTS not enabled, can't run tests." << std::endl;
		return 1;
#endif
	}
	else if(args.isArgPresent("--fuzz"))
	{
#if BUILD_TESTS
		// e.g. --fuzz N:/winter/trunk d:/fuzz_output
		fuzzTests(
			args.getArgStringValue("--fuzz", 0), // fuzzer input dir
			args.getArgStringValue("--fuzz", 1) // fuzzer output dir
		);
#endif
		return 0;
	}
	else if(args.isArgPresent("--astfuzz"))
	{
#if BUILD_TESTS
		doASTFuzzTests(
			args.getArgStringValue("--astfuzz", 0), // fuzzer input dir
			args.getArgStringValue("--astfuzz", 1) // fuzzer output dir
		);
#endif
		return 0;
	}
	else if(args.isArgPresent("--perftest"))
	{
		PerfTests::run();
		return 0;
	}

	try
	{
		std::string filecontents;
		FileUtils::readEntireFile(argv[1], filecontents);

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(SourceBufferRef(new SourceBuffer(argv[1], filecontents)));

		MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		VirtualMachine vm(vm_args);


		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeVRef>());
		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);


		void* f = vm.getJittedFunction(mainsig);

		// cast to correct type
		float_void_func mainf = (float_void_func)f;

		std::cout << "Calling JIT'd function..." << std::endl;

		// Call the JIT'd function
		const float result = mainf();

		std::cout << "JIT'd function returned " << result << std::endl;


		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		assert(maindef->built_llvm_function);
		ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();

		std::cout << "Program returned " << retval->toString() << std::endl;


		assert(vmstate.argument_stack.empty());
		//assert(vmstate.let_stack.empty());
		assert(vmstate.func_args_start.empty());
		//assert(vmstate.let_stack_start.empty());
		//assert(vmstate.working_stack.empty());
		

		/*for(unsigned int i=0; i<root->children.size(); ++i)
		{
			FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(root->children[i].getPointer());

			if(def->sig.name == "main")
			{
				def->exec(vmstate);
			}
		}*/
	}
	catch(Winter::BaseException& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

#ifdef _WIN32
	const bool found_mem_leaks = _CrtDumpMemoryLeaks() != 0; // == TRUE;
	if(found_mem_leaks)
	{
		std::cout << "***************** Memory leak(s) detected! **************" << std::endl;
	}
	else
		std::cout << "------------ No memory leaks detected. -----------" << std::endl;
#endif


	return 0;
}
