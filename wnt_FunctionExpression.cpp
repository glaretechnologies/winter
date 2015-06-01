/*=====================================================================
FunctionExpression.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-30 18:53:38 +0100
=====================================================================*/
#include "wnt_FunctionExpression.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_VectorLiteral.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_Variable.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "wnt_IfExpression.h"
#include "utils/StringUtils.h"
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
	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0),
	proven_defined(false)
{
}


FunctionExpression::FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0) // 1-arg function
:	ASTNode(FunctionExpressionType, src_loc),
	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0),
	proven_defined(false)
{
	function_name = func_name;
	argument_expressions.push_back(arg0);
}


FunctionExpression::FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1) // 2-arg function
:	ASTNode(FunctionExpressionType, src_loc),
	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0),
	proven_defined(false)
{
	function_name = func_name;
	argument_expressions.push_back(arg0);
	argument_expressions.push_back(arg1);
}


FunctionDefinition* FunctionExpression::runtimeBind(VMState& vmstate, const FunctionValue*& function_value_out)
{
	if(use_captured_var)
	{
		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		ValueRef captured_struct = vmstate.argument_stack.back();
		const StructureValue* s = checkedCast<StructureValue>(captured_struct);

		ValueRef func_val = s->fields[this->captured_var_index];

		const FunctionValue* function_val = checkedCast<FunctionValue>(func_val);

		function_value_out = function_val;

		return function_val->func_def;
	}


	if(target_function)
	{
		function_value_out = NULL;
		return target_function;
	}
	else if(this->binding_type == Arg)
	{
		ValueRef arg = vmstate.argument_stack[vmstate.func_args_start.back() + this->bound_index];
		const FunctionValue* function_value = checkedCast<FunctionValue>(arg);
		function_value_out = function_value;
		return function_value->func_def;
	}
	else
	{
		assert(this->binding_type == Let);

		//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];
		//ValueRef arg = vmstate.let_stack[let_stack_start + this->bound_index];

		ValueRef arg = this->bound_let_block->lets[this->bound_index]->exec(vmstate);

		//ValueRef arg = vmstate.let_stack[vmstate.let_stack_start.back() + this->bound_index];
		const FunctionValue* function_value = checkedCast<FunctionValue>(arg);
		function_value_out = function_value;
		return function_value->func_def;
	}
}


ValueRef FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) std::cout << indent(vmstate) << "FunctionExpression, target_name=" << this->function_name << "\n";

	if(vmstate.func_args_start.size() > 1000)
		throw BaseException("Function call level too deep, aborting.");


	if(this->binding_type == Unbound)
		throw BaseException("Function is not bound.");
	

	//assert(target_function);
	if(this->target_function != NULL && this->target_function->external_function.nonNull())
	{
		vector<ValueRef> args;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			args.push_back(this->argument_expressions[i]->exec(vmstate));

		ValueRef result = this->target_function->external_function->interpreted_func(args);

		return result;
	}

	// Get target function.  The target function is resolved at runtime, because it may be a function 
	// passed in as a variable to this function.
	const FunctionValue* function_value = NULL;
	FunctionDefinition* use_target_func = runtimeBind(vmstate, function_value);



	// Push arguments onto argument stack
	const unsigned int initial_arg_stack_size = (unsigned int)vmstate.argument_stack.size();

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
	if(use_target_func->use_captured_vars)
	{
		assert(function_value);
		vmstate.argument_stack.push_back(function_value->captured_vars.getPointer());
	}


	//assert(vmstate.argument_stack.size() == initial_arg_stack_size + this->argument_expressions.size());


	// Execute target function
	vmstate.func_args_start.push_back(initial_arg_stack_size);

	if(VERBOSE_EXEC)
		std::cout << indent(vmstate) << "Calling " << this->function_name << ", func_args_start: " << vmstate.func_args_start.back() << "\n";

	ValueRef ret = use_target_func->invoke(vmstate);
	vmstate.func_args_start.pop_back();

	// Remove arguments from stack
	while(vmstate.argument_stack.size() > initial_arg_stack_size) //for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
	{
		//delete vmstate.argument_stack.back();
		vmstate.argument_stack.pop_back();
	}
	assert(vmstate.argument_stack.size() == initial_arg_stack_size);

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


void FunctionExpression::linkFunctions(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	// Return if we have already bound this function in an earlier pass.
	if(this->binding_type != Unbound)
		return;

	bool found_binding = false;
	bool in_current_func_def = true;
	int use_let_frame_offset = 0;


	// We want to find a function that matches our argument expression types, and the function name



	// First, walk up tree, and see if such a target function has been given a name with a let.
	for(int s = (int)stack.size() - 1; s >= 0 && !found_binding; --s)
	{
		{
			if(stack[s]->nodeType() == ASTNode::FunctionDefinitionType) // If node is a function definition
			{
				FunctionDefinition* def = static_cast<FunctionDefinition*>(stack[s]);

				for(unsigned int i=0; i<def->args.size(); ++i)
				{
					// If the argument is a function, and its name and sig matches our function expression...
					if(def->args[i].name == this->function_name && doesFunctionTypeMatch(def->args[i].type))
					{
						// Then bind this function call to this argument
						this->bound_function = def;
						this->bound_index = i;
						this->binding_type = Arg;
						found_binding = true;

						if(!in_current_func_def)//  && payload.func_def_stack.back()->use_captured_vars)
						{
							this->captured_var_index = (int)payload.func_def_stack.back()->captured_vars.size(); //payload.captured_vars.size();
							this->use_captured_var = true;

							// Add this function argument as a variable that has to be captured for closures.
							CapturedVar var;
							var.vartype = CapturedVar::Arg;
							var.bound_function = def;
							var.index = i;

							//payload.captured_vars.push_back(var);
							payload.func_def_stack.back()->captured_vars.push_back(var);
						}
					}
				}
				in_current_func_def = false;
			}
			else if(stack[s]->nodeType() == ASTNode::LetBlockType)
			{
				LetBlock* let_block = static_cast<LetBlock*>(stack[s]);

				for(unsigned int i=0; i<let_block->lets.size(); ++i)
				{
					// If the function we are tring to bind is in a let expression for the current Let Block, then
					// we only want to bind to let functions from let expressions that are *before* the current let expression.
					// In cases like
					// let
					//   x = x
					// This avoids the x expression on the right binding to the x Let node on the left.
					// In cases like this:
					// let
					//	z = y
					//	y = x
					// it also prevent y from binding to the y from the line below. (which could cause a cycle of references)
					if((s + 1 < stack.size()) && (stack[s+1]->nodeType() == ASTNode::LetType) && (let_block->lets[i].getPointer() == stack[s+1]))
					{
						// We have reached the let expression for the current variable we are tring to bind, so don't try and bind with let variables equal to or past this one.
						break;
					}
					else
					{
						for(size_t v=0; v<let_block->lets[i]->vars.size(); ++v)
						{
							if(let_block->lets[i]->vars[v].name == this->function_name && 
								let_block->lets[i]->type().nonNull() && // Let var might be an unbound function -> no type
								doesFunctionTypeMatch(let_block->lets[i]->type()))
							{
								this->bound_index = i;
								this->binding_type = Let;
								this->bound_let_block = let_block;
								this->let_frame_offset = use_let_frame_offset;
								this->let_var_index = v;
								found_binding = true;

								if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
								{
									this->captured_var_index = (int)payload.func_def_stack.back()->captured_vars.size(); // payload.captured_vars.size();
									this->use_captured_var = true;

									// Add this function argument as a variable that has to be captured for closures.
									CapturedVar var;
									var.vartype = CapturedVar::Let;
									var.bound_let_block = let_block;
									var.index = i;
									var.let_frame_offset = use_let_frame_offset;
									//payload.captured_vars.push_back(var);
									payload.func_def_stack.back()->captured_vars.push_back(var);
								}
							}
						}
					}
				}

				// We only want to count an ancestor let block as an offsetting block if we are not currently in a let clause of it.
				// TODO: remove this stuff
				//bool is_this_let_clause = false;
				//if(s + 1 < (int)stack.size())
				//	for(size_t z=0; z<let_block->lets.size(); ++z)
				//		if(let_block->lets[z].getPointer() == stack[s+1])
				//			is_this_let_clause = true;
				//if(!is_this_let_clause)
					use_let_frame_offset++;
			}
		}
	}

	if(!found_binding)
	{
		vector<TypeRef> argtypes;
		bool all_argtypes_nonnull = true;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		{
			argtypes.push_back(this->argument_expressions[i]->type());
			if(argtypes.back().isNull())
				all_argtypes_nonnull = false;
		}

		if(all_argtypes_nonnull) // Don't try and bind if we have a null type
		{
			const FunctionSignature sig(this->function_name, argtypes);

			// Work out effective call site position.
			int effective_callsite_order_num = 1000000000;
			if(payload.current_named_constant)
				effective_callsite_order_num = payload.current_named_constant->order_num;
			for(size_t z=0; z<payload.func_def_stack.size(); ++z)
				effective_callsite_order_num = myMin(effective_callsite_order_num, payload.func_def_stack[z]->order_num);

			// Try and resolve to internal function.
			this->target_function = linker.findMatchingFunction(sig, this->srcLocation(), effective_callsite_order_num/*&payload.func_def_stack*/).getPointer();
			if(this->target_function/* && isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_function)*/) // Disallow recursion for now: Check the linked function is not the current function.
			{
				this->binding_type = BoundToGlobalDef;
			}
			else
			{
				// Try and promote integer args to float args.
				// TODO: try all possible coercion combinations.
				// This is not really the best way of doing this type coercion, the old approach of matching by name is better.
				
				vector<TypeRef> coerced_argtypes = argtypes;

				for(size_t i=0; i<argtypes.size(); ++i)
				{
					if(	argument_expressions[i]->nodeType() == ASTNode::IntLiteralType &&
						isIntExactlyRepresentableAsFloat(static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value))
					{
						coerced_argtypes[i] = new Float();
					}
				}

				// Try again with our coerced arguments
				const FunctionSignature coerced_sig(this->function_name, coerced_argtypes);

				this->target_function = linker.findMatchingFunction(coerced_sig, this->srcLocation(), effective_callsite_order_num/*&payload.func_def_stack*/).getPointer();
				if(this->target_function/* && isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_function)*/) // Disallow recursion for now: Check the linked function is not the current function.
				{
					this->binding_type = BoundToGlobalDef;

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
					throw BaseException(s + "." + errorContext(*this));
				}*/
			}

			if(this->target_function)
				this->target_function->num_uses++;

			//TEMP: don't fail now, maybe we can bind later.
			//if(this->binding_type == Unbound)
			//	throw BaseException("Failed to find function '" + sig.toString() + "'." + errorContext(*this));
		} // end if all_argtypes_nonnull
	}
}


/*void FunctionExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->bindVariables(s);
}*/


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
	
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			convertOverloadedOperators(argument_expressions[i], payload, stack);
	}*/



	// NOTE: we want to do a post-order traversal here.
	// This is because we want our argument expressions to be linked first.

	stack.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->traverse(payload, stack);

	
	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			checkInlineExpression(argument_expressions[i], payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			checkSubstituteVariable(argument_expressions[i], payload);
	}
	else if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
		// TEMP NEW: Do operator overloading now:
		for(size_t i=0; i<argument_expressions.size(); ++i)
			convertOverloadedOperators(argument_expressions[i], payload, stack);


		// If this is a generic function, we can't try and bind function expressions yet,
		// because the binding depends on argument type due to function overloading, so we have to wait
		// until we know the concrete type.

		if(payload.func_def_stack.empty() || !payload.func_def_stack.back()->isGenericFunction())
			linkFunctions(*payload.linker, payload, stack);

		// Set shuffle mask now
		if(this->target_function && ::hasPrefix(this->target_function->sig.name, "shuffle"))
		{
			assert(this->argument_expressions.size() == 2);
			if(!this->argument_expressions[1]->isConstant())
				throw BaseException("Second arg to shuffle must be constant");

			try
			{
				VMState vmstate;
				vmstate.func_args_start.push_back(0);

				ValueRef res = this->argument_expressions[1]->exec(vmstate);

				const VectorValue* res_v = checkedCast<VectorValue>(res);
				
				std::vector<int> mask(res_v->e.size());
				for(size_t i=0; i<mask.size(); ++i)
				{
					if(!(dynamic_cast<IntValue*>(res_v->e[i].getPointer())))
						throw BaseException("Element in shuffle mask was not an integer.");

					const int index = static_cast<IntValue*>(res_v->e[i].getPointer())->value;
					mask[i] = index;
				}

				assert(this->target_function->built_in_func_impl.nonNull());
				assert(dynamic_cast<ShuffleBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer()));
				static_cast<ShuffleBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer())->setShuffleMask(mask);
			}
			catch(BaseException& e)
			{
				throw BaseException("Failed to eval second arg of shuffle: " + e.what());
			}
		}
		// Set second arg now for elem(tuple, i)
		else if(this->target_function && this->target_function->sig.name == "elem" && target_function->sig.param_types[0]->getType() == Type::TupleTypeType)
		{
			assert(this->argument_expressions.size() == 2);
			if(!this->argument_expressions[1]->isConstant())
				throw BaseException("Second arg to elem(tuple, i) must be constant");

			int index;
			try
			{
				VMState vmstate;
				vmstate.func_args_start.push_back(0);

				ValueRef res = this->argument_expressions[1]->exec(vmstate);

				const IntValue* res_i = checkedCast<IntValue>(res.getPointer());

				index = res_i->value;
			}
			catch(BaseException& e)
			{
				throw BaseException("Failed to eval second arg of elem(tuple, i): " + e.what());
			}	

			assert(this->target_function->built_in_func_impl.nonNull());
			assert(dynamic_cast<GetTupleElementBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer()));
			GetTupleElementBuiltInFunc* tuple_elem_func = static_cast<GetTupleElementBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer());
				

			// bounds check index.
			if(index < 0 || index >= tuple_elem_func->tuple_type->component_types.size())
				throw BaseException("Second argument to tuple elem() function is out of range." + errorContext(*this));


			tuple_elem_func->setIndex(index);

			// Set proper return type for function definition.
			this->target_function->declared_return_type = tuple_elem_func->tuple_type->component_types[index];
			
		}
		else if(this->target_function && this->target_function->sig.name == "fold")
		{
			// TEMP: specialise fold for the passed in function now.
			if(this->binding_type == BoundToGlobalDef)
			{
				assert(this->target_function->built_in_func_impl.nonNull());
				assert(dynamic_cast<ArrayFoldBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer()));
				ArrayFoldBuiltInFunc* fold_func = static_cast<ArrayFoldBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer());

				// Eval first arg (to get function 'f')
				try
				{
					VMState vmstate;
					vmstate.func_args_start.push_back(0);
					ValueRef res = this->argument_expressions[0]->exec(vmstate);

					const FunctionValue* res_f = checkedCast<FunctionValue>(res);

					fold_func->specialiseForFunctionArg(res_f->func_def);
				}
				catch(BaseException& e)
				{
					throw BaseException("Failed to eval first arg of fold " + e.what());
				}
			}
		}
		else if(this->target_function && this->target_function->sig.name == "map")
		{
			// TEMP: specialise map for the passed in function now.
			if(this->binding_type == BoundToGlobalDef)
			{
				assert(this->target_function->built_in_func_impl.nonNull());
				assert(dynamic_cast<ArrayMapBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer()));
				ArrayMapBuiltInFunc* map_func = static_cast<ArrayMapBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer());

				// Eval first arg (to get function 'f')
				try
				{
					VMState vmstate;
					vmstate.func_args_start.push_back(0);
					ValueRef res = this->argument_expressions[0]->exec(vmstate);

					const FunctionValue* res_f = checkedCast<FunctionValue>(res);

					map_func->specialiseForFunctionArg(res_f->func_def);
				}
				catch(BaseException& e)
				{
					throw BaseException("Failed to eval first arg of map " + e.what());
				}
			}
		}

		if(payload.check_bindings && this->binding_type == Unbound)
		{
			//throw BaseException("Failed to find function '" + this->function_name + "' for the given argument types." + errorContext(*this));

			vector<TypeRef> argtypes;
			for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
				argtypes.push_back(this->argument_expressions[i]->type()); // may be NULL

			const FunctionSignature sig(this->function_name, argtypes);
		
			throw BaseException("Failed to find function '" + sig.toString() + "'." + errorContext(*this));
		}
	}
	else if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkInDomain(payload, stack);
		this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		assert(this->binding_type != Unbound);

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
		if(this->binding_type == BoundToGlobalDef)
		{
			// Check shuffle mask (arg 1) is a vector of ints
			if(::hasPrefix(this->target_function->sig.name, "shuffle"))
			{
				// TODO
			}
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		// TODO: check function is bound etc..?
		if(this->binding_type == BoundToGlobalDef)
		{
			/*this->can_constant_fold = true;
			for(size_t i=0; i<argument_expressions.size(); ++i)
				can_constant_fold = can_constant_fold && argument_expressions[i]->can_constant_fold;
			this->can_constant_fold = this->can_constant_fold && expressionIsWellTyped(*this, payload);*/
			this->can_maybe_constant_fold = true;
			for(size_t i=0; i<argument_expressions.size(); ++i)
			{
				const bool arg_is_literal = checkFoldExpression(argument_expressions[i], payload);
				this->can_maybe_constant_fold = this->can_maybe_constant_fold && arg_is_literal;
			}
		}
		else
			this->can_maybe_constant_fold = false;
	}
	
	stack.pop_back();
}


bool FunctionExpression::provenDefined() const
{
	//return proven_defined;
	if(this->target_function->sig.name == "elem")
		return proven_defined;

	return true;
}


void FunctionExpression::checkInDomain(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(this->target_function && this->target_function->sig.name == "elem" && this->argument_expressions.size() == 2)
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
				VMState vmstate;
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(dynamic_cast<IntValue*>(retval.getPointer()));

				const int index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < array_type->num_elems)
				{
					// Array index is in-bounds!
					return;
				}
				else
				{
					throw BaseException("Constant index with value " + toString(index_val) + " was out of bounds of array type " + array_type->toString());
				}
			}
			else
			{
				// Else index is not known at compile time.

				//int i_lower = std::numeric_limits<int32>::min();
				//int i_upper = std::numeric_limits<int32>::max();
				//Vec2<int> i_bounds(std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max());

				const IntervalSetInt i_bounds = ProofUtils::getIntegerRange(payload, stack, 
					this->argument_expressions[1] // integer value
				);

				// Now check our bounds against the array
				if(i_bounds.lower() >= 0 && i_bounds.upper() < array_type->num_elems)
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
						if(int_lit->value >= 0 && int_lit->value < array_type->num_elems) // if in-bounds
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
				VMState vmstate;
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(dynamic_cast<IntValue*>(retval.getPointer()));

				const int index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < (int)vector_type->num)
				{
					// Vector index is in-bounds!
					return;
				}
				else
				{
					throw BaseException("Constant index with value " + toString(index_val) + " was out of bounds of vector type " + vector_type->toString());
				}
			}
			else
			{
				// Else index is not known at compile time.

				const IntervalSetInt i_bounds = ProofUtils::getIntegerRange(payload, stack, 
					this->argument_expressions[1] // integer value
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

		throw BaseException("Failed to prove elem() argument is in-bounds." + errorContext(*this));
	}
	// truncateToInt
	else if(this->target_function && this->target_function->sig.name == "truncateToInt" && this->argument_expressions.size() == 1)
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
	else if(this->target_function && this->target_function->sig.name == "update" && this->argument_expressions.size() == 3)
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
				VMState vmstate;
				vmstate.func_args_start.push_back(0);

				ValueRef retval = this->argument_expressions[1]->exec(vmstate);

				assert(dynamic_cast<IntValue*>(retval.getPointer()));

				const int index_val = static_cast<IntValue*>(retval.getPointer())->value;

				if(index_val >= 0 && index_val < array_type->num_elems)
				{
					// Array index is in-bounds!
					return;
				}
				else
				{
					throw BaseException("Constant index with value " + toString(index_val) + " was out of bounds of array type " + array_type->toString());
				}
			}
			else
			{
				// Else index is not known at compile time.
				
				const IntervalSetInt i_bounds = ProofUtils::getIntegerRange(payload, stack, 
					this->argument_expressions[1] // integer value
				);

				// Now check our bounds against the array
				if(i_bounds.lower() >= 0 && i_bounds.upper() < array_type->num_elems)
				{
					// Array index is proven to be in-bounds.
					return;
				}
			}
		}

		throw BaseException("Failed to prove update() index argument is in-bounds." + errorContext(*this));
	}
}


void FunctionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionExpr (" << this->function_name << ")";
	if(this->target_function)
		s << "; target: " << this->target_function->sig.toString();
	else if(this->binding_type == Arg)
		s << "; runtime bound to arg index " << this->bound_index;
	else if(this->binding_type == Let)
		s << "; runtime bound to let index " << this->bound_index;
	s << "\n";
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->print(depth + 1, s);
}


std::string FunctionExpression::sourceString() const
{
	std::string s = this->function_name + "(";
	for(unsigned int i=0; i<argument_expressions.size(); ++i)
	{
		s += argument_expressions[i]->sourceString();
		if(i + 1 < argument_expressions.size())
			s += ", ";
	}
	return s + ")";
}


// From https://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/mathFunctions.html
static const char* opencl_built_in_func_names[] = { 
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
		if(!(func->sig.param_types[i]->getType() == Type::FloatType || func->sig.param_types[i]->getType() == Type::IntType || func->sig.param_types[i]->getType() == Type::VectorTypeType))
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
	if(this->function_name == "elem")
	{
		if(argument_expressions.size() != 2)
			throw BaseException("Error while emitting OpenCL C: elem() function with != 2 args.");

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
				throw BaseException("Error while emitting OpenCL C: elem() function with 2nd arg that is not an Int literal.");

			const int64 index = static_cast<const IntLiteral*>(argument_expressions[1].getPointer())->value;

			if(index < 0 || index >= 16)
				throw BaseException("Error while emitting OpenCL C: elem() function has invalid index: " + toString(index));

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
			return argument_expressions[0]->emitOpenCLC(params) + "[" + argument_expressions[1]->emitOpenCLC(params) + "]";
		}
		else if(this->argument_expressions[0]->type()->getType() == Type::TupleTypeType)
		{
			/*
			elem(a, i)		=>		a.field_i
			*/

			if(argument_expressions[1]->nodeType() != ASTNode::IntLiteralType)
				throw BaseException("Error while emitting OpenCL C: elem(tuple, i) function with 2nd arg that is not an Int literal.");

			const int64 index = static_cast<const IntLiteral*>(argument_expressions[1].getPointer())->value;

			return argument_expressions[0]->emitOpenCLC(params) + ".field_" + ::toString(index);
		}
		else
			throw BaseException("Error while emitting OpenCL C: elem() function first arg has unsupported type " + argument_expressions[0]->type()->toString() );
	}
	else if(this->function_name == "shuffle")
	{
		if(argument_expressions.size() != 2)
			throw BaseException("Error while emitting OpenCL C: shuffle() function with != 2 args.");

		if(argument_expressions[1]->nodeType() != ASTNode::VectorLiteralType)
			throw BaseException("Error while emitting OpenCL C: shuffle() function with 2nd arg that is not a Vector literal.");

		const VectorLiteral* vec_literal = static_cast<const VectorLiteral*>(argument_expressions[1].getPointer());

		std::string s = argument_expressions[0]->emitOpenCLC(params) + ".s";

		for(size_t i=0; i<vec_literal->getElements().size(); ++i)
		{
			if(vec_literal->getElements()[i]->nodeType() != ASTNode::IntLiteralType)
				throw BaseException("Error while emitting OpenCL C: shuffle() function with 2nd arg that does not have an int literal in the vector literal.");

			const int64 index = static_cast<const IntLiteral*>(vec_literal->getElements()[i].getPointer())->value;

			s.push_back(::intToHexChar((int)index));
		}

		return s;
	}
	else if(isENFunctionName(function_name) && 
		(this->argument_expressions[0]->type()->getType() == Type::VectorTypeType ||
		this->argument_expressions[0]->type()->getType() == Type::ArrayTypeType))
	{
		try
		{
			// eN() function
			const int index = stringToInt(function_name.substr(1, function_name.size() - 1));

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
				throw BaseException("Error while emitting OpenCL C: eN() function first arg not supported for type " + argument_expressions[0]->type()->toString());
		}
		catch(StringUtilsExcep&)
		{
			throw BaseException("Error while emitting OpenCL C: invalid eN() function '" + function_name + "'.");
		}
	}
	else if(target_function && target_function->built_in_func_impl.nonNull() && dynamic_cast<GetField*>(target_function->built_in_func_impl.getPointer()))
	{
		// Transform get field built-in functions like so:
		// struct s { int x; }
		// 
		// x(s)			=>		s.x

		if(argument_expressions.size() != 1)
			throw BaseException("Error while emitting OpenCL C: get field function with != 1 args.");

		return argument_expressions[0]->emitOpenCLC(params) + "." +  function_name;
	}
	else if(function_name == "inBounds")
	{
		// inBounds(a, i)			=>		i >= 0 && i < N

		if(argument_expressions.size() != 2)
			throw BaseException("Error while emitting OpenCL C: inBounds function with != 2 args.");

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
			throw BaseException("Error while emitting OpenCL C: inBounds arg 1 type must be vector array.");

		return "((" + argument_expressions[1]->emitOpenCLC(params) + " >= 0) && (" + argument_expressions[1]->emitOpenCLC(params) + " < " + toString(N) + "))";
	}
	else if(function_name == "abs" && (argument_expressions.size() == 1) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType)))
	{
		return "fabs(" + argument_expressions[0]->emitOpenCLC(params) + ")";
	}
	else if(function_name == "min" && (argument_expressions.size() == 2) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType)))
	{
		return "fmin(" + argument_expressions[0]->emitOpenCLC(params) + ", " + argument_expressions[1]->emitOpenCLC(params) + ")";
	}
	else if(function_name == "max" && (argument_expressions.size() == 2) && 
		(argument_expressions[0]->type()->getType() == Type::FloatType || 
		(argument_expressions[0]->type()->getType() == Type::VectorTypeType && argument_expressions[0]->type().downcastToPtr<VectorType>()->elem_type->getType() == Type::FloatType)))
	{
		return "fmax(" + argument_expressions[0]->emitOpenCLC(params) + ", " + argument_expressions[1]->emitOpenCLC(params) + ")";
	}
	else if(function_name == "iterate")
	{
		//TODO: check arg 0 is constant.
		if(!(this->argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType && this->argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::BoundToGlobalDefVariable))
			throw BaseException("Error while emitting OpenCL C: First arg to iterate must be a constant reference to a globally defined function.");


		

		return this->target_function->built_in_func_impl.downcastToPtr<IterateBuiltInFunc>()->emitOpenCLForFunctionArg(
			params,
			this->argument_expressions[0].downcastToPtr<Variable>()->bound_function,
			this->argument_expressions
		);
		/*if(argument_expressions.size() != 1)
			throw BaseException("Error while emitting OpenCL C: abs function with != 1 arg.");

		if(argument_expressions[0]->type()->getType() == Type::FloatType)
			return "fabs(" + argument_expressions[0]->emitOpenCLC(params) + ")";
		else
			return "abs(" + argument_expressions[0]->emitOpenCLC(params) + ")";*/
	}
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

		std::string use_func_name = (/*this->target_function->isExternalFunction() ||*/ isCallToBuiltInOpenCLFunction(this->target_function)) ? 
			this->target_function->sig.name : this->target_function->sig.typeMangledName();

		std::string s = use_func_name + "(";
		for(unsigned int i=0; i<argument_expressions.size(); ++i)
		{
			s += argument_expressions[i]->emitOpenCLC(params);
			if(i + 1 < argument_expressions.size())
				s += ", ";
		}
		return s + ")";
	}
}


TypeRef FunctionExpression::type() const
{
	if(this->binding_type == BoundToGlobalDef)
	{
		assert(this->target_function);
		return this->target_function->returnType();
	}
	else if(this->binding_type == Let)
	{
		TypeRef t = this->bound_let_block->lets[this->bound_index]->type();
		Function* func_type = static_cast<Function*>(t.getPointer());
		return func_type->return_type;
	}
	else if(this->binding_type == Arg)
	{
		Function* func_type = static_cast<Function*>(this->bound_function->args[this->bound_index].type.getPointer());
		return func_type->return_type;
	}
	else if(this->binding_type == Unbound)
	{
		return TypeRef(NULL);
	}
	else
	{
		assert(0);
		return TypeRef(NULL);
	}
}


llvm::Value* FunctionExpression::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	llvm::Value* target_llvm_func = NULL;
	TypeRef target_ret_type = this->type(); //this->target_function_return_type;

	llvm::Value* captured_var_struct_ptr = NULL;

	if(binding_type == Let)
	{
		//target_llvm_func = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index);

		//llvm::Value* closure_pointer = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index, ret_space_ptr);
		assert(params.let_block_let_values.find(this->bound_let_block) != params.let_block_let_values.end());
		llvm::Value* closure_pointer = params.let_block_let_values[this->bound_let_block][this->bound_index];


		
		//TEMP:
		//std::cout << "closure_pointer: \n";
		//closure_pointer->dump();

		// Load function pointer from closure.

		/*{
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* func_ptr_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end(),
			"func_ptr_ptr"
		);

		target_llvm_func = params.builder->CreateLoad(func_ptr_ptr, "func_ptr");
		}*/

		throw BaseException("Support for first class functions as let variables disabled.");//TEMP

		target_llvm_func = LLVMTypeUtils::createFieldLoad(
			closure_pointer,
			1, // field index
			params.builder,
			"target_llvm_func"
		);

		// NOTE: index 2 should hold the captured vars struct.
		captured_var_struct_ptr = params.builder->CreateStructGEP(closure_pointer, 
			2); // field index

	}
	else if(binding_type == Arg)
	{
		// If the current function returns its result via pointer, then all args are offset by one.
		//if(params.currently_building_func_def->returnType()->passByValue())
		//	closure_pointer = LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index);
		//else
		//	closure_pointer = LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index + 1);

		llvm::Value* closure_pointer = LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getLLVMArgIndex(this->bound_index)
		);

		//target_llvm_func = dynamic_cast<llvm::Function*>(func);

		// Load function pointer from closure.

		/*{
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end(),
			"func_ptr_ptr"
		);

		target_llvm_func = params.builder->CreateLoad(field_ptr, "func_ptr");
		}*/
		//closure_pointer->dump();

		throw BaseException("Support for first class functions passed as arguments disabled.");//TEMP

		target_llvm_func = LLVMTypeUtils::createFieldLoad(
			closure_pointer,
			1, // field index
			params.builder,
			"target_llvm_func"
		);

		// NOTE: index 2 should hold the captured vars struct.
		captured_var_struct_ptr = params.builder->CreateStructGEP(closure_pointer, 
			2); // field index
	}
	else if(binding_type == BoundToGlobalDef)
	{
		// For structure field access functions, instead of emitting an actual function call, just emit the LLVM code to access the field.
		if(dynamic_cast<GetField*>(this->target_function->built_in_func_impl.getPointer()))
		{
			const GetField* get_field_func = static_cast<const GetField*>(this->target_function->built_in_func_impl.getPointer());

			const int field_index = get_field_func->index;

			const TypeRef field_type = get_field_func->struct_type->component_types[field_index];
			const std::string field_name = get_field_func->struct_type->component_names[field_index];

			assert(argument_expressions.size() == 1);
			llvm::Value* struct_ptr = argument_expressions[0]->emitLLVMCode(params, NULL);

			llvm::Value* result;
			if(field_type->passByValue())
			{
				llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, field_index, get_field_func->struct_type->name + "." + field_name + " ptr");
				llvm::Value* loaded_val = params.builder->CreateLoad(field_ptr, field_name);

				// TEMP NEW: increment ref count if this is a string
				//if(field_type->getType() == Type::StringType)
				//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

				result = loaded_val;
			}
			else
			{
				result = params.builder->CreateStructGEP(struct_ptr, field_index, get_field_func->struct_type->name + "." + field_name + " ptr");
			}

			field_type->emitIncrRefCount(params, result, "GetField " + get_field_func->struct_type->name + "." + field_name + " result increment");

			// Decrement argument 0 structure ref count
			//if(!(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType)) // && argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::LetVariable)) // Don't decr let var ref counts, the ref block will do that.
			if(shouldRefCount(params, *argument_expressions[0]))
				emitDestructorOrDecrCall(params, *argument_expressions[0], struct_ptr, "GetField " + get_field_func->struct_type->name + "." + field_name + " struct arg decrement/destructor");

			return result;
		}
		// For tuple field access functions, instead of emitting an actual function call, just emit the LLVM code to access the field.
		else if(dynamic_cast<GetTupleElementBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer()))
		{
			const GetTupleElementBuiltInFunc* get_field_func = static_cast<const GetTupleElementBuiltInFunc*>(this->target_function->built_in_func_impl.getPointer());

			const int field_index = get_field_func->index;

			const TypeRef field_type = get_field_func->tuple_type->component_types[field_index];
			const std::string field_name = "field " + toString(field_index);

			assert(argument_expressions.size() == 2);
			llvm::Value* struct_ptr = argument_expressions[0]->emitLLVMCode(params, NULL);

			llvm::Value* result;
			if(field_type->passByValue())
			{
				llvm::Value* field_ptr = params.builder->CreateStructGEP(struct_ptr, field_index, field_name + " ptr");
				llvm::Value* loaded_val = params.builder->CreateLoad(field_ptr, field_name);

				// TEMP NEW: increment ref count if this is a string
				//if(field_type->getType() == Type::StringType)
				//	RefCounting::emitIncrementStringRefCount(params, loaded_val);

				result = loaded_val;
			}
			else
			{
				result = params.builder->CreateStructGEP(struct_ptr, field_index, field_name + " ptr");
			}

			field_type->emitIncrRefCount(params, result, "GetTupleElement " + get_field_func->tuple_type->toString() + " " + field_name + " result increment");

			// Decrement argument 0 structure ref count
			//if(!(argument_expressions[0]->nodeType() == ASTNode::VariableASTNodeType)) // && argument_expressions[0].downcastToPtr<Variable>()->vartype == Variable::LetVariable)) // Don't decr let var ref counts, the ref block will do that.
			if(shouldRefCount(params, *argument_expressions[0]))
				emitDestructorOrDecrCall(params, *argument_expressions[0], struct_ptr, "GetTupleElement " + get_field_func->tuple_type->toString() + "." + field_name + " tuple arg decrement/destructor");

			return result;
		}
			



		// Lookup LLVM function, which should already be created and added to the module.
		/*llvm::Function* target_llvm_func = params.module->getFunction(
			this->target_function->sig.toString() //internalFuncName(call_target_sig)
			);
		assert(target_llvm_func);*/

		/*FunctionSignature target_sig = this->target_function->sig;

		llvm::FunctionType* target_func_type = LLVMTypeUtils::llvmFunctionType(
			target_sig.param_types, 
			target_ret_type, 
			*params.context,
			params.hidden_voidptr_arg
		);

		target_llvm_func = params.module->getOrInsertFunction(
			target_sig.toString(), // Name
			target_func_type // Type
		);*/

		target_llvm_func = this->target_function->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
		);

		//closure_pointer = this->target_function->emitLLVMCode(params);
		//assert(closure_pointer);
	}
	else
	{
		assert(0);
	}

	//assert(closure_pointer);


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
				this->target_function->sig.name + "() ret"
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
			if(var_node->vartype == Variable::LetVariable || var_node->vartype == Variable::ArgumentVariable)
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

	// Set hidden voidptr argument
	//if(target_takes_voidptr_arg) // params.hidden_voidptr_arg)
	//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

	llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args);

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	call_inst->setCallingConv(llvm::CallingConv::C);

	// Decrement ref counts on arguments
	const int num_sret_args = target_ret_type->passByValue() ? 0 : 1;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		if(shouldRefCount(params, argument_expressions[i]) && do_ref_counting_for_arg[i])
			emitDestructorOrDecrCall(params, *argument_expressions[i], args[i + num_sret_args], "function expression '" + this->target_function->sig.toString() + "' argument " + toString(i) + " decrement");

	return target_ret_type->passByValue() ? call_inst : return_val_addr;
}


//void FunctionExpression::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
//{
////	RefCounting::emitCleanupLLVMCode(params, this->type(), val);
//}


llvm::Value* FunctionExpression::getConstantLLVMValue(EmitLLVMCodeParams& params) const
{
	return this->target_function->getConstantLLVMValue(params);
}


Reference<ASTNode> FunctionExpression::clone()
{
	FunctionExpression* e = new FunctionExpression(srcLocation());
	e->function_name = this->function_name;

	for(size_t i=0; i<argument_expressions.size(); ++i)
		e->argument_expressions.push_back(argument_expressions[i]->clone());
	
	e->target_function = this->target_function;
	e->bound_index = this->bound_index;
	e->bound_function = this->bound_function;
	e->bound_let_block = this->bound_let_block;
	e->binding_type = this->binding_type;

	return e;
}


bool FunctionExpression::isConstant() const
{
	// For now, we'll say a function expression bound to an argument of let var is not constant.
	if(this->binding_type != BoundToGlobalDef)
		return false;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		if(!argument_expressions[i]->isConstant())
			return false;

	return true;
}


} // end namespace Winter
