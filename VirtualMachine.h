/*=====================================================================
VirtualMachine.h
----------------
Copyright Glare Technologies Limited 2018 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#pragma once


#include "wnt_ExternalFunction.h"
#include "wnt_SourceBuffer.h"
#include "wnt_ASTNode.h"
#include "Linker.h"
#include <utils/Reference.h>
#include <utils/ThreadSafeRefCounted.h>
#include <unordered_map>
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


#if defined(_WIN32)
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
	VMConstructionArgs() : allow_unsafe_operations(false), emit_trace_code(false), build_llvm_code(true), floating_point_literals_default_to_double(true), 
		try_coerce_int_to_double_first(true), real_is_double(true), opencl_double_support(true), comments_in_opencl_output(true), emit_in_bound_asserts(false), emit_opencl_printf_calls(true),
		small_code_model(false) {}

	std::vector<ExternalFunctionRef> external_functions;
	std::vector<SourceBufferRef> source_buffers;
	std::vector<FunctionSignature> entry_point_sigs;

	std::vector<FunctionDefinitionRef> preconstructed_func_defs;//TEMP
	bool allow_unsafe_operations; // If this is true, in-bounds proof requirements of elem() etc.. are disabled.
	bool emit_trace_code; // If this is true, information is printed out to std out as the program executes.
	std::vector<Reference<FunctionRewriter> > function_rewriters;

	bool build_llvm_code; // JIT compile executable code with LLVM.  Can be set to false when e.g. you just want OpenCL code.

	// If true, literals like "1.2" (without an 'f' suffix) are interpreted as doubles.  Otherwise they are interpreted as floats.
	bool floating_point_literals_default_to_double; // true by default.

	// If true, an expression like sqrt(9) will call the sqrt(double), otherwise it will call sqrt(float)
	bool try_coerce_int_to_double_first; // true by default

	// If true, 'real' is treated as an alias for double, otherwise as an alias for 'float'.
	bool real_is_double; // True by default.

	// If true, some built-in OpenCL support functions that use double, such as print(double), are compiled.
	bool opencl_double_support; // True by default.

	// If true, some (hopefully) helpful comments are added to to OpenCL C output.
	bool comments_in_opencl_output; // True by default.

	// If true, emit winterAssert() calls that array indices are in-bounds.
	bool emit_in_bound_asserts; // False by default

	// if true, printf calls will be emitted in the body of print() methods.
	bool emit_opencl_printf_calls; // True by default.

	// If true, the small code model for code generation is used.
	bool small_code_model; // False by default.
};


struct ProgramStats
{
	uint64 num_heap_allocation_calls;
	uint64 num_closure_allocations; // Num closures allocated, either on stack or on the heap.
	uint64 num_free_vars; // Num free variables stored during closure allocation.
};


class VirtualMachine : public ThreadSafeRefCounted
{
public:
	VirtualMachine(const VMConstructionArgs& args); // throws BaseException
	~VirtualMachine();


	static void init(); // Initialise LLVM.  Throws Winter::BaseException on failure.
	static void shutdown(); // Calls llvm_shutdown()

	Reference<FunctionDefinition> findMatchingFunction(const FunctionSignature& sig);

	void* getJittedFunction(const FunctionSignature& sig);
	void* getJittedFunctionByName(const std::string& name);

	bool isFunctionCalled(const std::string& name);

	struct OpenCLCCode
	{
		std::string struct_def_code;
		std::string function_code;
	};
	struct BuildOpenCLCodeArgs
	{
		std::vector<TupleTypeRef> tuple_types_used; // For tuples used with external funcs, to make sure the OpenCL definitions for these are emitted.
	};

	OpenCLCCode buildOpenCLCode(const BuildOpenCLCodeArgs& args) const;
	std::string buildOpenCLCodeCombined(const BuildOpenCLCodeArgs& args) const;

	const ProgramStats& getProgramStats() const { return stats; }

private:
	void loadSource(const VMConstructionArgs& args, const std::vector<SourceBufferRef>& s, const std::vector<FunctionDefinitionRef>& preconstructed_func_defs);
	void build(const VMConstructionArgs& args);
	void addExternalFunction(const ExternalFunctionRef& f, llvm::LLVMContext& context, llvm::Module& module);
	void compileToNativeAssembly(llvm::Module* mod, const std::string& filename);
	void verifyModule(llvm::Module* mod);
	bool doInliningPass();
	bool doDeadCodeEliminationPass();

	struct AllocationBlock
	{
		uint8_t* alloced_mem;
		size_t size;
	};

	friend class WinterMemoryManager;

	std::vector<ExternalFunctionRef> external_functions;
	Linker linker;
	llvm::LLVMContext* llvm_context;
	llvm::Module* llvm_module;
	llvm::ExecutionEngine* llvm_exec_engine;
	llvm::TargetMachine* target_machine;
	std::string triple;
	std::unordered_map<std::string, void*> func_map;

	std::vector<TypeVRef> named_types_ordered;

	VMConstructionArgs vm_args;

	ProgramStats stats;

	std::vector<AllocationBlock> jit_mem_blocks;

	std::unordered_map<const llvm::Function*, uint64> stack_sizes;
};


typedef Reference<VirtualMachine> VirtualMachineRef;


} // end namespace Winter
