// Copyright 2009 Nicholas Chapman

#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/FileUtils.h"
#include "Lexer.h"
#include "TokenBase.h"
#include "LangParser.h"
#include "ASTNode.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "LanguageTests.h"

#if USE_LLVM
#include "llvm/Module.h"
//#include "llvm/ModuleProvider.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#endif
using namespace Winter;


int main(int argc, char** argv)
{
	if(argc < 2)
	{
		return 1;
	}

	if(std::string(argv[1]) == "--test")
	{
		LanguageTests::run();
		return 0;
	}

	try
	{
		std::string filecontents;
		FileUtils::readEntireFile(argv[1], filecontents);

		std::vector<Reference<TokenBase> > tokens;
		Lexer::process(filecontents, tokens);

		LangParser parser;
		ASTNodeRef rootref = parser.parseBuffer(tokens, filecontents.c_str());


		BufferRoot* root = dynamic_cast<BufferRoot*>(rootref.getPointer());

		// Bind variables
		//root->bindVariables(std::vector<ASTNode*>());
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::BindVariables);
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}

		// Link functions
		Linker linker;
		linker.addFunctions(*root);
		//linker.linkFunctions(*root);
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::LinkFunctions);
			payload.linker = &linker;
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}

		// TypeCheck
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::TypeCheck);
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}


		rootref->print(0, std::cout);


		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>());
		Reference<FunctionDefinition> maindef = linker.findMatchingFunction(mainsig);
		//Linker::FuncMapType::iterator res = linker.functions.find(mainsig);
		//if(res == linker.functions.end())
		//	throw BaseException("Could not find " + mainsig.toString());
		//Reference<FunctionDefinition> maindef = (*res).second;
		//if(!(*maindef->return_type == *TypeRef(new String())))
		//	throw BaseException("main must return string.");

		//TEMP:
		llvm::LLVMContext context;
		llvm::Module* module = new llvm::Module("WinterModule", context);

		llvm::InitializeNativeTarget();

		std::string error_str;
		llvm::ExecutionEngine* EE = llvm::ExecutionEngine::createJIT(
			module, 
			&error_str
		);



		/*for(unsigned int i = 0; i<root->func_defs.size(); ++i)
		{
			if(!root->func_defs[i]->isGenericFunction())
			{
				llvm::Function* func = root->func_defs[i]->buildLLVMFunction(module);
			}
		}*/
		linker.buildLLVMCode(module);

		error_str;
		const bool ver_errors = llvm::verifyModule(*module, llvm::ReturnStatusAction, &error_str);
		assert(!ver_errors);

		{
			module->dump();
			//std::ofstream f("module.txt");
			std::string errorinfo;
			llvm::raw_fd_ostream f(
				"module.txt",
				errorinfo
			);
			module->print(f, NULL);
		}

		assert(maindef->built_llvm_function);
		void* f = EE->getPointerToFunction(maindef->built_llvm_function);



		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();

		//IntValue* intval = dynamic_cast<IntValue*>(retval);
		//StringValue* intval = dynamic_cast<StringValue*>(retval);
		//StructureValue* val = dynamic_cast<StructureValue*>(retval);
		//ArrayValue* val = dynamic_cast<ArrayValue*>(retval);
		/*FloatValue* val = dynamic_cast<FloatValue*>(retval);
		if(!val)
		{
			std::cerr << "main() Return value was of unexpected type." << std::endl;
		}
		assert(val);*/

		std::cout << "Program returned " << retval->toString() << std::endl;

		delete retval;

		assert(vmstate.argument_stack.empty());
		assert(vmstate.let_stack.empty());
		assert(vmstate.func_args_start.empty());
		assert(vmstate.let_stack_start.empty());
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
	catch(Winter::LexerExcep& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	catch(Winter::LangParserExcep& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	const bool found_mem_leaks = _CrtDumpMemoryLeaks() != 0; // == TRUE;
	if(found_mem_leaks)
	{
		std::cout << "***************** Memory leak(s) detected! **************" << std::endl;
	}
	else
		std::cout << "------------ No memory leaks detected. -----------" << std::endl;


	return 0;
}


void doTestAssert(bool expr, const char* test, long line, const char* file)
{
	if(!expr)
	{
		std::cerr << "Test Assertion Failed: " << file << ", line " << line << ":\n" << test << std::endl;
		assert(0);
		exit(0);
	}
}


void conPrint(const std::string& s)
{
	std::cout << s << std::endl;
}
