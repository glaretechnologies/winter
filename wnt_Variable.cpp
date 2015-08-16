/*=====================================================================
wnt_Variable.cpp
----------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_Variable.h"


#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "maths/mathstypes.h"
#include "utils/StringUtils.h"
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
	binding_type(UnboundVariable),
	name(name_),
	bound_function(NULL),
	bound_let_node(NULL),
	bound_named_constant(NULL),
	arg_index(-1),
	let_var_index(-1),
	free_index(-1),
	enclosing_lambda(NULL)
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
	else
	{
		assert(!"invalid var type");
		return "";
	}
}


struct BindInfo
{
	BindInfo() : root_bound_node(NULL), bound_function(NULL), bound_let_node(NULL), enclosing_lambda(NULL), free_index(-1) {}

	Variable::BindingType vartype;

	ASTNode* root_bound_node; // Node furthest up the node stack that we are bound to, if the variable is bound through one or more captured vars.
	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetASTNode* bound_let_node;
	FunctionDefinition* enclosing_lambda;

	int arg_index;
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.
	int free_index;
};


// Walk up the AST, trying to find something to bind to.
// s = current stack level.
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
					bindinfo.arg_index = i;
					bindinfo.bound_function = def;
					bindinfo.root_bound_node = def;
					//std::cout << "Bound '" + name + "' to function arg, bound_index = " << bindinfo.bound_index << ", def = " << def->sig.toString() << std::endl;
					return bindinfo;
				}

			if(s >= 1)
			{
				assert(def->is_anon_func);

				// We have reached a lambda expression.
				// This means that the target of the current variable we are trying to bind must lie in the local environment, e.g. this is a free var.
				// So the variable we are trying to bind will be bound to capture result.  Now we need to determine what the capture result binds to.
				BindInfo bindinfo = doBind(stack, s - 1, name);

				if(bindinfo.vartype == Variable::UnboundVariable) // If binding failed:
					return bindinfo;

				// Add this variable to the list of captured vars for this function
				CapturedVar captured_var;
				if(bindinfo.vartype == Variable::ArgumentVariable)
					captured_var.vartype = CapturedVar::Arg;
				else if(bindinfo.vartype == Variable::LetVariable)
					captured_var.vartype = CapturedVar::Let;
				else
				{
					assert(0);
				}
				if(bindinfo.free_index >= 0)
				{
					captured_var.bound_function = bindinfo.enclosing_lambda;
					captured_var.vartype = CapturedVar::Captured;
				}


				captured_var.arg_index = bindinfo.arg_index;
				captured_var.let_var_index = bindinfo.let_var_index;
				captured_var.bound_let_node = bindinfo.bound_let_node;
				captured_var.bound_function = bindinfo.bound_function;
				captured_var.enclosing_lambda = bindinfo.enclosing_lambda;
				captured_var.free_index = bindinfo.free_index;

				def->captured_vars.push_back(captured_var);
				
				bindinfo.free_index = (int)def->captured_vars.size() - 1;
				bindinfo.enclosing_lambda = def;
				return bindinfo;
			}
		}
		else if(stack[s]->nodeType() == ASTNode::LetBlockType)
		{
			LetBlock* let_block = static_cast<LetBlock*>(stack[s]);
			
			for(unsigned int i=0; i<let_block->lets.size(); ++i) // For each let node in the block:
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

				// If the stack entry at the next level down is a let AST node, and the current variable lies is in the value expression for it:
				if((s + 1 < stack.size()) && (stack[s+1]->nodeType() == ASTNode::LetType) && (let_block->lets[i].getPointer() == stack[s+1]))
				{
					// We have reached the let expression for the current variable we are tring to bind.

					// It's an error to have code like "let x = x + 1"
					for(size_t v=0; v<let_block->lets[i]->vars.size(); ++v)
						if(let_block->lets[i]->vars[v].name == name)
							throw BaseException("Variable '" + name + "' is in a let expression with the same name");

					// Don't try and bind with let variables equal to or past this one.
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
							bindinfo.arg_index = -1;
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
	if(this->binding_type != UnboundVariable)
		return;

	try
	{
		BindInfo bindinfo = doBind(stack, (int)stack.size() - 1, name);
		if(bindinfo.vartype != UnboundVariable)
		{
			this->binding_type = bindinfo.vartype;
			this->arg_index = bindinfo.arg_index;
			this->let_var_index = bindinfo.let_var_index;
			this->bound_function = bindinfo.bound_function;
			this->bound_let_node = bindinfo.bound_let_node;
			this->enclosing_lambda = bindinfo.enclosing_lambda;
			this->free_index = bindinfo.free_index;
			return;
		}
	}
	catch(BaseException& e)
	{
		throw BaseException(e.what() + errorContext(*this, payload));
	}

	// Try and bind to a top level function definition
	vector<FunctionDefinitionRef> matching_functions;
	payload.linker->getFuncsWithMatchingName(this->name, matching_functions);

	if(!matching_functions.empty())
	{
		assert(matching_functions.size() > 0);

		if(matching_functions.size() > 1)
			throw BaseException("Ambiguous binding for variable '" + this->name + "': multiple functions with name." + errorContext(*this, payload));

		FunctionDefinition* target_func_def = matching_functions[0].getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_func_def->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_func_def->order_num) && !target_func_def->isGenericFunction())
		{
			this->binding_type = BoundToGlobalDefVariable;
			this->bound_function = target_func_def;

			// As the target function is being passed as an argument, we need a closure version of it.
			target_func_def->need_to_emit_captured_var_struct_version = true;
			return;
		}
	}

	// Try and bind to a named constant.
	Frame::NamedConstantMap::iterator name_res = payload.linker->named_constant_map.find(this->name);
	if(name_res != payload.linker->named_constant_map.end())
	{
		const NamedConstant* target_named_constant = name_res->second.getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_named_constant->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_named_constant->order_num))
		{
			this->binding_type = BoundToNamedConstant;
			this->bound_named_constant = name_res->second.getPointer();
			return;
		}
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
		assert(this->binding_type != UnboundVariable);
		if(this->binding_type == UnboundVariable)
			BaseException("No such function, function argument, named constant or let definition '" + this->name + "'." + errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		if(this->binding_type == LetVariable)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_let_node->expr, payload);
			
			this->can_maybe_constant_fold = let_val_is_literal;
		}
		else if(this->binding_type == BoundToNamedConstant)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_named_constant->value_expr, payload);
			this->can_maybe_constant_fold = let_val_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::UpdateUpRefs)
	{
		// When cloning a subtree of nodes, we will need to update upwards pointers to point into the new subtree.
		switch(binding_type)
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
		}
	}
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		// If this variable refers to a global function, then we will consider the global function reachable from this function.
		// This is conservative.
		if(this->binding_type == BoundToGlobalDefVariable)
		{
			payload.reachable_nodes.insert(this->bound_function);
			if(payload.processed_nodes.find(this->bound_function) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_function);
		}
		else if(this->binding_type == BoundToNamedConstant) // Similarly for named constants.
		{
			payload.reachable_nodes.insert(this->bound_named_constant);
			if(payload.processed_nodes.find(this->bound_named_constant) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_named_constant);
		}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_ComputeAlive)
	{
		if(binding_type == LetVariable)
		{
			payload.reachable_nodes.insert(this->bound_let_node); // Mark as alive
			if(payload.processed_nodes.find(this->bound_let_node) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_let_node); // Add to to-process list
		}
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		if(binding_type == LetVariable)
		{
			// Handle renaming of let variables in the cloned sub-tree.
			const auto res = payload.new_let_var_name_map.find(std::make_pair(this->bound_let_node, this->let_var_index));
			if(res != payload.new_let_var_name_map.end()) // If there is a new name for this let variable to use:
				this->name = res->second; // Use it
		}
	}
	else if(payload.operation == TraversalPayload::GetAllNamesInScope)
	{
		payload.used_names->insert(this->name);
	}
	else if(payload.operation == TraversalPayload::UnbindVariables)
	{
		if(this->binding_type == BoundToGlobalDefVariable || this->binding_type == BoundToNamedConstant)
		{
			// These bindings shouldn't change, so just leave them
		}
		else
		{
			// Set the vartype to unbound so that it can be rebound
			this->binding_type = UnboundVariable;
		}
	}
	
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(this->free_index >= 0)
	{
		// Get from closure

		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		if(vmstate.argument_stack.empty())
			throw BaseException("out of bounds");
		ValueRef captured_struct = vmstate.argument_stack.back();
		const StructureValue* s = checkedCast<StructureValue>(captured_struct.getPointer());

		return s->fields[this->free_index];
	}


	if(this->binding_type == ArgumentVariable)
	{
		if(vmstate.func_args_start.empty() || (vmstate.func_args_start.back() + arg_index >= vmstate.argument_stack.size()))
			throw BaseException("out of bounds");

		return vmstate.argument_stack[vmstate.func_args_start.back() + arg_index];
	}
	else if(this->binding_type == LetVariable)
	{
		// Instead of computing the values and placing on let stack, let's just execute the let expressions directly.
		// NOTE: this can be very inefficient!

		//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];
		//return vmstate.let_stack[let_stack_start + this->bound_index];

		ValueRef val = this->bound_let_node->exec(vmstate);
		if(this->bound_let_node->vars.size() == 1)
			return val;
		else
		{
			// Destructuring assignment, return the particular element from the tuple.
			const TupleValue* t = checkedCast<TupleValue>(val.getPointer());
			return t->e[this->let_var_index];
		}
	}
	else if(this->binding_type == BoundToGlobalDefVariable)
	{
		StructureValueRef captured_vars = new StructureValue(vector<ValueRef>());
		return new FunctionValue(this->bound_function, captured_vars);
	}
	else if(this->binding_type == BoundToNamedConstant)
	{
		return bound_named_constant->exec(vmstate);
	}
	else
	{
		assert(!"invalid vartype.");
		return NULL;
	}
}


TypeRef Variable::type() const
{
	if(this->binding_type == LetVariable)
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
	else if(this->binding_type == ArgumentVariable)
		return this->bound_function->args[this->arg_index].type;
	else if(this->binding_type == BoundToGlobalDefVariable)
		return this->bound_function->type();
	else if(this->binding_type == BoundToNamedConstant)
		return this->bound_named_constant->type();
	else
	{
		//assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);

	s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), free_index: " << free_index << " ";

	switch(binding_type)
	{
	case UnboundVariable:
		s << "unbound\n";
		break;
	case LetVariable:
		s << "bound to let node: " << toHexString((uint64)this->bound_let_node) + ", let_var_index=" << let_var_index << "\n";
		break;
	case ArgumentVariable:
		s << "bound to arg, function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + "), index=" << arg_index << "\n";
		break;
	case BoundToGlobalDefVariable:
		s << "bound to global function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + ")" << "\n";
		break;
	case BoundToNamedConstant:
		s << "bound to named constant: " << toHexString((uint64)this->bound_named_constant) << "\n";
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
	if(free_index >= 0)
	{
		// Get pointer to captured variables. structure.
		// This pointer will be passed after the normal arguments to the function.

		llvm::Value* base_cap_var_structure = LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getCapturedVarStructLLVMArgIndex()
		);

		llvm::Type* full_cap_var_type = LLVMTypeUtils::pointerType(
			*params.currently_building_func_def->getCapturedVariablesStructType()->LLVMType(*params.module)
		);

		llvm::Value* cap_var_structure = params.builder->CreateBitCast(
			base_cap_var_structure,
			full_cap_var_type, // destination type
			"cap_var_structure" // name
		);

		// Load the value from the correct field.
		llvm::Value* field_ptr = params.builder->CreateStructGEP(cap_var_structure, this->free_index);

		llvm::Value* field = params.builder->CreateLoad(field_ptr);

		// Increment reference count
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, field, "Variable::emitLLVMCode for captured var " + this->name);

		return field;
	}

	if(binding_type == LetVariable)
	{
		assert(params.let_values.find(this->bound_let_node) != params.let_values.end());

		llvm::Value* value = params.let_values[this->bound_let_node];

		if(this->bound_let_node->vars.size() == 1)
		{
			// Increment reference count
			if(params.emit_refcounting_code)
				this->type()->emitIncrRefCount(params, value, "Variable::emitLLVMCode for let var " + this->name);

			return value;
		}
		else
		{
			// Destructuring assignment, we just want to return the individual tuple element.
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

				// Increment reference count
				if(params.emit_refcounting_code)
					this->type()->emitIncrRefCount(params, tuple_elem, "Variable::emitLLVMCode for let var " + this->name);

				return tuple_elem;
			}
		}
	}
	else if(binding_type == ArgumentVariable)
	{
		assert(this->bound_function);

		// See if we should use the overriden argument values (used for function specialisation in array fold etc..)
		if(!params.argument_values.empty())
			return params.argument_values[this->arg_index];

		llvm::Value* arg = LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getLLVMArgIndex(this->arg_index)
		);

		// Increment reference count
		//if(*params.currently_building_func_def->returnType() == *this->type()) // Ref-counting optimisation: Only do ref counting for this argument value if it is of the enclosing function return type.
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, arg, "Variable::emitLLVMCode for argument var " + this->name);

		return arg;
	}
	else if(binding_type == BoundToGlobalDefVariable)
	{
		return this->bound_function->emitLLVMCode(params, ret_space_ptr);
	}
	else if(binding_type == BoundToNamedConstant)
	{
		return this->bound_named_constant->emitLLVMCode(params, ret_space_ptr);
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
	v->binding_type = binding_type;
	v->bound_function = bound_function;
	v->bound_let_node = bound_let_node;
	v->bound_named_constant = bound_named_constant;
	v->arg_index = arg_index;
	v->free_index = free_index;
	v->let_var_index = let_var_index;

	clone_map.insert(std::make_pair(this, v));
	return v;
}


bool Variable::isConstant() const
{
	switch(binding_type)
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
