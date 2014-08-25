/*=====================================================================
IfExpression.h
--------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_Frame.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
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
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const;
	//virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;
	virtual bool provenDefined() const;


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
