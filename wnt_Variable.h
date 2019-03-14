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
	// The kind of AST nodes that a variable can be bound to.
	enum BindingType
	{
		UnboundVariable,
		LetVariable,					// Bound to a let variable
		ArgumentVariable,				// Bound to an argument of the most-tightly enclosing function
		BoundToGlobalDefVariable,		// Bound to a globally defined (program scope) function
		BoundToNamedConstant			// Bound to a named constant
	};

	Variable(const std::string& name, const SrcLocation& loc);
	~Variable();

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

	FunctionDefinition* bound_function; // Function for which the variable is an argument of.  Used in binding_type == ArgumentVariable case.
	LetASTNode* bound_let_node; // Used in binding_type == LetVariable case.
	NamedConstant* bound_named_constant; // Used in binding_type == BoundToNamedConstant case.

	int arg_index; // index in function argument list, or index of captured var.
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.  Used in binding_type == LetVariable case.

	FunctionDefinition* enclosing_lambda; // Most tightly-enclosing Lambda expression in which this variable exists, 
	// if this variable is free (not bound to anything in the lambda expression).
};


typedef Reference<Variable> VariableRef;


} // end namespace Winter
