/*=====================================================================
FunctionExpression.cpp
----------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-04-30 18:53:38 +0100
=====================================================================*/
#include "wnt_FunctionExpression.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_VectorLiteral.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_VArrayLiteral.h"
#include "wnt_Variable.h"
#include "wnt_LetASTNode.h"
#include "wnt_LetBlock.h"
#include "VirtualMachine.h"
#include "VMState.h"
#include "Value.h"
#include "CompiledValue.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMUtils.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "wnt_IfExpression.h"
#include "utils/StringUtils.h"
#include "utils/ConPrint.h"
#include "utils/ContainerUtils.h"
#include "maths/mathstypes.h"
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
#include <iostream>


using std::vector;
using std::string;


namespace Winter
{


static const bool VERBOSE_EXEC = false;


FunctionExpression::FunctionExpression(const SrcLocation& src_loc) 
:	ASTNode(FunctionExpressionType, src_loc),
	static_target_function(NULL),
	proven_defined(false)
{
}


FunctionExpression::FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0) // 1-arg function
:	ASTNode(FunctionExpressionType, src_loc),
	static_target_function(NULL),
	proven_defined(false)
{
	static_function_name = func_name;

	argument_expressions.push_back(arg0);
}


FunctionExpression::FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1) // 2-arg function
:	ASTNode(FunctionExpressionType, src_loc),
	static_target_function(NULL),
	proven_defined(false)
{
	static_function_name = func_name;

	argument_expressions.push_back(arg0);
	argument_expressions.push_back(arg1);
}


typedef float (* FLOAT1_TO_FLOAT_TYPE)(float);
typedef float (* FLOAT2_TO_FLOAT_TYPE)(float, float);
typedef double (* DOUBLE1_TO_DOUBLE_TYPE)(double);
typedef double (* DOUBLE2_TO_DOUBLE_TYPE)(double, double);
typedef bool (* FLOAT1_TO_BOOL_TYPE)(float);
typedef bool (* DOUBLE1_TO_BOOL_TYPE)(double);


ValueRef FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) conPrint(vmstate.indent() + "FunctionExpression, target_name=" + this->functionName() + "\n");

	if(vmstate.func_args_start.size() > 1000)
		throw ExceptionWithPosition("Function call level too deep, aborting.", errorContext(this));


	if(this->static_target_function != NULL && this->static_target_function->external_function.nonNull())
	{
		// For external functions with certain type signatures, we can call the native function directly:
		if(static_target_function->returnType()->getType() == Type::FloatType)
		{
			if(static_target_function->args.size() == 1 && (static_target_function->args[0].type->getType() == Type::FloatType))
			{
				if(this->argument_expressions.size() != 1) throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				FLOAT1_TO_FLOAT_TYPE f = (FLOAT1_TO_FLOAT_TYPE)this->static_target_function->external_function->func;
				return vmstate.value_allocator->allocFloatValue(f(checkedCast<FloatValue>(arg0)->value));
			}
			else if(static_target_function->args.size() == 2 && 
				(static_target_function->args[0].type->getType() == Type::FloatType) &&
				(static_target_function->args[1].type->getType() == Type::FloatType))
			{
				if(this->argument_expressions.size() != 2) throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				ValueRef arg1 = this->argument_expressions[1]->exec(vmstate);
				FLOAT2_TO_FLOAT_TYPE f = (FLOAT2_TO_FLOAT_TYPE)this->static_target_function->external_function->func;
				return vmstate.value_allocator->allocFloatValue(f(checkedCast<FloatValue>(arg0)->value, checkedCast<FloatValue>(arg1)->value));
			}
		}

		if(static_target_function->returnType()->getType() == Type::DoubleType)
		{
			if(static_target_function->args.size() == 1 && (static_target_function->args[0].type->getType() == Type::DoubleType))
			{
				if(this->argument_expressions.size() != 1) throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				DOUBLE1_TO_DOUBLE_TYPE f = (DOUBLE1_TO_DOUBLE_TYPE)this->static_target_function->external_function->func;
				return new DoubleValue(f(checkedCast<DoubleValue>(arg0)->value));
			}
			else if(static_target_function->args.size() == 2 && 
				(static_target_function->args[0].type->getType() == Type::DoubleType) &&
				(static_target_function->args[1].type->getType() == Type::DoubleType))
			{
				if(this->argument_expressions.size() != 2) throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				ValueRef arg1 = this->argument_expressions[1]->exec(vmstate);
				DOUBLE2_TO_DOUBLE_TYPE f = (DOUBLE2_TO_DOUBLE_TYPE)this->static_target_function->external_function->func;
				return new DoubleValue(f(checkedCast<DoubleValue>(arg0)->value, checkedCast<DoubleValue>(arg1)->value));
			}
		}

		if(static_target_function->returnType()->getType() == Type::BoolType)
		{
			if(static_target_function->args.size() == 1 && (static_target_function->args[0].type->getType() == Type::FloatType))
			{
				if(this->argument_expressions.size() != 1)
					throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				FLOAT1_TO_BOOL_TYPE f = (FLOAT1_TO_BOOL_TYPE)this->static_target_function->external_function->func;
				return new BoolValue(f(checkedCast<FloatValue>(arg0)->value));
			}
			if(static_target_function->args.size() == 1 && (static_target_function->args[0].type->getType() == Type::DoubleType))
			{
				if(this->argument_expressions.size() != 1)
					throw ExceptionWithPosition("Invalid num args.", errorContext(this));
				ValueRef arg0 = this->argument_expressions[0]->exec(vmstate);
				DOUBLE1_TO_BOOL_TYPE f = (DOUBLE1_TO_BOOL_TYPE)this->static_target_function->external_function->func;
				return new BoolValue(f(checkedCast<DoubleValue>(arg0)->value));
			}
		}

		// Else call the interpreted function.
		vector<ValueRef> args;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			args.push_back(this->argument_expressions[i]->exec(vmstate));

		ValueRef result = this->static_target_function->external_function->interpreted_func(args);

		return result;
	}
	
	// Get target function from the expression that returns the function
	ValueRef base_target_function_val;
	const FunctionValue* target_func_val = NULL;
	FunctionDefinition* use_target_func;
	if(static_target_function)
	{
		use_target_func = static_target_function;
	}
	else
	{
		if(this->get_func_expr.isNull())
			throw ExceptionWithPosition("Function is not bound.", errorContext(this));

		base_target_function_val = this->get_func_expr->exec(vmstate);
		target_func_val = checkedCast<FunctionValue>(base_target_function_val);
		use_target_func = target_func_val->func_def;
	}



	// Push arguments onto argument stack
	const size_t initial_arg_stack_size = vmstate.argument_stack.size();

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
	{
		vmstate.argument_stack.push_back(this->argument_expressions[i]->exec(vmstate));
		if(VERBOSE_EXEC) 
		{
			//std::cout << indent(vmstate) << "Pushed arg " << vmstate.argument_stack.back()->toString() << "\n";
			//printStack(vmstate);
		}
	}

	// If the target function is an anon function and has captured values, push that onto the stack
	if(use_target_func->is_anon_func)// use_captured_vars)
	{
		assert(target_func_val);
		vmstate.argument_stack.push_back(target_func_val->captured_vars.getPointer());
	}


	// Execute target function
	vmstate.func_args_start.push_back((unsigned int)initial_arg_stack_size);

	if(VERBOSE_EXEC)
		conPrint(vmstate.indent() + "Calling " + use_target_func->sig.toString() + ", func_args_start: " + toString(vmstate.func_args_start.back()) + "\n");

	ValueRef ret = use_target_func->invoke(vmstate);
	vmstate.func_args_start.pop_back();

	// Remove arguments from stack
	vmstate.argument_stack.resize(initial_arg_stack_size);

	return ret;
}


bool FunctionExpression::doesFunctionTypeMatch(const TypeRef& type)
{
	if(type->getType() != Type::FunctionType)
		return false;

	const Function* func = static_cast<const Function*>(type.getPointer());

	std::vector<TypeRef> arg_types(this->argument_expressions.size());
	for(unsigned int i=0; i<arg_types.size(); ++i)
	{
		arg_types[i] = this->argument_expressions[i]->type();
		if(arg_types[i].isNull())
			return false;
	}

	if(arg_types.size() != func->arg_types.size())
		return false;

	for(unsigned int i=0; i<arg_types.size(); ++i)
		if(!(*(arg_types[i]) == *(func->arg_types[i])))
			return false;
	return true;
}


/*static bool couldCoerceFunctionCall(vector<ASTNodeRef>& argument_expressions, FunctionDefinitionRef func)
{
	if(func->args.size() != argument_expressions.size())
		return false;
	

	for(size_t i=0; i<argument_expressions.size(); ++i)
	{
		if(*func->args[i].type == *argument_expressions[i]->type())
		{
		}
		else if(	func->args[i].type->getType() == Type::FloatType &&
			argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
			isIntExactlyRepresentableAsFloat(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
		{
		}
		else
			return false;
	}

	return true;
}*/


/*static bool isTargetDefinedBeforeAllInStack(const std::vector<FunctionDefinition*>& func_def_stack, const FunctionDefinition* target_function)
{
	if(!target_function->srcLocation().isValid()) // If target is a built-in function etc.. then there are no ordering problems.
		return true;

	for(size_t i=0; i<func_def_stack.size(); ++i)
		if(target_function->order_num >= func_def_stack[i]->order_num)
			return false;

	return true;
}*/


void FunctionExpression::tryCoerceIntArgsToDoubles(Linker& linker, const std::vector<TypeVRef>& argtypes, int effective_callsite_order_num)
{
	if(this->static_target_function)
		return;

	vector<TypeVRef> coerced_argtypes = argtypes;

	for(size_t i=0; i<argtypes.size(); ++i)
	{
		if(	argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
			isIntExactlyRepresentableAsDouble(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
		{
			coerced_argtypes[i] = new Double();
		}
	}

	// Try again with our coerced arguments
	const FunctionSignature coerced_sig(this->static_function_name, coerced_argtypes);

	this->static_target_function = linker.findMatchingFunction(coerced_sig, this->srcLocation(), effective_callsite_order_num/*&payload.func_def_stack*/).getPointer();
	if(this->static_target_function/* && isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_function)*/) // Disallow recursion for now: Check the linked function is not the current function.
	{
		// Success!  We need to actually change the argument expressions now
		for(size_t i=0; i<argument_expressions.size(); ++i)
		{
			if(	argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
				isIntExactlyRepresentableAsDouble(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
			{
				// Replace int literal with double literal
				this->argument_expressions[i] = new DoubleLiteral(
					(float)static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value,
					argument_expressions[i]->srcLocation()
					);
			}
		}
	}
}


void FunctionExpression::tryCoerceIntArgsToFloats(Linker& linker, const std::vector<TypeVRef>& argtypes, int effective_callsite_order_num)
{
	if(this->static_target_function)
		return;

	vector<TypeVRef> coerced_argtypes = argtypes;

	for(size_t i=0; i<argtypes.size(); ++i)
	{
		if(	argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
			isIntExactlyRepresentableAsFloat(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
		{
			coerced_argtypes[i] = new Float();
		}
	}

	// Try again with our coerced arguments
	const FunctionSignature coerced_sig(this->static_function_name, coerced_argtypes);

	this->static_target_function = linker.findMatchingFunction(coerced_sig, this->srcLocation(), effective_callsite_order_num/*&payload.func_def_stack*/).getPointer();
	if(this->static_target_function/* && isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_function)*/) // Disallow recursion for now: Check the linked function is not the current function.
	{
		// Success!  We need to actually change the argument expressions now
		for(size_t i=0; i<argument_expressions.size(); ++i)
		{
			if(	argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
				isIntExactlyRepresentableAsFloat(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
			{
				// Replace int literal with float literal
				this->argument_expressions[i] = new FloatLiteral(
					(float)static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value,
					argument_expressions[i]->srcLocation()
					);
			}
		}
	}
}


// Statically bind to global function definition.
void FunctionExpression::bindFunction(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	// Return if we have already bound this function in an earlier pass.
	if(this->static_function_name.empty() || static_target_function != NULL)
		return;

	// We want to find a function that matches our argument expression types, and the function name

	{
		vector<TypeVRef> argtypes;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		{
			const TypeRef arg_expr_type = this->argument_expressions[i]->type();
			if(arg_expr_type.isNull())
				return; // Don't try and bind if we have a null type
			argtypes.push_back(TypeVRef(arg_expr_type));
		}

		const FunctionSignature sig(this->static_function_name, argtypes);

		// Work out effective call site position.
		int effective_callsite_order_num = 1000000000;
		if(payload.current_named_constant)
			effective_callsite_order_num = payload.current_named_constant->order_num;
		for(size_t z=0; z<payload.func_def_stack.size(); ++z)
			effective_callsite_order_num = myMin(effective_callsite_order_num, payload.func_def_stack[z]->order_num);

		// Try and resolve to internal function.
		this->static_target_function = linker.findMatchingFunction(sig, this->srcLocation(), effective_callsite_order_num/*&payload.func_def_stack*/).getPointer();
		if(this->static_target_function/* && isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_function)*/) // Disallow recursion for now: Check the linked function is not the current function.
		{
		}
		else
		{
			// Try and promote integer args to double args.
			// TODO: try all possible coercion combinations.
			// This is not really the best way of doing this type coercion, the old approach of matching by name is better.

			if(linker.try_coerce_int_to_double_first)
			{
				tryCoerceIntArgsToDoubles(linker, argtypes, effective_callsite_order_num);
				tryCoerceIntArgsToFloats(linker, argtypes, effective_callsite_order_num);
			}
			else
			{
				tryCoerceIntArgsToFloats(linker, argtypes, effective_callsite_order_num);
				tryCoerceIntArgsToDoubles(linker, argtypes, effective_callsite_order_num);
			}

			/*vector<FunctionDefinitionRef> funcs;
			linker.getFuncsWithMatchingName(sig.name, funcs);

			vector<FunctionDefinitionRef> possible_matches;

			for(size_t z=0; z<funcs.size(); ++z)
				if(couldCoerceFunctionCall(argument_expressions, funcs[z]))
					possible_matches.push_back(funcs[z]);

			if(possible_matches.size() == 1)
			{
				for(size_t i=0; i<argument_expressions.size(); ++i)
				{
					if(	possible_matches[0]->args[i].type->getType() == Type::FloatType &&
						argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
						isIntExactlyRepresentableAsFloat(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
					{
						// Replace int literal with float literal
						this->argument_expressions[i] = ASTNodeRef(new FloatLiteral(
							(float)static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value,
							argument_expressions[i]->srcLocation()
							));
					}
				}

				
				this->target_function = possible_matches[0].getPointer();
				this->binding_type = BoundToGlobalDef;
			}
			else if(possible_matches.size() > 1)
			{
				string s = "Found more than one possible match for overloaded function: \n";
				for(size_t z=0; z<possible_matches.size(); ++z)
					s += possible_matches[z]->sig.toString() + "\n";
				throw BaseException(s + "." , errorContext(*this));
			}*/
		}

		if(this->static_target_function)
			this->static_target_function->num_uses++;

		//TEMP: don't fail now, maybe we can bind later.
		//if(this->binding_type == Unbound)
		//	throw BaseException("Failed to find function '" + sig.toString() + "'." , errorContext(*this));
	}
}


static bool varWithNameIsInScope(const std::string& name, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	for(int s = (int)stack.size() - 1; s >= 0; --s) // Walk up the stack of ancestor nodes
	{
		if(stack[s]->nodeType() == ASTNode::FunctionDefinitionType) // If node is a function definition:
		{
			FunctionDefinition* def = static_cast<FunctionDefinition*>(stack[s]);

			for(unsigned int i=0; i<def->args.size(); ++i) // For each argument to the function:
				if(def->args[i].name == name) // If the argument name matches this variable name:
					if(def->args[i].type->getType() == Type::FunctionType) // Since this is a function argument, we can tell its scope.  Only consider it if it has function type.
						return true;
		}
		else if(stack[s]->nodeType() == ASTNode::LetBlockType)
		{
			LetBlock* let_block = static_cast<LetBlock*>(stack[s]);
			
			for(unsigned int i=0; i<let_block->lets.size(); ++i)
			{
				// If the variable we are tring to bind is in a let expression for the current Let Block, then
				// we only want to bind to let variables from let expressions that are *before* the current let expression.
				// In cases like
				// let
				//   x = x
				// This avoids the x expression on the right binding to the x Let node on the left.
				// In cases like this:
				// let
				//	z = y
				//	y = x
				// it also prevent y from binding to the y from the line below. (which could cause a cycle of references)
				if((s + 1 < (int)stack.size()) && (stack[s+1]->nodeType() == ASTNode::LetType) && (let_block->lets[i].getPointer() == stack[s+1]))
				{
					// We have reached the let expression for the current variable we are tring to bind, so don't try and bind with let variables equal to or past this one.
					break;
				}
				else
				{
					for(size_t v=0; v<let_block->lets[i]->vars.size(); ++v)
						if(let_block->lets[i]->vars[v].name == name)
							return true;
				}
			}
		}
	}

	// Consider named constants
	Linker::NamedConstantMap::iterator name_res = payload.linker->named_constant_map.find(name);
	if(name_res != payload.linker->named_constant_map.end())
	{
		const NamedConstant* target_named_constant = name_res->second.getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_named_constant->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_named_constant->order_num))
			return true;
	}

	return false;
}


void FunctionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	//if(payload.operation == TraversalPayload::ConstantFolding)
	//{
	//	for(size_t i=0; i<argument_expressions.size(); ++i)
	//		if(shouldFoldExpression(argument_expressions[i], payload))
	//		{
	//			try
	//			{
	//				argument_expressions[i] = foldExpression(argument_expressions[i], payload);
	//				payload.tree_changed = true;
	//			}
	//			catch(BaseException& )
	//			{
	//				// An invalid operation was performed, such as dividing by zero, while trying to eval the AST node.
	//				// In this case we will consider the folding as not taking place.
	//			}
	//		}
	//}
	/*else */if(payload.operation == TraversalPayload::TypeCoercion)
	{
	}
	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
	}
	

	// NOTE: we want to do a post-order traversal here.
	// This is because we want our argument expressions to be linked first.


	// If we have a get_func_expr, and it is a variable, and if there are no such names in scope to bind to, 
	// then convert to a 'static' function binding, so we can do stuff like function overloading.
	if(payload.operation == TraversalPayload::BindVariables)
	{
		if(get_func_expr.nonNull() && (get_func_expr->nodeType() == ASTNode::VariableASTNodeType))
		{
			if((get_func_expr.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Unbound) && !varWithNameIsInScope(get_func_expr.downcastToPtr<Variable>()->name, payload, stack))
			{
				// Convert to static function
				this->static_function_name = get_func_expr.downcastToPtr<Variable>()->name;
				get_func_expr = NULL;
			}
		}


		// Convert get_func_expr variable expressions bound to a global def to direct static bindings.  This kind of conversion may be possible after inlining.
		// NOTE: there is is probably a better way of doing this.
		if(!this->static_target_function && get_func_expr.nonNull())
		{
			if(this->get_func_expr->nodeType() == ASTNode::VariableASTNodeType && 
				this->get_func_expr.downcastToPtr<Variable>()->binding_type == Variable::BindingType_GlobalDef)
			{
				this->static_target_function = this->get_func_expr.downcastToPtr<Variable>()->bound_function;
				this->get_func_expr = NULL;
			}
		}
	}


	stack.push_back(this);

	if(get_func_expr.nonNull())
		this->get_func_expr->traverse(payload, stack);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->traverse(payload, stack);

	
	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(payload, stack);
	}
	else if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
		// If this is a generic function, we can't try and bind function expressions yet,
		// because the binding depends on argument type due to function overloading, so we have to wait
		// until we know the concrete type.

		if(payload.func_def_stack.empty() || !payload.func_def_stack.back()->isGenericFunction())
			bindFunction(*payload.linker, payload, stack);

		// Set shuffle mask now
		if(this->static_target_function && ::hasPrefix(this->static_target_function->sig.name, "shuffle"))
		{
			assert(this->argument_expressions.size() == 2);
			if(!this->argument_expressions[1]->isConstant())
				throw ExceptionWithPosition("Second arg to shuffle must be constant", errorContext(this));

			try
			{
				VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef res = this->argument_expressions[1]->exec(vmstate);

				const VectorValue* res_v = checkedCast<VectorValue>(res);
				
				std::vector<int> mask(res_v->e.size());
				for(size_t i=0; i<mask.size(); ++i)
				{
					if(res_v->e[i]->valueType() != Value::ValueType_Int)
						throw ExceptionWithPosition("Element in shuffle mask was not an integer.", errorContext(this));

					const int64 index = static_cast<IntValue*>(res_v->e[i].getPointer())->value;
					mask[i] = (int)index;
				}

				assert(this->static_target_function->built_in_func_impl.nonNull());
				assert(this->static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_ShuffleBuiltInFunc);
				static_cast<ShuffleBuiltInFunc*>(this->static_target_function->built_in_func_impl.getPointer())->setShuffleMask(mask);
			}
			catch(ExceptionWithPosition& e)
			{
				throw ExceptionWithPosition("Failed to eval second arg of shuffle: " + e.what(), errorContext(this));
			}
		}
		// Set second arg now for elem(tuple, i)
		else if(this->static_target_function && this->static_target_function->sig.name == "elem" && static_target_function->sig.param_types[0]->getType() == Type::TupleTypeType)
		{
			assert(this->argument_expressions.size() == 2);
			if(!this->argument_expressions[1]->isConstant())
				throw ExceptionWithPosition("Second arg to elem(tuple, i) must be constant", errorContext(this));

			int64 index;
			try
			{
				VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef res = this->argument_expressions[1]->exec(vmstate);

				const IntValue* res_i = checkedCast<IntValue>(res.getPointer());

				index = res_i->value;
			}
			catch(ExceptionWithPosition& e)
			{
				throw ExceptionWithPosition("Failed to eval second arg of elem(tuple, i): " + e.what(), errorContext(this));
			}	

			assert(this->static_target_function->built_in_func_impl.nonNull());
			assert(this->static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_GetTupleElementBuiltInFunc);
			GetTupleElementBuiltInFunc* tuple_elem_func = static_cast<GetTupleElementBuiltInFunc*>(this->static_target_function->built_in_func_impl.getPointer());
				

			// bounds check index.
			if(index < 0 || index >= (int64)tuple_elem_func->tuple_type->component_types.size())
				throw ExceptionWithPosition("Second argument to tuple elem() function is out of range.", errorContext(*this));


			tuple_elem_func->setIndex((int)index);//TODO: remove cast

			// Set proper return type for function definition.
			this->static_target_function->declared_return_type = tuple_elem_func->tuple_type->component_types[index];
			
		}
		else if(this->static_target_function && this->static_target_function->sig.name == "fold")
		{
			// TEMP: specialise fold for the passed in function now.
			//if(this->binding_type == BoundToGlobalDef)
			//{
				assert(this->static_target_function->built_in_func_impl.nonNull());
				assert(this->static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_ArrayFoldBuiltInFunc);
				ArrayFoldBuiltInFunc* fold_func = static_cast<ArrayFoldBuiltInFunc*>(this->static_target_function->built_in_func_impl.getPointer());

				// Eval first arg (to get function 'f')
				try
				{
					VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
					vmstate.func_args_start.push_back(0);
					ValueRef res = this->argument_expressions[0]->exec(vmstate);

					const FunctionValue* res_f = checkedCast<FunctionValue>(res);

					fold_func->specialiseForFunctionArg(res_f->func_def);
				}
				catch(ExceptionWithPosition& e)
				{
					throw ExceptionWithPosition("Failed to eval first arg of fold " + e.what(), errorContext(this));
				}
			//}
		}
		else if(this->static_target_function && this->static_target_function->sig.name == "map")
		{
			// TEMP: specialise map for the passed in function now.
			//if(this->binding_type == BoundToGlobalDef)
			//{
				assert(this->static_target_function->built_in_func_impl.nonNull());
				assert(this->static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_ArrayMapBuiltInFunc);
				ArrayMapBuiltInFunc* map_func = static_cast<ArrayMapBuiltInFunc*>(this->static_target_function->built_in_func_impl.getPointer());

				// Eval first arg (to get function 'f')
				try
				{
					VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
					vmstate.func_args_start.push_back(0);
					ValueRef res = this->argument_expressions[0]->exec(vmstate);

					const FunctionValue* res_f = checkedCast<FunctionValue>(res);

					map_func->specialiseForFunctionArg(res_f->func_def);
				}
				catch(ExceptionWithPosition& e)
				{
					throw ExceptionWithPosition("Failed to eval first arg of map " + e.what(), errorContext(this));
				}
			//}
		}

		if(payload.check_bindings && get_func_expr.isNull() && (static_target_function == NULL)) //this->binding_type == Unbound)
		{
			//throw BaseException("Failed to find function '" + this->function_name + "' for the given argument types." + errorContext(*this));

			vector<TypeVRef> argtypes;
			for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			{
				const TypeRef arg_expr_type = this->argument_expressions[i]->type(); // may be NULL
				if(arg_expr_type.isNull())
					throw ExceptionWithPosition("Failed to find function '" + this->static_function_name + "', argument " + toString(i + 1) + " had unknown type.", errorContext(*this));

				argtypes.push_back(TypeVRef(arg_expr_type));
			}

			const FunctionSignature sig(this->static_function_name, argtypes);

			std::string msg = "Failed to find function '" + sig.toString() + "'.";

			// Print out signatures of other functions with the same name
			std::vector<FunctionDefinitionRef> funcs_same_name;
			payload.linker->getFuncsWithMatchingName(this->static_function_name, funcs_same_name);
			if(!funcs_same_name.empty())
			{
				msg += "\nOther functions with the same name: \n";
				for(size_t i=0; i<funcs_same_name.size(); ++i)
					msg += funcs_same_name[i]->sig.toString() + "\n";
			}

			throw ExceptionWithPosition(msg, errorContext(this));
		}
	}
	else if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkInDomain(payload, stack);
		this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		// If the function is bound at runtime, need to do some type-checking here.
		if(this->get_func_expr.nonNull())
		{
			const TypeRef get_func_expr_type = this->get_func_expr->type();
			if(get_func_expr_type->getType() != Type::FunctionType)
				throw ExceptionWithPosition("expression did not have function type.", errorContext(*get_func_expr));

			const Function* function_type = get_func_expr_type.downcastToPtr<Function>();

			if(function_type->arg_types.size() != argument_expressions.size())
				throw ExceptionWithPosition("Incorrect number of arguments for function.", errorContext(*get_func_expr));

			for(size_t i=0; i<function_type->arg_types.size(); ++i)
			{
				if(*argument_expressions[i]->type() != *function_type->arg_types[i])
					throw ExceptionWithPosition("Invalid type for argument: argument type was " + argument_expressions[i]->type()->toString() + ", expected type " + function_type->arg_types[i]->toString() + ".", errorContext(*get_func_expr));
			}
		}

		// Check the argument expression types still match the function argument types.
		// They may have changed due to e.g. type coercion from int->float, in which case they won't be valid any more.
		/*vector<TypeRef> argtypes(argument_expressions.size());
		for(size_t i=0; i<argument_expressions.size(); ++i)
			argtypes[i] = argument_expressions[i]->type();

		if(this->binding_type == BoundToGlobalDef)
		{
			for(size_t i=0; i<argument_expressions.size(); ++i)
				if(*argument_expressions[i]->type() != *target_function->args[i].type)


		}*/


		//if(this->binding_type == Unbound)
		//{
		//	vector<TypeRef> argtypes;
		//	//bool has_null_argtype = false;
		//	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		//	{
		//		argtypes.push_back(this->argument_expressions[i]->type());
		//		//if(argtypes.back().isNull())
		//		//	has_null_argtype = true;
		//	}

		//	/*if(has_null_argtype)
		//	{
		//		throw BaseException("Failed to find function '" + this->function_name + "'." + errorContext(*this));
		//	}
		//	else*/
		//	{
		//		const FunctionSignature sig(this->function_name, argtypes);
		//
		//		throw BaseException("Failed to find function '" + sig.toString() + "'." + errorContext(*this));
		//	}
		//}
		if(this->static_target_function)//this->binding_type == BoundToGlobalDef)
		{
			// Check shuffle mask (arg 1) is a vector of ints
			if(::hasPrefix(this->static_target_function->sig.name, "shuffle"))
			{
				// TODO
			}
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		// TODO: check function is bound etc..?
		if(this->static_target_function)
		{
			// If the target function is an external function, but the function ptr is null, we can't call it, so don't try and constant fold.
			if(this->static_target_function->external_function.nonNull() && this->static_target_function->external_function->func == NULL)
				this->can_maybe_constant_fold = false;
			else
			{
				/*this->can_constant_fold = true;
				for(size_t i=0; i<argument_expressions.size(); ++i)
					can_constant_fold = can_constant_fold && argument_expressions[i]->can_constant_fold;
				this->can_constant_fold = this->can_constant_fold && expressionIsWellTyped(*this, payload);*/
				this->can_maybe_constant_fold = true;
				for(size_t i=0; i<argument_expressions.size(); ++i)
				{
					const bool arg_is_literal = checkFoldExpression(argument_expressions[i], payload, stack);
					this->can_maybe_constant_fold = this->can_maybe_constant_fold && arg_is_literal;
				}
			}
		}
		else
			this->can_maybe_constant_fold = false;
	}
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		// if we have traversed here in the DeadFunctionElimination pass, we know this function is reachable.
		if(this->static_target_function)
		{
			payload.reachable_nodes.insert(this->static_target_function); // Mark as alive
			if(payload.processed_nodes.find(this->static_target_function) == payload.processed_nodes.end()) // If not processed yet:
				payload.nodes_to_process.push_back(this->static_target_function); // Add to to-process list
		}
	}
	else if(payload.operation == TraversalPayload::CountFunctionCalls)
	{
		if(this->static_target_function)
			payload.calls_to_func_count[this->static_target_function]++;
		else if(this->get_func_expr.nonNull())
		{
			// Walk up the tree until we get to a node that is not a variable bound to a let node:
			ASTNode* cur = this->get_func_expr.getPointer();
			while((cur->nodeType() == ASTNode::VariableASTNodeType) && (((Variable*)cur)->binding_type == Variable::BindingType_Let))
				cur = ((Variable*)cur)->bound_let_node->expr.getPointer();

			if(cur->nodeType() == ASTNode::FunctionDefinitionType)
				payload.calls_to_func_count[(FunctionDefinition*)cur]++;
			else if(cur->nodeType() == ASTNode::VariableASTNodeType && ((Variable*)cur)->binding_type == Variable::BindingType_GlobalDef)
				payload.calls_to_func_count[((Variable*)cur)->bound_function]++;
		}
	}

	stack.pop_back();
}


/*
If node 'e' is a function expression, inline the target function by replacing e with the target function body.
*/
void FunctionExpression::checkInlineExpression(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	FunctionDefinition* target_func = NULL;
	bool is_beta_reduction = false; // Is this a lambda being applied directly, e.g. an expression like "(\\(float y) : y*y) (x)" ?
	if(this->static_target_function)
	{
		target_func = this->static_target_function;
	}
	else if(this->get_func_expr.nonNull())
	{
		if(this->get_func_expr->nodeType() == ASTNode::FunctionDefinitionType)
		{
			target_func = this->get_func_expr.downcastToPtr<FunctionDefinition>();
			is_beta_reduction = true;
		}
		else
		{
			// Walk up the tree until we get to a node that is not a variable bound to a let node:
			ASTNode* cur = this->get_func_expr.getPointer();
			while((cur->nodeType() == ASTNode::VariableASTNodeType) && (((Variable*)cur)->binding_type == Variable::BindingType_Let))
				cur = ((Variable*)cur)->bound_let_node->expr.getPointer();

			if(cur->nodeType() == ASTNode::FunctionDefinitionType)
				target_func = (FunctionDefinition*)cur;
			else if(cur->nodeType() == ASTNode::VariableASTNodeType && ((Variable*)cur)->binding_type == Variable::BindingType_GlobalDef)
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

	const bool verbose = false;

	// If we are optimising for OpenCL, and the target function has the opencl_noinline attribute, don't inline.
	const bool opencl_allow_inline = (payload.linker == NULL) || !payload.linker->optimise_for_opencl || !target_func || (payload.linker->optimise_for_opencl && !target_func->opencl_noinline);
	if(verbose && !opencl_allow_inline)
		conPrint("Not inlining due to opencl_noinline attribute.");
	

	if(target_func && !target_func->noinline && opencl_allow_inline && !target_func->isExternalFunction() && target_func->body.nonNull()) // If is potentially inlinable:
	{
		if(verbose) conPrint("\n=================== Considering inlining function call =====================\n");
		if(verbose) conPrint("target func: " + target_func->sig.toString());

		const int call_count = payload.calls_to_func_count[target_func];

		if(verbose) conPrint("target complexity: " + toString(target_func->getSubtreeCodeComplexity()));
		const bool target_func_simple = target_func->getSubtreeCodeComplexity() < 20;

		// Work out if the argument expressions are 'expensive' to evaluate.
		// If they are, don't inline this function expression if the expensive argument expression is duplicated.
		// e.g. def f(float x) : x + x + x + x, 
		// main(float x) : f(sin(x))    would get inlined to      main(float x) : sin(x) + sin(x) + sin(x) + sin(x)
		//
		// NOTE: Instead of not inlining the function body directly, we could inline to a let expression, e.g. to
		//
		// let f_arg0 = sin(x) in f(f_arg0)

		bool expensive_arg_expr_duplicated = false;
		for(size_t i=0; i<this->argument_expressions.size(); ++i)
		{
			bool arg_expr_is_expensive = false;
			if(this->argument_expressions[i]->nodeType() == ASTNode::FunctionExpressionType)
			{
				// Consider function calls expensive, with some exceptions, such as:
				//	* Call to getfield built-in function (field access)
				bool func_call_is_expensive = true;
				const FunctionDefinition* arg_target_func = this->argument_expressions[i].downcastToPtr<FunctionExpression>()->static_target_function;
				if(arg_target_func->built_in_func_impl.nonNull())
					func_call_is_expensive = arg_target_func->built_in_func_impl->callIsExpensive();

				if(func_call_is_expensive)
					arg_expr_is_expensive = true;
			}
			else
			{
				// Some literal expressions are sufficiently simple that it doesn't matter if they are duplicated.
				if(!(this->argument_expressions[i]->nodeType() == ASTNode::VariableASTNodeType || this->argument_expressions[i]->nodeType() == ASTNode::IntLiteralType ||
					this->argument_expressions[i]->nodeType() == ASTNode::FloatLiteralType || this->argument_expressions[i]->nodeType() == ASTNode::DoubleLiteralType ||
					this->argument_expressions[i]->nodeType() == ASTNode::BoolLiteralType || this->argument_expressions[i]->nodeType() == ASTNode::CharLiteralType))
				{
					// This argument expression is expensive to evaluate.
					arg_expr_is_expensive = true;
				}
			}

			if(verbose) conPrint("arg " + toString(i) + " expensive: " + boolToString(arg_expr_is_expensive));
			if(arg_expr_is_expensive)
			{
				// See if the arg is duplicated
				if(target_func->args[i].ref_count > 1)
				{
					expensive_arg_expr_duplicated = true;
					if(verbose) conPrint("Expensive arg " + toString(i) + " is duplicated (refs=" + toString(target_func->args[i].ref_count) + ") in target function.  Not inlining.");
				}
			}
		}

		if(verbose) conPrint("target: " + target_func->sig.toString() + ", call_count=" + toString(call_count) + ", beta reduction=" + boolToString(is_beta_reduction) +
			", target_func_simple: " + boolToString(target_func_simple) + ", expensive_arg_expr_duplicated: " + boolToString(expensive_arg_expr_duplicated) + "\n");


		const bool should_inline = (is_beta_reduction || (call_count <= 1) || target_func_simple) && !expensive_arg_expr_duplicated;
		if(should_inline)
		{
			if(verbose) conPrint("------------original expr----------: ");
			if(verbose) this->print(0, std::cout);

			if(verbose) conPrint("------------original target function body-----------: ");
			if(verbose) target_func->body->print(0, std::cout);

			// Replace e with a copy of the target function body.
			ASTNodeRef cloned_body = cloneASTNodeSubtree(target_func->body);
			ASTNodeRef new_expr = cloned_body;

			payload.garbage.push_back(this); // Store a ref in payload so this node won't get deleted while we are still executing this function.
			assert(stack.back() == this);
			stack[stack.size() - 2]->updateChild(this, new_expr); // Tell the parent of this node to set the new expression as the relevant child.

			// Since we have replaced this func expression with new_expr, we need to update the stack as well, for the traversals below.
			stack.pop_back();
			stack.push_back(new_expr.ptr());

			if(verbose) conPrint("------------new expr: (cloned function body)-----------: ");
			if(verbose) new_expr->print(0, std::cout);


			// Now replace all variables in the target function body with the argument values from func_expr
			TraversalPayload sub_payload(TraversalPayload::SubstituteVariables);
			sub_payload.used_names = payload.used_names;
			sub_payload.func_args_to_sub = target_func;
			sub_payload.variable_substitutes.resize(this->argument_expressions.size());

			// Work out effective call site position.
			int effective_callsite_order_num = 1000000000;
			if(payload.current_named_constant)
				effective_callsite_order_num = payload.current_named_constant->order_num;
			for(size_t z=0; z<payload.func_def_stack.size(); ++z)
				effective_callsite_order_num = myMin(effective_callsite_order_num, payload.func_def_stack[z]->order_num);

			sub_payload.new_order_num = effective_callsite_order_num;

			for(size_t i=0; i<this->argument_expressions.size(); ++i)
			{
				sub_payload.variable_substitutes[i] = this->argument_expressions[i]; // NOTE: Don't clone now, will clone the expressions when they are pulled out of argument_expressions.

				//std::cout << "------------sub_payload.variable_substitutes[i]: " << std::endl;
				//sub_payload.variable_substitutes[i]->print(0, std::cout);
			}

			new_expr->traverse(sub_payload, stack);

			// new_expr itself may have been substituted by a new expression (see Variable::traverse SubstituteVariables case), in which case the back of the stack will be updated. 
			new_expr = stack.back();

			if(verbose) conPrint("------------Substituted expression-----------: ");
			if(verbose) new_expr->print(0, std::cout);


			// NEW: SubstituteVariables pass has set all variables in e to unbound.  Rebind all variables in e.
			{
				TraversalPayload temp_payload(TraversalPayload::UnbindVariables);
				temp_payload.linker = payload.linker;
				temp_payload.func_def_stack = payload.func_def_stack;
				new_expr->traverse(temp_payload, stack);
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
				new_expr->traverse(temp_payload, stack);
			}
			// Do a CountArgumentRefs pass as the ref counts may have changed.  This will count the number of references to each function argument in the body of each function.
			if(!payload.func_def_stack.empty())
			{
				// Run on this entire function, so we zero out the counts when traverse the FunctionDef.
				TraversalPayload temp_payload(TraversalPayload::CountArgumentRefs);
				temp_payload.linker = payload.linker;
				temp_payload.func_def_stack = payload.func_def_stack;
				payload.func_def_stack[0]->traverse(temp_payload, stack);
			}

			if(verbose) conPrint("------------Rebound expression-----------: ");
			if(verbose) new_expr->print(0, std::cout);

			payload.tree_changed = true;
		}
		else
		{
			if(verbose) conPrint("not inlining.\n");
		}
	}
}


void FunctionExpression::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	if(get_func_expr.ptr() == old_val)
	{
		get_func_expr = new_val;
		return;
	}

	for(size_t i=0; i<argument_expressions.size(); ++i)
		if(argument_expressions[i].ptr() == old_val)
		{
			argument_expressions[i] = new_val;
			return;
		}
	assert(0);
}


bool FunctionExpression::provenDefined() const
{
	//return proven_defined;
	if(this->static_target_function && this->static_target_function->sig.name == "elem")
		return proven_defined;

	// TODO: tricky issues here on how to prove valid for dynamically bound functions.

	return true;
}


void FunctionExpression::checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(this->static_target_function && this->static_target_function->sig.name == "elem" && this->argument_expressions.size() == 2)
	{
		if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::IntType)
		{
			// elem(array, index)
			const Reference<ArrayType> array_type = this->argument_expressions[0]->type().downcast<ArrayType>();

			// If the index is constant, into a fixed length array, we can prove whether the index is in-bounds
			if(this->argument_expressions[1]->isConstant())
			{
				// Evaluate the index expression
				VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(retval->valueType() == Value::ValueType_Int);

				const int64 index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < (int64)array_type->num_elems)
				{
					// Array index is in-bounds!
					return;
				}
				else
				{
					throw ExceptionWithPosition("Constant index with value " + toString(index_val) + " was out of bounds of array type " + array_type->toString(), errorContext(*this));
				}
			}
			else
			{
				// Else index is not known at compile time.

				//int i_lower = std::numeric_limits<int32>::min();
				//int i_upper = std::numeric_limits<int32>::max();
				//Vec2<int> i_bounds(std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max());

				const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range(stack, 
					this->argument_expressions[1], // integer value
					payload.linker ? payload.linker->value_allocator : nullptr
				);

				// Now check our bounds against the array
				if(i_bounds.lower() >= 0 && i_bounds.upper() < (int64)array_type->num_elems)
				{
					// Array index is proven to be in-bounds.
					return;
				}

#if 0
				for(int z=(int)stack.size()-1; z >= 0; --z)
				{
					ASTNode* stack_node = stack[z];

					// Get next node up the call stack
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
							// Ok, now we need to check the condition of the if expression.
							// A valid proof condition will be of form
							// inBounds(array, index)
							// Where array and index are the same as the ones for this elem() call.

							if(if_node->argument_expressions[0]->nodeType() == ASTNode::FunctionExpressionType)
							{
								FunctionExpression* condition_func_express = static_cast<FunctionExpression*>(if_node->argument_expressions[0].getPointer());
								if(condition_func_express->target_function->sig.name == "inBounds")
								{
									// Is the array the same? 
									if(expressionsHaveSameValue(condition_func_express->argument_expressions[0], this->argument_expressions[0]))
									{
										// Is the index the same?
										if(expressionsHaveSameValue(condition_func_express->argument_expressions[1], this->argument_expressions[1]))
										{
											// Success, inBounds uses the same variables, proving that the array access is in-bounds
											return;
										}
									}
								}
							}
							/*else if(if_node->argument_expressions[0]->nodeType() == ASTNode::BinaryBooleanType)
							{
								BinaryBooleanExpr* bin = static_cast<BinaryBooleanExpr*>(if_node->argument_expressions[0].getPointer());
								if(bin->t == BinaryBooleanExpr::AND)
								{
									// We know condition expression is of type A AND B

									// Process A
									if(bin->a->nodeType() == ASTNode::ComparisonExpressionType)
									{
										ComparisonExpression* a = static_cast<ComparisonExpression*>(bin->a.getPointer());
										updateIndexBounds(payload, *a, this->argument_expressions[1], i_lower, i_upper);
									}

									// Process B
									if(bin->b->nodeType() == ASTNode::ComparisonExpressionType)
									{
										ComparisonExpression* b = static_cast<ComparisonExpression*>(bin->b.getPointer());
										updateIndexBounds(payload, *b, this->argument_expressions[1], i_lower, i_upper);
									}
								}

								// Now check our bounds against the array
								if(i_lower >= 0 && i_upper < array_type->num_elems)
								{
									// Array index is proven to be in-bounds.
									return;
								}
							}*/
						}
					}
				}
#endif
			}
		}
		// else if gather form of elem:  elem(array<T, n>, vector<int, m>) -> vector<T, m>
		else if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::VectorTypeType) 
		{
			const Reference<ArrayType> array_type = this->argument_expressions[0]->type().downcast<ArrayType>();

			// If index vector is a constant vector literal
			if(this->argument_expressions[1]->isConstant() && this->argument_expressions[1]->nodeType() == ASTNode::VectorLiteralType)
			{
				const Reference<VectorLiteral> vec_literal = this->argument_expressions[1].downcast<VectorLiteral>();

				bool all_elements_valid = true;
				for(size_t i=0; i<vec_literal->getElements().size(); ++i)
				{
					bool elem_valid = false;
					if(vec_literal->getElements()[i]->nodeType() == ASTNode::IntLiteralType)
					{
						const Reference<IntLiteral> int_lit = vec_literal->getElements()[i].downcast<IntLiteral>();
						if(int_lit->value >= 0 && int_lit->value < (int64)array_type->num_elems) // if in-bounds
							elem_valid = true;
					}

					all_elements_valid = all_elements_valid && elem_valid;
				}

				if(all_elements_valid)
					return;
			}
		}
		else if(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::IntType)
		{
			// elem(vector, index)
			const Reference<VectorType> vector_type = this->argument_expressions[0]->type().downcast<VectorType>();

			// If the index is constant, into a fixed length array, we can prove whether the index is in-bounds
			if(this->argument_expressions[1]->isConstant())
			{
				// Evaluate the index expression
				VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(retval->valueType() == Value::ValueType_Int);

				const int64 index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < (int64)vector_type->num)
				{
					// Vector index is in-bounds!
					return;
				}
				else
				{
					throw ExceptionWithPosition("Constant index with value " + toString(index_val) + " was out of bounds of vector type " + vector_type->toString(), errorContext(*this));
				}
			}
			else
			{
				// Else index is not known at compile time.

				const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range(stack, 
					this->argument_expressions[1], // integer value
					payload.linker ? payload.linker->value_allocator : nullptr
				);

				// Now check our bounds against the array
				if(i_bounds.lower() >= 0 && i_bounds.upper() < (int)vector_type->num)
				{
					// Array index is proven to be in-bounds.
					return;
				}


				// Get next node up the call stack
				if(stack.back()->nodeType() == ASTNode::IfExpressionType)
				{
					// AST node above this one is an "if" expression
					IfExpression* if_node = static_cast<IfExpression*>(stack.back());

					// Is this node the 1st arg of the if expression?
					// e.g. if condition then this_node else other_node

					if(if_node->then_expr.getPointer() == this)
					{
						// Ok, now we need to check the condition of the if expression.
						// A valid proof condition will be of form
						// inBounds(array, index)
						// Where array and index are the same as the ones for this elem() call.

						if(if_node->condition->nodeType() == ASTNode::FunctionExpressionType)
						{
							FunctionExpression* condition_func_express = static_cast<FunctionExpression*>(if_node->condition.getPointer());
							if(condition_func_express->static_target_function->sig.name == "inBounds")
							{
								// Is the array the same? 
								if(expressionsHaveSameValue(condition_func_express->argument_expressions[0], this->argument_expressions[0]))
								{
									// Is the index the same?
									if(expressionsHaveSameValue(condition_func_express->argument_expressions[1], this->argument_expressions[1]))
									{
										// Success, inBounds uses the same variables, proving that the array access is in-bounds
										return;
									}
								}
							}
						}
						/*else if(if_node->argument_expressions[0]->nodeType() == ASTNode::BinaryBooleanType)
						{
							int i_lower = std::numeric_limits<int32>::min();
							int i_upper = std::numeric_limits<int32>::max();

							BinaryBooleanExpr* bin = static_cast<BinaryBooleanExpr*>(if_node->argument_expressions[0].getPointer());
							if(bin->t == BinaryBooleanExpr::AND)
							{
								// We know condition expression is of type A AND B

								// Process A
								if(bin->a->nodeType() == ASTNode::ComparisonExpressionType)
								{
									ComparisonExpression* a = static_cast<ComparisonExpression*>(bin->a.getPointer());
									updateIndexBounds(payload, *a, this->argument_expressions[1], i_lower, i_upper);
								}

								// Process B
								if(bin->b->nodeType() == ASTNode::ComparisonExpressionType)
								{
									ComparisonExpression* b = static_cast<ComparisonExpression*>(bin->b.getPointer());
									updateIndexBounds(payload, *b, this->argument_expressions[1], i_lower, i_upper);
								}
							}

							// Now check our bounds against the array
							if(i_lower >= 0 && i_upper < vector_type->num)
							{
								// Array index is proven to be in-bounds.
								return;
							}
						}*/
					}
				}
			}
		}
		if(this->argument_expressions[0]->type()->getType() == Type::TupleTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::IntType)
		{
			// Second arg must be constant, and is (or should be) checked during binding that it is in range.
			return;
		}
		if(this->argument_expressions[0]->type()->getType() == Type::VArrayTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::IntType)
		{
			// Is argument_expressions[0] a varray literal?  In that case we know its size.
			// Or is a let variable bound to a varry literal?

			const VArrayLiteral* varray_literal = NULL;
			if(argument_expressions[0]->nodeType() == ASTNode::VArrayLiteralType)
			{
				varray_literal = argument_expressions[0].downcastToPtr<VArrayLiteral>();
			}
			else if(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType)
			{
				const Variable* var =  argument_expressions[0].downcastToPtr<Variable>();
				if(var->binding_type == Variable::BindingType_Let)
				{
					ASTNode* target_let_node = var->bound_let_node->expr.getPointer();
					if(target_let_node->nodeType() == ASTNode::VArrayLiteralType)
					{
						varray_literal = static_cast<const VArrayLiteral*>(target_let_node);
					}
				}
				else if(var->binding_type == Variable::BindingType_NamedConstant)
				{
					ASTNode* target_named_constant_val = var->bound_named_constant->value_expr.getPointer();
					if(target_named_constant_val->nodeType() == ASTNode::VArrayLiteralType)
					{
						varray_literal = static_cast<const VArrayLiteral*>(target_named_constant_val);
					}
				}
			}


			if(varray_literal != NULL) // argument_expressions[0]->nodeType() == ASTNode::VArrayLiteralType)
			{
				//const VArrayLiteral* varray_literal = argument_expressions[0].downcastToPtr<VArrayLiteral>();

				// If the index is constant, into a fixed length array, we can prove whether the index is in-bounds
				if(this->argument_expressions[1]->isConstant())
				{
					// Evaluate the index expression
					VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
					vmstate.func_args_start.push_back(0);
					ValueRef retval = this->argument_expressions[1]->exec(vmstate);
					assert(retval->valueType() == Value::ValueType_Int);
					const int64 index_val = static_cast<IntValue*>(retval.getPointer())->value;
					if(index_val >= 0 && index_val < (int64)varray_literal->numElementsInValue())
						return; // Array index is in-bounds!
					else
						throw ExceptionWithPosition("Constant index with value " + toString(index_val) + " was out of bounds of varray", errorContext(*this));
				}
				else
				{
					// Else index is not known at compile time.
				
					const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range(stack, 
						this->argument_expressions[1], // integer value
						payload.linker ? payload.linker->value_allocator : nullptr
					);

					// Now check our bounds against the array
					if(i_bounds.lower() >= 0 && i_bounds.upper() < (int64)varray_literal->numElementsInValue())
						return; // Array index is proven to be in-bounds.
				}
			}
		}

		throw ExceptionWithPosition("Failed to prove elem() argument is in-bounds.", errorContext(*this));
	}
	// truncateToInt
	else if(this->static_target_function && this->static_target_function->sig.name == "truncateToInt" && this->argument_expressions.size() == 1)
	{
		//TEMP: allow truncateToInt to be unsafe to allow ISL_stdlib.txt to compile

	/*	// LLVM lang ref says 'If the value cannot fit in ty2, the results are undefined.'
		// So we need to make sure that the arg has value x such that x > INT_MIN - 1 && x < INT_MIN + 1

		const IntervalSetFloat bounds = ProofUtils::getFloatRange(payload, stack, 
			this->argument_expressions[0] // float value
		);

		// Now check our bounds.
		// TODO: get the exactly correct expression here
		if(bounds.lower() >= (float)std::numeric_limits<int>::min() && bounds.upper() <= (float)std::numeric_limits<int>::max())
		{
			// value is proven to be in-bounds.
			return;
		}

		throw BaseException("Failed to prove truncateToInt() argument is in-bounds." + errorContext(*this));*/
	}
	else if(this->static_target_function && this->static_target_function->sig.name == "update" && this->argument_expressions.size() == 3)
	{
		// def update(CollectionType c, int index, T newval) CollectionType

		// NOTE: a lot of this code copied from elem() above.  Combine?
		if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType &&
			this->argument_expressions[1]->type()->getType() == Type::IntType)
		{
			// update(array, index, newval)
			const Reference<ArrayType> array_type = this->argument_expressions[0]->type().downcast<ArrayType>();

			// If the index is constant, into a fixed length array, we can prove whether the index is in-bounds
			if(this->argument_expressions[1]->isConstant())
			{
				// Evaluate the index expression
				VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(retval->valueType() == Value::ValueType_Int);

				const int64 index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < (int64)array_type->num_elems)
				{
					// Array index is in-bounds!
					return;
				}
				else
				{
					throw ExceptionWithPosition("Constant index with value " + toString(index_val) + " was out of bounds of array type " + array_type->toString(), errorContext(*this));
				}
			}
			else
			{
				// Else index is not known at compile time.
				
				const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range(stack, 
					this->argument_expressions[1], // integer value
					payload.linker ? payload.linker->value_allocator : nullptr
				);

				// Now check our bounds against the array
				if(i_bounds.lower() >= 0 && i_bounds.upper() < (int64)array_type->num_elems)
				{
					// Array index is proven to be in-bounds.
					return;
				}
			}
		}

		throw ExceptionWithPosition("Failed to prove update() index argument is in-bounds.", errorContext(*this));
	}
	else if(this->static_target_function && this->static_target_function->sig.name == "toInt32" && this->argument_expressions.size() == 1)
	{
		// If the argument is constant:
		if(this->argument_expressions[0]->isConstant())
		{
			// Evaluate the index expression
			VMState vmstate(payload.linker ? payload.linker->value_allocator : nullptr);
			vmstate.func_args_start.push_back(0);
			ValueRef retval = this->argument_expressions[0]->exec(vmstate);
			assert(retval->valueType() == Value::ValueType_Int);
			const int64 val = static_cast<IntValue*>(retval.getPointer())->value;

			if(val >= -2147483648LL && val <= 2147483647LL)
				return; // argument is in-bounds!
			else
				throw ExceptionWithPosition("Value " + toString(val) + " was out of domain of toInt32().", errorContext(*this));
		}
		else
		{
			// Else index is not known at compile time.
			
			const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range(stack, 
				this->argument_expressions[0], // integer value
				payload.linker ? payload.linker->value_allocator : nullptr
			);

			// Now check our bounds against the array
			if(i_bounds.lower() >= -2147483648LL && i_bounds.upper() <= 2147483647LL)
				return; // Argument is proven to be in-bounds.
		}

		throw ExceptionWithPosition("Failed to prove toInt32() argument is in-bounds.", errorContext(*this));
	}
}


void FunctionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionExpr, ";
	if(!this->static_function_name.empty())
		s << "static_function_name: " << static_function_name;
	if(this->get_func_expr.nonNull())
	{
		s << "get_func_expr: \n";
		get_func_expr->print(depth + 1, s);
	}
	
	if(this->static_target_function)
		s << "; target: " << this->static_target_function->sig.toString() << "\n";
	/*else if(this->binding_type == Arg)
		s << "; runtime bound to arg index " << this->bound_index;
	else if(this->binding_type == Let)
		s << "; runtime bound to let index " << this->bound_index;*/
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->print(depth + 1, s);
}


std::string FunctionExpression::sourceString(int depth) const
{
	std::string s;
	if(!this->static_function_name.empty())
		s += this->static_function_name;
	else if(this->get_func_expr.nonNull())
	{
		if(this->get_func_expr->nodeType() == ASTNode::VariableASTNodeType)
			s += this->get_func_expr.downcastToPtr<Variable>()->name;
		else
			s += "(" + this->get_func_expr->sourceString(depth) + ")"; // Wrap parens around the 'get func' expression if needed. (needed currently for lambdas etc..)
	}
		
	s += "(";
	for(unsigned int i=0; i<argument_expressions.size(); ++i)
	{
		s += argument_expressions[i]->sourceString(depth);
		if(i + 1 < argument_expressions.size())
			s += ", ";
	}
	return s + ")";
}


// From https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/mathFunctions.html
static const char* opencl_built_in_func_names[] = { 
	"sign",
	"clamp",

	"acos", "acosh", "acospi", "asin",
	"asinh", "asinpi", "atan", "atan2",
	"atanh", "atanpi", "atan2pi", "cbrt",
	"ceil", "copysign", "cos", "cosh",
	"cospi", "erfc", "erf", "exp",
	"exp2", "exp10", "expm1", "fabs",
	"fdim", "floor", "fma", "fmax",
	"fmin", "fmod", 
	//"fract", OpenCL built-in fract() differs from Winter's.
	"frexp",
	"hypot", "ilogb", "ldexp", "lgamma",
	"lgamma_r", "log", "log2", "log10",
	"log1p", "logb", "mad", "modf",
	"nan", "nextafter", "pow", "pown",
	"powr", "remainder", "remquo", "rint",
	"rootn", "round", "rsqrt", "sin",
	"sincos", "sinh", "sinpi", "sqrt",
	"tan", "tanh", "tanpi", "tgamma",

	"cross", "dot", "distance", "length", "normalize", "fast_distance", "fast_length", "fast_normalize",

	NULL};


static bool isCallToBuiltInOpenCLFunction(const FunctionDefinition* func)
{
	// See if the arguments are all basic types.  If not, this isn't a call to a basic OpenCL function
	for(size_t i=0; i<func->sig.param_types.size(); ++i)
		if(!(func->sig.param_types[i]->getType() == Type::FloatType || func->sig.param_types[i]->getType() == Type::DoubleType || func->sig.param_types[i]->getType() == Type::IntType || func->sig.param_types[i]->getType() == Type::VectorTypeType))
			return false;

	for(size_t i=0; opencl_built_in_func_names[i] != NULL; ++i)
		if(func->sig.name == opencl_built_in_func_names[i])
			return true;
	return false;
}


// Is the function name something like 'eXX'
static bool isENFunctionName(const std::string& name)
{
	if(name.size() < 2)
		return false;
	if(name[0] != 'e')
		return false;
	for(size_t i=1; i<name.size(); ++i)
		if(!isNumeric(name[i]))
			return false;
	return true;
}


std::string FunctionExpression::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	if(this->static_function_name == "elem")
	{
		if(argument_expressions.size() != 2)
			throw ExceptionWithPosition("Error while emitting OpenCL C: elem() function with != 2 args.", errorContext(this));

		if(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType)
		{
			/*
			elem(v, 0)		=>		v.s0
			elem(v, 1)		=>		v.s1

			elem(v, 9)		=>		v.s9
			elem(v, 10)		=>		v.sA
			elem(v, 11)		=>		v.sB

			elem(v, 15)		=>		v.sF
			*/

			if(argument_expressions[1]->nodeType() != ASTNode::IntLiteralType)
				throw ExceptionWithPosition("Error while emitting OpenCL C: elem() function with 2nd arg that is not an Int literal.", errorContext(this));

			const int64 index = static_cast<const IntLiteral*>(argument_expressions[1].getPointer())->value;

			if(index < 0 || index >= 16)
				throw ExceptionWithPosition("Error while emitting OpenCL C: elem() function has invalid index: " + toString(index), errorContext(this));

			return argument_expressions[0]->emitOpenCLC(params) + ".s" + ::intToHexChar((int)index);
		}
		else if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType)
		{
			/*
			elem(a, i)		=>		a[i]
			*/
			//if(argument_expressions[1]->nodeType() != ASTNode::IntLiteralType)
			//	throw BaseException("Error while emitting OpenCL C: elem() function with 2nd arg that is not an Int literal.");

			//const int64 index = static_cast<const IntLiteral*>(argument_expressions[1].getPointer())->value;

			//return argument_expressions[0]->emitOpenCLC(params) + "[" + ::intToHexChar((int)index) + "]";
			
			const std::string index_expr = argument_expressions[1]->emitOpenCLC(params);

			if(params.emit_in_bound_asserts)
			{
				std::string assert_s = "winterAssert((" + index_expr + ") >= 0 && (" + index_expr + ") < " + toString(this->argument_expressions[0]->type().downcastToPtr<ArrayType>()->num_elems) + ");\n";
				params.blocks.back() += assert_s;
			}

			return argument_expressions[0]->emitOpenCLC(params) + "[" + index_expr + "]";
		}
		else if(this->argument_expressions[0]->type()->getType() == Type::TupleTypeType)
		{
			/*
			elem(a, i)		=>		a.field_i
			*/

			if(argument_expressions[1]->nodeType() != ASTNode::IntLiteralType)
				throw ExceptionWithPosition("Error while emitting OpenCL C: elem(tuple, i) function with 2nd arg that is not an Int literal.", errorContext(this));

			const int64 index = static_cast<const IntLiteral*>(argument_expressions[1].getPointer())->value;

			if(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType && argument_expressions[0].downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument)
				return argument_expressions[0]->emitOpenCLC(params) + "->field_" + ::toString(index);
			else
				return argument_expressions[0]->emitOpenCLC(params) + ".field_" + ::toString(index);
		}
		else
			throw ExceptionWithPosition("Error while emitting OpenCL C: elem() function first arg has unsupported type " + argument_expressions[0]->type()->toString(), errorContext(this));
	}
	else if(this->static_function_name == "shuffle")
	{
		try
		{
			if(argument_expressions.size() != 2)
				throw ExceptionWithPosition("Error while emitting OpenCL C: shuffle() function with != 2 args.", errorContext(this));

			if(argument_expressions[1]->nodeType() != ASTNode::VectorLiteralType)
				throw ExceptionWithPosition("Error while emitting OpenCL C: shuffle() function with 2nd arg that is not a Vector literal.", errorContext(this));

			const VectorLiteral* vec_literal = static_cast<const VectorLiteral*>(argument_expressions[1].getPointer());

			std::string s = argument_expressions[0]->emitOpenCLC(params) + ".s";

			for(size_t i=0; i<vec_literal->getElements().size(); ++i)
			{
				if(vec_literal->getElements()[i]->nodeType() != ASTNode::IntLiteralType)
					throw ExceptionWithPosition("Error while emitting OpenCL C: shuffle() function with 2nd arg that does not have an int literal in the vector literal.", errorContext(this));

				const int64 index = static_cast<const IntLiteral*>(vec_literal->getElements()[i].getPointer())->value;

				s.push_back(::intToHexChar((int)index));
			}

			return s;
		}
		catch(StringUtilsExcep& e)
		{
			throw ExceptionWithPosition("Error while emitting shuffle function: " + e.what(), errorContext(this));
		}
	}
	else if(isENFunctionName(static_function_name) && 
		(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType ||
		this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType))
	{
		try
		{
			// eN() function
			const int index = stringToInt(static_function_name.substr(1, static_function_name.size() - 1));

			if(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType)
			{
				/*
				e0(v)		=>		v.s0
				e1(v)		=>		v.s1

				e9(v)		=>		v.s9
				e10(v)		=>		v.sA
				e11(v)		=>		v.sB

				e12(v)		=>		v.sF
				*/

			
				return argument_expressions[0]->emitOpenCLC(params) + ".s" + std::string(1, ::intToHexChar((int)index));
				
			}
			else if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType)
			{
				/*
				eN(a)		=>		a[N]
				*/
				return argument_expressions[0]->emitOpenCLC(params) + "[" + ::intToHexChar((int)index) + "]";
			}
			else
				throw ExceptionWithPosition("Error while emitting OpenCL C: eN() function first arg not supported for type " + argument_expressions[0]->type()->toString(), errorContext(this));
		}
		catch(StringUtilsExcep&)
		{
			throw ExceptionWithPosition("Error while emitting OpenCL C: invalid eN() function '" + static_function_name + "'.", errorContext(this));
		}
	}
	else if(static_target_function && static_target_function->built_in_func_impl.nonNull() && 
		(static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_GetField))
	{
		// Transform get field built-in functions like so:
		// struct s { int x; }
		// 
		// x(s)			=>		s->x

		if(argument_expressions.size() != 1)
			throw ExceptionWithPosition("Error while emitting OpenCL C: get field function with != 1 args.", errorContext(this));

		//if(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType && argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::BindingType_Let)

		// If arg 0 is a variable that is bound to an argument, and is not free:
		if(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType && (argument_expressions[0].downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument && argument_expressions[0].downcastToPtr<Variable>()->enclosing_lambdas.empty()))
			return argument_expressions[0]->emitOpenCLC(params) + "->" + static_function_name;
		else
			return argument_expressions[0]->emitOpenCLC(params) + "." + static_function_name;
	}
	else if(static_function_name == "inBounds")
	{
		// inBounds(a, i)			=>		i >= 0 && i < N

		if(argument_expressions.size() != 2)
			throw ExceptionWithPosition("Error while emitting OpenCL C: inBounds function with != 2 args.", errorContext(this));

		size_t N;
		if(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType)
		{
			N = static_cast<VectorType*>(this->argument_expressions[0]->type().getPointer())->num;
		}
		else if(this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType)
		{
			N = static_cast<ArrayType*>(this->argument_expressions[0]->type().getPointer())->num_elems;
		}
		else
			throw ExceptionWithPosition("Error while emitting OpenCL C: inBounds arg 1 type must be vector array.", errorContext(this));

		return "((" + argument_expressions[1]->emitOpenCLC(params) + " >= 0) && (" + argument_expressions[1]->emitOpenCLC(params) + " < " + toString(N) + "))";
	}
	else if(static_function_name == "abs" && (argument_expressions.size() == 1) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		argument_expressions[0]->type()->getType() == Type::DoubleType ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType)))
	{
		return "fabs(" + argument_expressions[0]->emitOpenCLC(params) + ")";
	}
	else if(static_function_name == "min" && (argument_expressions.size() == 2) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		argument_expressions[0]->type()->getType() == Type::DoubleType ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType)))
	{
		return "fmin(" + argument_expressions[0]->emitOpenCLC(params) + ", " + argument_expressions[1]->emitOpenCLC(params) + ")";
	}
	else if(static_function_name == "max" && (argument_expressions.size() == 2) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		argument_expressions[0]->type()->getType() == Type::DoubleType ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType)))
	{
		return "fmax(" + argument_expressions[0]->emitOpenCLC(params) + ", " + argument_expressions[1]->emitOpenCLC(params) + ")";
	}
	else if(static_function_name == "cross" && (argument_expressions.size() == 2) &&
		((argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType)))
	{
		return "cross(" + argument_expressions[0]->emitOpenCLC(params) + ", " + argument_expressions[1]->emitOpenCLC(params) + ")";
	}
	else if(static_function_name == "length" && (argument_expressions.size() == 1) &&
		((argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType) ||
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::DoubleType)))
		{
		return "length(" + argument_expressions[0]->emitOpenCLC(params) + ")";
	}
	else if(static_function_name == "iterate")
	{
		//TODO: check arg 0 is constant.
		if(this->argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType && this->argument_expressions[0].downcastToPtr<Variable>()->binding_type == Variable::BindingType_GlobalDef)
		{
			return this->static_target_function->built_in_func_impl.downcastToPtr<IterateBuiltInFunc>()->emitOpenCLForFunctionArg(
				params,
				this->argument_expressions[0].downcastToPtr<Variable>()->bound_function,
				this->argument_expressions
			);
		}
		else if(this->argument_expressions[0]->nodeType() == ASTNode::FunctionDefinitionType)
		{
			return this->static_target_function->built_in_func_impl.downcastToPtr<IterateBuiltInFunc>()->emitOpenCLForFunctionArg(
				params,
				this->argument_expressions[0].downcastToPtr<FunctionDefinition>(),
				this->argument_expressions
			);
		}
		else
			throw ExceptionWithPosition("Error while emitting OpenCL C: First arg to iterate must be a constant reference to a globally defined function.", errorContext(this));


		

		
		/*if(argument_expressions.size() != 1)
			throw BaseException("Error while emitting OpenCL C: abs function with != 1 arg.");

		if(argument_expressions[0]->type()->getType() == Type::FloatType)
			return "fabs(" + argument_expressions[0]->emitOpenCLC(params) + ")";
		else
			return "abs(" + argument_expressions[0]->emitOpenCLC(params) + ")";*/
	}
	/*else if(static_target_function && static_target_function->built_in_func_impl.nonNull() &&
		(static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_Constructor))
	{
		// Struct constructor: for this we will emit a 'compound literal' expression such as
		// b = (struct point) { 5, 6 };
		// (See http://nickdesaulniers.github.io/blog/2013/07/25/designated-initialization-with-pointers-in-c/)

		// NOTE: Unforunately this code crashes the AMD OpenCL C compiler, see
		// https://community.amd.com/message/2867567
		// So we can't use this approach for now.

		std::string s = "(" + this->type()->OpenCLCType() + ") {";
		for(unsigned int i=0; i<argument_expressions.size(); ++i)
		{
			if((argument_expressions[i]->nodeType() == ASTNode::VariableASTNodeType) && (argument_expressions[i].downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument) && argument_expressions[i]->type()->OpenCLPassByPointer())
				s += "*"; // Will need to deref pointer args.
			s += argument_expressions[i]->emitOpenCLC(params);

			if(i + 1 < argument_expressions.size())
				s += ", ";
		}
		s += "}";
		return s;
	}*/
	else
	{
		//std::string use_func_name;
		//if(function_name == "abs" && (argument_expressions.size() >= 1) && (argument_expressions[0]->type()->getType() == Type::FloatType))
		//	use_func_name = "fabs";
		//else
		//	use_func_name = function_name;

		//// If this is a call to a constructor built-in function, add the _cnstr suffix we will use in OpenCL code.
		//if(target_function && target_function->built_in_func_impl.nonNull() && dynamic_cast<Constructor*>(target_function->built_in_func_impl.getPointer()))
		//	use_func_name += "_cnstr";




		/*
			f(s1, s2, x)

			=>
			S arg_1 = ...;
			S arg_2 = ...;
			f(&arg1, &arg2, x)
		*/

		if(this->static_target_function == NULL)
			throw BaseException("static_target_function was NULL in FunctionExpression::emitOpenCLC(), static_function_name: '" + static_function_name + "'.");

		std::string arg_eval_s = "";

		std::string use_func_name = (/*this->target_function->isExternalFunction() ||*/ isCallToBuiltInOpenCLFunction(this->static_target_function)) ? 
			this->static_target_function->sig.name : this->static_target_function->sig.typeMangledName();

		std::string s = use_func_name + "(";
		for(unsigned int i=0; i<argument_expressions.size(); ++i)
		{
			s += emitCodeForFuncArg(params, argument_expressions[i], this->static_target_function, /*arg name=*/this->static_target_function->args[i].name);

			if(i + 1 < argument_expressions.size())
				s += ", ";
		}

		if(!arg_eval_s.empty())
		{
			if(params.emit_comments)
				params.blocks.back() += "// args for " + this->static_target_function->sig.toString() + ":\n";
			params.blocks.back() += arg_eval_s;
		}

		return s + ")";
	}
}


// Emit code for the function argument in a function call expression.
// May be as simple as "x", or a temporary may need to be allocated on the stack, so its address can be taken, for example
// SomeStruct f_arg_0 = someExpr();
// f_arg_0
std::string FunctionExpression::emitCodeForFuncArg(EmitOpenCLCodeParams& params, const ASTNodeRef& arg_expression, const FunctionDefinition* target_func, const std::string& func_arg_name)
{
	if(!arg_expression->type()->OpenCLPassByPointer())
		return arg_expression->emitOpenCLC(params);
	else
	{
		if(arg_expression->nodeType() == ASTNode::VariableASTNodeType && arg_expression.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument)
		{
			// If the argument expression is itself an argument to the current function, then it is already a pointer, so just use as-is.
			return arg_expression->emitOpenCLC(params);
		}
		else if(arg_expression->nodeType() == ASTNode::VariableASTNodeType && arg_expression.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Let)
		{
			// If the variable is bound to a let variable, then it is on the stack of the C function.  So it is not a pointer.
			return "&" + arg_expression->emitOpenCLC(params);
		}
		else
		{
			// Emit something like
			// "SomeStruct f_arg_xx = g();"

			const std::string arg_name = func_arg_name + "_arg_" + toString(params.uid++);
			const std::string arg_eval_s = arg_expression->type()->OpenCLCType(params) + " " + arg_name + " = " + arg_expression->emitOpenCLC(params) + ";\n";

			if(params.emit_comments)
				params.blocks.back() += "// arg for " + target_func->sig.toString() + ":\n";
			params.blocks.back() += arg_eval_s;

			return "&" + arg_name;
		}
	}
}


TypeRef FunctionExpression::type() const
{
	if(this->static_target_function)
	{
		return this->static_target_function->returnType();
	}
	else if(this->get_func_expr.nonNull())
	{
		TypeRef get_func_expr_type = this->get_func_expr->type();
		if(get_func_expr_type.isNull())
			return NULL;

		if(get_func_expr_type->getType() == Type::FunctionType)
		{
			return get_func_expr_type.downcastToPtr<Function>()->return_type;
		}
		else
			throw ExceptionWithPosition("expression does not have function type.", errorContext(*this));
	}
	else
	{
		//assert(0);
		return TypeRef(NULL);
	}
}


llvm::Value* FunctionExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	params.stats->initial_num_llvm_function_calls++;

	llvm::Value* target_llvm_func = NULL; // Pointer to target function to call.
	TypeRef target_ret_type = this->type();

	llvm::Value* closure_pointer = NULL;
	llvm::Value* captured_var_struct_ptr = NULL;

	if(!this->static_target_function)
	{
		std::vector<TypeVRef> arg_types;
		arg_types.reserve(argument_expressions.size());
		for(size_t i=0; i<argument_expressions.size(); ++i)
			arg_types.push_back(TypeVRef(argument_expressions[i]->type()));

		llvm::Type* target_func_type = LLVMTypeUtils::llvmFunctionType(
			arg_types,
			true, // use captured var struct ptr arg
			TypeVRef(target_ret_type),
			*params.module
		);
		llvm::Type* target_func_ptr_type = LLVMTypeUtils::pointerType(target_func_type);

		// Emit code to get the function closure.
		closure_pointer = this->get_func_expr->emitLLVMCode(params, NULL);

		llvm::Type* closure_type = this->get_func_expr->type().downcastToPtr<Function>()->closureLLVMStructType(*params.module);

		// Get the actual function pointer from the closure
		llvm::Value* target_llvm_func_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, Function::functionPtrIndex(), closure_type, "function_ptr_ptr");
		

		target_llvm_func = LLVMUtils::createLoad(params.builder, target_llvm_func_ptr, target_func_ptr_type, "function_ptr");

		// Get the captured var struct from the closure
		captured_var_struct_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, Function::capturedVarStructIndex(), closure_type, "captured_var_struct_ptr"); // field index
	}
	else // else if this->static_target_function:
	{
		// For structure field access functions, instead of emitting an actual function call, just emit the LLVM code to access the field.
		if(static_target_function->built_in_func_impl.nonNull() && (static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_GetField))
		{
			const GetField* get_field_func = static_cast<const GetField*>(this->static_target_function->built_in_func_impl.getPointer());

			const unsigned int field_index = get_field_func->index;

			const TypeRef field_type = get_field_func->struct_type->component_types[field_index];
			const std::string field_name = get_field_func->struct_type->component_names[field_index];

			assert(argument_expressions.size() == 1);
			llvm::Value* struct_ptr = argument_expressions[0]->emitLLVMCode(params, NULL);

			llvm::Value* result;
			if(field_type->passByValue())
			{
				llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, field_index, get_field_func->struct_type->LLVMType(*params.module), get_field_func->struct_type->name + "." + field_name + " ptr");
				llvm::Value* loaded_val = LLVMUtils::createLoad(params.builder, field_ptr, field_type->LLVMType(*params.module), field_name);

				// TEMP NEW: increment ref count if this is a string
				//if(field_type->getType() == Type::StringType)
				//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

				result = loaded_val;
			}
			else
			{
				result = LLVMUtils::createStructGEP(params.builder, struct_ptr, field_index, get_field_func->struct_type->LLVMType(*params.module), get_field_func->struct_type->name + "." + field_name + " ptr");
			}

			field_type->emitIncrRefCount(params, result, "GetField " + get_field_func->struct_type->name + "." + field_name + " result increment");

			// Decrement argument 0 structure ref count
			//if(!(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType)) // && argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::BindingType_Let)) // Don't decr let var ref counts, the ref block will do that.
			if(shouldRefCount(params, *argument_expressions[0]))
				emitDestructorOrDecrCall(params, *argument_expressions[0], struct_ptr, "GetField " + get_field_func->struct_type->name + "." + field_name + " struct arg decrement/destructor");

			return result;
		}
		// For tuple field access functions, instead of emitting an actual function call, just emit the LLVM code to access the field.
		else if(static_target_function->built_in_func_impl.nonNull() && (static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_GetTupleElementBuiltInFunc))
		{
			const GetTupleElementBuiltInFunc* get_field_func = static_cast<const GetTupleElementBuiltInFunc*>(this->static_target_function->built_in_func_impl.getPointer());

			const unsigned int field_index = get_field_func->index;

			const TypeRef field_type = get_field_func->tuple_type->component_types[field_index];
			const std::string field_name = "field " + toString(field_index);

			assert(argument_expressions.size() == 2);
			llvm::Value* struct_ptr = argument_expressions[0]->emitLLVMCode(params, NULL);

			llvm::Value* result;
			if(field_type->passByValue())
			{
				llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, struct_ptr, field_index, get_field_func->tuple_type->LLVMStructType(*params.module), field_name + " ptr");
				llvm::Value* loaded_val = LLVMUtils::createLoad(params.builder, field_ptr, field_type->LLVMType(*params.module), field_name);

				// TEMP NEW: increment ref count if this is a string
				//if(field_type->getType() == Type::StringType)
				//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

				result = loaded_val;
			}
			else
			{
				result = LLVMUtils::createStructGEP(params.builder, struct_ptr, field_index, get_field_func->tuple_type->LLVMStructType(*params.module), field_name + " ptr");
			}

			field_type->emitIncrRefCount(params, result, "GetTupleElement " + get_field_func->tuple_type->toString() + " " + field_name + " result increment");

			// Decrement argument 0 structure ref count
			//if(!(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType)) // && argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::BindingType_Let)) // Don't decr let var ref counts, the ref block will do that.
			if(shouldRefCount(params, *argument_expressions[0]))
				emitDestructorOrDecrCall(params, *argument_expressions[0], struct_ptr, "GetTupleElement " + get_field_func->tuple_type->toString() + "." + field_name + " tuple arg decrement/destructor");

			return result;
		}
			

		target_llvm_func = this->static_target_function->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr: False as we are making a staticly bound call.
		);
	}

	assert(target_llvm_func);

	//TEMP:
	//std::cout << "FunctionExpression, target name: " << this->function_name << ", target_llvm_func: \n";
	//target_llvm_func->dump();
	//std::cout << std::endl;

	

	vector<llvm::Value*> args;
	llvm::Value* return_val_addr = NULL;

	if(!target_ret_type->passByValue())
	{
		if(ret_space_ptr)
			return_val_addr = ret_space_ptr;
		else
		{
			// Allocate return value on stack
			// Emit the alloca in the entry block for better code-gen.
			llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().begin());

			return_val_addr = entry_block_builder.CreateAlloca(
				target_ret_type->LLVMType(*params.module), // type
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
				"res"
			);
		}

		args.push_back(return_val_addr); // First argument is return value pointer.
	}

	//----------------- Emit code for argument expressions ------------------
	vector<bool> do_ref_counting_for_arg(argument_expressions.size(), true);

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
	{
		if(argument_expressions[i]->nodeType() == ASTNode::VariableASTNodeType)
		{
			const Variable* var_node = argument_expressions[i].downcastToPtr<Variable>();
			if(var_node->binding_type == Variable::BindingType_Let || var_node->binding_type == Variable::BindingType_Argument)
			{
				const bool old_emit_refcounting_code = params.emit_refcounting_code;
				params.emit_refcounting_code = false; // Disable ref counting code

				args.push_back(argument_expressions[i]->emitLLVMCode(params, NULL));

				params.emit_refcounting_code = old_emit_refcounting_code; // restore emit_refcounting_code flag.

				do_ref_counting_for_arg[i] = false;
			}
			else
				args.push_back(argument_expressions[i]->emitLLVMCode(params, NULL));
		}
		else
			args.push_back(argument_expressions[i]->emitLLVMCode(params, NULL));
	}

	// Append pointer to Captured var struct, if this function was from a closure, and there are captured vars.
	if(captured_var_struct_ptr != NULL)
		args.push_back(captured_var_struct_ptr);

#if TARGET_LLVM_VERSION >= 110

	std::vector<TypeVRef> arg_types;
	arg_types.reserve(argument_expressions.size());
	for(size_t i=0; i<argument_expressions.size(); ++i)
		arg_types.push_back(TypeVRef(argument_expressions[i]->type()));

	llvm::Type* target_func_type = LLVMTypeUtils::llvmFunctionType(
		arg_types,
		captured_var_struct_ptr != NULL, // use captured var struct ptr arg
		TypeVRef(target_ret_type),
		*params.module
	);

	//llvm::Type* ptr_function_type = target_llvm_func->getType();
	//llvm::Type* function_type = target_func_type; // ptr_function_type->getPointerElementType();
	assert(llvm::isa<llvm::FunctionType>(target_func_type));
	llvm::FunctionCallee callee(llvm::cast<llvm::FunctionType>(target_func_type), target_llvm_func);
	llvm::CallInst* call_inst = params.builder->CreateCall(callee, args);
#else
	llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args);
#endif

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	call_inst->setCallingConv(llvm::CallingConv::C);

	// Decrement ref counts on arguments
	const int num_sret_args = target_ret_type->passByValue() ? 0 : 1;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		if(shouldRefCount(params, argument_expressions[i]) && do_ref_counting_for_arg[i])
			emitDestructorOrDecrCall(params, *argument_expressions[i], args[i + num_sret_args], "function expression '" + (this->static_target_function ? this->static_target_function->sig.toString() : "[runtime]") + "' argument " + toString(i) + " decrement");


	if(closure_pointer)
	{
		// Decrement ref count on closure that we evaluated.
		// TODO: call shouldRefCount?
		const std::string this_func_name = this->functionName();
		emitDestructorOrDecrCall(params, *this->get_func_expr, closure_pointer, "function expression " + this_func_name + " get_func_expr result decrement");
	}

	return target_ret_type->passByValue() ? call_inst : return_val_addr;
}


//void FunctionExpression::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
//{
////	RefCounting::emitCleanupLLVMCode(params, this->type(), val);
//}


Reference<ASTNode> FunctionExpression::clone(CloneMapType& clone_map)
{
	FunctionExpression* e = new FunctionExpression(srcLocation());
	
	if(get_func_expr.nonNull())
		e->get_func_expr = this->get_func_expr->clone(clone_map);

	for(size_t i=0; i<argument_expressions.size(); ++i)
		e->argument_expressions.push_back(argument_expressions[i]->clone(clone_map));
	
	e->static_function_name = this->static_function_name;
	e->static_target_function = this->static_target_function;

	clone_map.insert(std::make_pair(this, e));
	return e;
}


const std::string FunctionExpression::functionName() const
{
	if(get_func_expr.nonNull() && (get_func_expr->nodeType() == ASTNode::VariableASTNodeType))
		return get_func_expr.downcastToPtr<Variable>()->name;

	return static_function_name;
}


bool FunctionExpression::isBoundToGlobalDef() const // Is this function bound to a single global definition.  This should be the case for most normal function expressions like f(x)
{
	//return func->nodeType() == ASTNode::VariableASTNodeType && 
	//	func.downcastToPtr<Variable>()->vartype == Variable::BoundToGlobalDefVariable;
	return static_target_function != NULL;
}


bool FunctionExpression::isConstant() const
{
	// For now, we'll say a function expression bound to an argument or let var is not constant.
	if(!static_target_function)
		return false;

	for(size_t i=0; i<argument_expressions.size(); ++i)
		if(!argument_expressions[i]->isConstant())
			return false;

	return true;
}


size_t FunctionExpression::getTimeBound(GetTimeBoundParams& params) const
{
	params.steps++;
	if(params.steps > params.max_bound_computation_steps)
		throw ExceptionWithPosition("Too many steps when computing time bound.", errorContext(this));

	size_t arg_eval_bound = 0;
	for(size_t i=0; i<argument_expressions.size(); ++i)
		arg_eval_bound += argument_expressions[i]->getTimeBound(params);

	if(static_target_function)
	{
		if(static_target_function->built_in_func_impl.nonNull() &&
			static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_MakeVArrayBuiltInFunc)
		{
			if(this->argument_expressions[1]->isConstant())
			{
				// Evaluate the index expression
				VMState vmstate(nullptr);
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				const int64 arg_1_val = checkedCast<IntValue>(retval)->value;

				return arg_1_val;
			}
			else
			{
				// Else arg 1 (count) is not known at compile time.

				//const IntervalSetInt64 i_bounds = ProofUtils::getInt64Range((std::vector<ASTNode*>&)params.stack,
				//	this->argument_expressions[1] // integer value
				//);
				//
				//return i_bounds.upper();
			}
		}

		try
		{
			return arg_eval_bound + static_target_function->getTimeBound(params);
		}
		catch(BaseException& e)
		{
			throw ExceptionWithPosition(e.what(), errorContext(this->srcLocation()));
		}
	}
	else
	{
		// TODO: Compute a maximum over all functions that this expression may be calling.
		//TEMP:
		throw ExceptionWithPosition("Unable to bound time of function expression.", errorContext(this->srcLocation()));
	}
}


GetSpaceBoundResults FunctionExpression::getSpaceBound(GetSpaceBoundParams& params) const
{
	params.steps++;
	if(params.steps > params.max_bound_computation_steps)
		throw ExceptionWithPosition("Too many steps when computing space bound.", errorContext(this));

	GetSpaceBoundResults arg_eval_bound(0, 0);
	for(size_t i=0; i<argument_expressions.size(); ++i)
		arg_eval_bound += argument_expressions[i]->getSpaceBound(params);

	if(static_target_function)
	{
		// Handle special case of makeVArray built-in function, when the second arg (num elems) is constant.
		if(static_target_function->built_in_func_impl.nonNull() &&
			static_target_function->built_in_func_impl->builtInType() == BuiltInFunctionImpl::BuiltInType_MakeVArrayBuiltInFunc)
		{
			if(this->argument_expressions[1]->isConstant())
			{
				// Evaluate the num-values expression
				VMState vmstate(nullptr);
				vmstate.func_args_start.push_back(0);
				ValueRef retval = this->argument_expressions[1]->exec(vmstate);
				const int64 num_elems = checkedCast<IntValue>(retval)->value;
				
				VArrayType* varray_type = static_target_function->returnType().downcastToPtr<VArrayType>();
				const size_t single_elem_heap_size = varray_type->elem_type->isHeapAllocated() ? sizeof(void*) : varray_type->elem_type->memSize();
				const size_t header_and_data_size = sizeof(VArrayRep) + single_elem_heap_size * num_elems;

				// We have to take into account the stack space that the C++ function allocateVArray(), which will be called, will take.
				return arg_eval_bound +  GetSpaceBoundResults(1024, /*heap space=*/header_and_data_size);
			}
		}

		// NOTE: this logic is incorrect for variable ASTNode args.
		// if(static_target_function->sig.name == "concatStrings" && static_target_function->sig.param_types.size() == 2 &&
		// 	static_target_function->sig.param_types[0]->getType() == Type::StringType && 
		// 	static_target_function->sig.param_types[1]->getType() == Type::StringType)
		// {
		// 	return arg_eval_bound + arg_eval_bound; // The resulting in string size should be <= the sum of the arg sizes.
		// }

		try
		{
			return arg_eval_bound + static_target_function->getSpaceBound(params);
		}
		catch(BaseException& e)
		{
			throw ExceptionWithPosition(e.what(), errorContext(this->srcLocation()));
		}
	}
	else
	{
		// TODO: Compute a maximum over all functions that this expression may be calling.
		// TEMP:
		throw ExceptionWithPosition("Unable to bound space of function expression.", errorContext(this->srcLocation()));
	}
}


size_t FunctionExpression::getSubtreeCodeComplexity() const
{
	size_t sum = 0;
	for(size_t i=0; i<argument_expressions.size(); ++i)
		sum += argument_expressions[i]->getSubtreeCodeComplexity();
	return 1 + sum;
}


} // end namespace Winter
