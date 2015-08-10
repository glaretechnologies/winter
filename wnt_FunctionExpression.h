/*=====================================================================
FunctionExpression.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-30 18:53:38 +0100
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include "wnt_Type.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_Frame.h"
#include "wnt_Variable.h"
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
FunctionExpression
-------------------
Function application.
e.g.   f(a, 1)

There are two main types of function application:

Application of a globally defined, statically determined function., such as
square(x)
where square is defined with "def square" etc..

The other kind of function application is where there is an arbitrary expression returning the actual function value,
e.g. 

func_arg(x), where func_arg is a variable, or e..g

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
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const;
	virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	FunctionDefinition* runtimeBind(VMState& vmstate, const FunctionValue*& function_value_out);
	virtual bool provenDefined() const;

	///////

	const std::string functionName() const;

	bool isBoundToGlobalDef() const; // Is this function bound to a single global definition.  This should be the case for most normal function expressions like f(x)

private:
	void checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void bindFunction(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:

	ASTNodeRef get_func_expr; // Expression that returns the function being called.

	std::vector<ASTNodeRef> argument_expressions;

	std::string static_function_name;
	FunctionDefinition* static_target_function; // May be NULL.  For statically bound function applucation

private:
	bool proven_defined;
};


typedef Reference<FunctionExpression> FunctionExpressionRef;


} // end namespace Winter
