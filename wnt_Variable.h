/*=====================================================================
wnt_Variable.h
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
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
	enum BindingType
	{
		UnboundVariable,
		LetVariable,					// Bound to a let variable
		ArgumentVariable,				// Bound to an argument of the most-tightly enclosing function
		BoundToGlobalDefVariable,		// Bound to a globally defined (program scope) function
		BoundToNamedConstant			// Bound to a named constant
	};

	Variable(const std::string& name, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

private:
	void bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack);
public:


	std::string name; // variable name.

	BindingType binding_type; // one of BindingType above.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of.  Use in ArgumentVariable case.
	LetASTNode* bound_let_node; // Used in LetVariable case.
	NamedConstant* bound_named_constant; // Used in BoundToNamedConstant case.

	int arg_index; // index in function argument list, or index of captured var.

	FunctionDefinition* enclosing_lambda;
	int free_index; // If this variable is in a lambda expression, and the variable is free in the mostly tightly enclosing lambda, then this is the index of this variable in the list
	// of free variables of the lambda.  If it's not free in a lambda then it is -1.

	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.  Used in LetVariable case.
};


typedef Reference<Variable> VariableRef;


} // end namespace Winter
