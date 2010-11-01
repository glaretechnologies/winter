/*=====================================================================
ASTNode.cpp
-----------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
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


const bool VERBOSE_EXEC = false;


static void printMargin(int depth, std::ostream& s)
{
	for(int i=0; i<depth; ++i)
		s << "  ";
}


namespace Winter
{




/*
ASTNode::ASTNode()
{
	
}


ASTNode::~ASTNode()
{
	
}*/

/*void BufferRoot::linkFunctions(Linker& linker)
{
	for(unsigned int i=0; i<func_defs.size(); ++i)
		func_defs[i]->linkFunctions(linker);
}


void BufferRoot::bindVariables(const std::vector<ASTNode*>& stack)
{
	//std::vector<ASTNode*> s;
	for(unsigned int i=0; i<func_defs.size(); ++i)
		func_defs[i]->bindVariables(stack);
}*/


void BufferRoot::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.resize(0);
	stack.push_back(this);

	for(unsigned int i=0; i<func_defs.size(); ++i)
		func_defs[i]->traverse(payload, stack);

	stack.pop_back();
}



void BufferRoot::print(int depth, std::ostream& s) const
{
	//s << "========================================================\n";
	for(unsigned int i=0; i<func_defs.size(); ++i)
	{
		func_defs[i]->print(depth+1, s);
		s << "\n";
	}
}


llvm::Value* BufferRoot::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


Reference<ASTNode> BufferRoot::clone()
{
	throw BaseException("BufferRoot::clone()");
}


//----------------------------------------------------------------------------------


FunctionDefinition::FunctionDefinition(const std::string& name, const std::vector<FunctionArg>& args_, 
									   const vector<Reference<LetASTNode> >& lets_,
									   const ASTNodeRef& body_, const TypeRef& declared_rettype, 
									   BuiltInFunctionImpl* impl)
:	args(args_),
	lets(lets_),
	body(body_),
	declared_return_type(declared_rettype),
	built_in_func_impl(impl)
{
	sig.name = name;
	for(unsigned int i=0; i<args_.size(); ++i)
		sig.param_types.push_back(args_[i].type);

	// TODO: fix this, make into method
	function_type = TypeRef(new Function(sig.param_types, declared_rettype));

	this->let_exprs_llvm_value = std::vector<llvm::Value*>(this->lets.size(), NULL);
}


FunctionDefinition::~FunctionDefinition()
{
	delete built_in_func_impl;
}


TypeRef FunctionDefinition::returnType() const
{
	if(this->declared_return_type.nonNull())
		return this->declared_return_type;

	assert(this->body.nonNull());
	assert(this->body->type().nonNull());
	return this->body->type();
	//return this->body.nonNull() ? this->body->type() : TypeRef(NULL);
}


Value* FunctionDefinition::exec(VMState& vmstate)
{
	return new FunctionValue(this);
}


static const std::string indent(VMState& vmstate)
{
	std::string s;
	for(unsigned int i=0; i<vmstate.func_args_start.size(); ++i)
		s += "  ";
	return s;
}


static void printStack(VMState& vmstate)
{
	std::cout << indent(vmstate) << "arg Stack: [";
	for(unsigned int i=0; i<vmstate.argument_stack.size(); ++i)
		std::cout << vmstate.argument_stack[i]->toString() + ", ";
	std::cout << "]\n";
}



Value* FunctionDefinition::invoke(VMState& vmstate)
{
	if(VERBOSE_EXEC) 
	{
		std::cout << indent(vmstate) << "FunctionDefinition, name=" << this->sig.name << "\n";
		printStack(vmstate);
	}

	if(this->built_in_func_impl)
		return this->built_in_func_impl->invoke(vmstate);

	
	// Evaluate let clauses, which will each push the result onto the let stack
	vmstate.let_stack_start.push_back(vmstate.let_stack.size()); // Push let frame index
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	// Execute body of function
	Value* ret = body->exec(vmstate);

	// Pop things off let stack
	for(unsigned int i=0; i<lets.size(); ++i)
	{
		delete vmstate.let_stack.back();
		vmstate.let_stack.pop_back();
	}
	// Pop let frame index
	vmstate.let_stack_start.pop_back();

	return ret;
}


/*void FunctionDefinition::linkFunctions(Linker& linker)
{
	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->linkFunctions(linker);

	this->body->linkFunctions(linker);
}


void FunctionDefinition::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);

	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->bindVariables(s);

	this->body->bindVariables(s);
}*/


void FunctionDefinition::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->isGenericFunction())
			return; // Don't type check this.  Concrete versions of this func will be type checked individually.

		if(this->body.nonNull())
		{
			if(this->declared_return_type.nonNull())
			{
				// Check that the return type of the body expression is equal to the declared return type
				// of this function.
				if(*this->body->type() != *this->declared_return_type)
					throw BaseException("Type error for function '" + this->sig.toString() + "': Computed return type '" + this->body->type()->toString() + 
						"' is not equal to the declared return type '" + this->declared_return_type->toString() + "'.");
			}
			else
			{
				// Else return type is NULL, so infer it
				//this->return_type = this->body->type();
			}
		}
	}

	if(payload.operation == TraversalPayload::LinkFunctions)
	{
		if(this->isGenericFunction())
			return; // Don't try and bind functions yet.
	}


	stack.push_back(this);

	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->traverse(payload, stack);

	if(this->body.nonNull()) // !this->built_in_func_impl)
		this->body->traverse(payload, stack);

	stack.pop_back();
}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef: " << this->sig.toString() << " " << 
		(this->returnType().nonNull() ? this->returnType()->toString() : "[Unknown ret type]");
	if(this->declared_return_type.nonNull())
		s << " (Declared ret type: " + this->declared_return_type->toString() << ")";
	s << "\n";
	for(unsigned int i=0; i<this->lets.size(); ++i)
		lets[i]->print(depth + 1, s);

	if(this->built_in_func_impl)
	{
		printMargin(depth+1, s);
		s << "Built in Implementation.";
	}
	else if(body.nonNull())
	{
		body->print(depth+1, s);
	}
	else
	{
		printMargin(depth+1, s);
		s << "Null body.";
	}
}


llvm::Value* FunctionDefinition::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
	/*if(this->built_in_func_impl)
		return this->built_in_func_impl->emitLLVMCode(params);
	else
		return body->emitLLVMCode(params);*/
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module,
	const PlatformUtils::CPUInfo& cpu_info
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	)
{
#if USE_LLVM
	llvm::FunctionType* functype = LLVMTypeUtils::llvmInternalFunctionType(
		this->sig.param_types, 
		returnType(), 
		module->getContext()
	);

	// Make attribute list
	llvm::AttrListPtr attribute_list;
	/*if(!this->returnType()->passByValue())
	{
		// Add sret attribute to zeroth argument
		attribute_list = attribute_list.addAttr(
			1, // index (NOTE: starts at one)
			llvm::Attribute::StructRet
		);
	}*/

	llvm::Attributes function_attr = llvm::Attribute::NoUnwind; // Does not throw exceptions
	if(this->returnType()->passByValue())
	{
		//function_attr |= llvm::Attribute::ReadNone

		bool has_ptr_arg = false;
		for(unsigned int i=0; i<this->args.size(); ++i)
		{
			if(!this->args[i].type->passByValue())
				has_ptr_arg = true;
		}

		if(has_ptr_arg)
			function_attr |= llvm::Attribute::ReadOnly; // This attribute indicates that the function does not write through any pointer arguments etc..
		else
			function_attr |= llvm::Attribute::ReadNone; // Function computes its result based strictly on its arguments, without dereferencing any pointer arguments etc..
	}


	attribute_list = attribute_list.addAttr(4294967295U, function_attr);
	/*{
		SmallVector<AttributeWithIndex, 4> Attrs;
		AttributeWithIndex PAWI;
		PAWI.Index = 4294967295U; PAWI.Attrs = 0  | Attribute::NoUnwind | Attribute::ReadNone;
		Attrs.push_back(PAWI);
		func_f_PAL = AttrListPtr::get(Attrs.begin(), Attrs.end());

	}*/



	
	llvm::Function *internal_llvm_func = static_cast<llvm::Function*>(module->getOrInsertFunction(
		this->sig.toString(), // internalFuncName(this->getSig()), // Name
		functype//, // Type
		//attribute_list // attribute_list
		));

	internal_llvm_func->setAttributes(attribute_list);

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	internal_llvm_func->setCallingConv(llvm::CallingConv::C);

	//internal_llvm_func->setAttributes(

	// Set names for all arguments.
	int i = 0;
	for(llvm::Function::arg_iterator AI = internal_llvm_func->arg_begin(); AI != internal_llvm_func->arg_end(); ++AI, ++i)
	{
		if(this->returnType()->passByValue())
		{
			AI->setName(this->args[i].name);
		}
		else
		{
			if(i == 0)
				AI->setName("ret");
			else
				AI->setName(this->args[i-1].name);
		}
	}


	llvm::BasicBlock* block = llvm::BasicBlock::Create(
		module->getContext(), 
		"entry", 
		internal_llvm_func
	);
	llvm::IRBuilder<> builder(block);

	// Build body LLVM code
	EmitLLVMCodeParams params;
	params.currently_building_func_def = this;
	params.cpu_info = &cpu_info;
	params.builder = &builder;
	params.module = module;
	params.currently_building_func = internal_llvm_func;
	params.context = &module->getContext();

	//llvm::Value* body_code = NULL;
	if(this->built_in_func_impl)
	{
		llvm::Value* body_code = this->built_in_func_impl->emitLLVMCode(params);
		if(this->returnType()->passByValue())
			builder.CreateRet(body_code);
		else
			builder.CreateRetVoid();
	}
	else
	{
		llvm::Value* body_code = this->body->emitLLVMCode(params);

		if(this->returnType()->passByValue())
		{
			builder.CreateRet(body_code);
		}
		else
		{
			// body code will return a pointer to the result of the body expression, allocated on the stack.
			// So load from the stack, and save to the return pointer which will have been passed in as arg zero.
			llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(internal_llvm_func, 0);

			//if(*this->returnType() == 
			if(this->returnType()->getType() == Type::StructureTypeType)
			{
				StructureType* struct_type = static_cast<StructureType*>(this->returnType().getPointer());

				/*for(unsigned int i=0; i<struct_type->component_types.size(); ++i)
				{
					// Load the field
					vector<llvm::Value*> indices;
					indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
					indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)));

					llvm::Value* field_ptr = params.builder->CreateGEP(
						body_code, // ptr
						indices.begin(),
						indices.end()
						);

					llvm::Value* field = params.builder->CreateLoad(
						field_ptr
						);

					// Store the field.
					llvm::Value* store_field_ptr = params.builder->CreateGEP(
						return_val_ptr, // ptr
						indices.begin(),
						indices.end()
					);

					params.builder->CreateStore(
						field, // value
						store_field_ptr // ptr
					);
				}*/

				llvm::Value* struct_val = params.builder->CreateLoad(
					body_code
				);

				params.builder->CreateStore(
					struct_val, // value
					return_val_ptr // ptr
				);
			}
			else
			{
				assert(0);
			}

			//builder.CreateRet(return_val_ptr);
			builder.CreateRetVoid();
		}
	}


	this->built_llvm_function = internal_llvm_func;
	return internal_llvm_func;
#else
	return NULL;
#endif
}


Reference<ASTNode> FunctionDefinition::clone()
{
	throw BaseException("FunctionDefinition::clone()");
}


bool FunctionDefinition::isGenericFunction() const // true if it is parameterised by type.
{
	for(size_t i=0; i<this->args.size(); ++i)
		if(this->args[i].type->getType() == Type::GenericTypeType)
			return true;
	return false;
}


llvm::Value* FunctionDefinition::getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index)
{
	if(let_exprs_llvm_value[let_index] == NULL)
	{
		let_exprs_llvm_value[let_index] = this->lets[let_index]->emitLLVMCode(params);
	}

	return let_exprs_llvm_value[let_index];
}


//--------------------------------------------------------------------------------


FunctionDefinition* FunctionExpression::runtimeBind(VMState& vmstate)
{
	FunctionDefinition* use_target_function = NULL;
	if(target_function)
		use_target_function = target_function;
	else if(this->binding_type == Arg)
	{
		Value* arg = vmstate.argument_stack[vmstate.func_args_start.back() + this->argument_index];
		assert(dynamic_cast<FunctionValue*>(arg));
		FunctionValue* function_value = dynamic_cast<FunctionValue*>(arg);
		use_target_function = function_value->func_def;
	}
	else
	{
		Value* arg = vmstate.let_stack[vmstate.let_stack_start.back() + this->argument_index];
		assert(dynamic_cast<FunctionValue*>(arg));
		FunctionValue* function_value = dynamic_cast<FunctionValue*>(arg);
		use_target_function = function_value->func_def;
	}

	assert(use_target_function);
	return use_target_function;
}


Value* FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) std::cout << indent(vmstate) << "FunctionExpression, target_name=" << this->function_name << "\n";
	
	//assert(target_function);
	if(this->target_external_function.nonNull())
	{
		vector<const Value*> args;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			args.push_back(this->argument_expressions[i]->exec(vmstate));

		Value* result = this->target_external_function->interpreted_func(args);
		return result;
	}

	// Get target function
	FunctionDefinition* use_target_func = runtimeBind(vmstate);

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

	assert(vmstate.argument_stack.size() == initial_arg_stack_size + this->argument_expressions.size());

	if(VERBOSE_EXEC)
		std::cout << indent(vmstate) << "Calling " << this->function_name << ", func_args_start: " << vmstate.func_args_start.back() << "\n";

	// Execute target function
	vmstate.func_args_start.push_back(initial_arg_stack_size);
	Value* ret = use_target_func->invoke(vmstate);
	vmstate.func_args_start.pop_back();

	// Remove arguments from stack
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
	{
		delete vmstate.argument_stack.back();
		vmstate.argument_stack.pop_back();
	}
	assert(vmstate.argument_stack.size() == initial_arg_stack_size);

	return ret;
}


bool FunctionExpression::doesFunctionTypeMatch(TypeRef& type)
{
	if(type->getType() != Type::FunctionType)
		return false;

	Function* func = dynamic_cast<Function*>(type.getPointer());
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


void FunctionExpression::linkFunctions(Linker& linker, std::vector<ASTNode*>& stack)
{
	bool found_binding = false;
	// We want to find a function that matches our argument expression types, and the function name



	// First, walk up tree, and see if such a target function has been given a name with a let.
	for(int i = (int)stack.size() - 1; i >= 0 && !found_binding; --i)
	{
		{
			FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[i]);
			if(def != NULL)
			{
				for(unsigned int i=0; i<def->lets.size(); ++i)
					if(def->lets[i]->variable_name == this->function_name && doesFunctionTypeMatch(def->lets[i]->type()))
					{
						this->argument_index = i;
						//this->argument_offset = (int)def->lets.size() - i;
						this->binding_type = Let;
						// We know this lets_i body is a FunctionDefinition.
						FunctionDefinition* let_def = dynamic_cast<FunctionDefinition*>(def->lets[i]->expr.getPointer());
						//Function* let_func_type = dynamic_cast<Function*>(def->lets[i]->type().getPointer());
						//if(!let_func_type)
						//	throw BaseException(this->function_name + " used in function expression is not a function.");
						this->target_function_return_type = let_def->returnType();
						found_binding = true;
					}

				for(unsigned int i=0; i<def->args.size(); ++i)
					if(def->args[i].name == this->function_name && doesFunctionTypeMatch(def->args[i].type))
					{
						this->argument_index = i;
						//this->argument_offset = (int)def->args.size() - i;
						this->binding_type = Arg;
						//this->target_function_return_type = def->args[i].//def->returnType();
						found_binding = true;
					}

				//if(this->argument_offset == -1)
				//	throw BaseException("No such function argument '" + this->name + "'");
			}
		}
	}

	if(!found_binding)
	{
		vector<TypeRef> argtypes;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			argtypes.push_back(this->argument_expressions[i]->type());


		FunctionSignature sig(this->function_name, argtypes);

		//Linker::FuncMapType::iterator res = linker.functions.find(sig);
		//if(res == linker.functions.end())
		//	throw BaseException("Failed to find function with signature " + sig.toString());

		// Try and resolve to external function
		ExternalFunctionRef extern_func = linker.findMatchingExternalFunction(sig);

		if(extern_func.nonNull())
		{
			this->target_function = NULL;
			this->target_external_function = extern_func;
			this->binding_type = Bound;
			this->target_function_return_type = extern_func->return_type;
		}
		else
		{
			// Try and resolve to internal function.
			this->target_function = linker.findMatchingFunction(sig).getPointer();
			this->binding_type = Bound;
			this->target_function_return_type = this->target_function->returnType();
		}
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
	// NOTE: we want to do a post-order traversal here.
	// Thhis is because we want our argument expressions to be linked first.

	stack.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->traverse(payload, stack);

	stack.pop_back();

	if(payload.operation == TraversalPayload::LinkFunctions)
		linkFunctions(*payload.linker, stack);

}


void FunctionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionExpr";
	if(this->target_function)
		s << "; target: " << this->target_function->sig.toString();
	else if(this->target_external_function.nonNull())
		s << "; target (external): " << this->target_external_function->sig.toString();
	else if(this->binding_type == Arg)
		s << "; runtime bound to arg index " << this->argument_index;
	else if(this->binding_type == Let)
		s << "; runtime bound to let index " << this->argument_index;
	s << "\n";
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->print(depth + 1, s);
}


TypeRef FunctionExpression::type() const
{
	if(target_function_return_type.nonNull())
		return target_function_return_type;
	else
	{
		return this->target_function ? this->target_function->returnType() : TypeRef(NULL);
	}
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
	// Lookup LLVM function, which should already be created and added to the module.
	/*llvm::Function* target_llvm_func = params.module->getFunction(
		this->target_function->sig.toString() //internalFuncName(call_target_sig)
		);
	assert(target_llvm_func);*/

	FunctionSignature target_sig = this->target_function ? this->target_function->sig : this->target_external_function->sig;
	TypeRef target_ret_type = this->target_function_return_type;

	llvm::FunctionType* target_func_type = LLVMTypeUtils::llvmInternalFunctionType(
		target_sig.param_types, 
		target_ret_type, 
		*params.context
	);

	llvm::Function* target_llvm_func = static_cast<llvm::Function*>(params.module->getOrInsertFunction(
		target_sig.toString(), // Name
		target_func_type // Type
	));

	assert(target_llvm_func);


	//------------------
	// Build args list

	if(target_ret_type->passByValue())
	{
		vector<llvm::Value*> args;

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

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
			target_sig.toString() + " return_val_addr"
		));

		vector<llvm::Value*> args(1, return_val_addr);

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

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
	FunctionExpression* e = new FunctionExpression();
	e->function_name = this->function_name;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		e->argument_expressions.push_back(argument_expressions[i]->clone());
	
	e->target_function = this->target_function;
	e->target_external_function = this->target_external_function;
	e->argument_index = this->argument_index;
	e->binding_type = this->binding_type;

	return ASTNodeRef(e);
}


//-----------------------------------------------------------------------------------


Variable::Variable(const std::string& name_)
:	//ASTNode(parent),
	//referenced_var(NULL),
	name(name_),
	//argument_offset(-1),
	argument_index(-1),
	parent_function(NULL)
	//parent_anon_function(NULL)
{
/*	ASTNode* c = parent;
	while(c)
	{
		FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(c);
		if(def != NULL)
		{
			for(unsigned int i=0; i<def->lets.size(); ++i)
				if(def->lets[i]->variable_name == this->name)
				{
					this->argument_offset = (int)def->lets.size() - i;
					this->referenced_var_type = def->args[i].type;
					this->vartype = LetVariable;
					return;
				}

			for(unsigned int i=0; i<def->args.size(); ++i)
				if(def->args[i].name == this->name)
				{
					this->argument_offset = (int)def->args.size() - i;
					this->referenced_var_type = def->args[i].type;
					this->vartype = ArgumentVariable;
					return;
				}

			if(this->argument_offset == -1)
				throw BaseException("No such function argument '" + this->name + "'");
			c = NULL; // Break from while loop
		}
		else
			c = c->getParent();
	}

	throw BaseException("No such function argument '" + this->name + "'");
*/
}


void Variable::bindVariables(const std::vector<ASTNode*>& stack)
{
	for(int i = (int)stack.size() - 1; i >= 0; --i)
	{
		{
			FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[i]);
			if(def != NULL)
			{
				for(unsigned int i=0; i<def->lets.size(); ++i)
					if(def->lets[i]->variable_name == this->name)
					{
						this->argument_index = i;
						//this->argument_offset = (int)def->lets.size() - i;
						//this->referenced_var_type = def->lets[i]->type();
						this->vartype = LetVariable;
						this->parent_function = def;
						return;
					}

				for(unsigned int i=0; i<def->args.size(); ++i)
					if(def->args[i].name == this->name)
					{
						this->argument_index = i;
						//this->argument_offset = (int)def->args.size() - i;
						//is->referenced_var_type = def->args[i].type;
						this->vartype = ArgumentVariable;
						this->parent_function = def;
						return;
					}

				if(this->argument_index == -1)
					throw BaseException("No such function argument '" + this->name + "'");
			}
		}

#if 0
		{
			AnonFunction* def = dynamic_cast<AnonFunction*>(stack[i]);
			if(def != NULL)
			{
				/*for(unsigned int i=0; i<def->lets.size(); ++i)
					if(def->lets[i]->variable_name == this->name)
					{
						this->argument_index = i;
						this->argument_offset = (int)def->lets.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = LetVariable;
						this->parent_function = def;
						return;
					}*/

				for(unsigned int i=0; i<def->args.size(); ++i)
					if(def->args[i].name == this->name)
					{
						this->argument_index = i;
						//this->argument_offset = (int)def->args.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = ArgumentVariable;
						this->parent_anon_function = def;
						return;
					}

				if(this->argument_index == -1)
					throw BaseException("No such function argument '" + this->name + "'");
			}
		}
#endif
	}
	throw BaseException("Variable::bindVariables(): No such function argument '" + this->name + "'");
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
		this->bindVariables(stack);
}


Value* Variable::exec(VMState& vmstate)
{
	assert(this->argument_index >= 0);

	if(this->vartype == ArgumentVariable)
	{
		return vmstate.argument_stack[vmstate.func_args_start.back() + argument_index]->clone();
	}
	else
	{
		return vmstate.let_stack[vmstate.let_stack_start.back() + argument_index]->clone();
	}
}


TypeRef Variable::type() const
{
	assert(this->argument_index >= 0);
	//assert(referenced_var_type.nonNull());

	//if(!this->referenced_var)
	//	throw BaseException("referenced_var == NULL");
	//return this->referenced_var->type();
	//return this->referenced_var_type;

	if(this->vartype == LetVariable)
		return this->parent_function->lets[this->argument_index]->type();
	else if(this->vartype == ArgumentVariable)
		return this->parent_function->args[this->argument_index].type;
	else
	{
		assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


inline static const std::string varType(Variable::VariableType t)
{
	return t == Variable::LetVariable ? "Let" : "Arg";
}


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Variable, name=" << this->name << ", " + varType(this->vartype) + ", argument_index=" << argument_index << "\n";
}


/*static bool shouldPassByValue(const Type& type)
{
	return true;
}*/


llvm::Value* Variable::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(vartype == LetVariable)
	{
		return this->parent_function->getLetExpressionLLVMValue(params, this->argument_index);
	}
	else
	{
		assert(this->parent_function);

		//if(shouldPassByValue(*this->type()))
		//{
			// If the current function returns its result via pointer, then all args are offset by one.
			if(params.currently_building_func_def->returnType()->passByValue())
				return LLVMTypeUtils::getNthArg(params.currently_building_func, this->argument_index);
			else
				return LLVMTypeUtils::getNthArg(params.currently_building_func, this->argument_index + 1);
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
#else
	return NULL;
#endif
}


Reference<ASTNode> Variable::clone()
{
	return ASTNodeRef(new Variable(*this));
}


//------------------------------------------------------------------------------------


Value* FloatLiteral::exec(VMState& vmstate)
{
	return new FloatValue(value);
}


void FloatLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Float literal, value=" << this->value << "\n";
}


llvm::Value* FloatLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	return llvm::ConstantFP::get(
		*params.context, 
		llvm::APFloat(this->value)
	);
#else
	return NULL;
#endif
}


Reference<ASTNode> FloatLiteral::clone()
{
	return ASTNodeRef(new FloatLiteral(*this));
}


//------------------------------------------------------------------------------------


Value* IntLiteral::exec(VMState& vmstate)
{
	return new IntValue(value);
}


void IntLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Int literal, value=" << this->value << "\n";
}


llvm::Value* IntLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			32, // num bits
			this->value, // value
			true // signed
		)
	);
#else
	return NULL;
#endif
}


Reference<ASTNode> IntLiteral::clone()
{
	return ASTNodeRef(new IntLiteral(*this));
}


//-------------------------------------------------------------------------------------


Value* BoolLiteral::exec(VMState& vmstate)
{
	return new BoolValue(value);
}


void BoolLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Bool literal, value=" << this->value << "\n";
}


llvm::Value* BoolLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			1, // num bits
			this->value ? 1 : 0, // value
			false // signed
		)
	);
}


Reference<ASTNode> BoolLiteral::clone()
{
	return ASTNodeRef(new BoolLiteral(*this));
}


//----------------------------------------------------------------------------------------------


Value* MapLiteral::exec(VMState& vmstate)
{
/*	std::map<Value*, Value*> m;
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->exec(vmstate);
		Value* key = vmstate.working_stack.back();
		vmstate.working_stack.pop_back();

		this->items[i].second->exec(vmstate);
		Value* value = vmstate.working_stack.back();
		vmstate.working_stack.pop_back();

		m.insert(std::make_pair(key, value));
	}

	return new MapValue(m);
	*/
	assert(0);
	return NULL;
}


void MapLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Map literal\n";

	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		printMargin(depth+1, s);
		s << "Key:\n";
		this->items[i].first->print(depth+2, s);

		printMargin(depth+1, s);
		s << "Value:\n";
		this->items[i].second->print(depth+2, s);
	}
}


void MapLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->traverse(payload, stack);
		this->items[i].second->traverse(payload, stack);
	}
	stack.pop_back();
}


llvm::Value* MapLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return NULL;
}


Reference<ASTNode> MapLiteral::clone()
{
	MapLiteral* m = new MapLiteral();
	m->maptype = this->maptype;
	for(size_t i=0; i<items.size(); ++i)
		m->items.push_back(std::make_pair(items[0].first->clone(), items[0].second->clone()));
	return ASTNodeRef(m);
}


//----------------------------------------------------------------------------------------------
ArrayLiteral::ArrayLiteral(const std::vector<ASTNodeRef>& elems)
:	elements(elems)
{
	//this->t
	if(elems.empty())
		throw BaseException("Array literal can't be empty.");
}


TypeRef ArrayLiteral::type() const// { return array_type; }
{
	return TypeRef(new ArrayType(elements[0]->type()));
}


Value* ArrayLiteral::exec(VMState& vmstate)
{
	std::vector<Value*> elem_values(elements.size());

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		elem_values[i] = this->elements[i]->exec(vmstate);
	}

	return new ArrayValue(elem_values);
}


void ArrayLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Array literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		printMargin(depth+1, s);
		this->elements[i]->print(depth+2, s);
	}
}


void ArrayLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->traverse(payload, stack);
	}
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		// Check all the element expression types match the computed element type.
		const TypeRef elem_type = this->elements[0]->type();
		for(unsigned int i=0; i<this->elements.size(); ++i)
			if(*elem_type != *this->elements[i]->type())
				throw BaseException("Array element " + ::toString(i) + " did not have required type " + elem_type->toString() + ".");
	}
}


llvm::Value* ArrayLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return NULL;
}


Reference<ASTNode> ArrayLiteral::clone()
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone();
	return ASTNodeRef(new ArrayLiteral(elems));
}


//------------------------------------------------------------------------------------------


VectorLiteral::VectorLiteral(const std::vector<ASTNodeRef>& elems)
:	elements(elems)
{
	if(elems.empty())
		throw BaseException("Array literal can't be empty.");
}


TypeRef VectorLiteral::type() const
{
	return TypeRef(new VectorType(elements[0]->type(), (int)elements.size()));
}


Value* VectorLiteral::exec(VMState& vmstate)
{
	std::vector<Value*> elem_values(elements.size());

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		elem_values[i] = this->elements[i]->exec(vmstate);
	}

	return new VectorValue(elem_values);
}


void VectorLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Vector literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		printMargin(depth+1, s);
		this->elements[i]->print(depth+2, s);
	}
}


void VectorLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->traverse(payload, stack);
	}
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		// Check all the element expression types match the computed element type.
		const TypeRef elem_type = this->elements[0]->type();
		for(unsigned int i=0; i<this->elements.size(); ++i)
			if(*elem_type != *this->elements[i]->type())
				throw BaseException("Vector element " + ::toString(i) + " did not have required type " + elem_type->toString() + ".");
	}
}


llvm::Value* VectorLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Get LLVM vector type
	//const llvm::VectorType* llvm_vec_type = llvm::VectorType::get(
	//	this->elements[0]->type()->LLVMType(*params.context),
	//	this->elements.size()
	//);

	const llvm::VectorType* llvm_vec_type = (const llvm::VectorType*)this->type()->LLVMType(*params.context);

	//Value* default_val = this->elements[0]->type()->getDefaultValue();

	// Create an initial constant vector with default values.
	llvm::Value* v = llvm::ConstantVector::get(
		llvm_vec_type, 
		std::vector<llvm::Constant*>(
			this->elements.size(),
			this->elements[0]->type()->defaultLLVMValue(*params.context)
			//llvm::ConstantFP::get(*params.context, llvm::APFloat(0.0))
		)
	);

	llvm::Value* vec = v;
	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		llvm::Value* elem_llvm_code = this->elements[i]->emitLLVMCode(params);

		vec = params.builder->CreateInsertElement(
			vec, // vec
			elem_llvm_code, // new element
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, i)) // index
		);
	}
	return vec;
}


Reference<ASTNode> VectorLiteral::clone()
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone();
	return ASTNodeRef(new VectorLiteral(elems));
}


//------------------------------------------------------------------------------------------


/*void MapLiteral::linkFunctions(Linker& linker)
{
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->linkFunctions(linker);
		this->items[i].second->linkFunctions(linker);
	}
}*/


Value* StringLiteral::exec(VMState& vmstate)
{
	return new StringValue(value);
}


void StringLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "String literal, value='" << this->value << "'\n";
}


llvm::Value* StringLiteral::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return NULL;
}


Reference<ASTNode> StringLiteral::clone()
{
	return ASTNodeRef(new StringLiteral(*this));
}

//-----------------------------------------------------------------------------------------------


Value* AdditionExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	Value* retval = NULL;

	switch(this->type()->getType())
	{
	case Type::FloatType:
		retval = new FloatValue(static_cast<FloatValue*>(aval)->value + static_cast<FloatValue*>(bval)->value);
		break;
	case Type::IntType:
		retval = new IntValue(static_cast<IntValue*>(aval)->value + static_cast<IntValue*>(bval)->value);
		break;
	case Type::VectorTypeType:
		{
		TypeRef this_type = this->type();
		VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);
		vector<Value*> elem_values(aval_vec->e.size());
		switch(vectype->t->getType())
		{
		case Type::FloatType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new FloatValue(static_cast<FloatValue*>(aval_vec->e[i])->value + static_cast<FloatValue*>(bval_vec->e[i])->value);
			break;
		case Type::IntType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new IntValue(static_cast<IntValue*>(aval_vec->e[i])->value + static_cast<IntValue*>(bval_vec->e[i])->value);
			break;
		default:
			assert(!"additionexpression vector field type invalid!");
		};
		retval = new VectorValue(elem_values);
		break;
		}
	default:
		assert(!"additionexpression type invalid!");
	}
	delete aval;
	delete bval;

	return retval;
}


void AdditionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Addition Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


/*void AdditionExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}


void AdditionExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	this->a->bindVariables(s);
	this->b->bindVariables(s);
}*/


void AdditionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();
}


llvm::Value* AdditionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	return params.builder->CreateBinOp(
		llvm::Instruction::Add, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
#else
	return NULL;
#endif
}


Reference<ASTNode> AdditionExpression::clone()
{
	AdditionExpression* e = new AdditionExpression();
	e->a = this->a->clone();
	e->b = this->b->clone();
	return ASTNodeRef(e);
}


//-------------------------------------------------------------------------------------------------


Value* SubtractionExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	Value* retval = NULL;

	switch(this->type()->getType())
	{
	case Type::FloatType:
		retval = new FloatValue(static_cast<FloatValue*>(aval)->value - static_cast<FloatValue*>(bval)->value);
		break;
	case Type::IntType:
		retval = new IntValue(static_cast<IntValue*>(aval)->value - static_cast<IntValue*>(bval)->value);
		break;
	case Type::VectorTypeType:
		{
		TypeRef this_type = this->type();
		VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);
		vector<Value*> elem_values(aval_vec->e.size());
		switch(vectype->t->getType())
		{
		case Type::FloatType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new FloatValue(static_cast<FloatValue*>(aval_vec->e[i])->value - static_cast<FloatValue*>(bval_vec->e[i])->value);
			break;
		case Type::IntType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = new IntValue(static_cast<IntValue*>(aval_vec->e[i])->value - static_cast<IntValue*>(bval_vec->e[i])->value);
			break;
		default:
			assert(!"SubtractionExpression vector field type invalid!");
		};
		retval = new VectorValue(elem_values);
		break;
		}
	default:
		assert(!"SubtractionExpression type invalid!");
	}
	delete aval;
	delete bval;

	return retval;
}


void SubtractionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Subtraction Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


/*void SubtractionExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}


void SubtractionExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	this->a->bindVariables(s);
	this->b->bindVariables(s);
}*/


void SubtractionExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();
}


llvm::Value* SubtractionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	return params.builder->CreateBinOp(
		llvm::Instruction::Sub, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
#else
	return NULL;
#endif
}


Reference<ASTNode> SubtractionExpression::clone()
{
	SubtractionExpression* e = new SubtractionExpression();
	e->a = this->a->clone();
	e->b = this->b->clone();
	return ASTNodeRef(e);
}


//-------------------------------------------------------------------------------------------------------


Value* MulExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);
	Value* retval = NULL;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = new FloatValue(static_cast<FloatValue*>(aval)->value * static_cast<FloatValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		retval = new IntValue(static_cast<IntValue*>(aval)->value * static_cast<IntValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::VectorTypeType)
	{
		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);

		vector<Value*> elem_values(aval_vec->e.size());
		for(unsigned int i=0; i<elem_values.size(); ++i)
		{
			elem_values[i] = new FloatValue(static_cast<FloatValue*>(aval_vec->e[i])->value * static_cast<FloatValue*>(bval_vec->e[i])->value);
		}

		retval = new VectorValue(elem_values);
	}
	else
	{
		assert(!"mulexpression type invalid!");
	}
	delete aval;
	delete bval;
	return retval;
}


/*void MulExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	this->a->bindVariables(s);
	this->b->bindVariables(s);
}*/


void MulExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("MulExpression: Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
	}
}


void MulExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Mul Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


/*void MulExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}*/


llvm::Value* MulExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	return params.builder->CreateBinOp(
		llvm::Instruction::Mul, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
#else
	return NULL;
#endif
}


Reference<ASTNode> MulExpression::clone()
{
	MulExpression* e = new MulExpression();
	e->a = this->a->clone();
	e->b = this->b->clone();
	return ASTNodeRef(e);
}


//-------------------------------------------------------------------------------------------------------


Value* DivExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);
	Value* retval = NULL;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = new FloatValue(static_cast<FloatValue*>(aval)->value / static_cast<FloatValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		retval = new IntValue(static_cast<IntValue*>(aval)->value / static_cast<IntValue*>(bval)->value);
	}
	else
	{
		assert(!"divexpression type invalid!");
	}
	delete aval;
	delete bval;
	return retval;
}


void DivExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCheck)
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{}
		else
		{
			throw BaseException("Child type '" + this->type()->toString() + "' does not define binary operator '/'.");
		}
}



void DivExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Div Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


llvm::Value* DivExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(this->type()->getType() == Type::FloatType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FDiv, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::SDiv, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		assert(!"divexpression type invalid!");
		return NULL;
	}

#else
	return NULL;
#endif
}


Reference<ASTNode> DivExpression::clone()
{
	DivExpression* e = new DivExpression();
	e->a = this->a->clone();
	e->b = this->b->clone();
	return ASTNodeRef(e);
}


//----------------------------------------------------------------------------------------


Value* UnaryMinusExpression::exec(VMState& vmstate)
{
	Value* aval = expr->exec(vmstate);
	Value* retval = NULL;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = new FloatValue(-static_cast<FloatValue*>(aval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		retval = new IntValue(-static_cast<IntValue*>(aval)->value);
	}
	else
	{
		assert(!"UnaryMinusExpression type invalid!");
	}
	delete aval;
	return retval;
}


/*void MulExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	this->a->bindVariables(s);
	this->b->bindVariables(s);
}*/


void UnaryMinusExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	expr->traverse(payload, stack);
	stack.pop_back();

	/*if(payload.operation == TraversalPayload::TypeCheck)
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{}
		else
		{
			throw BaseException("Child type '" + this->type()->toString() + "' does not define binary operator '*'.");
		}
	*/
}



void UnaryMinusExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Unary Minus Expression\n";
	this->expr->print(depth+1, s);
}


/*void MulExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}*/


llvm::Value* UnaryMinusExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(this->type()->getType() == Type::FloatType)
	{
		return params.builder->CreateFNeg(
			expr->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateNeg(
			expr->emitLLVMCode(params)
		);
	}
	else
	{
		assert(!"UnaryMinusExpression type invalid!");
		return NULL;
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> UnaryMinusExpression::clone()
{
	UnaryMinusExpression* e = new UnaryMinusExpression();
	e->expr = this->expr->clone();
	return ASTNodeRef(e);
}


//----------------------------------------------------------------------------------------


Value* LetASTNode::exec(VMState& vmstate)
{
	return this->expr->exec(vmstate);
}


void LetASTNode::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Let, var_name = '" + this->variable_name + "'\n";
	this->expr->print(depth+1, s);
}


void LetASTNode::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	expr->traverse(payload, stack);
	stack.pop_back();
}


/*void LetASTNode::linkFunctions(Linker& linker)
{
	expr->linkFunctions(linker);
}


void LetASTNode::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	expr->bindVariables(s);
}*/


llvm::Value* LetASTNode::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return expr->emitLLVMCode(params);
}


Reference<ASTNode> LetASTNode::clone()
{
	LetASTNode* e = new LetASTNode(this->variable_name);
	e->expr = this->expr->clone();
	return ASTNodeRef(e);
}


//---------------------------------------------------------------------------------

template <class T> static bool lt(Value* a, Value* b)
{
	return static_cast<T*>(a)->value < static_cast<T*>(b)->value;
}


template <class T> static bool gt(Value* a, Value* b)
{
	return static_cast<T*>(a)->value > static_cast<T*>(b)->value;
}


template <class T> static bool lte(Value* a, Value* b)
{
	return static_cast<T*>(a)->value <= static_cast<T*>(b)->value;
}


template <class T> static bool gte(Value* a, Value* b)
{
	return static_cast<T*>(a)->value >= static_cast<T*>(b)->value;
}


template <class T> static bool eq(Value* a, Value* b)
{
	return static_cast<T*>(a)->value == static_cast<T*>(b)->value;
}


template <class T> static bool neq(Value* a, Value* b)
{
	return static_cast<T*>(a)->value != static_cast<T*>(b)->value;
}


template <class T>
static BoolValue* compare(unsigned int token_type, Value* a, Value* b)
{
	switch(token_type)
	{
	case LEFT_ANGLE_BRACKET_TOKEN:
		return new BoolValue(lt<T>(a, b));
	case RIGHT_ANGLE_BRACKET_TOKEN:
		return new BoolValue(gt<T>(a, b));
	case DOUBLE_EQUALS_TOKEN:
		return new BoolValue(eq<T>(a, b));
	case NOT_EQUALS_TOKEN:
		return new BoolValue(neq<T>(a, b));
	case LESS_EQUAL_TOKEN:
		return new BoolValue(lte<T>(a, b));
	case GREATER_EQUAL_TOKEN:
		return new BoolValue(gte<T>(a, b));
	default:
		assert(!"Unknown comparison token type.");
		return false;
	}
}


Value* ComparisonExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	Value* retval = NULL;

	switch(a->type()->getType())
	{
	case Type::FloatType:
		retval = compare<FloatValue>(this->token->getType(), aval, bval);
		break;
	case Type::IntType:
		retval = compare<IntValue>(this->token->getType(), aval, bval);
		break;
	default:
		assert(!"SubtractionExpression type invalid!");
	}
	delete aval;
	delete bval;

	return retval;
}


void ComparisonExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Comparison, token = '" + tokenName(this->token->getType()) + "'\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


void ComparisonExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(a->type()->getType() == Type::GenericTypeType || a->type()->getType() == Type::IntType || a->type()->getType() == Type::FloatType)
		{}
		else
		{
			throw BaseException("Child type '" + this->type()->toString() + "' does not define Comparison operators. '*'.");
		}
	}
}


llvm::Value* ComparisonExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a_code = a->emitLLVMCode(params);
	llvm::Value* b_code = b->emitLLVMCode(params);

	switch(a->type()->getType())
	{
	case Type::FloatType:
		{
			switch(this->token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: return params.builder->CreateFCmpOLT(a_code, b_code);
			case RIGHT_ANGLE_BRACKET_TOKEN: return params.builder->CreateFCmpOGT(a_code, b_code);
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateFCmpOEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateFCmpONE(a_code, b_code);
			case LESS_EQUAL_TOKEN: return params.builder->CreateFCmpOLE(a_code, b_code);
			case GREATER_EQUAL_TOKEN: return params.builder->CreateFCmpOGE(a_code, b_code);
			default: assert(0); throw BaseException("Unsupported token type for comparison");
			}
		}
		break;
	case Type::IntType:
		{
			switch(this->token->getType())
			{
			case LEFT_ANGLE_BRACKET_TOKEN: return params.builder->CreateICmpSLT(a_code, b_code);
			case RIGHT_ANGLE_BRACKET_TOKEN: return params.builder->CreateICmpSGT(a_code, b_code);
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateICmpEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateICmpNE(a_code, b_code);
			case LESS_EQUAL_TOKEN: return params.builder->CreateICmpSLE(a_code, b_code);
			case GREATER_EQUAL_TOKEN: return params.builder->CreateICmpSGE(a_code, b_code);
			default: assert(0); throw BaseException("Unsupported token type for comparison");
			}
		}
		break;
	default:
		assert(!"ComparisonExpression type invalid!");
		throw BaseException("ComparisonExpression type invalid");
	}
}


Reference<ASTNode> ComparisonExpression::clone()
{
	return Reference<ASTNode>(new ComparisonExpression(token, a, b));
}


//---------------------------------------------------------------------------------

#if 0
Value* AnonFunction::exec(VMState& vmstate)
{
	assert(0);

	// Evaluate let clauses, which will each push the result onto the let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	Value* ret = body->exec(vmstate);

	// Pop things off let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.pop_back();

	return ret;
	
}


void AnonFunction::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "AnonFunction\n";
	this->body->print(depth+1, s);
}


void AnonFunction::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);
	body->traverse(payload, stack);
	stack.pop_back();
}


llvm::Value* AnonFunction::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return NULL;
}

#endif


} //end namespace Lang
