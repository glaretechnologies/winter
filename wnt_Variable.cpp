/*=====================================================================
wnt_Variable.cpp
----------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_Variable.h"


#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_LetASTNode.h"
#include "wnt_LetBlock.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMUtils.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "maths/mathstypes.h"
#include "utils/StringUtils.h"
#include "utils/ConPrint.h"
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


using std::vector;
using std::string;


namespace Winter
{


Variable::Variable(const std::string& name_, const SrcLocation& loc)
:	ASTNode(VariableASTNodeType, loc),
	binding_type(BindingType_Unbound),
	name(name_),
	bound_function(NULL),
	bound_let_node(NULL),
	bound_named_constant(NULL),
	arg_index(-1),
	let_var_index(-1)
{
	this->can_maybe_constant_fold = false;
}


Variable::~Variable()
{
	// For any lambdas, erase this variable from the lambdas's free variable set, as we don't want dangling pointers.
	for(auto it = lambdas.begin(); it != lambdas.end(); ++it)
	{
		(*it)->free_variables.erase(this);
	}
}


// Are the variables 'a' and 'b' bound to the same AST node?
bool Variable::boundToSameNode(const Variable& a, const Variable& b)
{
	if(a.binding_type != b.binding_type)
		return false;

	switch(a.binding_type)
	{
	case BindingType_Unbound:
		return false;
	case BindingType_Let:
		return a.bound_let_node == b.bound_let_node && a.let_var_index == b.let_var_index;
	case BindingType_Argument:
		return a.bound_function == b.bound_function && a.arg_index == b.arg_index;
	case BindingType_GlobalDef:
		return a.bound_function == b.bound_function;
	case BindingType_NamedConstant:
		return a.bound_named_constant == b.bound_named_constant;
	};
	assert(0);
	return false;
}


/*
inline static const std::string varType(Variable::BindingType t)
{
	if(t == Variable::BindingType_Unbound)
		return "Unbound";
	else if(t == Variable::BindingType_Let)
		return "Let";
	else if(t == Variable::BindingType_Argument)
		return "Arg";
	else if(t == Variable::BindingType_GlobalDef)
		return "BoundToGlobalDef";
	else if(t == Variable::BindingType_NamedConstant)
		return "BoundToNamedConstant";
	else
	{
		assert(!"invalid var type");
		return "";
	}
}
*/


// Results of walking up the node tree to try and bind the variable.
struct BindInfo
{
	BindInfo() : bound_function(NULL), bound_let_node(NULL), arg_index(-1), let_var_index(-1) {}

	Variable::BindingType vartype;

	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetASTNode* bound_let_node;
	std::vector<FunctionDefinition*> enclosing_lambdas; // Most tightly enclosing lambda at rightmost index (back())

	int arg_index;
	int let_var_index; // Index of the let variable bound to, for destructing assignment case may be > 0.
};


// Walk up the AST, trying to find something to bind to.
// s = current stack level.
static BindInfo doBind(const std::vector<ASTNode*>& stack, int s, const std::string& name, Variable* var)
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
					bindinfo.vartype = Variable::BindingType_Argument;
					bindinfo.arg_index = i;
					bindinfo.bound_function = def;
					//std::cout << "Bound '" + name + "' to function arg, bound_index = " << bindinfo.bound_index << ", def = " << def->sig.toString() << std::endl;
					return bindinfo;
				}

			if(s >= 1) // If this function is not at the top of the stack, it must be an anonymous function definition (lambda expression)
			{
				assert(def->is_anon_func);

				// We have reached a lambda expression, and we have not bound the variable yet.
				// This means that the target of the current variable we are trying to bind must lie in the local environment, e.g. this is a free var.
				// So the variable we are trying to bind will be bound to capture result.  Now we need to determine what the capture result binds to.
				BindInfo bindinfo = doBind(stack, s - 1, name, var); // Start bounding process outside the lambda definition

				if(bindinfo.vartype == Variable::BindingType_Unbound) // If binding failed:
					return bindinfo;

				// We have found something to bind to in the lexical environment outside of the lambda definition.

				def->free_variables.insert(var); // Add this variable to the list of free vars for this lambda expression
				var->lambdas.insert(def);
				
				bindinfo.enclosing_lambdas.push_back(def);
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
				if((s + 1 < (int)stack.size()) && (stack[s+1]->nodeType() == ASTNode::LetType) && (let_block->lets[i].getPointer() == stack[s+1]))
				{
					// We have reached the let expression for the current variable we are tring to bind.

					// It's an error to have code like "let x = x + 1"
					for(size_t v=0; v<let_block->lets[i]->vars.size(); ++v)
						if(let_block->lets[i]->vars[v].name == name)
							throw ExceptionWithPosition("Variable '" + name + "' is in a let expression with the same name", errorContext(var));

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
							bindinfo.vartype = Variable::BindingType_Let;
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
	info.vartype = Variable::BindingType_Unbound;
	return info;
}


void Variable::bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack)
{
	// Don't try and do the binding process again if already bound.
	if(this->binding_type != BindingType_Unbound)
		return;

	try
	{
		BindInfo bindinfo = doBind(stack, (int)stack.size() - 1, name, this);
		if(bindinfo.vartype != BindingType_Unbound)
		{
			this->binding_type = bindinfo.vartype;
			this->arg_index = bindinfo.arg_index;
			this->let_var_index = bindinfo.let_var_index;
			this->bound_function = bindinfo.bound_function;
			this->bound_let_node = bindinfo.bound_let_node;
			this->enclosing_lambdas = bindinfo.enclosing_lambdas;
			return;
		}
	}
	catch(BaseException& e)
	{
		throw ExceptionWithPosition(e.what(), errorContext(*this, payload));
	}

	// Try and bind to a top level function definition
	vector<FunctionDefinitionRef> matching_functions;
	payload.linker->getFuncsWithMatchingName(this->name, matching_functions);

	if(!matching_functions.empty())
	{
		assert(matching_functions.size() > 0);

		if(matching_functions.size() > 1)
			throw ExceptionWithPosition("Ambiguous binding for variable '" + this->name + "': multiple functions with name.", errorContext(*this, payload));

		FunctionDefinition* target_func_def = matching_functions[0].getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_func_def->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_func_def->order_num) && !target_func_def->isGenericFunction())
		{
			this->binding_type = BindingType_GlobalDef;
			this->bound_function = target_func_def;

			// As the target function is being passed as an argument, we need a closure version of it.
			target_func_def->need_to_emit_captured_var_struct_version = true;
			return;
		}
	}

	// Try and bind to a named constant.
	Linker::NamedConstantMap::iterator name_res = payload.linker->named_constant_map.find(this->name);
	if(name_res != payload.linker->named_constant_map.end())
	{
		const NamedConstant* target_named_constant = name_res->second.getPointer();

		// Only bind to a named constant defined earlier, and only bind to a named constant earlier than all functions we are defining.
		if((!payload.current_named_constant || target_named_constant->order_num < payload.current_named_constant->order_num) &&
			isTargetDefinedBeforeAllInStack(payload.func_def_stack, target_named_constant->order_num))
		{
			this->binding_type = BindingType_NamedConstant;
			this->bound_named_constant = name_res->second.getPointer();
			return;
		}
	}

	throw ExceptionWithPosition("No such function, function argument, named constant or let definition '" + this->name + "'.", errorContext(*this, payload));
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
		this->bindVariables(payload, stack);
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		assert(this->binding_type != BindingType_Unbound);
		if(this->binding_type == BindingType_Unbound)
			throw ExceptionWithPosition("No such function, function argument, named constant or let definition '" + this->name + "'.", errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		if(this->binding_type == BindingType_Let)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_let_node->expr, payload, stack);
			
			this->can_maybe_constant_fold = let_val_is_literal;
		}
		else if(this->binding_type == BindingType_NamedConstant)
		{
			const bool let_val_is_literal = checkFoldExpression(this->bound_named_constant->value_expr, payload, stack);
			this->can_maybe_constant_fold = let_val_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::UpdateUpRefs)
	{
		// When cloning a subtree of nodes, we will need to update upwards pointers to point into the new subtree.
		switch(binding_type)
		{
		case BindingType_Unbound:
			break;
		case BindingType_Argument:
			{
				ASTNode* updated_node = payload.clone_map[bound_function];
				if(updated_node)
					bound_function = (FunctionDefinition*)updated_node;
				break;
			}
		case BindingType_GlobalDef:
			break;
		case BindingType_NamedConstant:
			break;
		case BindingType_Let:
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
		if(this->binding_type == BindingType_GlobalDef)
		{
			payload.reachable_nodes.insert(this->bound_function);
			if(payload.processed_nodes.find(this->bound_function) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_function);
		}
		else if(this->binding_type == BindingType_NamedConstant) // Similarly for named constants.
		{
			payload.reachable_nodes.insert(this->bound_named_constant);
			if(payload.processed_nodes.find(this->bound_named_constant) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_named_constant);
		}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_ComputeAlive)
	{
		if(binding_type == BindingType_Let)
		{
			//std::cout << "Marking var " << this->name << " as alive." << std::endl;
			payload.reachable_nodes.insert(this->bound_let_node); // Mark as alive
			if(payload.processed_nodes.find(this->bound_let_node) == payload.processed_nodes.end()) // If has not been processed yet:
				payload.nodes_to_process.push_back(this->bound_let_node); // Add to to-process list
		}
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		if(this->binding_type == Variable::BindingType_Argument && this->bound_function == payload.func_args_to_sub)
		{
			// Replace the variable with the argument value.	
			if(this->arg_index >= (int)payload.variable_substitutes.size())
				return; // May be out of bounds for invalid programs.
			ASTNodeRef new_expr = cloneASTNodeSubtree(payload.variable_substitutes[this->arg_index]);

			payload.tree_changed = true;
			payload.garbage.push_back(this); // Store a ref in payload so this node won't get deleted while we are still executing this function.
			if(stack[stack.size() - 1] == this)
			{
				stack[stack.size() - 2]->updateChild(this, new_expr); // Tell the parent of this node to set the new expression as the relevant child.
				stack[stack.size() - 1] = new_expr.ptr();
			}
			else
				stack[stack.size() - 1]->updateChild(this, new_expr); // Tell the parent of this node to set the new expression as the relevant child.
		}

		if(binding_type == BindingType_Let)
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
		if(this->binding_type == BindingType_GlobalDef || this->binding_type == BindingType_NamedConstant)
		{
			// These bindings shouldn't change, so just leave them
		}
		else
		{
			// Set the vartype to unbound so that it can be rebound
			this->binding_type = BindingType_Unbound;
			this->enclosing_lambdas.clear();
			// UnboundVariable pass should clear enclosing_lambda->free_variables set also so no need to remove here.
		}
	}
	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
	}
	else if(payload.operation == TraversalPayload::CountArgumentRefs)
	{
		if(this->binding_type == BindingType_Argument)
		{
			bound_function->args[arg_index].ref_count++;
		}
	}
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(!this->enclosing_lambdas.empty())
	{
		// Get from closure

		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		if(vmstate.argument_stack.empty())
			throw ExceptionWithPosition("out of bounds", errorContext(this));
		ValueRef captured_struct = vmstate.argument_stack.back();
		const StructureValue* s = checkedCast<StructureValue>(captured_struct.getPointer());

		const size_t free_index = enclosing_lambdas.back()->getFreeIndexForVar(this);

		return s->fields[free_index];
	}


	if(this->binding_type == BindingType_Argument)
	{
		if(vmstate.func_args_start.empty() || (vmstate.func_args_start.back() + arg_index >= vmstate.argument_stack.size()))
			throw ExceptionWithPosition("out of bounds", errorContext(this));

		return vmstate.argument_stack[vmstate.func_args_start.back() + arg_index];
	}
	else if(this->binding_type == BindingType_Let)
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
	else if(this->binding_type == BindingType_GlobalDef)
	{
		StructureValueRef captured_vars = new StructureValue(vector<ValueRef>());
		return new FunctionValue(this->bound_function, captured_vars);
	}
	else if(this->binding_type == BindingType_NamedConstant)
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
	if(this->binding_type == BindingType_Let)
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
	else if(this->binding_type == BindingType_Argument)
		return this->bound_function->args[this->arg_index].type;
	else if(this->binding_type == BindingType_GlobalDef)
		return this->bound_function->type();
	else if(this->binding_type == BindingType_NamedConstant)
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

	s << "Var '" << this->name << "' (" + toHexString((uint64)this) + "), free: " << boolToString(!this->enclosing_lambdas.empty()) << ", ";

	switch(binding_type)
	{
	case BindingType_Unbound:
		s << "unbound\n";
		break;
	case BindingType_Let:
		s << "bound to let node: " << toHexString((uint64)this->bound_let_node) + ", let_var_index=" << let_var_index << "\n";
		break;
	case BindingType_Argument:
		s << "bound to arg, function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + "), index=" << arg_index << "\n";
		break;
	case BindingType_GlobalDef:
		s << "bound to global function: " << toHexString((uint64)this->bound_function) + " (" + this->bound_function->sig.name + ")" << "\n";
		break;
	case BindingType_NamedConstant:
		s << "bound to named constant: " << toHexString((uint64)this->bound_named_constant) << "\n";
		break;
	};
}


std::string Variable::sourceString(int depth) const
{
	return this->name;
}


std::string Variable::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	if(!this->enclosing_lambdas.empty()) // If this is a free var:
		return "cap_var_struct->captured_var_" + toString((uint32)this->enclosing_lambdas.back()->getFreeIndexForVar(this));

	return mapOpenCLCVarName(params.opencl_c_keywords, this->name);
}


llvm::Value* Variable::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	if(!this->enclosing_lambdas.empty()) // If this variable is free inside a lambda:
	{
		// Get pointer to captured variables. structure.
		// This pointer will be passed after the normal arguments to the function.

		llvm::Value* base_cap_var_structure = LLVMUtils::getNthArg(
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
		const size_t free_index = enclosing_lambdas.back()->getFreeIndexForVar(this);

		llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, cap_var_structure, (unsigned int)free_index);

		// For pass-by-pointer types like struct, all we need is the pointer to the value (which is stored in the captured var struct),
		// so the GEP pointer is sufficient.
		llvm::Value* field = this->type()->passByValue() ?
			params.builder->CreateLoad(field_ptr) :
			field_ptr;

		// Increment reference count
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, field, "Variable::emitLLVMCode for captured var " + this->name);

		return field;
	}

	if(binding_type == BindingType_Let)
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
				llvm::Value* tuple_elem = params.builder->CreateLoad(LLVMUtils::createStructGEP(params.builder, value, this->let_var_index, "tuple_elem_ptr"));

				// Increment reference count
				if(params.emit_refcounting_code)
					this->type()->emitIncrRefCount(params, tuple_elem, "Variable::emitLLVMCode for let var " + this->name);

				return tuple_elem;
			}
			else
			{
				llvm::Value* tuple_elem = LLVMUtils::createStructGEP(params.builder, value, this->let_var_index, "tuple_elem_ptr");

				// Increment reference count
				if(params.emit_refcounting_code)
					this->type()->emitIncrRefCount(params, tuple_elem, "Variable::emitLLVMCode for let var " + this->name);

				return tuple_elem;
			}
		}
	}
	else if(binding_type == BindingType_Argument)
	{
		assert(this->bound_function);

		// See if we should use the overriden argument values (used for function specialisation in array fold etc..)
		if(!params.argument_values.empty())
			return params.argument_values[this->arg_index];

		llvm::Value* arg = LLVMUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getLLVMArgIndex(this->arg_index)
		);

		// Increment reference count
		//if(*params.currently_building_func_def->returnType() == *this->type()) // Ref-counting optimisation: Only do ref counting for this argument value if it is of the enclosing function return type.
		if(params.emit_refcounting_code && shouldRefCount(params, *this))
			this->type()->emitIncrRefCount(params, arg, "Variable::emitLLVMCode for argument var " + this->name);

		return arg;
	}
	else if(binding_type == BindingType_GlobalDef)
	{
		return this->bound_function->emitLLVMCode(params, ret_space_ptr);
	}
	else if(binding_type == BindingType_NamedConstant)
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
	v->enclosing_lambdas = enclosing_lambdas;
	v->let_var_index = let_var_index;

	clone_map.insert(std::make_pair(this, v));
	return v;
}


bool Variable::isConstant() const
{
	switch(binding_type)
	{
	case BindingType_Unbound:
		return false;
	case BindingType_Argument:
		return false;
	case BindingType_NamedConstant:
		{
			return bound_named_constant->isConstant();
		}
	case BindingType_Let:
		{
			return this->bound_let_node->isConstant();
		}
	default:
		return false;
	}
}


size_t Variable::getTimeBound(GetTimeBoundParams& params) const
{
	// A variable just refers to some existing value, so doesn't really take any time to compute.
	return 1;
}


GetSpaceBoundResults Variable::getSpaceBound(GetSpaceBoundParams& params) const
{
	// A variable just refers to some existing value, so doesn't really take any space.
	return GetSpaceBoundResults(0, 0);
}


size_t Variable::getSubtreeCodeComplexity() const
{
	return 1;
}


} // end namespace Winter
