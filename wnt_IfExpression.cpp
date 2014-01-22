/*=====================================================================
IfExpression.cpp
----------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#include "wnt_IfExpression.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "utils/stringutils.h"
#include "maths/mathstypes.h"
#if USE_LLVM
#pragma warning(push, 0) // Disable warnings
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#pragma warning(pop) // Re-enable warnings
#endif


namespace Winter
{


IfExpression::IfExpression(const SrcLocation& src_loc, const ASTNodeRef& condition_, const ASTNodeRef& then_expr_, const ASTNodeRef& else_expr_)
:	ASTNode(IfExpressionType, src_loc),
	condition(condition_),
	then_expr(then_expr_),
	else_expr(else_expr_)
{
}


ValueRef IfExpression::exec(VMState& vmstate)
{
	ValueRef condition_val = this->condition->exec(vmstate);
	assert(dynamic_cast<BoolValue*>(condition_val.getPointer()));

	if(static_cast<BoolValue*>(condition_val.getPointer())->value)
	{
		// If condition is true:
		return this->then_expr->exec(vmstate);
	}
	else
	{
		// Else if condition is false:
		return this->else_expr->exec(vmstate);
	}
}


void IfExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(condition, payload);
		checkFoldExpression(then_expr, payload);
		checkFoldExpression(else_expr, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// If we are the top level expression in a function definition, then see if we can coerce arg1 and arg2 to be what the function wants to return
		if(stack.back()->nodeType() == ASTNode::FunctionDefinitionType)
		{
			FunctionDefinition* f = (FunctionDefinition*)stack.back();

			if(f->declared_return_type.nonNull() && f->declared_return_type->getType() == Type::FloatType)
			{
				doImplicitIntToFloatTypeCoercionForFloatReturn(this->then_expr, payload);
				doImplicitIntToFloatTypeCoercionForFloatReturn(this->else_expr, payload);
			}
		}
	}
	

	stack.push_back(this);

	condition->traverse(payload, stack);
	then_expr->traverse(payload, stack);
	else_expr->traverse(payload, stack);

	
	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(condition, payload, stack);
		checkInlineExpression(then_expr, payload, stack);
		checkInlineExpression(else_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(condition, payload);
		checkSubstituteVariable(then_expr, payload);
		checkSubstituteVariable(else_expr, payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
		convertOverloadedOperators(condition, payload, stack);
		convertOverloadedOperators(then_expr, payload, stack);
		convertOverloadedOperators(else_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkInDomain(payload, stack);
		//this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->condition->type()->getType() != Type::BoolType)
			throw BaseException("First argument to if expression must have bool type." + errorContext(*this->condition));

		if(*this->then_expr->type() != *this->else_expr->type())
			throw BaseException("Second and third arguments to if expression must have same type." + errorContext(*this->then_expr));
	}
	
	stack.pop_back();
}


bool IfExpression::provenDefined() const
{
	return false;//TEMP
}


void IfExpression::checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
}


void IfExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "if" << "\n";
	this->condition->print(depth + 1, s);

	printMargin(depth, s);
	s << "then" << "\n";
	this->then_expr->print(depth + 1, s);

	printMargin(depth, s);
	s << "else" << "\n";
	this->else_expr->print(depth + 1, s);
}


std::string IfExpression::sourceString() const
{
	std::string s = "if " + condition->sourceString() + " then " + then_expr->sourceString() + " else " + else_expr->sourceString();
	return s;
}


TypeRef IfExpression::type() const
{
	return this->then_expr->type();
}


llvm::Value* IfExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	const TypeRef ret_type = this->then_expr->type();

	llvm::Value* return_val_addr = NULL;
	if(!ret_type->passByValue())
	{
		if(ret_space_ptr)
			return_val_addr = ret_space_ptr;
		else
		{
			// Allocate return value on stack

			// Emit the alloca in the entry block for better code-gen.
			// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
			llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

			return_val_addr = entry_block_builder.Insert(new llvm::AllocaInst(
				ret_type->LLVMType(*params.context), // type
				NULL, // ArraySize
				16 // alignment
				//"return_val_addr" // target_sig.toString() + " return_val_addr"
			),
			"if ret");
		}
	}

	llvm::Value* condition_code = this->condition->emitLLVMCode(params, NULL);

		
	// Get a pointer to the current function
	llvm::Function* the_function = params.currently_building_func; // params.builder->GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock* ThenBB  = llvm::BasicBlock::Create(*params.context, "then", the_function);
	llvm::BasicBlock* ElseBB  = llvm::BasicBlock::Create(*params.context, "else");
	llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*params.context, "ifcont");

	params.builder->CreateCondBr(condition_code, ThenBB, ElseBB);

	// Emit then value.
	params.builder->SetInsertPoint(ThenBB);

	llvm::Value* then_value = then_expr->emitLLVMCode(params, return_val_addr);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = params.builder->GetInsertBlock();

	// Emit else block.
	the_function->getBasicBlockList().push_back(ElseBB);
	params.builder->SetInsertPoint(ElseBB);

	llvm::Value* else_value = else_expr->emitLLVMCode(params, return_val_addr);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = params.builder->GetInsertBlock();

	// Emit merge block.
	the_function->getBasicBlockList().push_back(MergeBB);
	params.builder->SetInsertPoint(MergeBB);

	// Create phi node for result value
	llvm::PHINode* phi_node = params.builder->CreatePHI(
		ret_type->passByValue() ? ret_type->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*ret_type->LLVMType(*params.context)),
		0, // num reserved values
		"iftmp"
	);

	phi_node->addIncoming(then_value, ThenBB);
	phi_node->addIncoming(else_value, ElseBB);

	if(ret_type->passByValue())
	{
			

		//TEMP:
		//std::cout << "\nthen_value: " << std::endl;
		//then_value->dump();
		//std::cout << "\nelse_value: " << std::endl;
		//else_value->dump();

			

	//	return phi_node;
	}
	else
	{
		//the_function->getBasicBlockList().push_back(MergeBB);
		//params.builder->SetInsertPoint(MergeBB);

	//	return phi_node;

		//return return_val_addr;


		/*llvm::Value* arg_val = params.builder->CreateLoad(
			phi_result
		);

		llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		return params.builder->CreateStore(
			arg_val, // value
			return_val_ptr // ptr
		);*/
		//llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
		//return return_val_ptr;
	}

	// If this is a string value, need to decr ref count at end of func.
	if(ret_type->getType() == Type::StringType)
	{
		params.cleanup_values.push_back(CleanUpInfo(this, phi_node));
	}

	return phi_node;
}


void IfExpression::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
{
	RefCounting::emitCleanupLLVMCode(params, this->type(), val);
}


//llvm::Value* IfExpression::getConstantLLVMValue(EmitLLVMCodeParams& params) const
//{
//	return this->target_function->getConstantLLVMValue(params);
//}


Reference<ASTNode> IfExpression::clone()
{
	return new IfExpression(this->srcLocation(), this->condition->clone(), this->then_expr->clone(), this->else_expr->clone());
}


bool IfExpression::isConstant() const
{
	return this->condition->isConstant() && this->then_expr->isConstant() && this->else_expr->isConstant();
}


} // end namespace Winter
