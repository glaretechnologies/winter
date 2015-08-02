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
		LetVariable,
		ArgumentVariable,
		BoundToGlobalDefVariable,
		BoundToNamedConstant,
		CapturedVariable
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
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	// or for what it is a let of.
	//AnonFunction* parent_anon_function;
	//ASTNode* referenced_var;
	//TypeRef referenced_var_type; // Type of the variable.

	std::string name; // variable name.

	BindingType vartype; // one of BindingType above.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;
	NamedConstant* bound_named_constant;

	int bound_index; // index in parent function definition argument list, or index of let var in let block, or index of captured var.

	// Offset of zero means use the latest/deepest set of let values.  Offset 1 means the next oldest, etc.
	//int let_frame_offset;

	// bool use_captured_var;
	// int captured_var_index;

	//int uncaptured_bound_index;
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.

};



typedef Reference<Variable> VariableRef;


} // end namespace Winter
