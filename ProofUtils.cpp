#include "ProofUtils.h"


#include "wnt_FunctionExpression.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_IfExpression.h"
#include "VMState.h"


namespace Winter
{


static const IntervalSetInt64 updateIndexBounds(TraversalPayload& payload, const ComparisonExpression& comp_expr, const ASTNodeRef& index, const IntervalSetInt64& bounds)
{
	// We know comp_expr is of type 'i T x' where T is some comparison token

	// Check i is the same as the original array or vector index:
	if(expressionsHaveSameValue(comp_expr.a, index))
	{
		if(comp_expr.b->isConstant() && comp_expr.b->type()->getType() == Type::IntType) // if the x value is constant
		{
			// Evaluate the index expression
			VMState vmstate;
			vmstate.func_args_start.push_back(0);

			ValueRef retval = comp_expr.b->exec(vmstate);

			assert(retval->valueType() == Value::ValueType_Int);

			const int64 x_val = static_cast<IntValue*>(retval.getPointer())->value;
										
			switch(comp_expr.token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: // i < x
				return intervalSetIntersection(bounds, IntervalSetInt64(std::numeric_limits<int64>::min(), x_val - 1));
			case RIGHT_ANGLE_BRACKET_TOKEN: // i > x
				return intervalSetIntersection(bounds, IntervalSetInt64(x_val + 1, std::numeric_limits<int64>::max()));
			case DOUBLE_EQUALS_TOKEN: // i == x
				return IntervalSetInt64(x_val, x_val);
			case NOT_EQUALS_TOKEN: // i != x
				return intervalSetIntersection(bounds, intervalWithHole(x_val));
			case LESS_EQUAL_TOKEN: // i <= x
				return intervalSetIntersection(bounds, IntervalSetInt64(std::numeric_limits<int64>::min(), x_val));
			case GREATER_EQUAL_TOKEN: // i >= x
				return intervalSetIntersection(bounds, IntervalSetInt64(x_val, std::numeric_limits<int64>::max()));
			default:
				return bounds;
			}
		}
	}

	return bounds;
}


// NOTE: Are these correct?
static float myPriorFloat(float x)
{
	if(x == -std::numeric_limits<float>::infinity())
		return std::numeric_limits<float>::infinity();
	else
	{
		uint32 i;
		std::memcpy(&i, &x, 4);
		assert(i != 0);
		i--;
		std::memcpy(&x, &i, 4);
		return x;
	}
}


static float myNextFloat(float x)
{
	if(x == std::numeric_limits<float>::infinity())
		return std::numeric_limits<float>::infinity();
	else
	{
		uint32 i;
		std::memcpy(&i, &x, 4);
		assert(i != 0);
		i++;
		std::memcpy(&x, &i, 4);
		return x;
	}
}



static const IntervalSetFloat updateBounds(TraversalPayload& payload, const ComparisonExpression& comp_expr, const ASTNodeRef& index, const IntervalSetFloat& bounds)
{
	// We know comp_expr is of type 'i T x' where T is some comparison token

	// Check i is the same as the original array or vector index:
	if(expressionsHaveSameValue(comp_expr.a, index))
	{
		if(comp_expr.b->isConstant() && comp_expr.b->type()->getType() == Type::FloatType) // if the x value is constant
		{
			// Evaluate the index expression
			VMState vmstate;
			vmstate.func_args_start.push_back(0);

			ValueRef retval = comp_expr.b->exec(vmstate);

			assert(retval->valueType() == Value::ValueType_Float);

			const float x_val = static_cast<FloatValue*>(retval.getPointer())->value;
										
			switch(comp_expr.token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: // i < x
				return intervalSetIntersection(bounds, IntervalSetFloat(-std::numeric_limits<float>::infinity(), myPriorFloat(x_val)));
			case RIGHT_ANGLE_BRACKET_TOKEN: // i > x
				return intervalSetIntersection(bounds, IntervalSetFloat(myNextFloat(x_val), std::numeric_limits<float>::infinity()));
			case DOUBLE_EQUALS_TOKEN: // i == x
				return IntervalSetFloat(x_val, x_val);
			case NOT_EQUALS_TOKEN: // i != x
				return intervalSetIntersection<float>(bounds, intervalWithHole<float>(x_val));
			case LESS_EQUAL_TOKEN: // i <= x
				return intervalSetIntersection(bounds, IntervalSetFloat(-std::numeric_limits<float>::infinity(), x_val));
			case GREATER_EQUAL_TOKEN: // i >= x
				return intervalSetIntersection(bounds, IntervalSetFloat(x_val, std::numeric_limits<float>::infinity()));
			default:
				return bounds;
			}
		}
	}

	return bounds;
}


IntervalSetInt64 ProofUtils::getInt64Range(TraversalPayload& payload, std::vector<ASTNode*>& stack, const ASTNodeRef& integer_value)
{
	// Lower and upper inclusive bounds
	IntervalSetInt64 bounds;

	const TypeRef type = integer_value->type();

	// Get initial bounds based on type
	assert(type->getType() == Type::IntType);
	if(type.downcastToPtr<Int>()->numBits() == 32)
		bounds = IntervalSetInt64(std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max());
	else if(type.downcastToPtr<Int>()->numBits() == 64)
		bounds = IntervalSetInt64(std::numeric_limits<int64>::min(), std::numeric_limits<int64>::max());
	else
	{
		assert(0);
		throw BaseException("invalid num bits");
	}

	for(int z=(int)stack.size()-1; z >= 0; --z)
	{
		ASTNode* stack_node = stack[z];

		// Get next node up the call stack
		if(stack_node->nodeType() == ASTNode::IfExpressionType)
		{
			// AST node above this one is an "if" expression
			IfExpression* if_node = static_cast<IfExpression*>(stack_node);

			// Is this node the 1st arg of the if expression?
			// e.g. if condition then this_node else other_node
			// Or is this node a child of the 1st arg?
			if(/*if_node->argument_expressions[1].getPointer() == this || */((z+1) < (int)stack.size() && if_node->then_expr.getPointer() == stack[z+1]))
			{
				// Ok, now we need to check the condition of the if expression.
				// A valid proof condition will be of form
				// inBounds(array, index)
				// Where array and index are the same as the ones for this elem() call.

				if(if_node->condition->nodeType() == ASTNode::FunctionExpressionType)
				{
					FunctionExpression* condition_func_express = static_cast<FunctionExpression*>(if_node->condition.getPointer());
					
					if(condition_func_express->static_target_function && condition_func_express->static_target_function->sig.name == "inBounds")
					{
						// Is the index the same?
						if(expressionsHaveSameValue(condition_func_express->argument_expressions[1], integer_value))
						{
							const TypeRef container_type = condition_func_express->static_target_function->sig.param_types[0];

							// Update bounds to reflect that we are in bounds of the container - 
							// We know that the lower bound is >= 0 and the upper bound is < container size.
							if(container_type->getType() == Type::ArrayTypeType)
							{
								const ArrayType* array_type = static_cast<const ArrayType*>(container_type.getPointer());
								//bounds = Vec2<int>(myMax(bounds.x, 0), myMin(bounds.y, (int)array_type->num_elems - 1));
								bounds = intervalSetIntersection(bounds, IntervalSetInt64(0, (int64)array_type->num_elems - 1));
							}
							if(container_type->getType() == Type::VectorTypeType)
							{
								const VectorType* vector_type = static_cast<const VectorType*>(container_type.getPointer());
								//bounds = Vec2<int>(myMax(bounds.x, 0), myMin(bounds.y, (int)vector_type->num- 1));
								bounds = intervalSetIntersection(bounds, IntervalSetInt64(0, (int64)vector_type->num - 1));
							}
						}

						// Is the array the same? 
						/*if(expressionsHaveSameValue(condition_func_express->argument_expressions[0], this->argument_expressions[0]))
						{
							// Is the index the same?
							if(expressionsHaveSameValue(condition_func_express->argument_expressions[1], this->argument_expressions[1]))
							{
								// Success, inBounds uses the same variables, proving that the array access is in-bounds
								return;
							}
						}*/
					}
				}
				else if(if_node->condition->nodeType() == ASTNode::BinaryBooleanType)
				{
					BinaryBooleanExpr* bin = static_cast<BinaryBooleanExpr*>(if_node->condition.getPointer());
					if(bin->t == BinaryBooleanExpr::AND)
					{
						// We know condition expression is of type A AND B

						// Process A
						if(bin->a->nodeType() == ASTNode::ComparisonExpressionType)
						{
							ComparisonExpression* a = static_cast<ComparisonExpression*>(bin->a.getPointer());
							bounds = updateIndexBounds(payload, *a, integer_value, bounds);
						}

						// Process B
						if(bin->b->nodeType() == ASTNode::ComparisonExpressionType)
						{
							ComparisonExpression* b = static_cast<ComparisonExpression*>(bin->b.getPointer());
							bounds = updateIndexBounds(payload, *b, integer_value, bounds);
						}
					}
				}
				else if(if_node->condition->nodeType() == ASTNode::ComparisonExpressionType)
				{
					ComparisonExpression* comp = static_cast<ComparisonExpression*>(if_node->condition.getPointer());
					bounds = updateIndexBounds(payload, *comp, integer_value, bounds);
				}
			}
		}
	}

	return bounds;
}


IntervalSetFloat ProofUtils::getFloatRange(TraversalPayload& payload, std::vector<ASTNode*>& stack, const ASTNodeRef& float_value)
{
	// Lower and upper inclusive bounds
	IntervalSetFloat bounds(-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());

	for(int z=(int)stack.size()-1; z >= 0; --z)
	{
		ASTNode* stack_node = stack[z];

		// Get next node up the call stack
		if(stack_node->nodeType() == ASTNode::IfExpressionType)
		{
			// AST node above this one is an "if" expression
			IfExpression* if_node = static_cast<IfExpression*>(stack_node);

			// Is this node the 1st arg of the if expression?
			// e.g. if condition then this_node else other_node
			// Or is this node a child of the 1st arg?
			if(/*if_node->argument_expressions[1].getPointer() == this || */((z+1) < (int)stack.size() && if_node->then_expr.getPointer() == stack[z+1]))
			{
				// Ok, now we need to check the condition of the if expression.
				// A valid proof condition will be of form
				// inBounds(array, index)
				// Where array and index are the same as the ones for this elem() call.

				if(if_node->condition->nodeType() == ASTNode::FunctionExpressionType)
				{
					// FunctionExpression* condition_func_express = static_cast<FunctionExpression*>(if_node->condition.getPointer());
				}
				else if(if_node->condition->nodeType() == ASTNode::BinaryBooleanType)
				{
					BinaryBooleanExpr* bin = static_cast<BinaryBooleanExpr*>(if_node->condition.getPointer());
					if(bin->t == BinaryBooleanExpr::AND)
					{
						// We know condition expression is of type A AND B

						// Process A
						if(bin->a->nodeType() == ASTNode::ComparisonExpressionType)
						{
							ComparisonExpression* a = static_cast<ComparisonExpression*>(bin->a.getPointer());
							bounds = updateBounds(payload, *a, float_value, bounds);
						}

						// Process B
						if(bin->b->nodeType() == ASTNode::ComparisonExpressionType)
						{
							ComparisonExpression* b = static_cast<ComparisonExpression*>(bin->b.getPointer());
							bounds = updateBounds(payload, *b, float_value, bounds);
						}
					}
				}
				else if(if_node->condition->nodeType() == ASTNode::ComparisonExpressionType)
				{
					ComparisonExpression* comp = static_cast<ComparisonExpression*>(if_node->condition.getPointer());
					bounds = updateBounds(payload, *comp, float_value, bounds);
				}
			}
		}
	}

	return bounds;
}


} // end namespace Winter
