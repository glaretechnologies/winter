/*=====================================================================
wnt_LetBlock.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"


namespace Winter
{

/*
A let block is an expression of the form

let
	a = ..
	b = ..
in
	c

It has zero or more LetASTNodes (lets), and a value expression.
*/
class LetBlock : public ASTNode
{
public:
	LetBlock(const ASTNodeRef& e, const std::vector<Reference<LetASTNode> > lets_, const SrcLocation& loc) : 
	  ASTNode(LetBlockType, loc), expr(e), lets(lets_) 
	{
		//let_exprs_llvm_value = vector<llvm::Value*>(lets_.size(), NULL); 
	}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

	std::vector<Reference<LetASTNode> > lets;

	//llvm::Value* getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index, llvm::Value* ret_space_ptr);

	//std::vector<llvm::Value*> let_exprs_llvm_value;

	ASTNodeRef expr;
};


typedef Reference<LetBlock> LetBlockRef;


} //end namespace Winter
