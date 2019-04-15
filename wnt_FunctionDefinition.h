/*=====================================================================
FunctionDefinition.h
--------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-04-25 19:15:39 +0100
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
#include "BuiltInFunctionImpl.h"
#include <string>


namespace Winter
{


const std::string makeSafeStringForFunctionName(const std::string& s);
struct ProgramStats;
class Variable;


/*=====================================================================
FunctionDefinition
-------------------

=====================================================================*/
class FunctionDefinition : public ASTNode
{
public:
	class FunctionArg
	{
	public:
		FunctionArg(TypeVRef type_, const std::string& n) : type(type_), name(n), ref_count(0) {}
		TypeVRef type;
		std::string name;

		int ref_count; // number of references in the function body to the argument.  Built by CountArgumentRefs pass.
	};

	
	FunctionDefinition(const SrcLocation& src_loc, int order_num, const std::string& name, const std::vector<FunctionArg>& args, 
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		const BuiltInFunctionImplRef& impl
	);
	
	~FunctionDefinition();

	TypeRef returnType() const;
	
	
	virtual ValueRef invoke(VMState& vmstate);
	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;

	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

	bool isGenericFunction() const; // true if it is parameterised by type.
	bool isExternalFunction() const { return external_function.nonNull(); }

	// with_captured_var_struct_ptr: add a ptr to a captured var struct as the last argument.
	// This will be needed for when a global function is passed as a first-class function argument, e.g. when the top-level function definition square() is passed to map().
	llvm::Function* buildLLVMFunction(
		llvm::Module* module,
		const PlatformUtils::CPUInfo& cpu_info,
		const llvm::DataLayout* data_layout,
		const CommonFunctions& common_functions,
		std::set<VRef<const Type>, ConstTypeVRefLessThan>& destructors_called_types,
		ProgramStats& stats,
		bool emit_trace_code,
		bool with_captured_var_struct_ptr
	);

	// use_cap_var_struct_ptr = false
	llvm::Function* getOrInsertFunction(
		llvm::Module* module
	) const;

	llvm::Function* getOrInsertFunction(
		llvm::Module* module,
		bool use_cap_var_struct_ptr
	) const;

	// Conservative, >= than the actual num uses.
	inline int getNumUses() const { return num_uses; }


	// llvm::Type* getClosureStructLLVMType(llvm::LLVMContext& context) const;
	TypeRef getFullClosureType() const;
	VRef<StructureType> getCapturedVariablesStructType() const;

	// If the function is return by value, returns winter_index, else returns winter_index + 1
	// as the zeroth index will be the sret pointer.
	int getLLVMArgIndex(int winter_index);

	int getCapturedVarStructLLVMArgIndex();

	// Get index of this variable (which must be in the set of free variables for this lambda) in the list of free variables.
	size_t getFreeIndexForVar(const Variable* var) const;

	SmallVector<Variable*, 8> getUniqueFreeVarList() const;

	// Depending on the argument type, will return something like
	// const SomeStruct* const arg_name
	// or
	// const int arg_name
	static std::string openCLCArgumentCode(EmitOpenCLCodeParams& params, const TypeVRef& arg_type, const std::string& arg_name);


	std::vector<FunctionArg> args;
	ASTNodeRef body; // Body expression.
	TypeRef declared_return_type; // Will be null if no return type was declared.

	std::vector<std::string> generic_type_param_names;

	FunctionSignature sig;
	BuiltInFunctionImplRef built_in_func_impl;
	ExternalFunctionRef external_function;

	bool need_to_emit_captured_var_struct_version; 

	llvm::Function* built_llvm_function; // This pointer is set to a non-null value after buildLLVMFunction() has been called on this function.

	// If anon func is true, then we don't want to try and traverse to it by itself, but only when it's embedded
	// in the AST, so that vars can succesfully bind to the parent function.
	bool is_anon_func;

	int num_uses; // Conservative, >= than the actual num uses.

	int order_num; // Used for establishing an ordering between function definitions and named constants, to avoid circular references.

	bool noinline; // Optional attribute.  False by default.  If true, function won't be inlined.

	int64 llvm_reported_stack_size; // -1 if this information is not returned by LLVM

	// For lambda expressions (anon functions), this is the set of variables defined in the anon function that are free, e.g. bound to a node outside the anon function.
	typedef std::set<Variable*> FreeVariableSet;
	FreeVariableSet free_variables;

	// Types that are captured by any lambda expressions in this function definition, e.g. types of any free variables.
	std::set<TypeRef> captured_var_types;
private:
};


typedef Reference<FunctionDefinition> FunctionDefinitionRef;


} // end namespace Winter

