/*=====================================================================
ASTNode.cpp
-----------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
=====================================================================*/
#include "wnt_ASTNode.h"


#include "wnt_FunctionExpression.h"
#include "wnt_SourceBuffer.h"
#include "wnt_Diagnostics.h"
#include "wnt_RefCounting.h"
#include "wnt_ArrayLiteral.h"
#include "wnt_VectorLiteral.h"
#include "wnt_TupleLiteral.h"
#include "wnt_VArrayLiteral.h"
#include "wnt_Variable.h"
#include "wnt_LetASTNode.h"
#include "wnt_LetBlock.h"
#include "VMState.h"
#include "VirtualMachine.h"
#include "CompiledValue.h"
#include "Value.h"
#include "VMState.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMUtils.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "utils/StringUtils.h"
#include "utils/ConPrint.h"
#include "maths/mathstypes.h"
#include "maths/vec2.h"
#include <xmmintrin.h> // SSE header file
#include <ostream>
#include <iostream>
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#if TARGET_LLVM_VERSION < 36
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#endif
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;
using std::string;


namespace Winter
{


/*
	decrementStringRefCount(stringRep* s):
		r = load s->refcount
		if r == 1
			call freeString(s)
		else
			r' = r - 1
			store r' in s->refcount
		
*/






void printMargin(int depth, std::ostream& s)
{
	for(int i=0; i<depth; ++i)
		s << "  ";
}


bool isIntExactlyRepresentableAsFloat(int64 x)
{
	//return ((int64)((float)x)) == x;

	// Make the floating point value pass through an SSE register so it gets rounded to 32-bits.
	// Otherwise it may just get stored in a higher-precision float register.
	const float y = (float)x;
	float y2;
	_mm_store_ss(&y2, _mm_load_ss(&y));

	return (int64)y2 == x;
}


bool isIntExactlyRepresentableAsDouble(int64 x)
{
	//return ((int64)((float)x)) == x;

	// Make the floating point value pass through an SSE register so it gets rounded to 64-bits.
	// Otherwise it may just get stored in a higher-precision float register.
	const double y = (double)x;
	double y2;
	_mm_store_sd(&y2, _mm_load_sd(&y));

	return (int64)y2 == x;
}


bool expressionIsWellTyped(ASTNode& e, TraversalPayload& payload_)
{
	// NOTE: do this without exceptions?
	try
	{
		vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCheck);
		e.traverse(payload, stack);
		assert(stack.size() == 0);

		return true;
	}
	catch(BaseException& )
	{
		return false;
	}
}


/*static bool isLiteral(const ASTNode& e)
{
	return e.nodeType() == ASTNode::FloatLiteralType ||
		e.nodeType() == ASTNode::BoolLiteralType ||
		e.nodeType() == ASTNode::IntLiteralType ||
		e.nodeType() == ASTNode::VectorLiteralType ||
		e.nodeType() == ASTNode::ArrayLiteralType ||
		e.nodeType() == ASTNode::TupleLiteralType;
}


static bool isReferenceToArray(const ASTNodeRef& e)
{
	if(e->nodeType() == ASTNode::LetType &&
		e->type()->getType() == Type::ArrayTypeType)
		return true;
	return false;
}*/


bool shouldFoldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	assert(e.nonNull());

	if(e->can_maybe_constant_fold)
	{
		// Do type check of subtree.
		// We need to do this otherwise constant folding could allow otherwise mistyped programs to compile, e.g. elem([1, true]v, 0)
		const bool expr_is_well_typed = expressionIsWellTyped(*e, payload);
		const TypeRef e_type = e->type();
		return expr_is_well_typed && e_type.nonNull() && 
			(	(e_type->getType() == Type::FloatType && (e->nodeType() != ASTNode::FloatLiteralType)) ||
				(e_type->getType() == Type::DoubleType && (e->nodeType() != ASTNode::DoubleLiteralType)) ||
				(e_type->getType() == Type::BoolType &&	(e->nodeType() != ASTNode::BoolLiteralType)) ||
				(e_type->getType() == Type::IntType && (e->nodeType() != ASTNode::IntLiteralType)) ||
				(e_type->getType() == Type::VectorTypeType && (e->nodeType() != ASTNode::VectorLiteralType)) ||
				(e_type->getType() == Type::ArrayTypeType && (e->nodeType() != ASTNode::ArrayLiteralType && e->nodeType() != ASTNode::VariableASTNodeType)) ||
				(e_type->getType() == Type::TupleTypeType && (e->nodeType() != ASTNode::TupleLiteralType))
			);
	}

	return false;
	/*if(e.nonNull() && e->isConstant())
	{
		const TypeRef e_type = e->type();
		return e_type.nonNull() && 
			(	(e_type->getType() == Type::FloatType &&		
				(e->nodeType() != ASTNode::FloatLiteralType)) ||
				(e_type->getType() == Type::BoolType &&		
				(e->nodeType() != ASTNode::BoolLiteralType)) ||
				(e_type->getType() == Type::IntType &&
				(e->nodeType() != ASTNode::IntLiteralType))
			) &&
			expressionIsWellTyped(*e, payload);
	}
	else
		return false;*/
}


static ASTNodeRef makeLiteralASTNodeFromValue(const ValueRef& value, const SrcLocation& src_location, const TypeVRef& type)
{
	switch(value->valueType())
	{
	case Value::ValueType_Float:
	{
		if(type->getType() != Type::FloatType)
			throw BaseException("invalid type");

		return new FloatLiteral(value.downcastToPtr<FloatValue>()->value, src_location);
	}
	case Value::ValueType_Double:
	{
		if(type->getType() != Type::DoubleType)
			throw BaseException("invalid type");

		return new DoubleLiteral(value.downcastToPtr<DoubleValue>()->value, src_location);
	}
	case Value::ValueType_Int:
	{
		if(type->getType() != Type::IntType)
			throw BaseException("invalid type");

		return new IntLiteral(value.downcastToPtr<IntValue>()->value, type.downcastToPtr<Int>()->numBits(), value.downcastToPtr<IntValue>()->is_signed, src_location);
	}
	case Value::ValueType_Bool:
	{
		if(type->getType() != Type::BoolType)
			throw BaseException("invalid type");

		return new BoolLiteral(value.downcastToPtr<BoolValue>()->value, src_location);
	}
	case Value::ValueType_Array:
	{
		if(type->getType() != Type::ArrayTypeType)
			throw BaseException("invalid type");

		const ArrayValue* array_val = value.downcastToPtr<ArrayValue>();
		vector<ASTNodeRef> elem_literals(array_val->e.size());
		for(size_t i=0; i<array_val->e.size(); ++i)
			elem_literals[i] = makeLiteralASTNodeFromValue(array_val->e[i], src_location, type.downcastToPtr<ArrayType>()->elem_type);

		// TODO: preserve int suffix, useful for large arrays etc..
		return new ArrayLiteral(elem_literals, src_location,
			false, // has int suffix
			0 // int suffix
		);
	}
	case Value::ValueType_Vector:
	{
		if(type->getType() != Type::VectorTypeType)
			throw BaseException("invalid type");

		const VectorValue* vector_val = value.downcastToPtr<VectorValue>();
		vector<ASTNodeRef> elem_literals(vector_val->e.size());
		for(size_t i=0; i<vector_val->e.size(); ++i)
			elem_literals[i] = makeLiteralASTNodeFromValue(vector_val->e[i], src_location, type.downcastToPtr<VectorType>()->elem_type);

		// TODO: preserve int suffix, useful for large vectors etc..
		return new VectorLiteral(elem_literals, src_location,
			false, // has int suffix
			0 // int suffix
		);
	}
	default:
	{
		throw BaseException("invalid type");
	}
	};
}
	

// Replace an expression with a constant (literal AST node)
ASTNodeRef foldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	// Compute value of expression
	VMState vmstate;
	vmstate.func_args_start.push_back(0);

	ValueRef retval = e->exec(vmstate);

	vmstate.func_args_start.pop_back();

	const TypeRef e_type = e->type();
	assert(e_type.nonNull()); // This should have been checked in checkFoldExpression() etc..
	if(e_type.isNull())
		throw BaseException("Internal error: Expression type was null during constant folding.");

	const ASTNodeRef literal_node = makeLiteralASTNodeFromValue(retval, e->srcLocation(), TypeVRef(e_type));
	return literal_node;
}


// Returns true if folding took place or e is already a literal. (Or could be folded to a literal if needed, such as a let var referring to an array)
bool checkFoldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	if(e.isNull())
		return false;

	if(shouldFoldExpression(e, payload))
	{
		try
		{
			e = foldExpression(e, payload);
			payload.tree_changed = true;
			return true;
		}
		catch(BaseException& )
		{
			// An invalid operation was performed, such as dividing by zero, while trying to eval the AST node.
			// In this case we will consider the folding as not taking place.
			return false;
		}
	}
	
	// e may already be a literal
	const TypeRef e_type = e->type();
	const bool e_is_literal = e_type.nonNull() && 
		(	(e_type->getType() == Type::FloatType && (e->nodeType() == ASTNode::FloatLiteralType)) ||
			(e_type->getType() == Type::DoubleType && (e->nodeType() == ASTNode::DoubleLiteralType)) ||
			(e_type->getType() == Type::BoolType &&	(e->nodeType() == ASTNode::BoolLiteralType)) ||
			(e_type->getType() == Type::IntType && (e->nodeType() == ASTNode::IntLiteralType)) ||
			(e_type->getType() == Type::VectorTypeType && (e->nodeType() == ASTNode::VectorLiteralType)) ||
			(e_type->getType() == Type::ArrayTypeType && (e->nodeType() == ASTNode::ArrayLiteralType || e->nodeType() == ASTNode::VariableASTNodeType)) ||
			(e_type->getType() == Type::TupleTypeType && (e->nodeType() == ASTNode::TupleLiteralType))
		);
	return e_is_literal && e->can_maybe_constant_fold;
}


/*
If node 'e' is a function expression, inline the target function by replacing e with the target function body.
*/
void checkInlineExpression(ASTNodeRef& e, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(e->nodeType() == ASTNode::FunctionExpressionType)
	{
		FunctionExpressionRef func_expr = e.downcast<FunctionExpression>();

		FunctionDefinition* target_func = NULL;
		bool is_beta_reduction = false; // Is this a lambda being applied directly, e.g. an expression like "(\\(float y) : y*y) (x)" ?
		if(func_expr->static_target_function)
		{
			target_func = func_expr->static_target_function;
		}
		else if(func_expr->get_func_expr.nonNull())
		{
			if(func_expr->get_func_expr->nodeType() == ASTNode::FunctionDefinitionType)
			{
				target_func = func_expr->get_func_expr.downcastToPtr<FunctionDefinition>();
				is_beta_reduction = true;
			}
			else
			{
				// Walk up the tree until we get to a node that is not a variable bound to a let node:
				ASTNode* cur = func_expr->get_func_expr.getPointer();
				while((cur->nodeType() == ASTNode::VariableASTNodeType) && (((Variable*)cur)->binding_type == Variable::LetVariable))
					cur = ((Variable*)cur)->bound_let_node->expr.getPointer();

				if(cur->nodeType() == ASTNode::FunctionDefinitionType)
					target_func = (FunctionDefinition*)cur;
				else if(cur->nodeType() == ASTNode::VariableASTNodeType && ((Variable*)cur)->binding_type == Variable::BoundToGlobalDefVariable)
					target_func = ((Variable*)cur)->bound_function;
			}
			/*else if(func_expr->get_func_expr->isConstant())
			{
				//if(func_expr->get_func_expr.isType<Variable>())
				//{
				//	const Variable* var = func_expr->get_func_expr
				try
				{
					VMState vmstate;
					vmstate.capture_vars = false;
					vmstate.func_args_start.push_back(0);
					ValueRef base_target_function_val = func_expr->get_func_expr->exec(vmstate);
					const FunctionValue* target_func_val = checkedCast<FunctionValue>(base_target_function_val);
					target_func = target_func_val->func_def;
				}
				catch(BaseException&)
				{
				}
			}*/
		}


		if(target_func && !target_func->noinline && !target_func->isExternalFunction() && target_func->body.nonNull()) // If is potentially inlinable:
		{
			const bool verbose = false;
			if(verbose) conPrint("\n=================== Considering inlining function call =====================\n");
			
			const int call_count = payload.calls_to_func_count[target_func];
			
			const bool is_target_func_just_a_call = target_func->body->nodeType() == ASTNode::FunctionExpressionType;

			// Work out if the argument expressions are 'expensive' to evaluate.
			// If they are, don't inline this function expression, because the argument expression *may* be duplicated.
			// e.g. def f(float x) : x + x + x + x, 
			// main(float x) : f(sin(x))    would get inlined to      main(float x) : sin(x) + sin(x) + sin(x) + sin(x)
			//
			// This is very crude, and should be improved in the following way:
			// Instead of not inlining, inline to a let expression, e.g. to
			//
			// let f_arg0 = sin(x) in f(f_arg0)
			//
			// Also it doesn't matter if the argument expression is expensive, if it will only be substituted 0 or 1 times.

			bool are_arg_expressions_expensive = false;
			for(size_t i=0; i<func_expr->argument_expressions.size(); ++i)
			{
				if(func_expr->argument_expressions[i]->nodeType() == ASTNode::FunctionExpressionType)
				{
					// Consider function calls expensive, with some exceptions, such as:
					//	* Call to getfield built-in function (field access)
					bool func_call_is_expensive = true;
					const FunctionDefinition* arg_target_func = func_expr->argument_expressions[i].downcastToPtr<FunctionExpression>()->static_target_function;
					if(arg_target_func->built_in_func_impl.nonNull())
					{
						//if(dynamic_cast<const GetField*>(arg_target_func->built_in_func_impl.getPointer()))
						//	func_call_is_expensive = false;
						func_call_is_expensive = arg_target_func->built_in_func_impl->callIsExpensive();
					}
					
					if(func_call_is_expensive)
						are_arg_expressions_expensive = true;
				}
				else
				{
					if(!(func_expr->argument_expressions[i]->nodeType() == ASTNode::VariableASTNodeType || func_expr->argument_expressions[i]->nodeType() == ASTNode::IntLiteralType ||
						func_expr->argument_expressions[i]->nodeType() == ASTNode::FloatLiteralType || func_expr->argument_expressions[i]->nodeType() == ASTNode::DoubleLiteralType ||
						func_expr->argument_expressions[i]->nodeType() == ASTNode::BoolLiteralType || func_expr->argument_expressions[i]->nodeType() == ASTNode::CharLiteralType))
					{
						// This argument expression is expensive to evaluate.
						are_arg_expressions_expensive = true;
					}
				}
			}

			if(verbose) conPrint("target: " + target_func->sig.toString() + ", call_count=" + toString(call_count) + ", beta reduction=" + boolToString(is_beta_reduction) +
				", is_target_func_just_a_call: " + boolToString(is_target_func_just_a_call) + ", are_arg_expressions_expensive: " + boolToString(are_arg_expressions_expensive) + "\n");


			const bool should_inline = (is_beta_reduction || (call_count <= 1) || is_target_func_just_a_call) && !are_arg_expressions_expensive;
			if(should_inline)
			{
				if(verbose) conPrint("------------original expr----------: ");
				if(verbose) e->print(0, std::cout);

				if(verbose) conPrint("------------original target function body-----------: ");
				if(verbose) target_func->body->print(0, std::cout);

				// Replace e with a copy of the target function body.
				ASTNodeRef cloned_body = cloneASTNodeSubtree(target_func->body);
				e = cloned_body;

				if(verbose) conPrint("------------new expr: (cloned function body)-----------: ");
				if(verbose) e->print(0, std::cout);


				// Now replace all variables in the target function body with the argument values from func_expr
				TraversalPayload sub_payload(TraversalPayload::SubstituteVariables);
				sub_payload.used_names = payload.used_names;
				sub_payload.func_args_to_sub = target_func;
				sub_payload.variable_substitutes.resize(func_expr->argument_expressions.size());
				
				// Work out effective call site position.
				int effective_callsite_order_num = 1000000000;
				if(payload.current_named_constant)
					effective_callsite_order_num = payload.current_named_constant->order_num;
				for(size_t z=0; z<payload.func_def_stack.size(); ++z)
					effective_callsite_order_num = myMin(effective_callsite_order_num, payload.func_def_stack[z]->order_num);

				sub_payload.new_order_num = effective_callsite_order_num;

				for(size_t i=0; i<func_expr->argument_expressions.size(); ++i)
				{
					sub_payload.variable_substitutes[i] = func_expr->argument_expressions[i]; // NOTE: Don't clone now, will clone the expressions when they are pulled out of argument_expressions.

					//std::cout << "------------sub_payload.variable_substitutes[i]: " << std::endl;
					//sub_payload.variable_substitutes[i]->print(0, std::cout);
				}
				e->traverse(sub_payload, stack);

				checkSubstituteVariable(e, sub_payload); // e itself might be a variable that needs substituting.

				if(verbose) conPrint("------------Substituted expression-----------: ");
				if(verbose) e->print(0, std::cout);


				// NEW: SubstituteVariables pass has set all variables in e to unbound.  Rebind all variables in e.
				{
					TraversalPayload temp_payload(TraversalPayload::UnbindVariables);
					temp_payload.linker = payload.linker;
					temp_payload.func_def_stack = payload.func_def_stack;
					e->traverse(temp_payload, stack);
				}

				
				if(!payload.func_def_stack.empty())
				{
					if(verbose) conPrint("------------Full function-----------: ");
					if(verbose) payload.func_def_stack[0]->print(0, std::cout);
				}
				else if(payload.current_named_constant)
				{
					if(verbose) conPrint("------------Full named constant-----------: ");
					if(verbose) payload.current_named_constant->print(0, std::cout);
				}
				else
				{
					assert(0);
				}

				{
					TraversalPayload temp_payload(TraversalPayload::BindVariables);
					temp_payload.linker = payload.linker;
					temp_payload.func_def_stack = payload.func_def_stack;
					e->traverse(temp_payload, stack);
				}

				if(verbose) conPrint("------------Rebound expression-----------: ");
				if(verbose) e->print(0, std::cout);

				

				payload.tree_changed = true;
			}
			else
			{
				if(verbose) conPrint("not inlining.\n");
			}
		}
	}
}


void checkSubstituteVariable(ASTNodeRef& e, TraversalPayload& payload)
{
	if(e->nodeType() == ASTNode::VariableASTNodeType)
	{
		Reference<Variable> var = e.downcast<Variable>();

		if(var->binding_type == Variable::ArgumentVariable && var->bound_function == payload.func_args_to_sub)
		{
			// Replace the variable with the argument value.	
			if(var->arg_index >= (int)payload.variable_substitutes.size())
				return; // May be out of bounds for invalid programs.
			e = cloneASTNodeSubtree(payload.variable_substitutes[var->arg_index]);

			payload.tree_changed = true;
		}
	}
}


void doDeadCodeElimination(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(e->nodeType() == ASTNode::LetBlockType)
	{
		LetBlock* letblock = e.downcastToPtr<LetBlock>();
		if(letblock->lets.empty())
		{
			// The letblock has no let variables.  So replace it with the value expression.
			// e.g
			// let in x    =>   x
			e = letblock->expr;

			payload.tree_changed = true;
		}
	}
}


const std::string mapOpenCLCVarName(const std::unordered_set<std::string>& opencl_c_keywords, const std::string& s)
{
	if(opencl_c_keywords.count(s))
		return s + "_MODIFIED_";
	else
		return s;
}


void convertOverloadedOperators(ASTNodeRef& e, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(e.isNull())
		return;

	switch(e->nodeType())
	{
	case ASTNode::AdditionExpressionType:
	{
		AdditionExpression* expr = static_cast<AdditionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType || expr->a->type()->getType() == Type::ArrayTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType || expr->b->type()->getType() == Type::ArrayTypeType)
				{
					// Replace expr with an op_add function call.
					e = new FunctionExpression(expr->srcLocation(), "op_add", expr->a, expr->b);
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables);
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::SubtractionExpressionType:
	{
		SubtractionExpression* expr = static_cast<SubtractionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType || expr->a->type()->getType() == Type::ArrayTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType || expr->b->type()->getType() == Type::ArrayTypeType)
				{
					// Replace expr with an op_sub function call.
					e = new FunctionExpression(expr->srcLocation(), "op_sub", expr->a, expr->b);
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables);
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::MulExpressionType:
	{
		MulExpression* expr = static_cast<MulExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType || expr->a->type()->getType() == Type::ArrayTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType || expr->b->type()->getType() == Type::ArrayTypeType)
			{
				// Replace expr with an op_mul function call.
				e = new FunctionExpression(expr->srcLocation(), "op_mul", expr->a, expr->b);
				payload.tree_changed = true;

				// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
				// This is needed now because we need to know the type of op_X, which is only available once bound.
				TraversalPayload new_payload(TraversalPayload::BindVariables);
				new_payload.linker = payload.linker;
				new_payload.func_def_stack = payload.func_def_stack;
				e->traverse(new_payload, stack);
			}
		break;
	}
	case ASTNode::DivExpressionType:
	{
		DivExpression* expr = static_cast<DivExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType || expr->a->type()->getType() == Type::ArrayTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType || expr->b->type()->getType() == Type::ArrayTypeType)
				{
					// Replace expr with an op_div function call.
					e = new FunctionExpression(expr->srcLocation(), "op_div", expr->a, expr->b);
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables);
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::ComparisonExpressionType:
	{
		ComparisonExpression* expr = static_cast<ComparisonExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
		{
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
			{
				// == and != are not overloadable, but are instead built-in
				if(expr->token->getType() == DOUBLE_EQUALS_TOKEN)
				{
					// If op_eq for this structure has been defined, use it instead of the built-in __compare_equal.
					if(payload.linker->findMatchingFunctionSimple(
						FunctionSignature(expr->getOverloadedFuncName(), typePair(TypeVRef(expr->a->type()), TypeVRef(expr->b->type())))).isNull())
					{
						// Replace with built-in func call expression here
						e = new FunctionExpression(expr->srcLocation(), "__compare_equal", expr->a, expr->b);
						payload.tree_changed = true;
						break;
					}
				}
				else if(expr->token->getType() == NOT_EQUALS_TOKEN)
				{
					// If op_eq for this structure has been defined, use it instead of the built-in __compare_equal.
					if(payload.linker->findMatchingFunctionSimple(
						FunctionSignature(expr->getOverloadedFuncName(), typePair(TypeVRef(expr->a->type()), TypeVRef(expr->b->type())))).isNull())
					{
						// Replace with built-in func call expression here
						e = new FunctionExpression(expr->srcLocation(), "__compare_not_equal", expr->a, expr->b);
						payload.tree_changed = true;
						break;
					}
				}

				// Replace expr with a function call.
				e = new FunctionExpression(expr->srcLocation(), expr->getOverloadedFuncName(), expr->a, expr->b);
				payload.tree_changed = true;

				// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
				// This is needed now because we need to know the type of op_X, which is only available once bound.
				TraversalPayload new_payload(TraversalPayload::BindVariables);
				new_payload.linker = payload.linker;
				new_payload.func_def_stack = payload.func_def_stack;
				e->traverse(new_payload, stack);
				break;
			}

			if(expr->a->type()->requiresCompareEqualFunction())
			{
				// == and != are not overloadable, but are instead built-in
				if(expr->token->getType() == DOUBLE_EQUALS_TOKEN)
				{
					// Replace with built-in func call expression here
					e = new FunctionExpression(expr->srcLocation(), "__compare_equal", expr->a, expr->b);
					payload.tree_changed = true;
					break;
				}
				else if(expr->token->getType() == NOT_EQUALS_TOKEN)
				{
					// Replace with built-in func call expression here
					e = new FunctionExpression(expr->srcLocation(), "__compare_not_equal", expr->a, expr->b);
					payload.tree_changed = true;
					break;
				}
			}
		}
		break;
	}
	case ASTNode::UnaryMinusExpressionType:
	{
		UnaryMinusExpression* expr = static_cast<UnaryMinusExpression*>(e.getPointer());
		if(expr->expr->type().nonNull())
			if(expr->expr->type()->getType() == Type::StructureTypeType)
			{
				// Replace expr with a function call to op_unary_minus
				e = new FunctionExpression(expr->srcLocation(), "op_unary_minus", expr->expr);
				payload.tree_changed = true;

				// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
				// This is needed now because we need to know the type of op_X, which is only available once bound.
				TraversalPayload new_payload(TraversalPayload::BindVariables);
				new_payload.linker = payload.linker;
				new_payload.func_def_stack = payload.func_def_stack;
				e->traverse(new_payload, stack);
			}
			break;
	}
	default:
		break;
	};
}


/*
Process an AST node with two children, a and b.

Do implicit conversion from int to float
3.0 > 4      =>       3.0 > 4.0

Updates the nodes a and b in place if needed.
*/
static void doImplicitIntToFloatTypeCoercion(ASTNodeRef& a, ASTNodeRef& b, TraversalPayload& payload)
{
	// Type may be null if 'a' is a variable node that has not been bound yet.
	const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	// TODO: Handle integer bitness

	// 3.0 > 4		=>		3.0 > 4.0
	if(a_type.nonNull() && a_type->getType() == Type::FloatType && b->nodeType() == ASTNode::IntLiteralType)
	{
		const IntLiteral* b_lit = b.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsFloat(b_lit->value))
		{
			b = new FloatLiteral((float)b_lit->value, b->srcLocation());
			payload.tree_changed = true;
		}
	}

	// 3 > 4.0      =>        3.0 > 4.0
	if(b_type.nonNull() && b_type->getType() == Type::FloatType && a->nodeType() == ASTNode::IntLiteralType)
	{
		const IntLiteral* a_lit = a.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsFloat(a_lit->value))
		{
			a = new FloatLiteral((float)a_lit->value, a->srcLocation());
			payload.tree_changed = true;
		}
	}
}


static void doImplicitIntToDoubleTypeCoercion(ASTNodeRef& a, ASTNodeRef& b, TraversalPayload& payload)
{
	// Type may be null if 'a' is a variable node that has not been bound yet.
	const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	// 3.0 > 4		=>		3.0 > 4.0
	if(a_type.nonNull() && a_type->getType() == Type::DoubleType && b->nodeType() == ASTNode::IntLiteralType)
	{
		const IntLiteral* b_lit = b.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsDouble(b_lit->value))
		{
			b = new DoubleLiteral((double)b_lit->value, b->srcLocation());
			payload.tree_changed = true;
		}
	}

	// 3 > 4.0      =>        3.0 > 4.0
	if(b_type.nonNull() && b_type->getType() == Type::DoubleType && a->nodeType() == ASTNode::IntLiteralType)
	{
		const IntLiteral* a_lit = a.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsFloat(a_lit->value))
		{
			a = new DoubleLiteral((double)a_lit->value, a->srcLocation());
			payload.tree_changed = true;
		}
	}
}


static void doImplicitIntTypeCoercion(ASTNodeRef& a, ASTNodeRef& b, TraversalPayload& payload)
{
	// Type may be null if 'a' is a variable node that has not been bound yet.
	const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	if(a_type.nonNull() && a_type->getType() == Type::IntType && b_type.nonNull() && b_type->getType() == Type::IntType)
	{
		const Int* a_int_type = a_type.downcastToPtr<Int>();
		const Int* b_int_type = b_type.downcastToPtr<Int>();

		// 3i64 > 4i32		=>		3i64 > 4i64
		if((a_int_type->numBits() == 64) && (b_int_type->numBits() == 32) && (b->nodeType() == ASTNode::IntLiteralType))
		{
			b = new IntLiteral(b.downcastToPtr<IntLiteral>()->value, 64, b.downcastToPtr<IntLiteral>()->is_signed, b->srcLocation());
			payload.tree_changed = true;
		}

		// 3i32 > 4i64      =>        3i64 > 4i64
		if((b_int_type->numBits() == 64) && (a_int_type->numBits() == 32) && (a->nodeType() == ASTNode::IntLiteralType))
		{
			a = new IntLiteral(a.downcastToPtr<IntLiteral>()->value, 64, a.downcastToPtr<IntLiteral>()->is_signed, b->srcLocation());
			payload.tree_changed = true;
		}
	}
}



static bool canDoImplicitIntToFloatTypeCoercion(const ASTNodeRef& a, const ASTNodeRef& b)
{
	// Type may be null if 'a' is a variable node that has not been bound yet.
	const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	// TODO: Handle integer bitness

	if(a_type.nonNull() && b_type.nonNull())
	{
		// 3.0 > 4		=>		3.0 > 4.0
		if(a_type->getType() == Type::FloatType && b_type->getType() == Type::IntType)
			return true;

		// 3 > 4.0      =>        3.0 > 4.0
		if(a_type->getType() == Type::IntType && b_type->getType() == Type::FloatType)
			return true;
	}

	//// 3.0 > 4		=>		3.0 > 4.0
	//if(a_type.nonNull() && a_type->getType() == Type::FloatType && b_type.nonNull() && b_type->getType() == Type::IntType) //b->nodeType() == ASTNode::IntLiteralType)
	//	//if(isIntExactlyRepresentableAsFloat(b.downcastToPtr<IntLiteral>()->value))
	//		return true;

	//// 3 > 4.0      =>        3.0 > 4.0
	//if(b_type.nonNull() && b_type->getType() == Type::FloatType && a_type.nonNull() && a_type->getType() == Type::IntType) // a->nodeType() == ASTNode::IntLiteralType)
	//	//if(isIntExactlyRepresentableAsFloat(a.downcastToPtr<IntLiteral>()->value))
	//		return true;

	return false;
}


static bool canDoImplicitIntToDoubleTypeCoercion(const ASTNodeRef& a, const ASTNodeRef& b)
{
	// Type may be null if 'a' is a variable node that has not been bound yet.
	const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	if(a_type.nonNull() && b_type.nonNull())
	{
		// 3.0 > 4		=>		3.0 > 4.0
		if(a_type->getType() == Type::DoubleType && b_type->getType() == Type::IntType)
			return true;

		// 3 > 4.0      =>        3.0 > 4.0
		if(a_type->getType() == Type::IntType && b_type->getType() == Type::DoubleType)
			return true;
	}

	return false;
}


void doImplicitIntToFloatTypeCoercionForFloatReturn(ASTNodeRef& expr, TraversalPayload& payload)
{
	const FunctionDefinition* current_func = payload.func_def_stack.back();

	if(expr->nodeType() == ASTNode::IntLiteralType && 
		current_func->declared_return_type.nonNull() && current_func->declared_return_type->getType() == Type::FloatType
		)
	{
		const IntLiteral* body_lit = expr.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsFloat(body_lit->value))
		{
			expr = new FloatLiteral((float)body_lit->value, body_lit->srcLocation());
			payload.tree_changed = true;
		}
	}
}


void doImplicitIntToDoubleTypeCoercionForDoubleReturn(ASTNodeRef& expr, TraversalPayload& payload)
{
	const FunctionDefinition* current_func = payload.func_def_stack.back();

	if(expr->nodeType() == ASTNode::IntLiteralType && 
		current_func->declared_return_type.nonNull() && current_func->declared_return_type->getType() == Type::DoubleType
		)
	{
		const IntLiteral* body_lit = expr.downcastToPtr<IntLiteral>();
		if(isIntExactlyRepresentableAsDouble(body_lit->value))
		{
			expr = new DoubleLiteral((double)body_lit->value, body_lit->srcLocation());
			payload.tree_changed = true;
		}
	}
}


//static bool isIntLiteralAndExactlyRepresentableAsFloat(const ASTNodeRef& node)
//{
//	return node->nodeType() == ASTNode::IntLiteralType && isIntExactlyRepresentableAsFloat(node.downcastToPtr<IntLiteral>()->value);
//}


/*
Do two expressions have the same value?

Cases where they have the same value:

both variable nodes that refer to the same variable.

*/
bool expressionsHaveSameValue(const ASTNodeRef& a, const ASTNodeRef& b)
{
	if(a->nodeType() == ASTNode::VariableASTNodeType && b->nodeType() == ASTNode::VariableASTNodeType)
	{
		const Variable* avar = static_cast<const Variable*>(a.getPointer());
		const Variable* bvar = static_cast<const Variable*>(b.getPointer());

		if(avar->binding_type != bvar->binding_type)
			return false;

		if(avar->binding_type == Variable::ArgumentVariable)
		{
			return 
				avar->bound_function == bvar->bound_function && 
				avar->arg_index == bvar->arg_index;
		}
		else if(avar->binding_type == Variable::LetVariable)
		{
			return 
				avar->bound_let_node == bvar->bound_let_node && 
				avar->let_var_index == bvar->let_var_index;
		}
		else
		{
			// TODO: captured vars etc..
			assert(0);
		}
	}

	return false;
}


void emitDestructorOrDecrCall(EmitLLVMCodeParams& params, const ASTNode& e, llvm::Value* value, const std::string& comment)
{
	const TypeRef& type = e.type();
	if(type->isHeapAllocated())
		type->emitDecrRefCount(params, value, comment);
	else
		type->emitDestructorCall(params, value, comment);
}


bool mayEscapeCurrentlyBuildingFunction(EmitLLVMCodeParams& params, const TypeRef& type)
{
	const TypeRef return_type = params.currently_building_func_def->returnType();

	// If the type is the same as the function return type, or is contained by the return type (e.g. is a field of the return type), return true.
	if(isEqualToOrContains(*return_type, *type))
		return true;

	// Return true if the type may be captured by a closure being returned.
	if(params.currently_building_func_def->returnType()->getType() == Type::FunctionType)
	{
		for(auto i = params.currently_building_func_def->captured_var_types.begin(); i != params.currently_building_func_def->captured_var_types.end(); ++i)
		{
			const TypeRef captured_var_type = *i;

			if(isEqualToOrContains(*captured_var_type, *type)) // (*captured_var_type == *type) || captured_var_type->containsType(*this_type))
				return true;
		}
	}
	
	return false;
}


/*void replaceAllUsesWith(Reference<ASTNode>& old_node, Reference<ASTNode>& new_node)
{
	new_node->uprefs.resize(old_node->uprefs.size());
	for(size_t i=0; i<old_node->uprefs.size(); ++i)
	{
		old_node->uprefs[i]->updateTarget(new_node.getPointer());
		new_node->uprefs[i] = old_node->uprefs[i];
	}
}*/


Reference<ASTNode> cloneASTNodeSubtree(Reference<ASTNode>& n)
{
	TraversalPayload update_refs_payload(TraversalPayload::UpdateUpRefs);

	Reference<ASTNode> cloned = n->clone(update_refs_payload.clone_map);

	// Update all uprefs in the cloned body to point to the cloned body instead of the original body.
	std::vector<ASTNode*> stack; // NOTE: does it matter that stack is empty here?
	cloned->traverse(update_refs_payload, stack);

	return cloned;
}

//----------------------------------------------------------------------------------


//void ASTNode::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
//{
//	assert(0);
//}


/*
ASTNode::ASTNode()
{
	
}


ASTNode::~ASTNode()
{
	
}*/


//----------------------------------------------------------------------------------


void BufferRoot::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.resize(0);
	stack.push_back(this);

	//for(unsigned int i=0; i<func_defs.size(); ++i)
	//	func_defs[i]->traverse(payload, stack);

	for(unsigned int i=0; i<top_level_defs.size(); ++i)
		top_level_defs[i]->traverse(payload, stack);

	stack.pop_back();
}


void BufferRoot::print(int depth, std::ostream& s) const
{
	//s << "========================================================\n";
	for(unsigned int i=0; i<top_level_defs.size(); ++i)
	{
		top_level_defs[i]->print(depth+1, s);
		s << "\n";
	}
}


std::string BufferRoot::sourceString() const
{
	std::string s;
	for(unsigned int i=0; i<top_level_defs.size(); ++i)
	{
		//s += top_level_defs[i]->sourceString();

		switch(top_level_defs[i]->nodeType())
		{
		case Winter::ASTNode::FunctionDefinitionType:
			{
				if(top_level_defs[i].downcastToPtr<Winter::FunctionDefinition>()->body.nonNull()) // If not a built-in function:
				{
					std::string func_src = top_level_defs[i]->sourceString();
					s += func_src;
					s += "\n";
				}
				break;
			}
		default:
			{
				std::string node_src = top_level_defs[i]->sourceString();
				s += node_src;
				s += "\n";
				break;
			}
		};

		s += "\n";
	}
	return s;
}


std::string BufferRoot::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	std::string s;
	for(unsigned int i=0; i<top_level_defs.size(); ++i)
	{
		s += top_level_defs[i]->emitOpenCLC(params);
		s += "\n";
	}
	return s;
}



llvm::Value* BufferRoot::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	assert(0);
	return NULL;
}


Reference<ASTNode> BufferRoot::clone(CloneMapType& clone_map)
{
	throw BaseException("BufferRoot::clone()");
}


bool BufferRoot::isConstant() const
{
	assert(0);
	return false;
}


size_t BufferRoot::getTimeBound(GetTimeBoundParams& params) const
{
	assert(0);
	return 0;
}


GetSpaceBoundResults BufferRoot::getSpaceBound(GetSpaceBoundParams& params) const
{
	assert(0);
	return GetSpaceBoundResults(0, 0);
}


//----------------------------------------------------------------------------------


const std::string errorContext(const ASTNode* n)
{
	return errorContext(*n);
}


const std::string errorContext(const ASTNode& n)
{
	//if(!payload.func_def_stack.empty())
	//	return "In function " + payload.func_def_stack[payload.func_def_stack.size() - 1]->sig.toString();
	//return "";

	/*for(int i=(int)payload.func_def_stack.size() - 1; i >= 0; --i)
	{
	s +=  "In function " + payload.func_def_stack[i]->sig.toString();
	}*/

	return errorContext(n.srcLocation());
}


const std::string errorContext(const SrcLocation& src_location)
{
	const SourceBuffer* source_buffer = src_location.source_buffer;
	if(source_buffer == NULL)
		return "Invalid Location";

	return Diagnostics::positionString(*source_buffer, src_location.char_index);
}


const std::string errorContext(const ASTNode& n, TraversalPayload& payload)
{
	return errorContext(n);
}


bool isTargetDefinedBeforeAllInStack(const std::vector<FunctionDefinition*>& func_def_stack, int target_function_order_num)
{
	if(target_function_order_num == -1) // If target is a built-in function etc.. then there are no ordering problems.
		return true;

	for(size_t i=0; i<func_def_stack.size(); ++i)
		if(target_function_order_num >= func_def_stack[i]->order_num)
			return false;

	return true;
}


//------------------------------------------------------------------------------------


/*static const std::string floatValueString(float x)
{
	const std::string s = toString(x);

	// OpenCL seems fussy about types so make sure we have a 'f' suffix on our float literals.
	if(StringUtils::containsChar(s, '.'))
		return s + "f"; // e.g '2.3' -> '2.3f'
	else
		return s + ".f"; // e.g. '2'  ->  '2.f'
}*/


ValueRef FloatLiteral::exec(VMState& vmstate)
{
	return new FloatValue(value);
}


void FloatLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Float literal: " + floatLiteralString(this->value) + "\n";
}


std::string FloatLiteral::sourceString() const
{
	return floatLiteralString(this->value);
}


std::string FloatLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return floatLiteralString(this->value);
}


llvm::Value* FloatLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	return llvm::ConstantFP::get(
		*params.context, 
		llvm::APFloat(this->value)
	);
}


Reference<ASTNode> FloatLiteral::clone(CloneMapType& clone_map)
{
	FloatLiteral* res = new FloatLiteral(value, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


//------------------------------------------------------------------------------------


ValueRef DoubleLiteral::exec(VMState& vmstate)
{
	return new DoubleValue(value);
}


void DoubleLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Double literal: " + doubleLiteralString(this->value) + "\n";
}


std::string DoubleLiteral::sourceString() const
{
	return doubleLiteralString(this->value);
}


std::string DoubleLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return doubleLiteralString(this->value);
}


llvm::Value* DoubleLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	return llvm::ConstantFP::get(
		*params.context, 
		llvm::APFloat(this->value)
	);
}


Reference<ASTNode> DoubleLiteral::clone(CloneMapType& clone_map)
{
	DoubleLiteral* res = new DoubleLiteral(value, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


//------------------------------------------------------------------------------------


ValueRef IntLiteral::exec(VMState& vmstate)
{
	return new IntValue(value, is_signed);
}


void IntLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Int literal: " << this->value << ", signed: " << is_signed << "\n";
}


std::string IntLiteral::sourceString() const
{
	if(num_bits == 32)
	{
		if(is_signed)
			return toString(this->value);
		else
			return toString(this->value) + "u";
	}
	else
	{
		if(is_signed)
			return toString(this->value) + "i" + toString(num_bits);
		else
			return toString(this->value) + "u" + toString(num_bits);
	}
}


std::string IntLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	// NOTE: How does OpenCL handle literals with num bits > 32?
	if(is_signed)
		return toString(this->value);
	else
		return toString(this->value) + "u";
}


llvm::Value* IntLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			this->num_bits, // num bits
			this->value, // value
			this->is_signed // signed
		)
	);
}


Reference<ASTNode> IntLiteral::clone(CloneMapType& clone_map)
{
	IntLiteral* res = new IntLiteral(value, num_bits, is_signed, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


GetSpaceBoundResults IntLiteral::getSpaceBound(GetSpaceBoundParams& params) const
{
	return GetSpaceBoundResults(0, 0);
}


//-------------------------------------------------------------------------------------


ValueRef BoolLiteral::exec(VMState& vmstate)
{
	return new BoolValue(value);
}


void BoolLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Bool literal: " + boolToString(this->value) + "\n";
}


std::string BoolLiteral::sourceString() const
{
	return boolToString(this->value);
}


std::string BoolLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return boolToString(this->value);
}


llvm::Value* BoolLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			1, // num bits
			this->value ? 1 : 0, // value
			false // signed
		)
	);
}


Reference<ASTNode> BoolLiteral::clone(CloneMapType& clone_map)
{
	BoolLiteral* res = new BoolLiteral(value, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


//----------------------------------------------------------------------------------------------


ValueRef MapLiteral::exec(VMState& vmstate)
{
/*	std::map<Value*, Value*> m;
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->exec(vmstate);
		Value* key = vmstate.working_stack.back();
		vmstate.working_stack.pop_back();

		this->items[i].second->exec(vmstate);
		Value* value = vmstate.working_stack.back();
		vmstate.working_stack.pop_back();

		m.insert(std::make_pair(key, value));
	}

	return new MapValue(m);
	*/
	assert(0);
	return ValueRef();
}


void MapLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Map literal\n";

	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		printMargin(depth+1, s);
		s << "Key:\n";
		this->items[i].first->print(depth+2, s);

		printMargin(depth+1, s);
		s << "Value:\n";
		this->items[i].second->print(depth+2, s);
	}
}


std::string MapLiteral::sourceString() const
{
	assert(0);
	return "";
}


std::string MapLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	assert(0);
	return "";
}


void MapLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<items.size(); ++i)
		{
			checkFoldExpression(items[i].first, payload);
			checkFoldExpression(items[i].second, payload);
		}
	}
	else */if(payload.operation == TraversalPayload::BindVariables)
	{
		for(size_t i=0; i<items.size(); ++i)
		{
			convertOverloadedOperators(items[i].first, payload, stack);
			convertOverloadedOperators(items[i].second, payload, stack);
		}
	}


	stack.push_back(this);
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->traverse(payload, stack);
		this->items[i].second->traverse(payload, stack);
	}
	stack.pop_back();
}


llvm::Value* MapLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	return NULL;
}


Reference<ASTNode> MapLiteral::clone(CloneMapType& clone_map)
{
	MapLiteral* m = new MapLiteral(srcLocation());
	m->maptype = this->maptype;
	for(size_t i=0; i<items.size(); ++i)
		m->items.push_back(std::make_pair(items[0].first->clone(clone_map), items[0].second->clone(clone_map)));

	clone_map.insert(std::make_pair(this, m));
	return m;
}


bool MapLiteral::isConstant() const
{
	for(size_t i=0; i<items.size(); ++i)
		if(!items[i].first->isConstant() || !items[i].second->isConstant())
			return false;
	return true;
}


size_t MapLiteral::getTimeBound(GetTimeBoundParams& params) const
{
	size_t sum = 0;
	for(size_t i=0; i<items.size(); ++i)
		sum += items[i].first->getTimeBound(params) + items[i].second->getTimeBound(params);
	return sum;
}


GetSpaceBoundResults MapLiteral::getSpaceBound(GetSpaceBoundParams& params) const
{
	GetSpaceBoundResults sum(0, 0);
	for(size_t i=0; i<items.size(); ++i)
		sum += items[i].first->getSpaceBound(params) + items[i].second->getSpaceBound(params);

	sum += GetSpaceBoundResults(0, 0);
	return sum;
}


//----------------------------------------------------------------------------------------------


StringLiteral::StringLiteral(const std::string& v, const SrcLocation& loc) 
:	ASTNode(StringLiteralType, loc), value(v), llvm_allocated_on_heap(false)
{
	this->can_maybe_constant_fold = true;
}


ValueRef StringLiteral::exec(VMState& vmstate)
{
	return new StringValue(value);
}


void StringLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "String literal: '" << this->value << "'\n";
}


std::string StringLiteral::sourceString() const
{
	return "\"" + this->value + "\"";
}


std::string StringLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	assert(0);
	return "";
}


void StringLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new VoidPtrType()));

		// Try and resolve to internal function.
		this->allocateStringFunc = payload.linker->findMatchingFunction(allocateStringSig).getPointer();

		assert(this->allocateStringFunc);



		const FunctionSignature freeStringSig("freeString", vector<TypeRef>(1, new String()));

		// Try and resolve to internal function.
		this->freeStringFunc = payload.linker->findMatchingFunction(freeStringSig).getPointer();

		assert(this->freeStringFunc);
	}*/
}


llvm::Value* StringLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// Make a global constant character array for the string data.
	llvm::Value* string_global = params.builder->CreateGlobalString(this->value);

	// Get a pointer to the zeroth elem
	llvm::Value* elem_0 = LLVMUtils::createStructGEP(params.builder, string_global, 0);


	const bool alloc_on_heap = mayEscapeCurrentlyBuildingFunction(params, this->type());
	this->llvm_allocated_on_heap = alloc_on_heap;

	llvm::Value* string_value;
	uint64 initial_flags;
	if(alloc_on_heap)
	{
		params.stats->num_heap_allocation_calls++;

		llvm::Value* elem_bitcast = params.builder->CreateBitCast(elem_0, LLVMTypeUtils::voidPtrType(*params.context));

		// Emit a call to allocateString
		llvm::Function* allocateStringLLVMFunc = params.common_functions.allocateStringFunc->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
		);

		llvm::SmallVector<llvm::Value*, 4> args(1, elem_bitcast);
		
		// Set hidden voidptr argument
		/*const bool target_takes_voidptr_arg = true;
		if(target_takes_voidptr_arg)
			args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));*/


		//allocateStringLLVMFunc->dump(); // TEMP

		//args[0]->dump();
		//args[1]->dump();

		llvm::CallInst* call_inst = params.builder->CreateCall(allocateStringLLVMFunc, args, "str");

		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);
		
		string_value = call_inst;
		initial_flags = 1;// heap allocated
	}
	else
	{
		// Allocate space on stack for array.
		// Allocate as just an array of bytes.
		// Then cast to the needed type.  We do this because our LLVM Varray type has only zero length for the actual data, so can't be used for the alloca.
		
		// Emit the alloca in the entry block for better code-gen.
		// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
		llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

		const uint64 total_string_size_B = sizeof(uint64)*3 + value.size();

		//llvm::Value* alloca_ptr = entry_block_builder.CreateAlloca(
		//	llvm::Type::getInt8Ty(*params.context), // byte
		//	llvm::ConstantInt::get(*params.context, llvm::APInt(64, total_string_size_B, true)), // number of bytes needed.
		//	"string_stack_space"
		//);
		llvm::Value* alloca_ptr = entry_block_builder.Insert(new llvm::AllocaInst(
			llvm::Type::getInt8Ty(*params.context), // type - byte
#if TARGET_LLVM_VERSION >= 60
			0, // address space
#endif
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, total_string_size_B, true)), // number of bytes needed.
			8, // alignment
			"string_stack_space"
		));


		// Cast resulting allocated uint8* to string type.
		llvm::Type* string_type = this->type()->LLVMType(*params.module);
		assert(string_type->isPointerTy());
		string_value = params.builder->CreatePointerCast(alloca_ptr, string_type);

		// Emit a memcpy from the global data to the string value
		llvm::Value* data_ptr = LLVMUtils::createStructGEP(params.builder, string_value, 3, "string_literal_data_ptr");

		params.builder->CreateMemCpy(data_ptr, elem_0, value.size(), /*align=*/1);

		initial_flags = 0; // flag = 0 = not heap allocated
	}


	// Set the reference count to 1
	llvm::Value* ref_ptr = LLVMUtils::createStructGEP(params.builder, string_value, 0, "string_ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, /*signed=*/true));
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, ref_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "string literal set intial ref count to 1");

	// Set the length field
	llvm::Value* len_ptr = LLVMUtils::createStructGEP(params.builder, string_value, 1, "string_len_ptr");
	llvm::Value* len_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, this->value.size(), /*signed=*/true));
	llvm::StoreInst* len_store_inst = params.builder->CreateStore(len_val, len_ptr);
	addMetaDataCommentToInstruction(params, len_store_inst, "string literal set intial length to " + toString(this->value.size()));

	// Set the flags
	llvm::Value* flags_ptr = LLVMUtils::createStructGEP(params.builder, string_value, 2, "string_literal_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "string literal set intial flags to " + toString(initial_flags));

	return string_value;
}


//void StringLiteral::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const
//{
//	//RefCounting::emitStringCleanupLLVMCode(params, string_val);
//}


Reference<ASTNode> StringLiteral::clone(CloneMapType& clone_map)
{
	StringLiteral* res = new StringLiteral(value, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


size_t StringLiteral::getTimeBound(GetTimeBoundParams& params) const
{
	return value.size();
}


GetSpaceBoundResults StringLiteral::getSpaceBound(GetSpaceBoundParams& params) const
{
	// Currently strings may be allocated either on the stack or the heap.
	// If the string is allocated on the heap, then we have to bound the stack space that allocateString will take.
	return GetSpaceBoundResults(llvm_allocated_on_heap ? 1024 : 0, /*heap_space=*/llvm_allocated_on_heap ? (sizeof(StringRep) + value.size()) : 0);
}


//-----------------------------------------------------------------------------------------------


CharLiteral::CharLiteral(const std::string& v, const SrcLocation& loc) 
:	ASTNode(CharLiteralType, loc), value(v)
{
	this->can_maybe_constant_fold = true;
}


ValueRef CharLiteral::exec(VMState& vmstate)
{
	return new CharValue(value);
}


void CharLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Char literal: '" << this->value << "'\n";
}


std::string CharLiteral::sourceString() const
{
	return "'" + this->value + "'";
}


std::string CharLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "'" + this->value + "'";
}


void CharLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
}


llvm::Value* CharLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	assert(this->value.size() >= 1 && this->value.size() <= 4);

	uint64 val = 0;
	std::memcpy(&val, &this->value[0], this->value.size());

	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			32, // num bits
			val, // value
			false // signed
		)
	);
}


//void CharLiteral::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const
//{
//	//emitStringCleanupLLVMCode(params, string_val);
//}


Reference<ASTNode> CharLiteral::clone(CloneMapType& clone_map)
{
	CharLiteral* res = new CharLiteral(value, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


//-----------------------------------------------------------------------------------------------


class AddOp
{
public:
	float operator() (float x, float y) { return x + y; }
	double operator() (double x, double y) { return x + y; }
	int64 operator() (int64 x, int64 y) { return x + y; }
};


class SubOp
{
public:
	float operator() (float x, float y) { return x - y; }
	double operator() (double x, double y) { return x - y; }
	int64 operator() (int64 x, int64 y) { return x - y; }
};


class MulOp
{
public:
	float operator() (float x, float y) { return x * y; }
	double operator() (double x, double y) { return x * y; }
	int64 operator() (int64 x, int64 y) { return x * y; }
};


template <class Op>
ValueRef execBinaryOp(VMState& vmstate, ASTNodeRef& a, ASTNodeRef& b, Op op)
{
	const ValueRef aval = a->exec(vmstate);
	const ValueRef bval = b->exec(vmstate);

	switch(a->type()->getType())
	{
	case Type::FloatType:
		{
			if(b->type()->getType() == Type::VectorTypeType) // float * vector<float, N>
			{
				const VectorValue* bval_vec = checkedCast<VectorValue>(bval);

				vector<ValueRef> elem_values(bval_vec->e.size());
				for(unsigned int i=0; i<elem_values.size(); ++i)
					elem_values[i] = new FloatValue(op(
						checkedCast<FloatValue>(aval)->value,
						checkedCast<FloatValue>(bval_vec->e[i])->value
					));
				return new VectorValue(elem_values);
			}
			else if(b->type()->getType() == Type::FloatType) // Else float * float
			{
				return new FloatValue(op(
					checkedCast<FloatValue>(aval)->value,
					checkedCast<FloatValue>(bval)->value
				));
			}
			else
				throw BaseException("Invalid types to binary op.");
		}
	case Type::DoubleType:
		{
			if(b->type()->getType() == Type::VectorTypeType) // double * vector<double, N>
			{
				const VectorValue* bval_vec = checkedCast<VectorValue>(bval);

				vector<ValueRef> elem_values(bval_vec->e.size());
				for(unsigned int i=0; i<elem_values.size(); ++i)
					elem_values[i] = new DoubleValue(op(
						checkedCast<DoubleValue>(aval)->value,
						checkedCast<DoubleValue>(bval_vec->e[i])->value
					));
				return new VectorValue(elem_values);
			}
			else if(b->type()->getType() == Type::DoubleType) // Else double * double
			{
				return new DoubleValue(op(
					checkedCast<DoubleValue>(aval)->value,
					checkedCast<DoubleValue>(bval)->value
				));
			}
			else
				throw BaseException("Invalid types to binary op.");
		}
	case Type::IntType:
		{
			if(b->type()->getType() == Type::VectorTypeType) // int * vector
			{
				const VectorValue* bval_vec = checkedCast<VectorValue>(bval);

				vector<ValueRef> elem_values(bval_vec->e.size());
				for(unsigned int i=0; i<elem_values.size(); ++i)
					elem_values[i] = new IntValue(op(
							checkedCast<IntValue>(aval)->value,
							checkedCast<IntValue>(bval_vec->e[i])->value
						),
						checkedCast<IntValue>(aval)->is_signed
					);
				return new VectorValue(elem_values);
			}
			else if(b->type()->getType() == Type::IntType) // Else int * int
			{
				return new IntValue(op(
						checkedCast<IntValue>(aval)->value,
						checkedCast<IntValue>(bval)->value
					),
					checkedCast<IntValue>(aval)->is_signed
				);
			}
			else
				throw BaseException("Invalid types to binary op.");
		}
	case Type::VectorTypeType:
		{
			const TypeRef this_type = a->type();

			const VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

			const VectorValue* aval_vec = checkedCast<VectorValue>(aval);
		
			vector<ValueRef> elem_values(aval_vec->e.size());
			switch(vectype->elem_type->getType()) // Swith on 'a' element type:
			{
			case Type::FloatType:
				{
					if(b->type()->getType() == Type::VectorTypeType) // Vector<float, N> * vector<float, N>
					{
						if(b->type().downcast<VectorType>()->num != vectype->num)
							throw BaseException("Invalid types to binary op.");

						const VectorValue* bval_vec = checkedCast<VectorValue>(bval);
						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new FloatValue(op(
								checkedCast<FloatValue>(aval_vec->e[i])->value,
								checkedCast<FloatValue>(bval_vec->e[i])->value
							));
					}
					else if(b->type()->getType() == Type::FloatType) // Vector<float, N> * float
					{
						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new FloatValue(op(
								checkedCast<FloatValue>(aval_vec->e[i])->value,
								checkedCast<FloatValue>(bval)->value
							));
					}
					else
					{
						throw BaseException("Invalid types to binary op.");
					}
					break;
				}
			case Type::DoubleType:
				{
					if(b->type()->getType() == Type::VectorTypeType) // Vector<double, N> * vector<double, N>
					{
						if(b->type().downcast<VectorType>()->num != vectype->num)
							throw BaseException("Invalid types to binary op.");

						const VectorValue* bval_vec = checkedCast<VectorValue>(bval);
						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new DoubleValue(op(
								checkedCast<DoubleValue>(aval_vec->e[i])->value,
								checkedCast<DoubleValue>(bval_vec->e[i])->value
							));
					}
					else if(b->type()->getType() == Type::DoubleType) // Vector<double, N> * double
					{
						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new DoubleValue(op(
								checkedCast<DoubleValue>(aval_vec->e[i])->value,
								checkedCast<DoubleValue>(bval)->value
							));
					}
					else
					{
						throw BaseException("Invalid types to binary op.");
					}
					break;
				}
			case Type::IntType:
				{
					if(b->type()->getType() == Type::VectorTypeType) // Vector<int, N> * vector<int, N>
					{
						const VectorValue* bval_vec = checkedCast<VectorValue>(bval);

						if(b->type().downcast<VectorType>()->num != vectype->num)
							throw BaseException("Invalid types to binary op.");

						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new IntValue(op(
									checkedCast<IntValue>(aval_vec->e[i])->value,
									checkedCast<IntValue>(bval_vec->e[i])->value
								),
								checkedCast<IntValue>(aval_vec->e[i])->is_signed
							);
					}
					else if(b->type()->getType() == Type::IntType) // Vector<int, N> * int
					{
						for(unsigned int i=0; i<elem_values.size(); ++i)
							elem_values[i] = new IntValue(op(
								checkedCast<IntValue>(aval_vec->e[i])->value,
								checkedCast<IntValue>(bval)->value
								),
								checkedCast<IntValue>(aval_vec->e[i])->is_signed
							);
					}
					else
					{
						throw BaseException("Invalid types to binary op.");
					}
					break;
				}
			default:
				throw BaseException("expression vector field type invalid!");
			};
			return new VectorValue(elem_values);
		}
	default:
		throw BaseException("expression type invalid!");
	}
}


ValueRef AdditionExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, AddOp());
}


void AdditionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Addition Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


std::string AdditionExpression::sourceString() const
{
	return "(" + a->sourceString() + " + " + b->sourceString() + ")";
}


std::string AdditionExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + " + " + b->emitOpenCLC(params) + ")";
}


TypeRef AdditionExpression::type() const
{
	if(expr_type.isNull())
	{
		if(canDoImplicitIntToFloatTypeCoercion(a, b))
			expr_type = new Float();
		else if(canDoImplicitIntToDoubleTypeCoercion(a, b))
			expr_type = new Double();
		else
			expr_type = a->type();
	}
	return expr_type;
}


void AdditionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/
	

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		doImplicitIntToFloatTypeCoercion(a, b, payload);
		doImplicitIntToDoubleTypeCoercion(a, b, payload);

		// implicit conversion from int to float in addition operation:
		// 3.0 + 4
		/*if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 + 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}*/
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType || this_type->getType() == Type::DoubleType)
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(a_type->getType() == Type::VectorTypeType && b_type->getType() == Type::VectorTypeType) // Vector + vector addition.
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));

			// Check element type is int or float
			if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || a_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
				throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else
		{
			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold; // && expressionIsWellTyped(*this, payload);
		/*if(this->can_constant_fold)
		{
			try
			{
				typeCheck(payload);
			}
			catch(BaseException&)
			{
				this->can_constant_fold = false;
			}
		}*/
		//if(this->can_constant_fold)
		//{
		//a->print(0, std::cout);
		//b->print(0, std::cout);
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
		//}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


//bool AdditionExpression::typeCheck(TraversalPayload& payload) const
//{
//	const TypeRef& this_type = this->type();
//	if(this_type.isNull())
//		throw BaseException("Unknown operand type." + errorContext(*this, payload));
//
//	const TypeRef a_type = a->type();
//	const TypeRef b_type = b->type();
//
//	if(a_type.isNull() || b_type.isNull())
//		throw BaseException("AdditionExpression: Binary operator '+' not defined for Unknown types." + errorContext(*this, payload));
//
//	if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType)
//	{
//		if(*a_type != *b_type)
//			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
//	}
//	else if(a_type->getType() == Type::VectorTypeType && b_type->getType() == Type::VectorTypeType) // Vector + vector addition.
//	{
//		if(*a_type != *b_type)
//			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
//
//		// Check element type is int or float
//		if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType))
//			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
//	}
//	else
//	{
//		throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'." + errorContext(*this, payload));
//	}
//}


llvm::Value* AdditionExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::VectorTypeType)
	{
		const TypeRef elem_type = this->type().downcast<VectorType>()->elem_type;
		if(elem_type->getType() == Type::FloatType || elem_type->getType() == Type::DoubleType)
		{
			return params.builder->CreateFAdd(
				a->emitLLVMCode(params), 
				b->emitLLVMCode(params)
			);
		}
		else if(elem_type->getType() == Type::IntType)
		{
			return params.builder->CreateAdd(
				a->emitLLVMCode(params), 
				b->emitLLVMCode(params)
			);
		}
		else
		{
			assert(0);
			return NULL;
		}
	}
	else if(this->type()->getType() == Type::FloatType || this->type()->getType() == Type::DoubleType)
	{
		return params.builder->CreateFAdd(
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateAdd(
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for AdditionExpression code emission");
	}
}


Reference<ASTNode> AdditionExpression::clone(CloneMapType& clone_map)
{
	AdditionExpression* res = new AdditionExpression(this->srcLocation(), this->a->clone(clone_map), this->b->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool AdditionExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


size_t AdditionExpression::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	const size_t op_cost = (type()->getType() == Type::VectorTypeType) ? (type().downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;

	return a->getTimeBound(params) + b->getTimeBound(params) + op_cost;
}


GetSpaceBoundResults AdditionExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}



//-------------------------------------------------------------------------------------------------


ValueRef SubtractionExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, SubOp());
}


void SubtractionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Subtraction Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


std::string SubtractionExpression::sourceString() const
{
	return "(" + a->sourceString() + " - " + b->sourceString() + ")";
}


std::string SubtractionExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + " - " + b->emitOpenCLC(params) + ")";
}


TypeRef SubtractionExpression::type() const
{
	if(expr_type.isNull())
	{
		if(canDoImplicitIntToFloatTypeCoercion(a, b))
			expr_type = new Float();
		else if(canDoImplicitIntToDoubleTypeCoercion(a, b))
			expr_type = new Double();
		else
			expr_type = a->type();
	}
	return expr_type;
}


void SubtractionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		doImplicitIntToFloatTypeCoercion(a, b, payload);
		doImplicitIntToDoubleTypeCoercion(a, b, payload);

		// implicit conversion from int to float
		// 3.0 - 4
		/*if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 - 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}*/
	}

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType || this_type->getType() == Type::DoubleType)
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '-' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(a_type->getType() == Type::VectorTypeType && b_type->getType() == Type::VectorTypeType) // Vector + vector addition.
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '-' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));

			// Check element type is int or float
			if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || a_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
				throw BaseException("AdditionExpression: Binary operator '-' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else
		{
			throw BaseException("AdditionExpression: Binary operator '-' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold;// && expressionIsWellTyped(*this, payload);
		//if(this->can_constant_fold)
		//{
		//	try
		//	{
		//		typeCheck(payload);
		//	}
		//	catch(BaseException&)
		//	{
		//		this->can_constant_fold = false;
		//	}
		//}
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


//bool SubtractionExpression::typeCheck(TraversalPayload& payload) const
//{
//	const TypeRef& this_type = this->type();
//	if(this_type.isNull())
//		throw BaseException("Unknown operand type." + errorContext(*this, payload));
//
//	const TypeRef a_type = a->type();
//	const TypeRef b_type = b->type();
//	if(a_type.isNull() || b_type.isNull())
//		throw BaseException("SubtractionExpression: Binary operator '-' not defined for Unknown types." + errorContext(*this, payload));
//
//	if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType)
//	{
//		if(*a->type() != *b->type())
//			throw BaseException("SubtractionExpression: Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//	}
//	else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType) // Vector + vector addition.
//	{
//		if(*a->type() != *b->type())
//			throw BaseException("SubtractionExpression: Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//
//		// Check element type is int or float
//		if(!(a->type().downcast<VectorType>()->elem_type->getType() == Type::IntType || a->type().downcast<VectorType>()->elem_type->getType() == Type::FloatType))
//			throw BaseException("SubtractionExpression: Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//	}
//	else
//	{
//		throw BaseException("SubtractionExpression: Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
//	}
//}


llvm::Value* SubtractionExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::FloatType || this->type()->getType() == Type::DoubleType || 
		(this->type()->getType() == Type::VectorTypeType && this->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(this->type()->getType() == Type::VectorTypeType && this->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType))
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FSub, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType || (this->type()->getType() == Type::VectorTypeType && this->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::IntType))
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Sub, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for SubtractionExpression code emission");
	}
}


Reference<ASTNode> SubtractionExpression::clone(CloneMapType& clone_map)
{
	SubtractionExpression* res = new SubtractionExpression(this->srcLocation(), this->a->clone(clone_map), this->b->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool SubtractionExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


size_t SubtractionExpression::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	const size_t op_cost = (type()->getType() == Type::VectorTypeType) ? (type().downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;

	return a->getTimeBound(params) + b->getTimeBound(params) + op_cost;
}


GetSpaceBoundResults SubtractionExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}


//-------------------------------------------------------------------------------------------------------


ValueRef MulExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, MulOp());
}


TypeRef MulExpression::type() const
{
	if(expr_type.isNull())
	{
		// For cases like vector<float, n> * float, we want to return the vector type.
		const TypeRef a_type = a->type(); // May be null if non-bound var.
		const TypeRef b_type = b->type();
		if(a_type.nonNull() && a_type->getType() == Type::VectorTypeType)
			expr_type = a->type();
		else if(b_type.nonNull() && b_type->getType() == Type::VectorTypeType)
			expr_type = b->type();
		else
		{
			if(canDoImplicitIntToFloatTypeCoercion(a, b))
				expr_type = new Float();
			else if(canDoImplicitIntToDoubleTypeCoercion(a, b))
				expr_type = new Double();
			else
				expr_type = a->type();
		}
	}
	return expr_type;
}


void MulExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		doImplicitIntToFloatTypeCoercion(a, b, payload);
		doImplicitIntToDoubleTypeCoercion(a, b, payload);

		// implicit conversion from int to float
		// 3.0 * 4
		/*if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 * 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}*/
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType || this_type->getType() == Type::DoubleType)
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(a_type->getType() == Type::VectorTypeType && b_type->getType() == Type::VectorTypeType) // Vector + vector addition.
		{
			if(*a_type != *b_type)
				throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));

			// Check element type is int or float
			if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || a_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
				throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(a_type->getType() == Type::VectorTypeType && *b_type == *a_type.downcast<VectorType>()->elem_type)
		{
			// A is a vector<T>, and B is of type T

			// Check element type is int or float
			if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || a_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
				throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(b_type->getType() == Type::VectorTypeType && *a_type == *b_type.downcast<VectorType>()->elem_type)
		{
			// B is a vector<T>, and A is of type T

			// Check element type is int or float
			if(!(b_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || b_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || b_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
				throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else
		{
			throw BaseException("AdditionExpression: Binary operator '*' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold;// && expressionIsWellTyped(*this, payload);
		//if(this->can_constant_fold)
		//{
		//	try
		//	{
		//		typeCheck(payload);
		//	}
		//	catch(BaseException&)
		//	{
		//		this->can_constant_fold = false;
		//	}
		//}
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


//bool MulExpression::typeCheck(TraversalPayload& payload) const
//{
//	const TypeRef& this_type = this->type();
//	if(this_type.isNull())
//		throw BaseException("Unknown operand type." + errorContext(*this, payload));
//
//	const TypeRef a_type = a->type();
//	const TypeRef b_type = b->type();
//	if(a_type.isNull() || b_type.isNull())
//		throw BaseException("MulExpression: Binary operator '*' not defined for Unknown types." + errorContext(*this, payload));
//
//
//		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType)
//		{
//			if(*a->type() != *b->type())
//				throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//		}
//		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType) // Vector + vector addition.
//		{
//			if(*a->type() != *b->type())
//				throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//
//			// Check element type is int or float
//			if(!(a->type().downcast<VectorType>()->elem_type->getType() == Type::IntType || a->type().downcast<VectorType>()->elem_type->getType() == Type::FloatType))
//				throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//		}
//		else if(a->type()->getType() == Type::VectorTypeType && *b->type() == *a->type().downcast<VectorType>()->elem_type)
//		{
//			// A is a vector<T>, and B is of type T
//
//			// Check element type is int or float
//			if(!(a->type().downcast<VectorType>()->elem_type->getType() == Type::IntType || a->type().downcast<VectorType>()->elem_type->getType() == Type::FloatType))
//				throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//		}
//		else if(b->type()->getType() == Type::VectorTypeType && *a->type() == *b->type().downcast<VectorType>()->elem_type)
//		{
//			// B is a vector<T>, and A is of type T
//
//			// Check element type is int or float
//			if(!(b->type().downcast<VectorType>()->elem_type->getType() == Type::IntType || b->type().downcast<VectorType>()->elem_type->getType() == Type::FloatType))
//				throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
//		}
//		else
//		{
//			throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
//		}
//}


void MulExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Mul Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


std::string MulExpression::sourceString() const
{
	return "(" + a->sourceString() + " * " + b->sourceString() + ")";
}


std::string MulExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	const std::string a_res = a->emitOpenCLC(params);
	const std::string b_res = b->emitOpenCLC(params);

	return "(" + a_res + " * " + b_res + ")";
}


/*void MulExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}*/


llvm::Value* MulExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::VectorTypeType)
	{
		if(a->type()->getType() == Type::FloatType || a->type()->getType() == Type::DoubleType)
		{
			// float * vector<float>
			assert(b->type()->getType() == Type::VectorTypeType);

			// Splat a to a vector<float> value.
			llvm::Value* aval = a->emitLLVMCode(params);
			llvm::Value* avec = params.builder->CreateVectorSplat(
				b->type().downcast<VectorType>()->num,
				aval
			);

			return params.builder->CreateFMul(
				avec, 
				b->emitLLVMCode(params)
			);
		}
		else if(b->type()->getType() == Type::FloatType || b->type()->getType() == Type::DoubleType)
		{
			// vector<float> * float
			assert(a->type()->getType() == Type::VectorTypeType);

			llvm::Value* bval = b->emitLLVMCode(params);
			llvm::Value* bvec = params.builder->CreateVectorSplat(
				a->type().downcast<VectorType>()->num,
				bval
			);

			return params.builder->CreateFMul(
				a->emitLLVMCode(params), 
				bvec
			);
		}
		else if(a->type()->getType() == Type::IntType)
		{
			// int * vector<int>
			assert(b->type()->getType() == Type::VectorTypeType);

			// Splat a to a vector<float> value.
			llvm::Value* aval = a->emitLLVMCode(params);
			llvm::Value* avec = params.builder->CreateVectorSplat(
				b->type().downcast<VectorType>()->num,
				aval
			);

			return params.builder->CreateMul(
				avec, 
				b->emitLLVMCode(params)
			);
		}
		else if(b->type()->getType() == Type::IntType)
		{
			// vector<int> * int
			assert(a->type()->getType() == Type::VectorTypeType);

			llvm::Value* bval = b->emitLLVMCode(params);
			llvm::Value* bvec = params.builder->CreateVectorSplat(
				a->type().downcast<VectorType>()->num,
				bval
			);

			return params.builder->CreateMul(
				a->emitLLVMCode(params), 
				bvec
			);
		}
		else
		{
			// vector<T> * vector<T>
			assert(a->type()->getType() == Type::VectorTypeType);
			assert(b->type()->getType() == Type::VectorTypeType);

			const TypeRef elem_type = a->type().downcast<VectorType>()->elem_type;
			if(elem_type->getType() == Type::FloatType || elem_type->getType() == Type::DoubleType)
			{
				return params.builder->CreateFMul(
					a->emitLLVMCode(params), 
					b->emitLLVMCode(params)
				);
			}
			else if(elem_type->getType() == Type::IntType)
			{
				return params.builder->CreateMul(
					a->emitLLVMCode(params), 
					b->emitLLVMCode(params)
				);
			}
			else
			{
				assert(0);
				return NULL;
			}
		}
	}
	else if(this->type()->getType() == Type::FloatType || this->type()->getType() == Type::DoubleType)
	{
		return params.builder->CreateFMul(
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Mul, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for MulExpression code emission");
	}
}


Reference<ASTNode> MulExpression::clone(CloneMapType& clone_map)
{
	MulExpression* res = new MulExpression(this->srcLocation(), this->a->clone(clone_map), this->b->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool MulExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


size_t MulExpression::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 1;
	const size_t op_cost = (type()->getType() == Type::VectorTypeType) ? (type().downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;

	return a->getTimeBound(params) + b->getTimeBound(params) + op_cost;
}


GetSpaceBoundResults MulExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}


//-------------------------------------------------------------------------------------------------------


ValueRef DivExpression::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);

	if(this->type()->getType() == Type::FloatType)
	{
		if(a->type()->getType() == Type::FloatType && b->type()->getType() == Type::FloatType)
			return new FloatValue(checkedCast<FloatValue>(aval)->value / checkedCast<FloatValue>(bval)->value);
		else
			throw BaseException("invalid types for div op.");
	}
	if(this->type()->getType() == Type::DoubleType)
	{
		if(a->type()->getType() == Type::DoubleType && b->type()->getType() == Type::DoubleType)
			return new DoubleValue(checkedCast<DoubleValue>(aval)->value / checkedCast<DoubleValue>(bval)->value);
		else
			throw BaseException("invalid types for div op.");
	}
	else if(this->type()->getType() == Type::IntType)
	{
		if(!(a->type()->getType() == Type::IntType && b->type()->getType() == Type::IntType))
			throw BaseException("invalid types for div op.");

		const int64 a_int_val = checkedCast<IntValue>(aval)->value;
		const int64 b_int_val = checkedCast<IntValue>(bval)->value;

		if(b_int_val == 0)
			throw BaseException("Divide by zero.");

		if(a_int_val == std::numeric_limits<int32>::min() && b_int_val == -1)
			throw BaseException("Tried to compute -2147483648 / -1.");

		// TODO: handle other bitness and signedness.

		return new IntValue(a_int_val / b_int_val, checkedCast<IntValue>(aval)->is_signed);
	}
	else if(this->type()->getType() == Type::VectorTypeType)
	{
		const VectorType* vectype = this->type().downcastToPtr<VectorType>();

		const VectorValue* aval_vec = checkedCast<VectorValue>(aval);
		
		if(vectype->elem_type->getType() == Type::FloatType)
		{
			const float bval_float = checkedCast<FloatValue>(bval)->value;

			vector<ValueRef> elem_values(aval_vec->e.size());
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new FloatValue(checkedCast<FloatValue>(aval_vec->e[i])->value / bval_float);
			return new VectorValue(elem_values);
		}
		else if(vectype->elem_type->getType() == Type::DoubleType)
		{
			const double bval_double = checkedCast<DoubleValue>(bval)->value;

			vector<ValueRef> elem_values(aval_vec->e.size());
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new DoubleValue(checkedCast<DoubleValue>(aval_vec->e[i])->value / bval_double);
			return new VectorValue(elem_values);
		}
		else
			throw BaseException("invalid types for div op.");
	}
	else
	{
		throw BaseException("invalid types for div op.");
	}
}


TypeRef DivExpression::type() const
{
	// See if we can do type coercion

	// Type may be null if 'a' is a variable node that has not been bound yet.
	/*const TypeRef a_type = a->type(); 
	const TypeRef b_type = b->type();

	if(a_type.nonNull() && a_type->getType() == Type::FloatType && b->nodeType() == ASTNode::IntLiteralType)
	{
		IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
		if(isIntExactlyRepresentableAsFloat(b_lit->value) && (b_lit->value != 0))
			return new Float();
	}

	// 3 / 4.0 => 3.0 / 4.0
	if(b_type.nonNull() && b_type->getType() == Type::FloatType && a->nodeType() == ASTNode::IntLiteralType)
	{
		IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
		if(isIntExactlyRepresentableAsFloat(a_lit->value))
			return new Float();
	}

	return a->type();*/
	if(expr_type.isNull())
	{
		if(canDoImplicitIntToFloatTypeCoercion(a, b))
			expr_type = new Float();
		else if(canDoImplicitIntToDoubleTypeCoercion(a, b))
			expr_type = new Double();
		else
			expr_type = a->type();
	}
	return expr_type;
}


void DivExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 / 4
		// Only do this if b is != 0.  Otherwise we are messing with divide by zero semantics.

		// Type may be null if 'a' is a variable node that has not been bound yet.
		{
			const TypeRef a_type = a->type(); 
			const TypeRef b_type = b->type();

			if(a_type.nonNull() && a_type->getType() == Type::FloatType && b->nodeType() == ASTNode::IntLiteralType)
			{
				IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
				if(isIntExactlyRepresentableAsFloat(b_lit->value) && (b_lit->value != 0))
				{
					b = new FloatLiteral((float)b_lit->value, b->srcLocation());
					payload.tree_changed = true;
				}
			}

			// 3 / 4.0 => 3.0 / 4.0
			if(b_type.nonNull() && b_type->getType() == Type::FloatType && a->nodeType() == ASTNode::IntLiteralType)
			{
				IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
				if(isIntExactlyRepresentableAsFloat(a_lit->value))
				{
					a = new FloatLiteral((float)a_lit->value, a->srcLocation());
					payload.tree_changed = true;
				}
			}
		}


		// implicit conversion from int to double
		// 3.0 / 4
		// Only do this if b is != 0.  Otherwise we are messing with divide by zero semantics.

		// Type may be null if 'a' is a variable node that has not been bound yet.
		{
			const TypeRef a_type = a->type(); 
			const TypeRef b_type = b->type();

			if(a_type.nonNull() && a_type->getType() == Type::DoubleType && b->nodeType() == ASTNode::IntLiteralType)
			{
				IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
				if(isIntExactlyRepresentableAsDouble(b_lit->value) && (b_lit->value != 0))
				{
					b = new DoubleLiteral((double)b_lit->value, b->srcLocation());
					payload.tree_changed = true;
				}
			}

			// 3 / 4.0 => 3.0 / 4.0
			if(b_type.nonNull() && b_type->getType() == Type::DoubleType && a->nodeType() == ASTNode::IntLiteralType)
			{
				IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
				if(isIntExactlyRepresentableAsDouble(a_lit->value))
				{
					a = new DoubleLiteral((float)a_lit->value, a->srcLocation());
					payload.tree_changed = true;
				}
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType || this_type->getType() == Type::DoubleType)
		{
			// Make sure both operands have the same type
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
		else if(this_type->getType() == Type::VectorTypeType &&
			(this_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || this_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
		{
			if(*this_type.downcast<VectorType>()->elem_type != *b->type())
				throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
		/*else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}*/
		else
		{
			throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkNoZeroDivide(payload, stack);

		checkNoOverflow(payload, stack);

		this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold;// && expressionIsWellTyped(*this, payload);
		//if(this->can_constant_fold)
		//{
		//	try
		//	{
		//		typeCheck(payload);
		//	}
		//	catch(BaseException&)
		//	{
		//		this->can_constant_fold = false;
		//	}
		//}
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


//bool DivExpression::typeCheck(TraversalPayload& payload) const
//{
//	const TypeRef& this_type = this->type();
//	if(this_type.isNull())
//		throw BaseException("Unknown operand type." + errorContext(*this, payload));
//
//	const TypeRef a_type = a->type();
//	const TypeRef b_type = b->type();
//	if(a_type.isNull() || b_type.isNull())
//		throw BaseException("DivExpression: Binary operator '/' not defined for Unknown types." + errorContext(*this, payload));
//
//
//		if(this_type->getType() == Type::GenericTypeType || *this_type == Int() || *this_type == Float())
//		{
//			// Make sure both operands have the same type
//			if(*a->type() != *b->type())
//				throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
//		}
//		/*else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
//		{
//			// this is alright.
//			// NOTE: need to do more checking tho.
//			// Need to check number of elements is same in both vectors, and field types are the same.
//		}*/
//		else
//		{
//			throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
//		}
//}


bool DivExpression::provenDefined() const
{
	return false; // TEMP
}


// Try and prove we are not doing INT_MIN / -1
void DivExpression::checkNoOverflow(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(this->type()->getType() == Type::IntType)
	{
		// See if the numerator is contant
		if(a->isConstant())
		{
			// Evaluate the numerator expression
			VMState vmstate;
			vmstate.func_args_start.push_back(0);

			ValueRef retval = a->exec(vmstate);

			assert(retval->valueType() == Value::ValueType_Int);

			const int64 numerator_val = static_cast<IntValue*>(retval.getPointer())->value;

			if(numerator_val != std::numeric_limits<int32>::min())
				return; // Success
		}

		// See if the divisor is contant
		if(b->isConstant())
		{
			// Evaluate the divisor expression
			VMState vmstate;
			vmstate.func_args_start.push_back(0);

			ValueRef retval = b->exec(vmstate);

			assert(retval->valueType() == Value::ValueType_Int);

			const int64 divisor_val = static_cast<IntValue*>(retval.getPointer())->value;

			if(divisor_val != -1)
				return; // Success
		}

		// See if we can bound the numerator or denominator ranges
		const IntervalSetInt64 a_bounds = ProofUtils::getInt64Range(stack, 
			a // integer value
		);

		if(a_bounds.lower() > std::numeric_limits<int32>::min())
		{
			// We have proven numerator > INT_MIN
			return;
		}

		const IntervalSetInt64 b_bounds = ProofUtils::getInt64Range(stack, 
			b // integer value
		);

		/*if(b_bounds. > -1) // If denom lower bound is > -1
			return;
		if(b_bounds.y < -1) // If denom upper bound is < -1
			return;*/
		if(!b_bounds.includesValue(-1))
			return;



		/*int numerator_lower = std::numeric_limits<int32>::min();
		int numerator_upper = std::numeric_limits<int32>::max();

		// Walk up stack
		for(int z=(int)stack.size()-1; z >= 0; --z)
		{
			ASTNode* stack_node = stack[z];

			if(stack_node->nodeType() == ASTNode::FunctionExpressionType && 
				static_cast<FunctionExpression*>(stack_node)->target_function->sig.name == "if")
			{
				// AST node above this one is an "if" expression
				FunctionExpression* if_node = static_cast<FunctionExpression*>(stack_node);

				// Is this node the 1st arg of the if expression?
				// e.g. if condition then this_node else other_node
				// Or is this node a child of the 1st arg?
				if(if_node->argument_expressions[1].getPointer() == this || ((z+1) < (int)stack.size() && if_node->argument_expressions[1].getPointer() == stack[z+1]))
				{
					if(if_node->argument_expressions[0]->nodeType() == ASTNode::ComparisonExpressionType) // If condition is a comparison:
					{
						ComparisonExpression* comp = static_cast<ComparisonExpression*>(if_node->argument_expressions[0].getPointer());

						if(expressionsHaveSameValue(comp->a, this->b)) // if condition left side is equal to div expression divisor
						{
							if(comp->token->getType() == NOT_EQUALS_TOKEN) // if comparison is 'divisor != x'
							{
								if(comp->b->isConstant())
								{
									// Evaluate the x expression
									VMState vmstate(payload.hidden_voidptr_arg);
									vmstate.func_args_start.push_back(0);
									if(payload.hidden_voidptr_arg)
										vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(payload.env)));

									ValueRef retval = comp->b->exec(vmstate);

									assert(dynamic_cast<IntValue*>(retval.getPointer()));

									const int divisor_val = static_cast<IntValue*>(retval.getPointer())->value;

									if(divisor_val == -1)
									{
										// We know the comparison is effectively 'divisor != -1', which proves we are not doing INT_MIN / -1.
										return; 
									}
								}
							}
						}
					}
				}
			}
		}*/

		throw BaseException("Failed to prove division is not -2147483648 / -1.  (INT_MIN / -1)" + errorContext(*this));
	}
}


void DivExpression::checkNoZeroDivide(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(this->type()->getType() == Type::IntType)
	{
		// See if the divisor is contant
		if(b->isConstant())
		{
			// Evaluate the divisor expression
			VMState vmstate;
			vmstate.func_args_start.push_back(0);

			ValueRef retval = b->exec(vmstate);

			assert(retval->valueType() == Value::ValueType_Int);

			const int64 divisor_val = static_cast<IntValue*>(retval.getPointer())->value;

			if(divisor_val == 0)
			{
				throw BaseException("Integer division by zero." + errorContext(*this));
			}
			else
			{
				return; // Success, we have proven the divisor != 0.
			}
		}
		else
		{
			// b is not constant.

			const IntervalSetInt64 b_bounds = ProofUtils::getInt64Range(stack, 
				b // integer value
			);

			/*if(b_bounds.x > 0) // If denom lower bound is > 0
				return;
			if(b_bounds.y < 0) // If denom upper bound is < 0
				return;*/
			if(!b_bounds.includesValue(0))
				return;

			// Walk up stack, until we get to a divisor != 0 test
			/*for(int z=(int)stack.size()-1; z >= 0; --z)
			{
				ASTNode* stack_node = stack[z];

				if(stack_node->nodeType() == ASTNode::FunctionExpressionType && 
					static_cast<FunctionExpression*>(stack_node)->target_function->sig.name == "if")
				{
					// AST node above this one is an "if" expression
					FunctionExpression* if_node = static_cast<FunctionExpression*>(stack_node);

					// Is this node the 1st arg of the if expression?
					// e.g. if condition then this_node else other_node
					// Or is this node a child of the 1st arg?
					if(if_node->argument_expressions[1].getPointer() == this || ((z+1) < (int)stack.size() && if_node->argument_expressions[1].getPointer() == stack[z+1]))
					{
						if(if_node->argument_expressions[0]->nodeType() == ASTNode::ComparisonExpressionType) // If condition is a comparison:
						{
							ComparisonExpression* comp = static_cast<ComparisonExpression*>(if_node->argument_expressions[0].getPointer());

							if(expressionsHaveSameValue(comp->a, this->b)) // if condition left side is equal to div expression divisor
							{
								if(comp->token->getType() == NOT_EQUALS_TOKEN) // if comparison is 'divisor != x'
								{
									if(comp->b->isConstant())
									{
										// Evaluate the x expression
										VMState vmstate(payload.hidden_voidptr_arg);
										vmstate.func_args_start.push_back(0);
										if(payload.hidden_voidptr_arg)
											vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(payload.env)));

										ValueRef retval = comp->b->exec(vmstate);

										assert(dynamic_cast<IntValue*>(retval.getPointer()));

										const int divisor_val = static_cast<IntValue*>(retval.getPointer())->value;

										if(divisor_val == 0)
										{
											// We know the comparison is effectively 'divisor != 0', which is a valid proof.
											return; 
										}
									}
								}
							}
						}
					}
				}
			}*/
		}

		throw BaseException("Failed to prove divisor is != 0." + errorContext(*this));
	}
}


void DivExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Div Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


std::string DivExpression::sourceString() const
{
	return "(" + a->sourceString() + " / " + b->sourceString() + ")";
}


std::string DivExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + " / " + b->emitOpenCLC(params) + ")";
}


llvm::Value* DivExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::VectorTypeType && (b->type()->getType() == Type::FloatType || b->type()->getType() == Type::DoubleType))
	{
		// vector<float> / float
		assert(a->type()->getType() == Type::VectorTypeType);

		llvm::Value* bval = b->emitLLVMCode(params);
		llvm::Value* bvec = params.builder->CreateVectorSplat(
			a->type().downcast<VectorType>()->num,
			bval
		);

		return params.builder->CreateFDiv(
			a->emitLLVMCode(params),
			bvec
		);
	}
	else if(this->type()->getType() == Type::FloatType || this->type()->getType() == Type::DoubleType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FDiv, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::SDiv, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		assert(!"divexpression type invalid!");
		return NULL;
	}
}


Reference<ASTNode> DivExpression::clone(CloneMapType& clone_map)
{
	DivExpression* res = new DivExpression(this->srcLocation(), this->a->clone(clone_map), this->b->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool DivExpression::isConstant() const
{
	return /*this->proven_defined && */a->isConstant() && b->isConstant();
}


size_t DivExpression::getTimeBound(GetTimeBoundParams& params) const
{
	const size_t scalar_cost = 10;
	const size_t op_cost = (type()->getType() == Type::VectorTypeType) ? (type().downcastToPtr<VectorType>()->num * scalar_cost) : scalar_cost;

	return a->getTimeBound(params) + b->getTimeBound(params) + op_cost;
}


GetSpaceBoundResults DivExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}


//-----------------------------------------------------------------------------------------------


BinaryBitwiseExpression::BinaryBitwiseExpression(BitwiseType t_, const ASTNodeRef& a_, const ASTNodeRef& b_, const SrcLocation& loc_)
:	ASTNode(BinaryBitwiseExpressionType, loc_),
	t(t_),
	a(a_),
	b(b_)
{}


ValueRef BinaryBitwiseExpression::exec(VMState& vmstate)
{
	const ValueRef aval = a->exec(vmstate);
	const ValueRef bval = b->exec(vmstate);

	const IntValue* aint = checkedCast<IntValue>(aval);
	const IntValue* bint = checkedCast<IntValue>(bval);

	switch(t)
	{
	case BITWISE_AND: { return new IntValue(aint->value & bint->value, aint->is_signed); }
	case BITWISE_OR: { return new IntValue(aint->value | bint->value, aint->is_signed); }
	case BITWISE_XOR: { return new IntValue(aint->value ^ bint->value, aint->is_signed); }
	case BITWISE_LEFT_SHIFT:
		{
			// TODO: handle a being negative, undefined?

			if(bint->value < 0)
				throw BaseException("left shift by negative value.");
			if(bint->value >= a->type().downcastToPtr<Int>()->numBits())
				throw BaseException("left shift by value >= bit width");

			return new IntValue(aint->value << bint->value, aint->is_signed);
		}
	case BITWISE_RIGHT_SHIFT:
		{
			// TODO: handle a being negative, undefined?

			if(bint->value < 0)
				throw BaseException("right shift by negative value.");
			if(bint->value >= a->type().downcastToPtr<Int>()->numBits())
				throw BaseException("right shift by value >= bit width");

			return new IntValue(aint->value >> bint->value, aint->is_signed);
		}
	default:
		throw BaseException("Internal error in BinaryBitwiseExpression::exec()");
	};
}


void BinaryBitwiseExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Binary Bitwise Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


const std::string BinaryBitwiseExpression::opToken() const
{
	std::string op;
	switch(t)
	{
	case BITWISE_AND: { op = "&"; break; }
	case BITWISE_OR: { op = "|"; break; }
	case BITWISE_XOR: { op = "^"; break; }
	case BITWISE_LEFT_SHIFT: { op = "<<"; break; }
	case BITWISE_RIGHT_SHIFT: { op = ">>"; break; }
	};
	return op;
}


std::string BinaryBitwiseExpression::sourceString() const
{
	return "(" + a->sourceString() + " " + opToken() + " " + b->sourceString() + ")";
}


std::string BinaryBitwiseExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + " " + opToken() + " " + b->emitOpenCLC(params) + ")";
}


TypeRef BinaryBitwiseExpression::type() const
{
	return a->type();
}


void BinaryBitwiseExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType)
		{
			if(*a_type != *b_type)
				throw BaseException("Binary operator '" + opToken() + "' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		else if(this_type->getType() == Type::IntType)
		{
			if(*a_type != *b_type)
				throw BaseException("Binary operator '" + opToken() + "' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));

			//const Int* a_int_type = a_type.downcastToPtr<Int>();
			//const Int* b_int_type = b_type.downcastToPtr<Int>();

			//if(a_int_type->num_bits != b_int_type->num_bits)
			//	throw BaseException("AdditionExpression: Binary operator '" + opToken() + "' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		}
		//else if(a_type->getType() == Type::VectorTypeType && b_type->getType() == Type::VectorTypeType) // Vector + vector addition.
		//{
		//	if(*a_type != *b_type)
		//		throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));

		//	// Check element type is int or float
		//	if(!(a_type.downcast<VectorType>()->elem_type->getType() == Type::IntType || a_type.downcast<VectorType>()->elem_type->getType() == Type::FloatType || a_type.downcast<VectorType>()->elem_type->getType() == Type::DoubleType))
		//		throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'" + errorContext(*this, payload));
		//}
		else
		{
			throw BaseException(" Binary operator '" + opToken() + "' not defined for types '" +  a_type->toString() + "' and '" +  b_type->toString() + "'." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


llvm::Value* BinaryBitwiseExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::IntType)
	{
		llvm::Value* a_expr = a->emitLLVMCode(params);
		llvm::Value* b_expr = b->emitLLVMCode(params);

		switch(t)
		{
		case BITWISE_AND: return params.builder->CreateAnd(a_expr, b_expr);
		case BITWISE_OR:  return params.builder->CreateOr(a_expr, b_expr);
		case BITWISE_XOR:  return params.builder->CreateXor(a_expr, b_expr);
		case BITWISE_LEFT_SHIFT:  return params.builder->CreateShl(a_expr, b_expr);
		case BITWISE_RIGHT_SHIFT:  return params.builder->CreateLShr(a_expr, b_expr); // Logical shift right, fills leading bits with zeros.
		default:
			{
				assert(0);
				throw BaseException("Internal error in BinaryBitwiseExpression code emission");
			}
		}
	}
	else
	{
		throw BaseException("Unknown type for BinaryBitwiseExpression code emission");
	}
}


Reference<ASTNode> BinaryBitwiseExpression::clone(CloneMapType& clone_map)
{
	BinaryBitwiseExpression* res = new BinaryBitwiseExpression(t, this->a->clone(clone_map), this->b->clone(clone_map), this->srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool BinaryBitwiseExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


size_t BinaryBitwiseExpression::getTimeBound(GetTimeBoundParams& params) const
{
	return a->getTimeBound(params) + b->getTimeBound(params) + 1;
}


GetSpaceBoundResults BinaryBitwiseExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}

//-------------------------------------------------------------------------------------------------------


BinaryBooleanExpr::BinaryBooleanExpr(Type t_, const ASTNodeRef& a_, const ASTNodeRef& b_, const SrcLocation& loc)
:	ASTNode(BinaryBooleanType, loc),
	t(t_), a(a_), b(b_)
{
}


ValueRef BinaryBooleanExpr::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);
	ValueRef retval;

	if(t == OR)
	{
		return new BoolValue(
			checkedCast<BoolValue>(aval)->value || 
			checkedCast<BoolValue>(bval)->value
		);
	}
	else if(t == AND)
	{
		return new BoolValue(
			checkedCast<BoolValue>(aval)->value &&
			checkedCast<BoolValue>(bval)->value
		);
	}
	else
	{
		assert(!"invalid t");
		return ValueRef();
	}
}


void BinaryBooleanExpr::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& a_type = this->a->type();
		if(a_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));
		const TypeRef& b_type = this->b->type();
		if(b_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(a_type->getType() != Winter::Type::BoolType)
			throw BaseException("First child does not have boolean type." + errorContext(*this, payload));

		if(b_type->getType() != Winter::Type::BoolType)
			throw BaseException("Second child does not have boolean type." + errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


void BinaryBooleanExpr::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Binary boolean Expression ";
	if(t == OR)
		s << "OR";
	else if(t == AND)
		s << "AND";
	s << "\n";

	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


std::string BinaryBooleanExpr::sourceString() const
{
	return "(" + a->sourceString() + (this->t == OR ? " || " : " && ") + b->sourceString() + ")";
}


std::string BinaryBooleanExpr::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + (this->t == OR ? " || " : " && ") + b->emitOpenCLC(params) + ")";
}


llvm::Value* BinaryBooleanExpr::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(t == AND)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::And, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
			);
	}
	else if(t == OR)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Or, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
			);
	}
	else
	{
		assert(!"t type invalid!");
		return NULL;
	}
}


Reference<ASTNode> BinaryBooleanExpr::clone(CloneMapType& clone_map)
{
	BinaryBooleanExpr* res = new BinaryBooleanExpr(t, a->clone(clone_map), b->clone(clone_map), srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool BinaryBooleanExpr::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


size_t BinaryBooleanExpr::getTimeBound(GetTimeBoundParams& params) const
{
	return a->getTimeBound(params) + b->getTimeBound(params) + 1;
}


GetSpaceBoundResults BinaryBooleanExpr::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}


//----------------------------------------------------------------------------------------


ValueRef UnaryMinusExpression::exec(VMState& vmstate)
{
	ValueRef aval = expr->exec(vmstate);

	if(this->type()->getType() == Type::FloatType)
	{
		return new FloatValue(-checkedCast<FloatValue>(aval)->value);
	}
	else if(this->type()->getType() == Type::DoubleType)
	{
		return new DoubleValue(-checkedCast<DoubleValue>(aval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return new IntValue(-checkedCast<IntValue>(aval)->value, checkedCast<IntValue>(aval)->is_signed);
	}
	else if(this->type()->getType() == Type::VectorTypeType)
	{
		const TypeRef this_type = expr->type();
		const VectorType* vectype = this_type.downcastToPtr<VectorType>();

		const VectorValue* aval_vec = checkedCast<VectorValue>(aval);
		
		vector<ValueRef> elem_values(aval_vec->e.size());
		switch(vectype->elem_type->getType())
		{
			case Type::FloatType:
			{
				for(unsigned int i=0; i<elem_values.size(); ++i)
					elem_values[i] = new FloatValue(-checkedCast<FloatValue>(aval_vec->e[i])->value);
				break;
			}
			case Type::IntType:
			{
				// TODO: over/under float check
				for(unsigned int i=0; i<elem_values.size(); ++i)
					elem_values[i] = new IntValue(-checkedCast<IntValue>(aval_vec->e[i])->value, checkedCast<IntValue>(aval_vec->e[i])->is_signed);
				break;
			}
			default:
			{
				throw BaseException("UnaryMinusExpression type invalid!");
			}
		}
		return new VectorValue(elem_values);
	}
	else
	{
		throw BaseException("UnaryMinusExpression type invalid!");
	}
}


void UnaryMinusExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(expr, payload);
	}*/


	stack.push_back(this);
	expr->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(expr, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() == Type::GenericTypeType || this_type->getType() == Type::IntType || this_type->getType() == Type::FloatType || this_type->getType() == Type::DoubleType)
		{
			if(this_type->getType() == Type::IntType && !this_type.downcastToPtr<Int>()->is_signed)
				throw BaseException("Unary minus not defined for unsigned type '" + this->type()->toString() + "'.");
		}
		else if(this_type->getType() == Type::VectorTypeType && 
			(static_cast<VectorType*>(this_type.getPointer())->elem_type->getType() == Type::FloatType || static_cast<VectorType*>(this_type.getPointer())->elem_type->getType() == Type::DoubleType || static_cast<VectorType*>(this_type.getPointer())->elem_type->getType() == Type::IntType))
		{
		}
		else
		{
			throw BaseException("Type '" + this->type()->toString() + "' does not define unary operator '-'.");
		}
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = expr->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool is_literal = checkFoldExpression(expr, payload);
		this->can_maybe_constant_fold = is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(expr, payload, stack);
	}

	stack.pop_back();
}


void UnaryMinusExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Unary Minus Expression\n";
	this->expr->print(depth+1, s);
}


std::string UnaryMinusExpression::sourceString() const
{
	return "-" + expr->sourceString();
}


std::string UnaryMinusExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	// Put some space around the '-'.  Otherwise two unary minuses will be interepreted as decrement operator.
	return " - " + expr->emitOpenCLC(params);
}


llvm::Value* UnaryMinusExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::FloatType || this->type()->getType() == Type::DoubleType)
	{
		return params.builder->CreateFNeg(
			expr->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateNeg(
			expr->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::VectorTypeType)
	{
		Reference<VectorType> vec_type = this->type().downcast<VectorType>();

		// Multiple with a vector of -1
		if(vec_type->elem_type->getType() == Type::FloatType)
		{
			llvm::Value* neg_one_vec = llvm::ConstantVector::getSplat(
				vec_type->num,
				llvm::ConstantFP::get(*params.context, llvm::APFloat(-1.0f))
			);

			return params.builder->CreateFMul(
				expr->emitLLVMCode(params), 
				neg_one_vec
			);
		}
		else if(vec_type->elem_type->getType() == Type::DoubleType)
		{
			llvm::Value* neg_one_vec = llvm::ConstantVector::getSplat(
				vec_type->num,
				llvm::ConstantFP::get(*params.context, llvm::APFloat(-1.0))
			);

			return params.builder->CreateFMul(
				expr->emitLLVMCode(params), 
				neg_one_vec
			);
		}
		else if(vec_type->elem_type->getType() == Type::IntType)
		{
			llvm::Value* neg_one_vec = llvm::ConstantVector::getSplat(
				vec_type->num,
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, (uint64_t)-1, true))
			);

			return params.builder->CreateMul(
				expr->emitLLVMCode(params), 
				neg_one_vec
			);
		}
	}

	assert(!"UnaryMinusExpression type invalid!");
	throw BaseException("UnaryMinusExpression type invalid!");
	
}


Reference<ASTNode> UnaryMinusExpression::clone(CloneMapType& clone_map)
{
	UnaryMinusExpression* res = new UnaryMinusExpression(this->srcLocation(), this->expr->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool UnaryMinusExpression::isConstant() const
{
	return expr->isConstant();
}


size_t UnaryMinusExpression::getTimeBound(GetTimeBoundParams& params) const
{
	return expr->getTimeBound(params) + 1;
}


GetSpaceBoundResults UnaryMinusExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return expr->getSpaceBound(params);
}


//----------------------------------------------------------------------------------------


ValueRef LogicalNegationExpr::exec(VMState& vmstate)
{
	const ValueRef expr_val = expr->exec(vmstate);

	return new BoolValue(!checkedCast<BoolValue>(expr_val)->value);
}


void LogicalNegationExpr::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(expr, payload);
	}*/


	stack.push_back(this);
	expr->traverse(payload, stack);
	

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(expr, payload);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef& this_type = this->type();
		if(this_type.isNull())
			throw BaseException("Unknown operand type." + errorContext(*this, payload));

		if(this_type->getType() != Type::BoolType)
			throw BaseException("Type '" + this->type()->toString() + "' does not define logical negation operator '!'." + errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = expr->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool is_literal = checkFoldExpression(expr, payload);
		this->can_maybe_constant_fold = is_literal;
	}

	stack.pop_back();
}


void LogicalNegationExpr::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Logical negation Expression\n";
	this->expr->print(depth+1, s);
}


std::string LogicalNegationExpr::sourceString() const
{
	return "!" + expr->sourceString();
}


std::string LogicalNegationExpr::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "!" + expr->emitOpenCLC(params);
}


llvm::Value* LogicalNegationExpr::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(this->type()->getType() == Type::BoolType)
	{
		return params.builder->CreateNot(
			expr->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("LogicalNegationExpr type invalid!");
	}
}


Reference<ASTNode> LogicalNegationExpr::clone(CloneMapType& clone_map)
{
	LogicalNegationExpr* res = new LogicalNegationExpr(this->srcLocation(), this->expr->clone(clone_map));
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool LogicalNegationExpr::isConstant() const
{
	return expr->isConstant();
}



size_t LogicalNegationExpr::getTimeBound(GetTimeBoundParams& params) const
{
	return expr->getTimeBound(params) + 1;
}


GetSpaceBoundResults LogicalNegationExpr::getSpaceBound(GetSpaceBoundParams& params) const
{
	return expr->getSpaceBound(params);
}


//---------------------------------------------------------------------------------


template <class T> static bool lt(Value* a, Value* b)
{
	return checkedCast<T>(a)->value < checkedCast<T>(b)->value;
}


template <class T> static bool gt(Value* a, Value* b)
{
	return checkedCast<T>(a)->value > checkedCast<T>(b)->value;
}


template <class T> static bool lte(Value* a, Value* b)
{
	return checkedCast<T>(a)->value <= checkedCast<T>(b)->value;
}


template <class T> static bool gte(Value* a, Value* b)
{
	return checkedCast<T>(a)->value >= checkedCast<T>(b)->value;
}


template <class T> static bool eq(Value* a, Value* b)
{
	return checkedCast<T>(a)->value == checkedCast<T>(b)->value;
}


template <class T> static bool neq(Value* a, Value* b)
{
	return checkedCast<T>(a)->value != checkedCast<T>(b)->value;
}


template <class T>
static BoolValue* compare(unsigned int token_type, Value* a, Value* b)
{
	switch(token_type)
	{
	case LEFT_ANGLE_BRACKET_TOKEN:
		return new BoolValue(lt<T>(a, b));
	case RIGHT_ANGLE_BRACKET_TOKEN:
		return new BoolValue(gt<T>(a, b));
	case DOUBLE_EQUALS_TOKEN:
		return new BoolValue(eq<T>(a, b));
	case NOT_EQUALS_TOKEN:
		return new BoolValue(neq<T>(a, b));
	case LESS_EQUAL_TOKEN:
		return new BoolValue(lte<T>(a, b));
	case GREATER_EQUAL_TOKEN:
		return new BoolValue(gte<T>(a, b));
	default:
		assert(!"Unknown comparison token type.");
		return NULL;
	}
}


ValueRef ComparisonExpression::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);

	ValueRef retval;

	switch(a->type()->getType())
	{
	case Type::FloatType:
		retval = compare<FloatValue>(this->token->getType(), aval.getPointer(), bval.getPointer());
		break;
	case Type::DoubleType:
		retval = compare<DoubleValue>(this->token->getType(), aval.getPointer(), bval.getPointer());
		break;
	case Type::IntType:
		retval = compare<IntValue>(this->token->getType(), aval.getPointer(), bval.getPointer());
		break;
	case Type::BoolType:
		retval = compare<BoolValue>(this->token->getType(), aval.getPointer(), bval.getPointer());
		break;
	default:
		throw BaseException("ComparisonExpression type invalid!");
	}

	return retval;
}


void ComparisonExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Comparison, token = '" + tokenName(this->token->getType()) + "'\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


static const std::string tokenString(unsigned int token_type)
{
	switch(token_type)
	{
	case LEFT_ANGLE_BRACKET_TOKEN:
		return " < ";
	case RIGHT_ANGLE_BRACKET_TOKEN:
		return " > ";
	case DOUBLE_EQUALS_TOKEN:
		return " == ";
	case NOT_EQUALS_TOKEN:
		return " != ";
	case LESS_EQUAL_TOKEN:
		return " <= ";
	case GREATER_EQUAL_TOKEN:
		return " >= ";
	default:
		assert(!"Unknown comparison token type.");
		return NULL;
	}
}


std::string ComparisonExpression::sourceString() const
{
	return "(" + a->sourceString() + tokenString(this->token->getType()) + b->sourceString() + ")";
}


std::string ComparisonExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return "(" + a->emitOpenCLC(params) + tokenString(this->token->getType()) + b->emitOpenCLC(params) + ")";
}


void ComparisonExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(a, payload);
		checkFoldExpression(b, payload);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);


	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(a, payload, stack);
		checkInlineExpression(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(a, payload);
		checkSubstituteVariable(b, payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		doImplicitIntToFloatTypeCoercion(a, b, payload);
		doImplicitIntToDoubleTypeCoercion(a, b, payload);

		doImplicitIntTypeCoercion(a, b, payload);

		// implicit conversion from int to float
		// 3.0 > 4      =>       3.0 > 4.0

		// Type may be null if 'a' is a variable node that has not been bound yet.
		/*const TypeRef a_type = a->type(); 
		const TypeRef b_type = b->type();

		if(a_type.nonNull() && a_type->getType() == Type::FloatType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 > 4.0      =>        3.0 > 4.0
		if(b_type.nonNull() && b_type->getType() == Type::FloatType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}*/
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		const TypeRef a_type = a->type();
		const TypeRef b_type = b->type();
		if(a_type.isNull() || b_type.isNull())
			throw BaseException("Unknown type");

		if(*a_type != *b_type)
			throw BaseException("Comparison operand types must be the same.  Left operand type: " + a_type->toString() + ", right operand type: " + b_type->toString() + "." + errorContext(*this, payload));

		if(a_type->getType() == Type::GenericTypeType || a_type->getType() == Type::IntType ||
			a_type->getType() == Type::FloatType || a_type->getType() == Type::DoubleType ||
			a_type->getType() == Type::BoolType ||
			a_type->getType() == Type::ArrayTypeType ||
			a_type->getType() == Type::VectorTypeType ||
			a_type->getType() == Type::StructureTypeType)
		{
		}
		else
		{
			throw BaseException("Type '" + a_type->toString() + "' does not define comparison operators." + errorContext(*this, payload));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool a_is_literal = checkFoldExpression(a, payload);
		const bool b_is_literal = checkFoldExpression(b, payload);
			
		this->can_maybe_constant_fold = a_is_literal && b_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(a, payload, stack);
		doDeadCodeElimination(b, payload, stack);
	}

	stack.pop_back();
}


llvm::Value* ComparisonExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	llvm::Value* a_code = a->emitLLVMCode(params);
	llvm::Value* b_code = b->emitLLVMCode(params);

	switch(a->type()->getType())
	{
	case Type::FloatType:
	case Type::DoubleType:
		{
			switch(this->token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: return params.builder->CreateFCmpOLT(a_code, b_code);
			case RIGHT_ANGLE_BRACKET_TOKEN: return params.builder->CreateFCmpOGT(a_code, b_code);
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateFCmpOEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateFCmpONE(a_code, b_code);
			case LESS_EQUAL_TOKEN: return params.builder->CreateFCmpOLE(a_code, b_code);
			case GREATER_EQUAL_TOKEN: return params.builder->CreateFCmpOGE(a_code, b_code);
			default: assert(0); throw BaseException("Unsupported token type for comparison.");
			}
		}
		break;
	case Type::IntType:
		{
			switch(this->token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: return params.builder->CreateICmpSLT(a_code, b_code);
			case RIGHT_ANGLE_BRACKET_TOKEN: return params.builder->CreateICmpSGT(a_code, b_code);
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateICmpEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateICmpNE(a_code, b_code);
			case LESS_EQUAL_TOKEN: return params.builder->CreateICmpSLE(a_code, b_code);
			case GREATER_EQUAL_TOKEN: return params.builder->CreateICmpSGE(a_code, b_code);
			default: assert(0); throw BaseException("Unsupported token type for comparison");
			}
		}
		break;
	case Type::BoolType:
		{
			switch(this->token->getType())
			{
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateICmpEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateICmpNE(a_code, b_code);
			default: assert(0); throw BaseException("Unsupported token type for comparison");
			}
		}
		break;
	
	// NOTE: shouldn't get structure type here because the comparison expression should have been replaced with a call to __compare_equal().
	// Likewise for array type.

	case Type::VectorTypeType:
		{
			const VectorType* a_vector_type = a->type().downcastToPtr<VectorType>();

			llvm::Value* par_eq = params.builder->CreateFCmpOEQ(a_code, b_code);

			llvm::Value* elem_0 = params.builder->CreateExtractElement(par_eq, 
				llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, /*value=*/0)));

			llvm::Value* conjunction = elem_0;
			for(unsigned int i=0; i<a_vector_type->num; ++i)
			{
				llvm::Value* elem_i = params.builder->CreateExtractElement(par_eq, 
					llvm::ConstantInt::get(*params.context, llvm::APInt(/*num bits=*/32, /*value=*/i)));

				conjunction = params.builder->CreateBinOp(
					llvm::Instruction::And,
					conjunction,
					elem_i
				);
			}

			return conjunction;
		}
	default:
		assert(!"ComparisonExpression type invalid!");
		throw BaseException("ComparisonExpression type invalid");
	}
}


Reference<ASTNode> ComparisonExpression::clone(CloneMapType& clone_map)
{
	ComparisonExpression* res = new ComparisonExpression(token, a->clone(clone_map), b->clone(clone_map), this->srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool ComparisonExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


const std::string ComparisonExpression::getOverloadedFuncName() const // returns e.g. op_lt, op_gt   etc..
{
	switch(this->token->getType())
	{
	case LEFT_ANGLE_BRACKET_TOKEN: return "op_lt";
	case RIGHT_ANGLE_BRACKET_TOKEN: return "op_gt";
	case DOUBLE_EQUALS_TOKEN: return "op_eq";
	case NOT_EQUALS_TOKEN: return "op_neq";
	case LESS_EQUAL_TOKEN: return "op_lte";
	case GREATER_EQUAL_TOKEN: return "op_gte";
	default: assert(0); throw BaseException("Unsupported token type for comparison");
	}
}



size_t ComparisonExpression::getTimeBound(GetTimeBoundParams& params) const
{
	return a->getTimeBound(params) + b->getTimeBound(params) + 1;
}


GetSpaceBoundResults ComparisonExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	return a->getSpaceBound(params) + b->getSpaceBound(params);
}


//---------------------------------------------------------------------------------


ValueRef ArraySubscript::exec(VMState& vmstate)
{
	assert(0); // Not called currently.

	// Array pointer is in arg 0.
	// Index is in arg 1.
	const ArrayValue* arr = checkedCast<const ArrayValue>(vmstate.argument_stack[vmstate.func_args_start.back()]);
	const IntValue* index = checkedCast<const IntValue>(vmstate.argument_stack[vmstate.func_args_start.back() + 1]);

	return arr->e[index->value];
}


void ArraySubscript::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "ArraySubscript\n";
	printMargin(depth, s); s << "subscript_expr:\n";
	this->subscript_expr->print(depth+1, s);
}


std::string ArraySubscript::sourceString() const
{
	assert(0);
	return "";
}


std::string ArraySubscript::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	assert(0);
	return "";
}


void ArraySubscript::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(subscript_expr, payload);
	}*/

	
	stack.push_back(this);
	subscript_expr->traverse(payload, stack);

	
	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(subscript_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(subscript_expr, payload);
	}
	// Convert overloaded operators before we pop this node off the stack.
	// This node needs to be on the node stack if an operator overloading substitution is made,
	// as the new op_X function will need to have a bind variables pass run on it.
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(subscript_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = subscript_expr->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool is_literal = checkFoldExpression(subscript_expr, payload);
		this->can_maybe_constant_fold = is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(subscript_expr, payload, stack);
	}

	stack.pop_back();
}


llvm::Value* ArraySubscript::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	assert(0);
	return NULL;
}


Reference<ASTNode> ArraySubscript::clone(CloneMapType& clone_map)
{
	ArraySubscript* res = new ArraySubscript(subscript_expr->clone(clone_map), this->srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool ArraySubscript::isConstant() const
{
	return subscript_expr->isConstant();
}


size_t ArraySubscript::getTimeBound(GetTimeBoundParams& params) const
{
	return subscript_expr->getTimeBound(params) + 1;
}


GetSpaceBoundResults ArraySubscript::getSpaceBound(GetSpaceBoundParams& params) const
{
	return subscript_expr->getSpaceBound(params);
}


//----------------------------------------------------------------------------------------


TypeRef NamedConstant::type() const
{
	if(declared_type.nonNull())
		return declared_type;
	
	return value_expr->type();
}


ValueRef NamedConstant::exec(VMState& vmstate)
{
	return this->value_expr->exec(vmstate);
}


void NamedConstant::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Named constant.  name='" << name << "'\n";
	printMargin(depth, s);
	this->value_expr->print(depth + 1, s);
}


std::string NamedConstant::sourceString() const
{
	return name + " = " + value_expr->sourceString();
}


std::string NamedConstant::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	if(value_expr->nodeType() == ASTNode::ArrayLiteralType)
	{
		/*
		Special case for array literal named constants.
		The following type of code does not work on Nvidia:

		//----------------------------------------------------------------
		__constant float array_literal_49[] = {1.6592417f, 1.9547194f, 1.654599f, 1.9570677f};
		__constant float* __constant some_named_constant = array_literal_49;
		
		void someFunc()
		{
			// ...
			float x = some_named_constant[i]
		}
		//----------------------------------------------------------------

		Presumably due to the assignment to the named constant and global scope.
		So we have to avoid that, and emit code like this:

		//----------------------------------------------------------------
		__constant float some_named_constant[] = {1.6592417f, 1.9547194f, 1.654599f, 1.9570677f};
		void someFunc()
		{
			// ...
			float x = some_named_constant[i]
		}
		//----------------------------------------------------------------
		*/
		
		params.file_scope_code += value_expr.downcastToPtr<ArrayLiteral>()->getFileScopeOpenCLC(params, name);
		return "";
	}
	else
	{
		// Need to declare this as constant otherwise get "error: global variable must be declared in addrSpace constant"
		return type()->OpenCLCType() + " __constant " + name + " = " + value_expr->emitOpenCLC(params) + ";";
	}
}


void NamedConstant::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(value_expr, payload);
	}*/

	//payload.named_constant_stack.push_back(this);
	payload.current_named_constant = this;
	stack.push_back(this);

	value_expr->traverse(payload, stack);

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(value_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		checkSubstituteVariable(value_expr, payload);
	}
	// Convert overloaded operators before we pop this node off the stack.
	// This node needs to be on the node stack if an operator overloading substitution is made,
	// as the new op_X function will need to have a bind variables pass run on it.
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(value_expr, payload, stack);
	}
	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		if(declared_type.nonNull() && declared_type->getType() == Type::FloatType && 
			value_expr.nonNull() && value_expr->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* body_lit = static_cast<IntLiteral*>(value_expr.getPointer());
			if(isIntExactlyRepresentableAsFloat(body_lit->value))
			{
				this->value_expr = new FloatLiteral((float)body_lit->value, body_lit->srcLocation());
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		//TEMP:
		//if(!isLiteral(*this->value_expr))
		//	throw BaseException("Named constant was not reduced to a literal. " + errorContext(*this, payload));

		// Check that value_expr is constant now.  NOTE: not sure this is the best place/phase to do it.
		if(!value_expr->isConstant())
			throw BaseException("Named constant value was not constant. " + errorContext(*this, payload));

		const TypeRef expr_type = value_expr->type();
		if(expr_type.isNull())
			throw BaseException("Failed to compute type for named constant. " + errorContext(*this, payload));

		// Check that the type of the body expression is equal to the declared type.
		if(this->declared_type.nonNull())
		{
			if(*expr_type != *this->declared_type)
				throw BaseException("Type error for named constant '" + name + "': Computed return type '" + expr_type->toString() + 
					"' is not equal to the declared return type '" + declared_type->toString() + "'." + errorContext(*this));
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = value_expr->can_constant_fold && expressionIsWellTyped(*this, payload);
		
		const bool is_literal = checkFoldExpression(value_expr, payload);
		this->can_maybe_constant_fold = is_literal;

	/*	if(!this->isConstant())
			throw BaseException("Named constant value expression was not constant." + errorContext(*this));

		VMState vmstate;
		vmstate.func_args_start.push_back(0);

		ValueRef retval = this->value_expr->exec(vmstate);

		this->value_expr = makeLiteralASTNodeFromValue(retval, this->srcLocation(), this->type());*/
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(value_expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
	}


	stack.pop_back();
	//payload.named_constant_stack.pop_back();
	payload.current_named_constant = NULL;
}


llvm::Value* NamedConstant::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	/*if(isLiteral(*this->value_expr))
	{
		if(!llvm_value)
			llvm_value = value_expr->emitLLVMCode(params, ret_space_ptr);
		return llvm_value;
	}
	else
		return value_expr->emitLLVMCode(params, ret_space_ptr);*/
	return value_expr->emitLLVMCode(params, ret_space_ptr);
}


Reference<ASTNode> NamedConstant::clone(CloneMapType& clone_map)
{
	NamedConstant* res = new NamedConstant(declared_type, name, value_expr->clone(clone_map), srcLocation(), order_num);
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool NamedConstant::isConstant() const
{
	return value_expr->isConstant();
}


size_t NamedConstant::getTimeBound(GetTimeBoundParams& params) const
{
	return value_expr->getTimeBound(params);
}


GetSpaceBoundResults NamedConstant::getSpaceBound(GetSpaceBoundParams& params) const
{
	return value_expr->getSpaceBound(params);
}


//---------------------------------------------------------------------------------

#if 0
Value* AnonFunction::exec(VMState& vmstate)
{
	assert(0);

	// Evaluate let clauses, which will each push the result onto the let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	Value* ret = body->exec(vmstate);

	// Pop things off let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.pop_back();

	return ret;
	
}


void AnonFunction::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "AnonFunction\n";
	this->body->print(depth+1, s);
}


void AnonFunction::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	body->traverse(payload, stack);
	stack.pop_back();
}


llvm::Value* AnonFunction::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return NULL;
}

#endif


} //end namespace Lang

