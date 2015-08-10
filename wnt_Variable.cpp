/*=====================================================================
wnt_Variable.cpp
----------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_Variable.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
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
//#include <iostream>


using std::vector;
using std::string;


namespace Winter
{


Variable::Variable(const std::string& name_, const SrcLocation& loc)
:	ASTNode(VariableASTNodeType, loc),
	vartype(UnboundVariable),
	name(name_),
	bound_index(-1),
	bound_function(NULL),
	bound_let_node(NULL),
	bound_named_constant(NULL),
	let_var_index(-1)
	//use_captured_var(false),
	//captured_var_index(0)
{
	this->can_maybe_constant_fold = false;
}


inline static const std::string varType(Variable::BindingType t)
{
	if(t == Variable::UnboundVariable)
		return "Unbound";
	else if(t == Variable::LetVariable)
		return "Let";
	else if(t == Variable::ArgumentVariable)
		return "Arg";
	else if(t == Variable::BoundToGlobalDefVariable)
		return "BoundToGlobalDef";
	else if(t == Variable::BoundToNamedConstant)
		return "BoundToNamedConstant";
	else if(t == Variable::CapturedVariable)
		return "Captured";
	else
	{
		assert(!"invalid var type");
		return "";
	}
}


struct BindInfo
{
	BindInfo() : root_bound_node(NULL), bound_function(NULL), bound_let_node(NULL) {}

	Variable::BindingType vartype;

	int bound_index;
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.

	ASTNode* root_bound_node; // Node furthest up the node stack that we are bound to, if the variable is bound through one or more captured vars.
	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetASTNode* bound_let_node;
};


BindInfo doBind(const std::vector<ASTNode*>& stack, int s, const std::string& name)
{
	for(; s >= 0; --s) // Walk up the stack of ancestor nodes
	{
		if(stack[s]->nodeType() == ASTNode::FunctionDefinitionType) // If node is a function definition:
		{
			FunctionDefinition* def = static_cast<FunctionDefinition*>(stack[s]);

			// Try and bind to one of the function arguments:
			for(unsigned int i=0; i<def->args.size(); ++i) // For each argument to the function:
				if(def->args[i].name == name) // If the argument name matches this variable name:
				{
					// Bind this variable to the argument.
					BindInfo bindinfo;
					bindinfo.vartype = Variable::ArgumentVariable;
					bindinfo.bound_index = i;
					bindinfo.bound_function = def;
					bindinfo.root_bound_node = def;
					//std::cout << "Bound '" + name + "' to function arg, bound_index = " << bindinfo.bound_index << ", def = " << def->sig.toString() << std::endl;
					return bindinfo;
				}

			if(s >= 1)
			{
				assert(def->is_anon_func);

				// We have reached a lambda expression.
				// This means that the target of the current variable we are trying to bind must lie in the local environment, e.g. this is a captured var.
				// So the variable we are trying to bind will be bound to capture result.  Now we need to determine what the capture result binds to.
				BindInfo cap_var_bindinfo = doBind(stack, s - 1, name);

				if(cap_var_bindinfo.vartype == Variable::UnboundVariable) // If binding failed:
					return cap_var_bindinfo;

				// Add this variable to the list of captured vars for this function
				CapturedVar var;
				if(cap_var_bindinfo.vartype == Variable::ArgumentVariable)
					var.vartype = CapturedVar::Arg;
				else if(cap_var_bindinfo.vartype == Variable::LetVariable)
					var.vartype = CapturedVar::Let;
				else if(cap_var_bindinfo.vartype == Variable::CapturedVariable)
					var.vartype = CapturedVar::Captured;
				else
				{
					assert(0);
				}
				var.index = cap_var_bindinfo.bound_index;
				var.bound_let_node = cap_var_bindinfo.bound_let_node;
				var.bound_function = cap_var_bindinfo.bound_function;
				def->captured_vars.push_back(var);

				BindInfo bindinfo;
				bindinfo.vartype = Variable::CapturedVariable;
				bindinfo.bound_index = (int)def->captured_vars.size() - 1;
				bindinfo.root_bound_node = cap_var_bindinfo.root_bound_node;
				bindinfo.bound_function = def;
				//std::cout << "Bound '" + name + "' to captured var, bound_index = " << bindinfo.bound_index << ", captured var type = " << varType(cap_var_bindinfo.vartype) << std::endl;
				return bindinfo;
			}
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
				if((s + 1 < stack.size()) && (stack[s+1]->nodeType() == ASTNode::LetType) && (let_block->lets[i].getPointer() == stack[s+1]))
				{
					// We have reached the let expression for the current variable we are tring to bind, so don't try and bind with let variables equal to or past this one.
					break;
				}
				else
				{
					for(size_t v=0; v<let_block->lets[i]->vars.size(); ++v)
					{
						if(let_block->lets[i]->vars[v].name == name)
						{
							BindInfo bindinfo;
							bindinfo.vartype = Variable::LetVariable;
							bindinfo.bound_let_node = let_block->lets[i].getPointer();
							bindinfo.bound_index = i;
							bindinfo.let_var_index = (int)v;
							//std::cout << "Bound '" + name + "' to let variable, bound_index = " << bindinfo.bound_index << std::endl;
							return bindinfo;
						}
					}
				}
			}
		}
	}
	
	BindInfo info;
	info.vartype = Variable::UnboundVariable;
	return info;
}


void Variable::bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack)
{
	// Don't try and do the binding process again if already bound.
	if(this->vartype != UnboundVariable)
		return;

	BindInfo bindinfo = doBind(stack, (int)stack.size() - 1, name);
	if(bindinfo.vartype != UnboundVariable)
	{
		this->vartype = bindinfo.vartype;
		this->bound_index = bindinfo.bound_index;
		this->let_var_index = bindinfo.let_var_index;
		this->bound_function = bindinfo.bound_function;
		this->bound_let_node = bindinfo.bound_let_node;
		return;
	}

	// Try and bind to top level function definition
//	BufferRoot* root = static_cast<BufferRoot*>(stack[0]);
//	vector<FunctionDefinitionRef
//	for(size_t i=0; i<stack[0]->get

//	Frame::NameToFuncMapType::iterator res = payload.top_lvl_frame->name_to_functions_map.find(this->name);
//	if(res != payload.top_lvl_frame->name_to_functions_map.end())
	//Frame::NameToFuncMapType::iterator res = payload.linker->findMatchingFunctionByName(this->name);
	vector<FunctionDefinitionRef> matching_functions;
	payload.linker->getFuncsWithMatchingName(this->name, matching_functions);

	if(!matching_functions.empty())
	{
		//vector<FunctionDefinitionRef>& matching_functions = res->second;

	

		assert(matching_functions.size() > 0);

		if(matching_functions.size() > 1)
			throw BaseException("Ambiguous binding for variable '" + this->name + "': multiple functions with name." + errorContext(*this, payload));

		//if(contains(payload.func_def_stack, matching_functions[0].getPointer()))
		//	throw BaseException("Variable refer to current function definition." + errorContext(*this, payload));


		FunctionDefinition* target_func_def = matching_functions[0].getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_func_def->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_func_def->order_num) && !target_func_def->isGenericFunction())
		{
			this->vartype = BoundToGlobalDefVariable;
			this->bound_function = target_func_def;

			// As the target function is being passed as an argument, we need a closure version of it.
			target_func_def->need_to_emit_captured_var_struct_version = true;

			return;
		}
	}

	// Try and bind to a named constant.
	//Frame::NamedConstantMap::iterator name_res = payload.top_lvl_frame->named_constant_map.find(this->name);
	//if(name_res != payload.top_lvl_frame->named_constant_map.end())
	Frame::NamedConstantMap::iterator name_res = payload.linker->named_constant_map.find(this->name);
	if(name_res != payload.linker->named_constant_map.end())
	{
		//if(payload.current_named_constant)
		//{
			const NamedConstant* target_named_constant = name_res->second.getPointer();

			// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
			if((!payload.current_named_constant || target_named_constant->order_num < payload.current_named_constant->order_num) &&
				isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_named_constant->order_num))
			{
				this->vartype = BoundToNamedConstant;
				this->bound_named_constant = name_res->second.getPointer();
				return;
			}
		//}


		/*if(payload.current_named_constant)
		{
			const int current_named_constant_src_pos = payload.current_named_constant->srcLocation().char_index;
			const int target_named_constant_src_pos = name_res->second->srcLocation().char_index;

			// Only bind to a named constant defined earlier in the file.
			// NOTE: kind of an abuse of src location here.
			if(target_named_constant_src_pos < current_named_constant_src_pos)
			{
				this->vartype = BoundToNamedConstant;
				this->bound_named_constant = name_res->second.getPointer();
				return;
			}
		}
		else
		{
			this->vartype = BoundToNamedConstant;
			this->bound_named_constant = name_res->second.getPointer();
			return;
		}*/

		// Don't try to bind to the named constant we are in the value expression for.
		//if(payload.named_constant_stack.empty() || (payload.named_constant_stack[0] != name_res->second.getPointer()))
		/*if(payload.current_named_constant != name_res->second.getPointer())
		{
			this->vartype = BoundToNamedConstant;
			this->bound_named_constant = name_res->second.getPointer();
			return;
		}*/
	}


	throw BaseException("No such function, function argument, named constant or let definition '" + this->name + "'." + 
		errorContext(*this, payload));
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
		this->bindVariables(payload, stack);
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		assert(this->vartype != UnboundVariable);
		if(this->vartype == UnboundVariable)
			BaseException("No such function, function argument, named constant or let definition '" + this->name + "'." + errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		if(this->vartype == LetVariable)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_let_node->expr, payload);
			//this->can_constant_fold = a->can_constant_fold && b->can_constant_fold && expressionIsWellTyped(*this, payload);
			//const bool a_is_literal = checkFoldExpression(a, payload);
			//const bool b_is_literal = checkFoldExpression(b, payload);
			
			this->can_maybe_constant_fold = let_val_is_literal;
		}
		else if(this->vartype == BoundToNamedConstant)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_named_constant->value_expr, payload);
			this->can_maybe_constant_fold = let_val_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::UpdateUpRefs)
	{
		switch(vartype)
		{
		case UnboundVariable:
			break;
		case ArgumentVariable:
			{
				ASTNode* updated_node = payload.clone_map[bound_function];
				if(updated_node)
					bound_function = (FunctionDefinition*)updated_node;
				break;
			}
		case BoundToNamedConstant:
			{
				break;
			}
		case LetVariable:
			{
				ASTNode* updated_node = payload.clone_map[bound_let_node];
				if(updated_node)
					bound_let_node = (LetASTNode*)updated_node;
				break;
			}
		case CapturedVariable:
			{
				ASTNode* updated_node = payload.clone_map[bound_function];
				if(updated_node)
					bound_function = (FunctionDefinition*)updated_node;
				break;
			}
		}
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		if(this->vartype == CapturedVariable)
		{
			CapturedVar& captured_var = this->bound_function->captured_vars[this->bound_index];
			if(captured_var.vartype == CapturedVar::Arg)
			{
				if(captured_var.bound_function == payload.func_args_to_sub)
				{

				}
			}
		}
		/*for(size_t i=0; i<this->captured_vars.size(); ++i)
		{
			if(captured_vars[i].vartype == CapturedVar::Arg)
			{
				// We need to change the captured var to capture the expression that is the argument value.
				//captured_vars[i].substituted_expr = payload.variable_substitutes[i]->clone();
				captured_vars[i].substituted_expr = cloneASTNodeSubtree(payload.variable_substitutes[i]);
			}
		}*/
	}
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		// If this variable refers to a global function, then we will consider the global function reachable from this function.
		// This is conservative.
		if(this->vartype == BoundToGlobalDefVariable)
		{
			payload.reachable_defs.insert(this->bound_function);
			if(payload.processed_defs.find(this->bound_function) == payload.processed_defs.end()) // If has not been processed yet:
				payload.defs_to_process.push_back(this->bound_function);
		}
		else if(this->vartype == BoundToNamedConstant)
		{
			payload.reachable_defs.insert(this->bound_named_constant);
			if(payload.processed_defs.find(this->bound_named_constant) == payload.processed_defs.end()) // If has not been processed yet:
				payload.defs_to_process.push_back(this->bound_named_constant);
		}
	}
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(this->vartype == ArgumentVariable)
	{
		if(vmstate.func_args_start.back() + bound_index >= vmstate.argument_stack.size())
			throw BaseException("out of bounds");

		return vmstate.argument_stack[vmstate.func_args_start.back() + bound_index];
	}
	else if(this->vartype == LetVariable)
	{
		// Instead of computing the values and placing on let stack, let's just execute the let expressions directly.

		//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];
		//return vmstate.let_stack[let_stack_start + this->bound_index];
		LetASTNode* let_node = this->bound_let_node;
		ValueRef val = let_node->exec(vmstate);
		if(let_node->vars.size() == 1)
			return val;
		else
		{
			// Destructuring assignment, return the particular element from the tuple.
			const TupleValue* t = checkedCast<TupleValue>(val.getPointer());
			return t->e[this->let_var_index];
		}
	}
	else if(this->vartype == BoundToGlobalDefVariable)
	{
		StructureValueRef captured_vars = new StructureValue(vector<ValueRef>());
		return new FunctionValue(this->bound_function, captured_vars);
	}
	else if(this->vartype == BoundToNamedConstant)
	{
		return bound_named_constant->exec(vmstate);
	}
	else if(this->vartype == CapturedVariable)
	{
		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		ValueRef captured_struct = vmstate.argument_stack.back();
		const StructureValue* s = checkedCast<StructureValue>(captured_struct.getPointer());

		return s->fields[this->bound_index];
	}
	else
	{
		assert(!"invalid vartype.");
		return NULL;
	}
}


TypeRef Variable::type() const
{
	if(this->vartype == LetVariable)
	{
		const TypeRef let_var_type = this->bound_let_node->type();
		if(this->bound_let_node->vars.size() == 1)
			return let_var_type;
		else // Else if destructuring assignment:
		{
			if(let_var_type.isNull() || (let_var_type->getType() != Type::TupleTypeType))
				return NULL;
			const TupleType* tuple_type = let_var_type.downcastToPtr<TupleType>();
			if(this->let_var_index >= 0 && this->let_var_index < (int)tuple_type->component_types.size()) // If in bounds:
				return tuple_type->component_types[this->let_var_index];
			else
				return NULL;
		}
	}
	else if(this->vartype == ArgumentVariable)
		return this->bound_function->args[this->bound_index].type;
	else if(this->vartype == BoundToGlobalDefVariable)
		return this->bound_function->type();
	else if(this->vartype == BoundToNamedConstant)
		return this->bound_named_constant->type();
	else if(this->vartype == CapturedVariable)
	{
		FunctionDefinition* def = this->bound_function;

		return def->captured_vars[this->bound_index].type();
	}
	else
	{
		//assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);

	switch(vartype)
	{
	case UnboundVariable:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), unbound\n";
		break;
	case LetVariable:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), bound to let node: " << toHexString((uint64)this->bound_let_node) + ", let_var_index=" << let_var_index << "\n";
		break;
	case ArgumentVariable:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), bound to arg, function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + "), index=" << bound_index << "\n";
		break;
	case BoundToGlobalDefVariable:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), bound to global function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + ")" << "\n";
		break;
	case BoundToNamedConstant:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), bound to named constant: " << toHexString((uint64)this->bound_named_constant) << "\n";
		break;
	case CapturedVariable:
		s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), captured, function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + "), index: " << this->bound_index << "\n";
		break;
	};
}


std::string Variable::sourceString() const
{
	return this->name;
}


std::string Variable::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	return this->name;
}


llvm::Value* Variable::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(vartype == LetVariable)
	{
		//return this->bound_let_block->getLetExpressionLLVMValue(params, this->bound_index, ret_space_ptr);
		//TEMP:
		assert(params.let_values.find(this->bound_let_node) != params.let_values.end());

		llvm::Value* value = params.let_values[this->bound_let_node];

		LetASTNode* let_node = this->bound_let_node;
		if(let_node->vars.size() == 1)
		{
			// Increment reference count
			if(params.emit_refcounting_code)
				this->type()->emitIncrRefCount(params, value, "Variable::emitLLVMCode for let var " + this->name);

			return value;
		}
		else
		{
			// destructuring assignment, we just want to return the individual tuple element.
			// Value should be a pointer to a tuple struct.
			if(type()->passByValue())
			{
				llvm::Value* tuple_elem = params.builder->CreateLoad(params.builder->CreateStructGEP(value, this->let_var_index, "tuple_elem_ptr"));

				// Increment reference count
				if(params.emit_refcounting_code)
					this->type()->emitIncrRefCount(params, tuple_elem, "Variable::emitLLVMCode for let var " + this->name);

				return tuple_elem;
			}
			else
			{
				llvm::Value* tuple_elem = params.builder->CreateStructGEP(value, this->let_var_index, "tuple_elem_ptr");
				//tuple_elem->dump();
				//tuple_elem->getType()->dump();

				// Increment reference count
				if(params.emit_refcounting_code)
					this->type()->emitIncrRefCount(params, tuple_elem, "Variable::emitLLVMCode for let var " + this->name);

				return tuple_elem;
			}
		}
	}
	else if(vartype == ArgumentVariable)
	{
		assert(this->bound_function);

		// See if we should use the overriden argument values (used for function specialisation in array fold etc..)
		if(!params.argument_values.empty())
			return params.argument_values[this->bound_index];

		//if(shouldPassByValue(*this->type()))
		//{
			// If the current function returns its result via pointer, then all args are offset by one.
			//if(params.currently_building_func_def->returnType()->passByValue())
			//	return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index);
			//else
			//	return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index + 1);

		llvm::Value* arg = LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getLLVMArgIndex(this->bound_index)
		);

		// Increment reference count
		//if(*params.currently_building_func_def->returnType() == *this->type()) // Ref-counting optimisation: Only do ref counting for this argument value if it is of the enclosing function return type.
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, arg, "Variable::emitLLVMCode for argument var " + this->name);

		return arg;

		/*if(ret_space_ptr)
		{
			assert(!this->type()->passByValue());

			llvm::Value* size;
			if(this->type()->getType() == Type::ArrayTypeType)
			{
				size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(float) * 4, true)); // TEMP HACK
				//size = //this->type().downcast<ArrayType>()->t->LLVMType(*params.context)->getPrimitiveSizeInBits() * 8;
			}
			else
			{
				assert(0);
			}
			// Need to copy the value from the mem at arg to the mem at ret_space_ptr
			params.builder->CreateMemCpy(
				ret_space_ptr, // dest
				arg, // src
				size, // size
				4 // align
			);
		}

		return ret_space_ptr;*/

		/*}
		else
		{
			return params.builder->CreateLoad(
				LLVMTypeUtils::getNthArg(params.currently_building_func, this->argument_index),
				false, // true,// TEMP: volatile = true to pick up returned vector);
				"argument" // name
			);

		}*/
	}
	else if(vartype == BoundToGlobalDefVariable)
	{
		return this->bound_function->emitLLVMCode(params, ret_space_ptr);
	}
	else if(vartype == BoundToNamedConstant)
	{
		return this->bound_named_constant->emitLLVMCode(params, ret_space_ptr);
	}
	else if(vartype == CapturedVariable)
	{
		// Get pointer to captured variables. structure.
		// This pointer will be passed after the normal arguments to the function.

		llvm::Value* base_cap_var_structure = LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getCapturedVarStructLLVMArgIndex()
		);

		//std::cout << "base_cap_var_structure: " << std::endl;
		//base_cap_var_structure->dump();
		//std::cout << std::endl;
		

		llvm::Type* full_cap_var_type = LLVMTypeUtils::pointerType(
			*params.currently_building_func_def->getCapturedVariablesStructType()->LLVMType(*params.module)
		);

		//std::cout << "full_cap_var_type: " << std::endl;
		//full_cap_var_type->dump();
		//std::cout << std::endl;

		llvm::Value* cap_var_structure = params.builder->CreateBitCast(
			base_cap_var_structure,
			full_cap_var_type, // destination type
			"cap_var_structure" // name
		);

		// Load the value from the correct field.
		llvm::Value* field_ptr = params.builder->CreateStructGEP(cap_var_structure, this->bound_index);

		llvm::Value* field = params.builder->CreateLoad(field_ptr);

		// Increment reference count
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, field, "Variable::emitLLVMCode for captured var " + this->name);

		return field;
	}
	else
	{
		assert(!"invalid vartype");
		return NULL;
	}
}


Reference<ASTNode> Variable::clone(CloneMapType& clone_map)
{
	Variable* v = new Variable(name, srcLocation());
	v->vartype = vartype;
	v->bound_function = bound_function;
	v->bound_let_node = bound_let_node;
	v->bound_named_constant = bound_named_constant;
	v->bound_index = bound_index;
	//v->let_frame_offset = let_frame_offset;
	//v->uncaptured_bound_index = uncaptured_bound_index;
	v->let_var_index = let_var_index;

	clone_map.insert(std::make_pair(this, v));
	return v;
}


bool Variable::isConstant() const
{
	switch(vartype)
	{
	case UnboundVariable:
		return false;
	case ArgumentVariable:
		return false;
	case BoundToNamedConstant:
		{
			return bound_named_constant->isConstant();
		}
	case LetVariable:
		{
			return this->bound_let_node->isConstant();
		}
	default:
		return false;
	}
}


} // end namespace Winter
