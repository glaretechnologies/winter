/*=====================================================================
VirtualMachine.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#include "VirtualMachine.h"


#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/FileUtils.h"
#include "utils/stringutils.h"
#include "Lexer.h"
#include "TokenBase.h"
#include "LangParser.h"
#include "ASTNode.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "LanguageTests.h"
#include "VirtualMachine.h"
#if USE_LLVM
#include "llvm/Module.h"
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


namespace Winter
{

	
VirtualMachine::VirtualMachine()
{
	this->llvm_context = new llvm::LLVMContext();
	
	this->llvm_module = new llvm::Module("WinterModule", *this->llvm_context);

	llvm::InitializeNativeTarget();

	// NOTE: ExecutionEngine takes ownership of the module if createJIT is successful.
	std::string error_str;
	this->llvm_exec_engine = llvm::ExecutionEngine::createJIT(
		this->llvm_module, 
		&error_str
	);

	assert(this->llvm_exec_engine);
	assert(error_str.empty());
}


VirtualMachine::~VirtualMachine()
{
	// llvm_exec_engine will delete llvm_module.

	delete this->llvm_exec_engine;

	delete llvm_context;
}


static void optimiseFunctions(llvm::FunctionPassManager& fpm, llvm::Module* module, bool verbose)
{
	for(llvm::Module::iterator i = module->begin(); i != module->end(); ++i)
	{
		if(!i->isIntrinsic())
		{
			fpm.run(*i);
		}
	}
}



void VirtualMachine::loadSource(const std::string& source)
{
	std::vector<Reference<TokenBase> > tokens;
	Lexer::process(source, tokens);

	LangParser parser;
	this->rootref = parser.parseBuffer(tokens, source.c_str());


	BufferRoot* root = dynamic_cast<BufferRoot*>(rootref.getPointer());

	// Bind variables
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		root->traverse(payload, stack);
		assert(stack.size() == 0);
	}

	// Link functions
	//Linker linker;
	linker.addFunctions(*root);
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



	linker.buildLLVMCode(this->llvm_module);

	string error_str;
	const bool ver_errors = llvm::verifyModule(
		*this->llvm_module, 
		llvm::ReturnStatusAction, // Action to take
		&error_str
	);
	assert(!ver_errors);


	const bool optimise = true;
	const bool verbose = true;

	// Do LLVM optimisatons
	if(optimise)
	{
		llvm::FunctionPassManager fpm(this->llvm_module);

		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
		fpm.add(new llvm::TargetData(*this->llvm_exec_engine->getTargetData()));

		// Do simple "peephole" optimizations and bit-twiddling optzns.
		//TEMP fpm.add(llvm::createInstructionCombiningPass());
		// Reassociate expressions.
		fpm.add(llvm::createReassociatePass());
		// Eliminate Common SubExpressions.
		fpm.add(llvm::createGVNPass());
		// Simplify the control flow graph (deleting unreachable blocks, etc).
		fpm.add(llvm::createCFGSimplificationPass());

		fpm.add(llvm::createPromoteMemoryToRegisterPass());

		llvm::PassManager pm;
		pm.add(new llvm::TargetData(*this->llvm_exec_engine->getTargetData()));
		pm.add(llvm::createFunctionInliningPass());



/*		// Build list of functions with external linkage (entry points)
		std::vector<const char*> export_list;
		std::vector<std::string> export_list_strings;
		for(unsigned int i=0; i<entry_point_function_sigs.size(); ++i)
		{
			//if(compiled_functions.count(entry_point_function_sigs[i]) == 0)
			//	throw VMExcep("entry_point_function_sigs");

			if(compiled_functions.count(entry_point_function_sigs[i]) > 0)
			{
				if(compiled_functions[entry_point_function_sigs[i]]->llvm_func == NULL)
					throw VMInternalExcep("compiled_functions[entry_point_function_sigs[i]]->llvm_func == NULL");

				export_list_strings.push_back(compiled_functions[entry_point_function_sigs[i]]->llvm_func->getName());
			}
		}

		for(unsigned int i=0; i<export_list_strings.size(); ++i)
			export_list.push_back(export_list_strings[i].c_str());

		pm.add(llvm::createInternalizePass(export_list));
		
		pm.add(llvm::createGlobalDCEPass()); // Delete unreachable internal functions / global vars
*/
		optimiseFunctions(fpm, this->llvm_module, verbose);

		// Run module optimisation.  This may remove some functions, so we have to be careful accessing llvm functions from now on.
		{
		if(verbose)
			std::cout << "Optimising module... " << std::endl;
		const bool changed = pm.run(*this->llvm_module);
		if(verbose)
			std::cout << "Done. (changed = " + toString(changed) + ")" << std::endl;
		}

		optimiseFunctions(fpm, this->llvm_module, verbose);
	}


	{
		// Dump to stdout
		this->llvm_module->dump();

		std::string errorinfo;
		llvm::raw_fd_ostream f(
			"module.txt",
			errorinfo
		);
		this->llvm_module->print(f, NULL);
	}


}


Reference<FunctionDefinition> VirtualMachine::findMatchingFunction(
	const FunctionSignature& sig)
{
	return linker.findMatchingFunction(sig);
}


void* VirtualMachine::getJittedFunction(const FunctionSignature& sig)
{
	FunctionDefinitionRef func = linker.findMatchingFunction(sig);
	
	return this->llvm_exec_engine->getPointerToFunction(
		func->built_llvm_function
	);
}



} // end namespace Winter