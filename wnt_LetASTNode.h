/*=====================================================================
LetASTNode.h
------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
#include <string>


namespace Winter
{


/*
In a let block,
like:
	let
		a = f()
		b, c = g()
	in
		a + b + c

Then 'a = f()' and 'b, c = g()' are nodes of type LetASTNode.

a, b, c individually are LetNodeVars.
*/
class LetNodeVar
{
public:
	LetNodeVar() {}
	LetNodeVar(const std::string& name_) : name(name_) {}
	std::string name;
	TypeRef declared_type; // may be NULL
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(const std::vector<LetNodeVar>& vars, const ASTNodeRef& expr, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;

	ASTNodeRef expr;
	std::vector<LetNodeVar> vars; // One or more variable names (And possible associated declared types).  Will be more than one in the case of destructuring assignment.
	//mutable llvm::Value* llvm_value;
	bool traced;
};


} //end namespace Winter
