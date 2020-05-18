/*=====================================================================
IfExpression.h
--------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include <llvm/IR/IRBuilder.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <string>
#include <vector>
namespace llvm { class Function; };
namespace llvm { class Value; };
namespace llvm { class Module; };
namespace llvm { class LLVMContext; };
namespace PlatformUtils { class CPUInfo; }


namespace Winter
{


/*=====================================================================
IfExpression
-------------------
if x then y else z
=====================================================================*/
class IfExpression : public ASTNode
{
public:
	IfExpression(const SrcLocation& src_loc, const ASTNodeRef& condition, const ASTNodeRef& then_expr, const ASTNodeRef& else_expr);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;

	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual bool provenDefined() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;


private:
	void checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:
	ASTNodeRef condition;
	ASTNodeRef then_expr;
	ASTNodeRef else_expr;
	//bool proven_defined;
};


typedef Reference<IfExpression> IfExpressionRef;


} // end namespace Winter
