/*=====================================================================
LetASTNode.cpp
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "wnt_LetASTNode.h"


#include "VMState.h"
#include "utils/StringUtils.h"


namespace Winter
{


LetASTNode::LetASTNode(const std::vector<LetNodeVar>& vars_, const ASTNodeRef& expr_, const SrcLocation& loc)
:	ASTNode(LetType, loc), 
	expr(expr_),
	vars(vars_),
	traced(false)
{
	assert(!vars.empty());
}


ValueRef LetASTNode::exec(VMState& vmstate)
{
	ValueRef res = this->expr->exec(vmstate);

	if(vmstate.trace && !traced)
	{
		*vmstate.ostream << vmstate.indent() << "    " << this->vars[0].name << " = " << res->toString() << std::endl;
		traced = true;
	}

	return res;
}


void LetASTNode::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Let node (" + toHexString((uint64)this) + "), var_name = '" + this->vars[0].name + "'\n"; // TODO: print out other var names as well
	this->expr->print(depth+1, s);
}


std::string LetASTNode::sourceString() const
{
	if(vars.size() == 1)
	{
		return vars[0].name + " = " + expr->sourceString();
	}
	else
	{
		std::string s;
		for(size_t i=0; i<vars.size(); ++i)
		{
			s += vars[i].name;
			if(i + 1 < vars.size())
				s += ", ";
		}
		s += " = " + expr->sourceString();
		return s;
	}
}


std::string LetASTNode::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return this->expr->emitOpenCLC(params);
}


void LetASTNode::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(expr, payload);
	}*/
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload, stack);
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
	else if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(vars.size() == 1)
		{
			// Check that the return type of the body expression is equal to the declared return type
			// of this function.
			if(vars[0].declared_type.nonNull())
				if(*expr->type() != *vars[0].declared_type)
					throw BaseException("Type error for let '" + vars[0].name + "': Computed return type '" + this->expr->type()->toString() + 
						"' is not equal to the declared return type '" + vars[0].declared_type->toString() + "'." + errorContext(*this));
		}
		else
		{
			assert(vars.size() > 1);
			if(expr->type()->getType() != Type::TupleTypeType)
				throw BaseException("Type error for let with destructuring assignment.  Value expression must have tuple type." + errorContext(*this)); 

			const TupleTypeRef tuple_type = expr->type().downcast<TupleType>();

			if(tuple_type->component_types.size() != vars.size())
				throw BaseException("Number of let vars must equal num elements in tuple." + errorContext(*this)); 


			for(size_t i=0; i<vars.size(); ++i)
			{
				if(vars[i].declared_type.nonNull())
				{
					if(*tuple_type->component_types[i] != *vars[i].declared_type)
						throw BaseException("Type error for let '" + vars[i].name + "': Computed return type '" + tuple_type->component_types[i]->toString() + 
							"' is not equal to the declared return type '" + vars[i].declared_type->toString() + "'." + errorContext(*this));
				}
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		
		for(size_t i=0; i<vars.size(); ++i)
		{
			// Do int -> float coercion
			if(expr->nodeType() == ASTNode::IntLiteralType && vars[i].declared_type.nonNull() && vars[i].declared_type->getType() == Type::FloatType)
			{
				const IntLiteral* body_lit = static_cast<const IntLiteral*>(expr.getPointer());
				if(isIntExactlyRepresentableAsFloat(body_lit->value))
				{
					expr = new FloatLiteral((float)body_lit->value, body_lit->srcLocation());
					payload.tree_changed = true;
				}
			}

			// Do int -> double coercion
			if(expr->nodeType() == ASTNode::IntLiteralType && vars[i].declared_type.nonNull() && vars[i].declared_type->getType() == Type::DoubleType)
			{
				const IntLiteral* body_lit = static_cast<const IntLiteral*>(expr.getPointer());
				expr = new DoubleLiteral((double)body_lit->value, body_lit->srcLocation());
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		//this->can_constant_fold = expr->can_constant_fold && expressionIsWellTyped(*this, payload);
		const bool is_literal = checkFoldExpression(expr, payload);
		this->can_maybe_constant_fold = is_literal;
	}
	else if(payload.operation == TraversalPayload::GetAllNamesInScope)
	{
		for(size_t i=0; i<vars.size(); ++i)
			payload.used_names->insert(vars[i].name);
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		doDeadCodeElimination(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
	}

	stack.pop_back();
}


llvm::Value* LetASTNode::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	//if(!llvm_value)
	//	llvm_value = expr->emitLLVMCode(params);

	//return llvm_value;
	return expr->emitLLVMCode(params);

	//llvm::Value* v = expr->emitLLVMCode(params);
	
	// If this is a string value, need to decr ref count at end of func.
	/*if(this->type()->getType() == Type::StringType)
	{
		params.cleanup_values.push_back(CleanUpInfo(this, v));
	}*/

	//return v;


}


//void LetASTNode::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
//{
//	//if(!(expr->nodeType() == ASTNode::VariableASTNodeType && expr.downcastToPtr<Variable>()->vartype == Variable::LetVariable)) // Don't decr let var ref counts, the ref block will do that.
//	//	this->type()->emitDecrRefCount(params, val);
//		// RefCounting::emitCleanupLLVMCode(params, this->type(), val);
//}


Reference<ASTNode> LetASTNode::clone(CloneMapType& clone_map)
{
	LetASTNode* e = new LetASTNode(this->vars, this->expr->clone(clone_map), this->srcLocation());
	clone_map.insert(std::make_pair(this, e));
	return e;
}


bool LetASTNode::isConstant() const
{
	return expr->isConstant();
}


} // end namespace Winter
