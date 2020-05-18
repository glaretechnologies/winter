/*=====================================================================
IfExpression.cpp
----------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#include "wnt_IfExpression.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_Variable.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "utils/StringUtils.h"
#include "maths/mathstypes.h"
#include <ostream>
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#ifdef _MSC_VER
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
	const ValueRef condition_val = this->condition->exec(vmstate);
	
	if(checkedCast<BoolValue>(condition_val)->value)
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
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(condition, payload);
		checkFoldExpression(then_expr, payload);
		checkFoldExpression(else_expr, payload);
	}
	else */if(payload.operation == TraversalPayload::TypeCoercion)
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

			if(f->declared_return_type.nonNull() && f->declared_return_type->getType() == Type::DoubleType)
			{
				doImplicitIntToDoubleTypeCoercionForDoubleReturn(this->then_expr, payload);
				doImplicitIntToDoubleTypeCoercionForDoubleReturn(this->else_expr, payload);
			}
		}
	}
	

	stack.push_back(this);

	condition->traverse(payload, stack);
	then_expr->traverse(payload, stack);
	else_expr->traverse(payload, stack);

	
	if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkInDomain(payload, stack);
		//this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->condition->type().isNull() || this->then_expr->type().isNull() || this->else_expr->type().isNull())
			throw ExceptionWithPosition("Unknown type.", errorContext(*this->condition));

		if(this->condition->type()->getType() != Type::BoolType)
			throw ExceptionWithPosition("First argument to if expression must have bool type.", errorContext(*this->condition));

		if(*this->then_expr->type() != *this->else_expr->type())
			throw ExceptionWithPosition("Second and third arguments to if expression must have same type.", errorContext(*this->then_expr));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = condition->can_constant_fold && then_expr->can_constant_fold && else_expr->can_constant_fold && expressionIsWellTyped(*this, payload);

		const bool a_is_literal = checkFoldExpression(condition, payload, stack);
		const bool b_is_literal = checkFoldExpression(then_expr, payload, stack);
		const bool c_is_literal = checkFoldExpression(else_expr, payload, stack);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal && c_is_literal;
	}
	else if(payload.operation == TraversalPayload::SimplifyIfExpression)
	{
		if(condition->nodeType() == ASTNode::BoolLiteralType)
		{
			const bool val = condition.downcast<BoolLiteral>()->value;
			ASTNodeRef replacement = val ? then_expr : else_expr;

			payload.garbage.push_back(this); // Store a ref in payload so this node won't get deleted while we are still executing this function.
			assert(stack.back() == this);
			stack[stack.size() - 2]->updateChild(this, replacement);
			payload.tree_changed = true;
		}
	}
	
	stack.pop_back();
}


void IfExpression::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	if(condition.ptr() == old_val)
		condition = new_val;
	else if(then_expr.ptr() == old_val)
		then_expr = new_val;
	else if(else_expr.ptr() == old_val)
		else_expr = new_val;
	else
		assert(0);
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


std::string IfExpression::sourceString(int depth) const
{
	std::string s = "if " + condition->sourceString(depth) + " then " + then_expr->sourceString(depth) + " else " + else_expr->sourceString(depth) + "";
	return s;
}


std::string IfExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	bool can_use_ternary_if = true;

	// Emit 'then' code
	std::string then_block_code;
	params.blocks.push_back(std::string());
	std::string then_code = then_expr->emitOpenCLC(params);
	if(then_expr->type()->OpenCLPassByPointer() && (then_expr->nodeType() == ASTNode::VariableASTNodeType) && (then_expr.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument))
		then_code = "*" + then_code;

	if(!params.blocks.back().empty()) // If some additional statements are required to compute the 'then' expression, then we can't use ternary if operator.
	{
		can_use_ternary_if = false;
		StringUtils::appendTabbed(then_block_code, params.blocks.back(), 1);
	}
	params.blocks.pop_back();

	// Emit 'else' code
	std::string else_block_code;
	params.blocks.push_back(std::string());
	std::string else_code = else_expr->emitOpenCLC(params);
	if(else_expr->type()->OpenCLPassByPointer() && (else_expr->nodeType() == ASTNode::VariableASTNodeType) && (else_expr.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument))
		else_code = "*" + else_code;
	if(!params.blocks.back().empty())
	{
		can_use_ternary_if = false;
		StringUtils::appendTabbed(else_block_code, params.blocks.back(), 1);
	}
	params.blocks.pop_back();

	if(can_use_ternary_if)
		return "(" + condition->emitOpenCLC(params) + " ? " + then_code + " : " + else_code + ")";

	const std::string result_var_name = "if_res_" + toString(params.uid++);

	std::string s;
	s += this->type()->OpenCLCType() + " " + result_var_name + ";\n";

	s += "if(" + condition->emitOpenCLC(params) + ") {\n";

	s += then_block_code;

	s += "\t" + result_var_name + " = " + then_code + ";\n";

	s += "} else {\n";

	s += else_block_code;

	s += "\t" + result_var_name + " = " + else_code + ";\n";

	s += "}\n";

	params.blocks.back() += s;

	return result_var_name;
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

			return_val_addr = entry_block_builder.CreateAlloca(
				ret_type->LLVMType(*params.module), // type
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
				"if ret"
			);
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
		ret_type->passByValue() ? ret_type->LLVMType(*params.module) : LLVMTypeUtils::pointerType(*ret_type->LLVMType(*params.module)),
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

		llvm::Value* return_val_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);

		return params.builder->CreateStore(
			arg_val, // value
			return_val_ptr // ptr
		);*/
		//llvm::Value* return_val_ptr = LLVMUtils::getNthArg(params.currently_building_func, 0);
		//return return_val_ptr;
	}

	// If this is a string value, need to decr ref count at end of func.
	/*if(ret_type->getType() == Type::StringType)
	{
		params.cleanup_values.push_back(CleanUpInfo(this, phi_node));
	}*/

	return phi_node;
}


//void IfExpression::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
//{
//	//RefCounting::emitCleanupLLVMCode(params, this->type(), val);
//}


Reference<ASTNode> IfExpression::clone(CloneMapType& clone_map)
{
	IfExpression* res = new IfExpression(this->srcLocation(), this->condition->clone(clone_map), this->then_expr->clone(clone_map), this->else_expr->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool IfExpression::isConstant() const
{
	return this->condition->isConstant() && this->then_expr->isConstant() && this->else_expr->isConstant();
}


size_t IfExpression::getTimeBound(GetTimeBoundParams& params) const
{
	// Assuming we don't know the value of condition here.
	return this->condition->getTimeBound(params) + myMax(this->then_expr->getTimeBound(params), this->else_expr->getTimeBound(params)) + 1;
}


GetSpaceBoundResults IfExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	// Assuming we don't know the value of condition here.
	return this->condition->getSpaceBound(params) + this->then_expr->getSpaceBound(params) +  this->else_expr->getSpaceBound(params);
}


size_t IfExpression::getSubtreeCodeComplexity() const
{
	return this->condition->getSubtreeCodeComplexity() + 
		this->then_expr->getSubtreeCodeComplexity() + 
		this->else_expr->getSubtreeCodeComplexity();
}


} // end namespace Winter
