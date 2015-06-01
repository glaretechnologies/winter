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
	bound_let_block(NULL),
	bound_named_constant(NULL),
	let_var_index(-1)
	//use_captured_var(false),
	//captured_var_index(0)
{
	this->can_maybe_constant_fold = false;
}


void Variable::bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack)
{
	// Don't try and do the binding process again if already bound.
	if(this->vartype != UnboundVariable)
		return;

	bool in_current_func_def = true;
	int use_let_frame_offset = 0;
	for(int s = (int)stack.size() - 1; s >= 0; --s) // Walk up the stack of ancestor nodes
	{
		if(stack[s]->nodeType() == ASTNode::FunctionDefinitionType) // If node is a function definition:
		{
			FunctionDefinition* def = static_cast<FunctionDefinition*>(stack[s]);

			for(unsigned int i=0; i<def->args.size(); ++i) // For each argument to the function:
				if(def->args[i].name == this->name) // If the argument name matches this variable name:
				{
					if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
					{
						//this->captured_var_index = payload.captured_vars.size();
						//this->use_captured_var = true;
						this->vartype = CapturedVariable;
						this->bound_index = (int)payload.func_def_stack.back()->captured_vars.size(); // payload.captured_vars.size();

						// Save info to get bound function argument, so we can query it for the type of the captured var.
						this->bound_function = def;
						this->uncaptured_bound_index = i;

						// Add this function argument as a variable that has to be captured for closures.
						CapturedVar var;
						var.vartype = CapturedVar::Arg;
						var.bound_function = def;
						var.index = i;
						//payload.captured_vars.push_back(var);
						payload.func_def_stack.back()->captured_vars.push_back(var);
					}
					else
					{
						// Bind this variable to the argument.
						this->vartype = ArgumentVariable;
						this->bound_index = i;
						this->bound_function = def;
					}

					//def->args[i].ref_count++;

					return;
				}

			in_current_func_def = false;
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
						if(let_block->lets[i]->vars[v].name == this->name)
						{
							if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
							{
								//this->captured_var_index = payload.captured_vars.size();
								//this->use_captured_var = true;
								this->vartype = CapturedVariable;
								this->bound_index = (int)payload.func_def_stack.back()->captured_vars.size(); // payload.captured_vars.size();

								// Save info to get bound let, so we can query it for the type of the captured var.
								this->bound_let_block = let_block;
								this->uncaptured_bound_index = i;
								this->let_var_index = (int)v;

								// Add this function argument as a variable that has to be captured for closures.
								CapturedVar var;
								var.vartype = CapturedVar::Let;
								var.bound_let_block = let_block;
								var.index = i;
								var.let_frame_offset = use_let_frame_offset;
								//payload.captured_vars.push_back(var);
								payload.func_def_stack.back()->captured_vars.push_back(var);
							}
							else
							{
								this->vartype = LetVariable;
								this->bound_let_block = let_block;
								this->bound_index = i;
								this->let_frame_offset = use_let_frame_offset;
								this->let_var_index = (int)v;
							}
		
							return;
						}
					}
				}
			}

			// We only want to count an ancestor let block as an offsetting block if we are not currently in a let clause of it.
			/*bool is_this_let_clause = false;
			for(size_t z=0; z<let_block->lets.size(); ++z)
				if(let_block->lets[z].getPointer() == stack[s+1])
					is_this_let_clause = true;
			if(!is_this_let_clause)*/
				use_let_frame_offset++;
		}
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
			const bool let_val_is_literal = checkFoldExpression(this->bound_let_block->lets[this->bound_index]->expr, payload);
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
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(this->vartype == ArgumentVariable)
	{
		return vmstate.argument_stack[vmstate.func_args_start.back() + bound_index];
	}
	else if(this->vartype == LetVariable)
	{
		// Instead of computing the values and placing on let stack, let's just execute the let expressions directly.

		//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];
		//return vmstate.let_stack[let_stack_start + this->bound_index];
		LetASTNode* let_node = this->bound_let_block->lets[this->bound_index].getPointer();
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
		StructureValueRef captured_vars(new StructureValue(vector<ValueRef>()));
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
		assert(dynamic_cast<StructureValue*>(captured_struct.getPointer()));
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
		const TypeRef let_var_type = this->bound_let_block->lets[this->bound_index]->type();
		if(this->bound_let_block->lets[this->bound_index]->vars.size() == 1)
			return let_var_type;
		else
		{
			if(let_var_type->getType() != Type::TupleTypeType)
				return NULL;
			return let_var_type.downcastToPtr<TupleType>()->component_types[this->let_var_index];
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
		if(this->bound_function != NULL)
			return this->bound_function->args[this->uncaptured_bound_index].type;
		else
			return this->bound_let_block->lets[this->uncaptured_bound_index]->type();
	}
	else
	{
		//assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


inline static const std::string varType(Variable::VariableType t)
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


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Variable, name=" << this->name << ", " + varType(this->vartype) + ", bound_index=" << bound_index << "\n";
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
#if USE_LLVM
	if(vartype == LetVariable)
	{
		//return this->bound_let_block->getLetExpressionLLVMValue(params, this->bound_index, ret_space_ptr);
		//TEMP:
		assert(params.let_block_let_values.find(this->bound_let_block) != params.let_block_let_values.end());

		llvm::Value* value = params.let_block_let_values[this->bound_let_block][this->bound_index];

		LetASTNode* let_node = this->bound_let_block->lets[this->bound_index].getPointer();
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

		return params.builder->CreateLoad(field_ptr);
	}
	else
	{
		assert(!"invalid vartype");
		return NULL;
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> Variable::clone()
{
	Variable* v = new Variable(name, srcLocation());
	v->vartype = vartype;
	v->bound_function = bound_function;
	v->bound_let_block = bound_let_block;
	v->bound_named_constant = bound_named_constant;
	v->bound_index = bound_index;
	v->let_frame_offset = let_frame_offset;
	v->uncaptured_bound_index = uncaptured_bound_index;
	v->let_var_index = let_var_index;
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
			return this->bound_let_block->lets[this->bound_index]->isConstant();
		}
	default:
		return false;
	}
}


} // end namespace Winter
