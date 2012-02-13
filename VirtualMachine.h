/*=====================================================================
VirtualMachine.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#pragma once


#include "wnt_ExternalFunction.h"
#include "wnt_SourceBuffer.h"
#include <utils/reference.h>
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


#if (defined(WIN32) || defined(WIN64))
#define WINTER_JIT_CALLING_CONV __cdecl
#else 
#define WINTER_JIT_CALLING_CONV
#endif


/*=====================================================================
VirtualMachine
-------------------

=====================================================================*/
class VMConstructionArgs
{
public:
	VMConstructionArgs() : env(NULL) {}
	std::vector<ExternalFunctionRef> external_functions;
	std::vector<SourceBufferRef> source_buffers;
	void* env;
};


class VirtualMachine
{
public:
	VirtualMachine(const VMConstructionArgs& args); // throws BaseException
	~VirtualMachine();


	static void shutdown(); // Calls llvm_shutdown()

	Reference<FunctionDefinition> findMatchingFunction(const FunctionSignature& sig);

	void* getJittedFunction(const FunctionSignature& sig);
	void* getJittedFunctionByName(const std::string& name);

private:
	void loadSource(const std::vector<SourceBufferRef>& s);
	void build();
	void addExternalFunction(const ExternalFunctionRef& f, llvm::LLVMContext& context, llvm::Module& module);
	void compileToNativeAssembly(llvm::Module* mod, const std::string& filename);

	std::vector<ExternalFunctionRef> external_functions;
	//ASTNodeRef rootref;
	Linker linker;
	llvm::LLVMContext* llvm_context;
	llvm::Module* llvm_module;
	llvm::ExecutionEngine* llvm_exec_engine;
	bool hidden_voidptr_arg;
	void* env;
};


} // end namespace Winter
