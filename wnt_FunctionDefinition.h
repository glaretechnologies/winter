/*=====================================================================
FunctionDefinition.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:39 +0100
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"


namespace Winter
{


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
		FunctionArg(){}
		FunctionArg(TypeRef type_, const string& n) : type(type_), name(n) {}
		/*enum TypeKind
		{
			GENERIC_TYPE,
			CONCRETE_TYPE
		};

		TypeKind type_kind;*/
		TypeRef type;
		//int generic_type_param_index;
		string name;
	};

	
	FunctionDefinition(const std::string& name, const std::vector<FunctionArg>& args, 
		//const vector<Reference<LetASTNode> >& lets,
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		BuiltInFunctionImpl* impl);
	
	~FunctionDefinition();

	TypeRef returnType() const;

	vector<FunctionArg> args;
	ASTNodeRef body;
	TypeRef declared_return_type;
	//TypeRef function_type;
	//vector<Reference<LetASTNode> > lets;

	FunctionSignature sig;
	BuiltInFunctionImpl* built_in_func_impl;
	ExternalFunctionRef external_function;

	bool use_captured_vars; // Set to true if this is an anonymous function, in which case we will always pass it in a closure.
	vector<CapturedVar> captured_vars; // For when parsing anon functions


	virtual ValueRef invoke(VMState& vmstate);
	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionDefinitionType; }
	virtual TypeRef type() const;// { return function_type; }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	bool isGenericFunction() const; // true if it is parameterised by type.
	bool isExternalFunction() const { return external_function.nonNull(); }

	llvm::Function* buildLLVMFunction(
		llvm::Module* module,
		const PlatformUtils::CPUInfo& cpu_info,
		bool hidden_voidptr_arg
		//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	);

	llvm::Function* getOrInsertFunction(
		llvm::Module* module,
		bool hidden_voidptr_arg
	) const;

	// llvm::Type* getClosureStructLLVMType(llvm::LLVMContext& context) const;
	TypeRef getCapturedVariablesStructType() const;

	// If the function is return by value, returns winter_index, else returns winter_index + 1
	// as the zeroth index will be the sret pointer.
	int getLLVMArgIndex(int winter_index);

	int getCapturedVarStructLLVMArgIndex();


	llvm::Type* closure_type;

	llvm::Function* built_llvm_function;
	void* jitted_function;


private:

};


typedef Reference<FunctionDefinition> FunctionDefinitionRef;


} // end namespace Winter

