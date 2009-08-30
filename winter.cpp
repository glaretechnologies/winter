// Copyright 2009 Nicholas Chapman

#include <iostream>
#include <cassert>
#include <fstream>
#include "../../indigosvn/trunk/utils/FileUtils.h"
#include "Lexer.h"
#include "TokenBase.h"
#include "LangParser.h"
#include "ASTNode.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"

#if USE_LLVM
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetSelect.h"
#endif
using namespace Winter;


int main(int argc, char** argv)
{
	if(argc < 2)
	{
		return 1;
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
			TraversalPayload payload;
			payload.linker = NULL;
			payload.operation = TraversalPayload::BindVariables;
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}

		// Link functions
		Linker linker;
		linker.addFunctions(*root);
		//linker.linkFunctions(*root);
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload;
			payload.operation = TraversalPayload::LinkFunctions;
			payload.linker = &linker;
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}

		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload;
			payload.linker = NULL;
			payload.operation = TraversalPayload::TypeCheck;
			root->traverse(payload, stack);
			assert(stack.size() == 0);
		}


		rootref->print(0, std::cout);


		// Get main function
		FunctionSignature mainsig("main", std::vector<TypeRef>());
		Linker::FuncMapType::iterator res = linker.functions.find(mainsig);
		if(res == linker.functions.end())
			throw BaseException("Could not find " + mainsig.toString());
		Reference<FunctionDefinition> maindef = (*res).second;
		//if(!(*maindef->return_type == *TypeRef(new String())))
		//	throw BaseException("main must return string.");

		//TEMP:
		/*{
		llvm::Module* module = new llvm::Module("WinterModule");
		llvm::ExistingModuleProvider* MP = new llvm::ExistingModuleProvider(module);

		llvm::InitializeNativeTarget();

		std::string error_str;
		llvm::ExecutionEngine* EE = llvm::ExecutionEngine::createJIT(MP, &error_str);



		for(unsigned int i = 0; i<root->func_defs.size(); ++i)
		{
			llvm::Function* func = root->func_defs[i]->buildLLVMFunction(module);
		}

		error_str;
		const bool ver_errors = llvm::verifyModule(*module, llvm::ReturnStatusAction, &error_str);
		assert(!ver_errors);

		{
			module->dump();
			std::ofstream f("module.txt");
			module->print(f, NULL);
		}


		}*/
		

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		Value* retval = maindef->invoke(vmstate);

		vmstate.func_args_start.pop_back();

		//IntValue* intval = dynamic_cast<IntValue*>(retval);
		//StringValue* intval = dynamic_cast<StringValue*>(retval);
		//StructureValue* val = dynamic_cast<StructureValue*>(retval);
		FloatValue* val = dynamic_cast<FloatValue*>(retval);
		assert(val);

		std::cout << "Program returned " << val->value << std::endl;

		delete val;

		assert(vmstate.argument_stack.empty());
		assert(vmstate.let_stack.empty());
		assert(vmstate.func_args_start.empty());
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
