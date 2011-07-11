/*=====================================================================
FunctionExpression.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-30 18:53:38 +0100
=====================================================================*/
#include "wnt_FunctionExpression.h"


#include "wnt_ASTNode.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "utils/stringutils.h"
#if USE_LLVM
#include "llvm/Type.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CallingConv.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#endif


namespace Winter
{


static const bool VERBOSE_EXEC = false;


FunctionExpression::FunctionExpression(const SrcLocation& src_loc) 
:	ASTNode(src_loc),
	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0)
{
}


FunctionExpression::FunctionExpression(const SrcLocation& src_loc, const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1) // 2-arg function
:	ASTNode(src_loc),
	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0)
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
		FunctionValue* function_value = dynamic_cast<FunctionValue*>(arg.getPointer());
		function_value_out = function_value;
		return function_value->func_def;
	}
	else
	{
		assert(this->binding_type == Let);

		const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];
		ValueRef arg = vmstate.let_stack[let_stack_start + this->bound_index];

		//ValueRef arg = vmstate.let_stack[vmstate.let_stack_start.back() + this->bound_index];
		assert(dynamic_cast<FunctionValue*>(arg.getPointer()));
		FunctionValue* function_value = dynamic_cast<FunctionValue*>(arg.getPointer());
		function_value_out = function_value;
		return function_value->func_def;
	}
}


ValueRef FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) std::cout << indent(vmstate) << "FunctionExpression, target_name=" << this->function_name << "\n";
	if(this->function_name == "sin")
		int a = 9; // TEMP

	


	//assert(target_function);
	if(this->target_function != NULL && this->target_function->external_function.nonNull())
	{
		vector<ValueRef> args;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			args.push_back(this->argument_expressions[i]->exec(vmstate));

		if(this->target_function->external_function->takes_hidden_voidptr_arg) //vmstate.hidden_voidptr_arg)
			args.push_back(vmstate.argument_stack.back());

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

	if(vmstate.hidden_voidptr_arg)
		vmstate.argument_stack.push_back(vmstate.argument_stack[initial_arg_stack_size - 1]);

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


static bool couldCoerceFunctionCall(vector<ASTNodeRef>& argument_expressions, FunctionDefinitionRef func)
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
}


void FunctionExpression::linkFunctions(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	bool found_binding = false;
	bool in_current_func_def = true;
	int use_let_frame_offset = 0;


	// We want to find a function that matches our argument expression types, and the function name



	// First, walk up tree, and see if such a target function has been given a name with a let.
	for(int s = (int)stack.size() - 1; s >= 0 && !found_binding; --s)
	{
		{
			if(FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[s])) // If node is a function definition
			{
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
			else if(LetBlock* let_block = dynamic_cast<LetBlock*>(stack[s]))
			{
				for(unsigned int i=0; i<let_block->lets.size(); ++i)
				{
					//TypeRef type_ref = let_block->lets[i]->type();
					if(let_block->lets[i]->variable_name == this->function_name && doesFunctionTypeMatch(let_block->lets[i]->type()))
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
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			argtypes.push_back(this->argument_expressions[i]->type());


		FunctionSignature sig(this->function_name, argtypes);

		// Try and resolve to internal function.
		this->target_function = linker.findMatchingFunction(sig).getPointer();
		if(this->target_function)
		{
			this->binding_type = BoundToGlobalDef;
		}
		else
		{
			// Try and promote integer args to float args.
			vector<FunctionDefinitionRef> funcs;
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
			}
		}

		if(this->binding_type == Unbound)
			throw BaseException("Failed to find function '" + sig.toString() + "'." + errorContext(*this));
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			if(shouldFoldExpression(argument_expressions[i], payload))
			{
				argument_expressions[i] = foldExpression(argument_expressions[i], payload);
				payload.tree_changed = true;
			}
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

	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
		// TEMP NEW: Do operator overloading now:
		for(size_t i=0; i<argument_expressions.size(); ++i)
			convertOverloadedOperators(argument_expressions[i], payload, stack);


		// If this is a generic function, we can't try and bind function expressions yet,
		// because the binding depends on argument type due to function overloading, so we have to wait
		// until we know the concrete type.

		if(!payload.func_def_stack.back()->isGenericFunction())
			linkFunctions(*payload.linker, payload, stack);
	}
}


void FunctionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionExpr";
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

	// If got there, binding type is probably unbound.
	assert(0);
	return TypeRef(NULL);

	//if(target_function_return_type.nonNull())
	//	return target_function_return_type;
	//else
	//{
	//	return this->target_function ? this->target_function->returnType() : TypeRef(NULL);
	//}
/*
	if(!target_function)
	{
		assert(0);
		throw BaseException("Tried to get type from an unlinked function expression.");
	}
	return this->target_function->type();*/
}


llvm::Value* FunctionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	llvm::Value* target_llvm_func = NULL;
	TypeRef target_ret_type = this->type(); //this->target_function_return_type;

	llvm::Value* captured_var_struct_ptr = NULL;

	bool target_takes_voidptr_arg = params.hidden_voidptr_arg;

	if(binding_type == Let)
	{
		//target_llvm_func = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index);

		llvm::Value* closure_pointer = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index);

		
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
			indices.begin(),
			indices.end(),
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
			indices.begin(),
			indices.end(),
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

		if(this->target_function->external_function.nonNull())
			target_takes_voidptr_arg = this->target_function->external_function->takes_hidden_voidptr_arg;

		target_llvm_func = this->target_function->getOrInsertFunction(
			params.module,
			false, // use_cap_var_struct_ptr: False as global functions don't have captured vars. ?!?!?
			target_takes_voidptr_arg // params.hidden_voidptr_arg
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


	//------------------
	// Build args list

	if(target_ret_type->passByValue())
	{
		vector<llvm::Value*> args;

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

		// Append pointer to Captured var struct, if this function was from a closure, and there are captured vars.
		if(captured_var_struct_ptr != NULL)
			args.push_back(captured_var_struct_ptr);

		//std::cout << "captured_var_struct_ptr: " << std::endl;
		//captured_var_struct_ptr->dump();

		// Set hidden voidptr argument
		if(target_takes_voidptr_arg) // params.hidden_voidptr_arg)
			args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args.begin(), args.end());

		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);

		return call_inst;
	}
	else
	{
		//llvm::Value* return_val_addr = NULL;
		//if(parent->passByValue())
		//{

		// Allocate return value on stack
		llvm::Value* return_val_addr = params.builder->Insert(new llvm::AllocaInst(
			target_ret_type->LLVMType(*params.context), // type
			NULL, // ArraySize
			16, // alignment
			"return_val_addr" // target_sig.toString() + " return_val_addr"
		));

		vector<llvm::Value*> args(1, return_val_addr);

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

		// Append pointer to Captured var struct, if this function was from a closure.
		if(captured_var_struct_ptr != NULL)
			args.push_back(captured_var_struct_ptr);

		// Set hidden voidptr argument
		if(target_takes_voidptr_arg) // params.hidden_voidptr_arg)
			args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		llvm::CallInst* call_inst = params.builder->CreateCall(target_llvm_func, args.begin(), args.end());
		
		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);

		return return_val_addr;
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> FunctionExpression::clone()
{
	FunctionExpression* e = new FunctionExpression(srcLocation());
	e->function_name = this->function_name;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
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
	return false; // TEMP HACK

	// For now, we'll say a function expression bound to an argument of let var is not constant.
	if(this->binding_type != BoundToGlobalDef)
		return false;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		if(!argument_expressions[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
