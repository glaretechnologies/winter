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


class CapturedVar
{
public:
	CapturedVar();

	void print(int depth, std::ostream& s) const;

	enum CapturedVarType
	{
		Let,
		Arg,
		Captured // we are capturing a captured var from an enclosing lambda expression :)
	};

	TypeRef type() const;


	CapturedVarType vartype;
	int arg_index; // Argument index
	//int let_frame_offset; // how many let blocks we need to ignore before we get to the let block with the var we are bound to.
	int free_index;
	int let_var_index;


	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetASTNode* bound_let_node;
	FunctionDefinition* enclosing_lambda;
};


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
		FunctionArg(TypeVRef type_, const std::string& n) : type(type_), name(n)/*, referenced(true)*/ {}
		TypeVRef type;
		std::string name;

		//bool isReferenced() const { return referenced; }
		//bool referenced; // Is this argument bound to by a variable in the function body?  Will be conservatively set to true if unknown.
		//int ref_count;
	};

	
	FunctionDefinition(const SrcLocation& src_loc, int order_num, const std::string& name, const std::vector<FunctionArg>& args, 
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		const BuiltInFunctionImplRef& impl
	);
	
	~FunctionDefinition();

	TypeRef returnType() const;
	

	//bool use_captured_vars; // Set to true if this is an anonymous function, in which case we will always pass it in a closure.
	std::vector<CapturedVar> captured_vars; // For when parsing anon functions

	// Types that are captured by any lambda expressions in this function definition.
	std::set<TypeRef> captured_var_types;


	virtual ValueRef invoke(VMState& vmstate);
	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;

	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;

	bool isGenericFunction() const; // true if it is parameterised by type.
	bool isExternalFunction() const { return external_function.nonNull(); }

	// with_captured_var_struct_ptr: add a ptr to a captured var struct as the last argument.
	// This will be needed for when a global function is passed as a first-class function argument, e.g. when the top-level function definition square() is passed to map().
	llvm::Function* buildLLVMFunction(
		llvm::Module* module,
		const PlatformUtils::CPUInfo& cpu_info,
		bool hidden_voidptr_arg, 
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
		//bool hidden_voidptr_arg
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
private:
};


typedef Reference<FunctionDefinition> FunctionDefinitionRef;


} // end namespace Winter

