/*=====================================================================
wnt_VArrayLiteral.h
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
namespace llvm { class Value; };


namespace Winter
{


/*=====================================================================
VArrayLiteral
-------------
Variable-length array literal.
e.g. [1, 2, 3]va
=====================================================================*/
class VArrayLiteral : public ASTNode
{
public:
	VArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix, int int_suffix);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;


	size_t numElementsInValue() const;
private:
	bool areAllElementsConstant() const;
	
	std::vector<ASTNodeRef> elements;
	bool has_int_suffix;
	int int_suffix; // e.g. number of elements

	//mutable llvm::Value* ptr_alloca;
	FunctionDefinition* make_varray_func_def;

	mutable bool llvm_heap_allocated;
};


typedef Reference<VArrayLiteral> VArrayLiteralRef;


} // end namespace Winter
