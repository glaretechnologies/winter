/*=====================================================================
VirtualMachine.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#pragma once


#include "wnt_ExternalFunction.h"
#include "utils/reference.h"
#include "wnt_ASTNode.h"
#include "Linker.h"
#include <string>
namespace llvm
{
	class LLVMContext;
	class Module;
	class ExecutionEngine;
}


namespace Winter
{


class FunctionSignature;
class FunctionDefinition;


#define WINTER_JIT_CALLING_CONV __cdecl


/*=====================================================================
VirtualMachine
-------------------

=====================================================================*/
class VirtualMachine
{
public:
	VirtualMachine();
	~VirtualMachine();

	//TEMP
	std::vector<ExternalFunction> external_functions;
	
	void loadSource(const std::string& s);


	Reference<FunctionDefinition> findMatchingFunction(const FunctionSignature& sig);

	void* getJittedFunction(const FunctionSignature& sig);

private:
	void addExternalFunction(const ExternalFunction& f, llvm::LLVMContext& context, llvm::Module& module);

	ASTNodeRef rootref;
	Linker linker;
	llvm::LLVMContext* llvm_context;
	llvm::Module* llvm_module;
	llvm::ExecutionEngine* llvm_exec_engine;

};


} // end namespace Winter
