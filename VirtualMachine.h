/*=====================================================================
VirtualMachine.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#pragma once


#include "wnt_ExternalFunction.h"
#include "wnt_SourceBuffer.h"
#include "wnt_ASTNode.h"
#include "Linker.h"
#include <utils/Reference.h>
#include <string>
namespace llvm
{
	class LLVMContext;
	class Module;
	class ExecutionEngine;
	class TargetMachine;
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

// This class allows a user to rewrite the AST as they see fit.  Function rewriting is done immediately after parsing, before any other passes.
class FunctionRewriter : public RefCounted
{
public:
	virtual ~FunctionRewriter() {}

	virtual void rewrite(std::vector<FunctionDefinitionRef>& function_definitions, const SourceBufferRef& source_buffer) = 0;
};


class VMConstructionArgs
{
public:
	VMConstructionArgs() : env(NULL), allow_unsafe_operations(false)/*, add_opaque_env_arg(false)*/ {}
	std::vector<ExternalFunctionRef> external_functions;
	std::vector<SourceBufferRef> source_buffers;
	std::vector<FunctionSignature> entry_point_sigs;

	std::vector<FunctionDefinitionRef> preconstructed_func_defs;//TEMP
	void* env;
	bool allow_unsafe_operations; // If this is true, in-bounds proof requirements of elem() etc.. are disabled.

	std::vector<Reference<FunctionRewriter> > function_rewriters;
};


struct ProgramStats
{
	uint64 num_heap_allocation_calls;

};


class VirtualMachine
{
public:
	VirtualMachine(const VMConstructionArgs& args); // throws BaseException
	~VirtualMachine();


	static void init(); // Initialise LLVM
	static void shutdown(); // Calls llvm_shutdown()

	Reference<FunctionDefinition> findMatchingFunction(const FunctionSignature& sig);

	void* getJittedFunction(const FunctionSignature& sig);
	void* getJittedFunctionByName(const std::string& name);

	bool isFunctionCalled(const std::string& name);

	const std::string buildOpenCLCode();

	const ProgramStats& getProgramStats() const { return stats; }

private:
	void loadSource(const VMConstructionArgs& args, const std::vector<SourceBufferRef>& s, const std::vector<FunctionDefinitionRef>& preconstructed_func_defs);
	void build(const VMConstructionArgs& args);
	void addExternalFunction(const ExternalFunctionRef& f, llvm::LLVMContext& context, llvm::Module& module);
	void compileToNativeAssembly(llvm::Module* mod, const std::string& filename);

	std::vector<ExternalFunctionRef> external_functions;
	//ASTNodeRef rootref;
	Linker linker;
	llvm::LLVMContext* llvm_context;
	llvm::Module* llvm_module;
	llvm::ExecutionEngine* llvm_exec_engine;
	llvm::TargetMachine* target_machine;
	bool hidden_voidptr_arg;
	void* env;
	std::string triple;
	std::map<std::string, void*> func_map;

	std::vector<TypeRef> named_types_ordered;

	ProgramStats stats;
};


} // end namespace Winter
