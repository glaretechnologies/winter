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
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
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


FunctionDefinition* FunctionExpression::runtimeBind(VMState& vmstate, FunctionValue*& function_value_out)
{
	if(use_captured_var)
	{
		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		ValueRef captured_struct = vmstate.argument_stack.back();
		assert(dynamic_cast<StructureValue*>(captured_struct.getPointer()));
		StructureValue* s = static_cast<StructureValue*>(captured_struct.getPointer());

		ValueRef func_val = s->fields[this->captured_var_index];

		assert(dynamic_cast<FunctionValue*>(func_val.getPointer()));

		FunctionValue* function_val = static_cast<FunctionValue*>(func_val.getPointer());

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
		assert(dynamic_cast<FunctionValue*>(arg.getPointer()));
		FunctionValue* function_value = static_cast<FunctionValue*>(arg.getPointer());
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
		assert(dynamic_cast<FunctionValue*>(arg.getPointer()));
		FunctionValue* function_value = static_cast<FunctionValue*>(arg.getPointer());
		function_value_out = function_value;
		return function_value->func_def;
	}
}


ValueRef FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) std::cout << indent(vmstate) << "FunctionExpression, target_name=" << this->function_name << "\n";

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
	FunctionValue* function_value = NULL;
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
		vmstate.argument_stack.push_back(ValueRef(function_value->captured_vars.getPointer()));
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
	assert(func);

	std::vector<TypeRef> arg_types(this->argument_expressions.size());
	for(unsigned int i=0; i<arg_types.size(); ++i)
		arg_types[i] = this->argument_expressions[i]->type();

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


static bool isNodeAncestor(const std::vector<ASTNode*>& stack, const ASTNode* node)
{
	for(size_t i=0; i<stack.size(); ++i)
		if(node == stack[i])
			return true;
	return false;
}


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
					if(let_block->lets[i]->variable_name == this->function_name && 
						!isNodeAncestor(stack, let_block->lets[i].getPointer()) && // Don't try to bind to ancestor let variables, because their type will not be computed yet.
																					// Also binding to such a variable would result in a circular definition of the form
																					// let  x = x()
						doesFunctionTypeMatch(let_block->lets[i]->type()))
					{
						this->bound_index = i;
						this->binding_type = Let;
						this->bound_let_block = let_block;
						this->let_frame_offset = use_let_frame_offset;
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

			// Try and resolve to internal function.
			this->target_function = linker.findMatchingFunction(sig).getPointer();
			if(this->target_function)
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

				this->target_function = linker.findMatchingFunction(coerced_sig).getPointer();
				if(this->target_function)
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


// Return true iff the function requires an opaque env arg.  This will be the case for all functions, *unless* they are calling 'special' functions from Linker.

static const char* plain_func_names[] = { "floor", "ceil", "sqrt", "sin", "cos", "exp", "log", "abs", "truncateToInt", "toFloat", "map", "elem", "inBounds", "shuffle", "min", "max", "pow", "dot", "if", 
	"vec2", "op_add", "op_sub", "op_mul", "op_eq", "vec3", "vec4", "x", "y", "z", "w", "mat2x2", "mul", "mat3x3", "mat4x4", "col0", "col1", "col2", "col3", "or", "and", "not", "xor", "add", "sub", "div",
	"lt", "lte", "gt", "gte", "eq", "neq", "doti", "dotj", "dotk", "fract", "floorToInt", "ceilToInt", "lerp", "step", "get_t", "smoothstep", "smootherstep", "pulse", "smoothPulse", "clamp", "cross",
	"length", "dist", "neg", "recip", "normalise", "real", "pi", "noise", "noise3Valued", "fbm", "fbm3Valued", "noise01", "fbm01", "gridNoise", "voronoiDist", "randomCellShade",

	// From WinterExternalFuncs.cpp:
	"print", "rotationMatrix", "fbm", "fbm4Valued", "multifractal", "noise", "noise4Valued", "voronoi", "voronoi3d", "gridNoise", "tan", "asin", "acos", "atan", "atan2", "fastPow", "mod",
	NULL};


static bool doesFunctionRequireOpaqueEnvArg(const FunctionExpression& f)
{
	if(f.function_name.empty())
		return false;

	// Check for eN functions
	if(f.function_name[0] == 'e' && f.function_name.size() >= 2 && isNumeric(f.function_name[1]))
		return false;

	for(size_t i=0; plain_func_names[i] != NULL; ++i)
	{
		if(f.function_name == plain_func_names[i])
			return false;
	}

	return true;
}


// Some functions from e.g. ISL_stdlib may be correct already
static bool isFunctionAlreadyCorrect(const FunctionDefinition& f)
{
	// If this function def is from ISL_stdlib.txt, then it should be fine.
	return f.srcLocation().isValid() && ::hasSuffix(f.srcLocation().source_buffer->name, "ISL_stdlib.txt");
}


void FunctionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			if(shouldFoldExpression(argument_expressions[i], payload))
			{
				try
				{
					argument_expressions[i] = foldExpression(argument_expressions[i], payload);
					payload.tree_changed = true;
				}
				catch(BaseException& )
				{
					// An invalid operation was performed, such as dividing by zero, while trying to eval the AST node.
					// In this case we will consider the folding as not taking place.
				}
			}
	}
	else if(payload.operation == TraversalPayload::AddOpaqueEnvArg)
	{
		const bool target_requires_opaque_arg = doesFunctionRequireOpaqueEnvArg(*this);

		if(target_requires_opaque_arg)
		{
			FunctionDefinition* current_func = payload.func_def_stack.back();

			if(!isFunctionAlreadyCorrect(*current_func))
			{
				assert(!current_func->args.empty() && current_func->args.back().type->getType() == Type::OpaqueTypeType);

				bool last_arg_is_opaque_arg = false;
				if(!argument_expressions.empty() && argument_expressions.back()->nodeType() == ASTNode::VariableASTNodeType)
				{
					if(argument_expressions.back().downcast<Variable>()->name == "env")
						last_arg_is_opaque_arg = true;
				}
					
				if(!last_arg_is_opaque_arg)
					argument_expressions.push_back(new Variable(current_func->args.back().name, SrcLocation::invalidLocation()));
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
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

		if(!payload.func_def_stack.back()->isGenericFunction())
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

				assert(dynamic_cast<VectorValue*>(res.getPointer()));

				VectorValue* res_v = static_cast<VectorValue*>(res.getPointer());
				
				std::vector<int> mask(res_v->e.size());
				for(size_t i=0; i<mask.size(); ++i)
				{
					if(!(dynamic_cast<IntValue*>(res_v->e[i].getPointer())))
						throw BaseException("Element in shuffle mask was not an integer.");

					mask[i] = static_cast<IntValue*>(res_v->e[i].getPointer())->value;
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
	}
	else if(payload.operation == TraversalPayload::CheckInDomain)
	{
		checkInDomain(payload, stack);
		this->proven_defined = true;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->binding_type == Unbound)
		{
			vector<TypeRef> argtypes;
			for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
				argtypes.push_back(this->argument_expressions[i]->type());

			const FunctionSignature sig(this->function_name, argtypes);
		
			throw BaseException("Failed to find function '" + sig.toString() + "'." + errorContext(*this));
		}
		else if(this->binding_type == BoundToGlobalDef)
		{
			// Check shuffle mask (arg 1) is a vector of ints
			if(::hasPrefix(this->target_function->sig.name, "shuffle"))
			{
				// TODO
			}
		}
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
#if USE_LLVM
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

		target_llvm_func = LLVMTypeUtils::createFieldLoad(
			closure_pointer,
			1, // field index
			params.builder, *params.context,
			"target_llvm_func"
		);

		{
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		// NOTE: index 2 should hold the captured vars struct.
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 2, true))); // field index
		
		captured_var_struct_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices,
			"captured_var_struct_ptr"
		);
		}

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

		target_llvm_func = LLVMTypeUtils::createFieldLoad(
			closure_pointer,
			1, // field index
			params.builder, *params.context,
			"target_llvm_func"
		);

		{
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		// NOTE: index 2 should hold the captured vars struct.
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 2, true))); // field index
		
		captured_var_struct_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices,
			"captured_var_struct_ptr"
		);
		}
	}
	else if(binding_type == BoundToGlobalDef)
	{
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

	


	


	if(target_ret_type->passByValue())
	{
		vector<llvm::Value*> args;

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params, NULL));

		// Append pointer to Captured var struct, if this function was from a closure, and there are captured vars.
		if(captured_var_struct_ptr != NULL)
			args.push_back(captured_var_struct_ptr);

		//std::cout << "captured_var_struct_ptr: " << std::endl;
		//captured_var_struct_ptr->dump();

		// Set hidden voidptr argument
		//if(target_takes_voidptr_arg) // params.hidden_voidptr_arg)
		//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args);

		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);

		// If this is a string value, need to decr ref count at end of func.
		if(target_ret_type->getType() == Type::StringType)
		{
			params.cleanup_values.push_back(CleanUpInfo(this, call_inst));
		}

		return call_inst;
	}
	else
	{
		llvm::Value* return_val_addr;
		if(ret_space_ptr)
			return_val_addr = ret_space_ptr;
		else
		{
			// Allocate return value on stack

			// Emit the alloca in the entry block for better code-gen.
			llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().begin());

			return_val_addr = entry_block_builder.Insert(new llvm::AllocaInst(
				target_ret_type->LLVMType(*params.context), // type
				NULL, // ArraySize
				16 // alignment
				//"return_val_addr" // target_sig.toString() + " return_val_addr"
			),
			"" + this->target_function->sig.name + "() ret");
		}

		vector<llvm::Value*> args(1, return_val_addr); // First argument is return value pointer.

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

		// Append pointer to Captured var struct, if this function was from a closure.
		if(captured_var_struct_ptr != NULL)
			args.push_back(captured_var_struct_ptr);

		// Set hidden voidptr argument
		//if(target_takes_voidptr_arg) // params.hidden_voidptr_arg)
		//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		//TEMP:
		/*std::cout << "Args to CreateCall() for " << this->target_function->sig.toString() << ":" << std::endl;
		for(int z=0; z<args.size(); ++z)
		{
			std::cout << "arg " << z << ": ";
			args[z]->dump();
		}*/

		llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args);
		
		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);

		// If this is a string value, need to decr ref count at end of func.
		/*if(target_ret_type->getType() == Type::StringType)
		{
			params.cleanup_values.push_back(CleanUpInfo(this, call_inst));
		}
		else */if(target_ret_type->getType() == Type::StructureTypeType)
		{
			//const StructureType& struct_type = static_cast<const StructureType&>(*target_ret_type);

			//for(size_t i=0; i<struct_type.component_types.size(); ++i)

			params.cleanup_values.push_back(CleanUpInfo(this, return_val_addr));
		}

		return return_val_addr;
	}
#else
	return NULL;
#endif
}


void FunctionExpression::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const
{
	RefCounting::emitCleanupLLVMCode(params, this->type(), val);
}


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

	return ASTNodeRef(e);
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
