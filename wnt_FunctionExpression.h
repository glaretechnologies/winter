/*=====================================================================
FunctionExpression.h
--------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-04-30 18:53:38 +0100
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_Variable.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#include <string>
#include <vector>
namespace llvm { class Function; };
namespace llvm { class Value; };
namespace llvm { class Module; };
namespace llvm { class LLVMContext; };


namespace Winter
{


/*=====================================================================
FunctionExpression
-------------------
Function application.
e.g.   f(a, 1)

There are two main types of function application:

Application of a globally defined, statically determined function, such as
square(x)
where square is defined with "def square" etc..

The other kind of function application is where there is an arbitrary expression returning the actual function value,
e.g. 

func_arg(x), where func_arg is a variable, or e.g.

getFunc()(x), where getFunc() returns a function.

=====================================================================*/
class FunctionExpression : public ASTNode
{
public:
	FunctionExpression(const SrcLocation& src_loc);
	FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0); // 1-arg function
	FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1); // 2-arg function

	bool doesFunctionTypeMatch(const TypeRef& type);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual bool provenDefined() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

	///////

	const std::string functionName() const;

	bool isBoundToGlobalDef() const; // Is this function bound to a single global definition.  This should be the case for most normal function expressions like f(x)

	// Emit code for the given function argument in a function call expression.
	// May be as simple as "x", or a temporary may need to be allocated on the stack, so its address can be taken, for example
	// SomeStruct f_arg_0 = someExpr();
	// f_arg_0
	static std::string emitCodeForFuncArg(EmitOpenCLCodeParams& params, const ASTNodeRef& arg_expression, const FunctionDefinition* target_func, const std::string& func_arg_name);

private:
	void checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void bindFunction(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void tryCoerceIntArgsToDoubles(Linker& linker, const std::vector<TypeVRef>& argtypes, int effective_callsite_order_num);
	void tryCoerceIntArgsToFloats(Linker& linker, const std::vector<TypeVRef>& argtypes, int effective_callsite_order_num);
	void checkInlineExpression(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:

	ASTNodeRef get_func_expr; // Expression that returns the function being called.

	std::vector<ASTNodeRef> argument_expressions;

	std::string static_function_name;
	FunctionDefinition* static_target_function; // May be NULL.  For statically bound function application

private:
	bool proven_defined;
};


typedef Reference<FunctionExpression> FunctionExpressionRef;


} // end namespace Winter
