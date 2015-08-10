/*=====================================================================
wnt_TupleLiteral.h
------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
namespace llvm { class Value; };


namespace Winter
{


/*=====================================================================
TupleLiteral
-------------------
e.g. [1, 2, 3]t
=====================================================================*/
class TupleLiteral : public ASTNode
{
public:
	TupleLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	const std::vector<ASTNodeRef>& getElements() const { return elements; }
private:
	bool areAllElementsConstant() const;
	std::vector<ASTNodeRef> elements;
};


typedef Reference<TupleLiteral> TupleLiteralRef;


} // end namespace Winter
