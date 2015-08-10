/*=====================================================================
FunctionDefinition.h
-------------------
Copyright Nicholas Chapman
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
		Captured, // we are capturing a captured var from an enclosing lambda expression :)
		Removed // deleted. Left in the vector so that indices don't change.
	};

	TypeRef type() const;


	CapturedVarType vartype;
	int index; // Argument index, or index of let variable inside let block, or index of captured var.
	//int let_frame_offset; // how many let blocks we need to ignore before we get to the let block with the var we are bound to.


	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetASTNode* bound_let_node;
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
		FunctionArg()/* : referenced(true)*/ {}
		FunctionArg(TypeRef type_, const std::string& n) : type(type_), name(n)/*, referenced(true)*/ {}
		/*enum TypeKind
		{
			GENERIC_TYPE,
			CONCRETE_TYPE
		};

		TypeKind type_kind;*/
		TypeRef type;
		//int generic_type_param_index;
		std::string name;

		//bool isReferenced() const { return referenced; }

		//bool referenced; // Is this argument bound to by a variable in the function body?  Will be conservatively set to true if unknown.

		//int ref_count;
	};

	
	FunctionDefinition(const SrcLocation& src_loc, int order_num, const std::string& name, const std::vector<FunctionArg>& args, 
		//const vector<Reference<LetASTNode> >& lets,
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		const BuiltInFunctionImplRef& impl
	);
	
	~FunctionDefinition();

	TypeRef returnType() const;

	std::vector<FunctionArg> args;
	ASTNodeRef body;
	TypeRef declared_return_type;
	//TypeRef function_type;
	//vector<Reference<LetASTNode> > lets;

	FunctionSignature sig;
	BuiltInFunctionImplRef built_in_func_impl;
	ExternalFunctionRef external_function;

	bool need_to_emit_captured_var_struct_version; 

	//bool use_captured_vars; // Set to true if this is an anonymous function, in which case we will always pass it in a closure.
	std::vector<CapturedVar> captured_vars; // For when parsing anon functions

	// Types that are captured by any lambda expressions in this function definition.
	std::set<TypeRef> captured_var_types;


	virtual ValueRef invoke(VMState& vmstate);
	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;// { return function_type; }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	bool isGenericFunction() const; // true if it is parameterised by type.
	bool isExternalFunction() const { return external_function.nonNull(); }

	// with_captured_var_struct_ptr: add a ptr to a captured var struct as the last argument.
	// This will be needed for when a global function is passed as a first-class function argument, e.g. when the top-level function definition square() is passed to map().
	llvm::Function* buildLLVMFunction(
		llvm::Module* module,
		const PlatformUtils::CPUInfo& cpu_info,
		bool hidden_voidptr_arg, 
		const llvm::DataLayout/*TargetData*/* target_data,
		const CommonFunctions& common_functions,
		std::set<Reference<const Type>, ConstTypeRefLessThan>& destructors_called_types,
		ProgramStats& stats,
		bool emit_trace_code,
		bool with_captured_var_struct_ptr
		//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
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


	// NOTE: type() returns the 


	// llvm::Type* getClosureStructLLVMType(llvm::LLVMContext& context) const;
	TypeRef getFullClosureType() const;
	Reference<StructureType> getCapturedVariablesStructType() const;

	// If the function is return by value, returns winter_index, else returns winter_index + 1
	// as the zeroth index will be the sret pointer.
	int getLLVMArgIndex(int winter_index);

	int getCapturedVarStructLLVMArgIndex();


	llvm::Type* closure_type;

	llvm::Function* built_llvm_function;
	void* jitted_function;

	// If anon func is true, then we don't want to try and traverse to it by itself, but only when it's embedded
	// in the AST, so that vars can succesfully bind to the parent function.
	bool is_anon_func;

	int num_uses; // Conservative, >= than the actual num uses.

	int order_num; // Used for establishing an ordering between function definitions and named constants, to avoid circular references.
private:
	FunctionDefinition* alloc_func; // NOTE: make this just a ptr?
};


typedef Reference<FunctionDefinition> FunctionDefinitionRef;


} // end namespace Winter

