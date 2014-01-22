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
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#if USE_LLVM
#include <llvm/IR/IRBuilder.h>
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
e.g.   f(a, 1)
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

	virtual void linkFunctions(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const;
	virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;
	FunctionDefinition* runtimeBind(VMState& vmstate, FunctionValue*& function_value_out);
	virtual bool provenDefined() const;

	///////

private:
	void checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:

	string function_name;
	vector<Reference<ASTNode> > argument_expressions;

	//Reference<ASTNode> target_function;
	//ASTNode* target_function;
	FunctionDefinition* target_function; // May be NULL
	//Reference<ExternalFunction> target_external_function; // May be NULL
	int bound_index;
	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	enum BindingType
	{
		Unbound,
		Let,
		Arg,
		BoundToGlobalDef
	};
	BindingType binding_type;

	//TypeRef target_function_return_type;
	bool use_captured_var;
	int captured_var_index;
	int let_frame_offset;

private:
	bool proven_defined;
};


typedef Reference<FunctionExpression> FunctionExpressionRef;


} // end namespace Winter
