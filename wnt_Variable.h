/*=====================================================================
wnt_Variable.h
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
#include <set>
#include <vector>
namespace llvm { class Value; };


namespace Winter
{


/*=====================================================================
Variable
--------
Variable AST node.
=====================================================================*/
class Variable : public ASTNode
{
public:
	// The kind of AST nodes that a variable can be bound to.
	enum BindingType
	{
		BindingType_Unbound,
		BindingType_Let,			// Bound to a let variable
		BindingType_Argument,		// Bound to an argument of the most-tightly enclosing function
		BindingType_GlobalDef,		// Bound to a globally defined (program scope) function
		BindingType_NamedConstant	// Bound to a named constant
	};

	Variable(const std::string& name, const SrcLocation& loc);
	~Variable();

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

	// Are the variables 'a' and 'b' bound to the same AST node?
	static bool boundToSameNode(const Variable& a, const Variable& b);
private:
	void bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack);
public:

	std::string name; // variable name.

	BindingType binding_type; // one of BindingType above.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of.  Used in BindingType_Argument and BindingType_GlobalDef cases.
	LetASTNode* bound_let_node; // Used in binding_type == BindingType_Let case.
	NamedConstant* bound_named_constant; // Used in binding_type == BindingType_NamedConstant case.

	int arg_index; // index in function argument list, or index of captured var.  Used in binding_type == BindingType_Argument case.
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.  Used in binding_type == BindingType_Let case.

	std::vector<FunctionDefinition*> enclosing_lambdas; // Enclosing lambda expressions (anonymous functions) for which this variable is free
	// (not bound to anything in the lambda expression).
	// Lambda at rightmost index (back()) is the most lexically tightly enclosing.

	std::set<FunctionDefinition*> lambdas; // Lambdas for which this variable is in the lambda's free_variables set.  Used for reference management.
};


typedef Reference<Variable> VariableRef;


} // end namespace Winter
