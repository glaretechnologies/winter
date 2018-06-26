/*=====================================================================
wnt_VectorLiteral.h
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
namespace llvm { class Value; };


namespace Winter
{


/*=====================================================================
VectorLiteral
-------------------
e.g. [1, 2, 3]v
=====================================================================*/
class VectorLiteral : public ASTNode
{
public:
	VectorLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix, int int_suffix);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
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

	const std::vector<ASTNodeRef>& getElements() const { return elements; }
private:
	bool areAllElementsConstant() const;
	std::vector<ASTNodeRef> elements;
	bool has_int_suffix;
	int int_suffix;
};


typedef Reference<VectorLiteral> VectorLiteralRef;


} // end namespace Winter
