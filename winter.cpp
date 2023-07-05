/*=====================================================================
winter.cpp
----------
Copyright Glare Technologies Limited 2023 -
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
#include "utils/StringUtils.h"
#include "utils/ConPrint.h"
#include "utils/ArgumentParser.h"
#include <cassert>
#include <fstream>


typedef float(*floatVoidFuncType)();


int main(int argc, char** argv)
{
	Clock::init();

	if(argc < 2)
	{
		conPrint("Usage: \nwinter program.win\nor\nwinter --test");
		return 1;
	}

	Winter::VirtualMachine::init();

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
		conPrint("Running Winter tests...");
		Winter::LanguageTests::run();
		return 0;
#else
		stdErrPrint("BUILD_TESTS not enabled, can't run tests.");
		return 1;
#endif
	}
	else if(args.isArgPresent("--fuzz"))
	{
#if BUILD_TESTS
		// e.g. --fuzz N:/winter/trunk d:/fuzz_output
		Winter::fuzzTests(
			args.getArgStringValue("--fuzz", 0), // fuzzer input dir
			args.getArgStringValue("--fuzz", 1) // fuzzer output dir
		);
#endif
		return 0;
	}
	else if(args.isArgPresent("--astfuzz"))
	{
#if BUILD_TESTS
		Winter::doASTFuzzTests(
			args.getArgStringValue("--astfuzz", 0), // fuzzer input dir
			args.getArgStringValue("--astfuzz", 1) // fuzzer output dir
		);
#endif
		return 0;
	}
	else if(args.isArgPresent("--perftest"))
	{
		Winter::PerfTests::run();
		return 0;
	}

	try
	{
		std::string filecontents;
		FileUtils::readEntireFile(argv[1], filecontents);

		const Winter::FunctionSignature main_sig(/*name=*/"main", /*param_types=*/std::vector<Winter::TypeVRef>());

		Winter::VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new Winter::SourceBuffer(argv[1], filecontents));
		vm_args.entry_point_sigs.push_back(main_sig); // It's important to set this, so main() is not optimised away as dead code.
		Winter::MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		Winter::VirtualMachine vm(vm_args);

		// Get main function
		Reference<Winter::FunctionDefinition> maindef = vm.findMatchingFunction(main_sig);
		if(maindef.isNull())
			throw Winter::BaseException("Failed to find function with signature " + main_sig.toString());

		// Check return type
		Winter::TypeVRef float_type = new Winter::Float();
		if(*maindef->returnType() != *float_type)
			throw Winter::BaseException("main must return a float");

		void* f = vm.getJittedFunction(main_sig);

		// cast to correct type
		floatVoidFuncType jitted_main = (floatVoidFuncType)f;

		conPrint("Calling JIT'd function...");

		// Call the JIT'd function
		const float result = jitted_main();

		conPrint("JIT'd function returned " + toString(result));


		// Call the interpreted function
		Winter::VMState vmstate;
		vmstate.func_args_start.push_back(0); // Push index at which the function arguments start

		Winter::ValueRef retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();

		conPrint("Interpreted main returned " + retval->toString());

		assert(vmstate.argument_stack.empty());
		assert(vmstate.func_args_start.empty());
	}
	catch(Winter::ExceptionWithPosition& e)
	{
		stdErrPrint(e.messageWithPosition());
		return 1;
	}
	catch(Winter::BaseException& e)
	{
		stdErrPrint(e.what());
		return 1;
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		stdErrPrint(e.what());
		return 1;
	}

#ifdef _WIN32
	const bool found_mem_leaks = _CrtDumpMemoryLeaks() != 0; // == TRUE;
	if(found_mem_leaks)
	{
		conPrint("***************** Memory leak(s) detected! **************");
	}
	else
		conPrint("------------ No memory leaks detected. -----------");
#endif


	return 0;
}
