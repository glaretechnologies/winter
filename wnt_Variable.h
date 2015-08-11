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
		BoundToNamedConstant,			// Bound to a named constant
		CapturedVariable				// Bound to a captured variable of the most-tightly enclosing anonymous function.
	};

	Variable(const std::string& name, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	void bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;



	std::string name; // variable name.

	BindingType vartype; // one of BindingType above.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of.  Use in ArgumentVariable case.
	LetASTNode* bound_let_node; // Used in LetVariable case.
	NamedConstant* bound_named_constant; // Used in BoundToNamedConstant case.

	int bound_index; // index in function argument list, or index of captured var.

	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.  Used in LetVariable case.
};


typedef Reference<Variable> VariableRef;


} // end namespace Winter
