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


static bool isIntExactlyRepresentableAsFloat(int x)
{
	return ((int)((float)x)) == x;
}


static bool expressionIsWellTyped(ASTNodeRef& e, TraversalPayload& payload_)
{
	// NOTE: do this without exceptions?
	try
	{
		vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCheck, payload_.hidden_voidptr_arg, payload_.env);
		e->traverse(payload, stack);
		assert(stack.size() == 0);

		return true;
	}
	catch(BaseException& )
	{
		return false;
	}
}


static bool shouldFoldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	return	e.nonNull() &&
			e->isConstant() && 
			(	(e->type()->getType() == Type::FloatType &&		
				(e->nodeType() != ASTNode::FloatLiteralType)) ||
				(e->type()->getType() == Type::BoolType &&		
				(e->nodeType() != ASTNode::BoolLiteralType)) ||
				(e->type()->getType() == Type::IntType &&
				(e->nodeType() != ASTNode::IntLiteralType))
			) &&
			expressionIsWellTyped(e, payload);
}
	

static ASTNodeRef foldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	VMState vmstate(payload.hidden_voidptr_arg);
	vmstate.func_args_start.push_back(0);
	if(payload.hidden_voidptr_arg)
		vmstate.argument_stack.push_back(ValueRef(new VoidPtrValue(payload.env)));

	ValueRef retval = e->exec(vmstate);

	assert(vmstate.argument_stack.size() == 1);
	//delete vmstate.argument_stack[0];
	vmstate.func_args_start.pop_back();

	if(e->type()->getType() == Type::FloatType)
	{
		assert(dynamic_cast<FloatValue*>(retval.getPointer()));
		FloatValue* val = static_cast<FloatValue*>(retval.getPointer());

		return ASTNodeRef(new FloatLiteral(val->value));
	}
	else if(e->type()->getType() == Type::IntType)
	{
		assert(dynamic_cast<IntValue*>(retval.getPointer()));
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		return ASTNodeRef(new IntLiteral(val->value));
	}
	else if(e->type()->getType() == Type::BoolType)
	{
		assert(dynamic_cast<BoolValue*>(retval.getPointer()));
		BoolValue* val = static_cast<BoolValue*>(retval.getPointer());

		return ASTNodeRef(new BoolLiteral(val->value));
	}
	else
	{
		assert(0);
		return ASTNodeRef(NULL);
	}
}


void checkFoldExpression(ASTNodeRef& e, TraversalPayload& payload)
{
	if(shouldFoldExpression(e, payload))
	{
		e = foldExpression(e, payload);
		payload.tree_changed = true;
	}
}


void convertOverloadedOperators(ASTNodeRef& e, TraversalPayload& payload)
{
	if(e.isNull())
		return;

	switch(e->nodeType())
	{
	case ASTNode::AdditionExpressionType:
	{
		AdditionExpression* expr = static_cast<AdditionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(*expr->a->type() == *expr->b->type()) // If a and b have the same types
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_add function call.
					e = ASTNodeRef(new FunctionExpression("op_add", expr->a, expr->b));
					payload.tree_changed = true;
				}
		break;
	}
	case ASTNode::SubtractionExpressionType:
	{
		SubtractionExpression* expr = static_cast<SubtractionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(*expr->a->type() == *expr->b->type()) // If a and b have the same types
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_add function call.
					e = ASTNodeRef(new FunctionExpression("op_sub", expr->a, expr->b));
					payload.tree_changed = true;
				}
		break;
	}
	case ASTNode::MulExpressionType:
	{
		MulExpression* expr = static_cast<MulExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(*expr->a->type() == *expr->b->type()) // If a and b have the same types
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_add function call.
					e = ASTNodeRef(new FunctionExpression("op_mul", expr->a, expr->b));
					payload.tree_changed = true;
				}
		break;
	}
	case ASTNode::DivExpressionType:
	{
		DivExpression* expr = static_cast<DivExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(*expr->a->type() == *expr->b->type()) // If a and b have the same types
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_add function call.
					e = ASTNodeRef(new FunctionExpression("op_div", expr->a, expr->b));
					payload.tree_changed = true;
				}
		break;
	}
	};
}


template <class T> 
T cast(ValueRef& v)
{
	assert(dynamic_cast<T>(v.getPointer()) != NULL);
	return static_cast<T>(v.getPointer());
}


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


bool BufferRoot::isConstant() const
{
	assert(0);
	return false;
}


//----------------------------------------------------------------------------------


FunctionDefinition::FunctionDefinition(const std::string& name, const std::vector<FunctionArg>& args_, 
									   //const vector<Reference<LetASTNode> >& lets_,
									   const ASTNodeRef& body_, const TypeRef& declared_rettype, 
									   BuiltInFunctionImpl* impl)
:	args(args_),
	//lets(lets_),
	body(body_),
	declared_return_type(declared_rettype),
	built_in_func_impl(impl),
	built_llvm_function(NULL),
	jitted_function(NULL),
	use_captured_vars(false),
	closure_type(false)
{
	sig.name = name;
	for(unsigned int i=0; i<args_.size(); ++i)
		sig.param_types.push_back(args_[i].type);

	// TODO: fix this, make into method
	//function_type = TypeRef(new Function(sig.param_types, declared_rettype));

	//this->let_exprs_llvm_value = std::vector<llvm::Value*>(this->lets.size(), NULL);
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


TypeRef FunctionDefinition::type() const
{
	vector<TypeRef> arg_types(this->args.size());
	for(size_t i=0; i<this->args.size(); ++i)
		arg_types[i] = this->args[i].type;

	return TypeRef(new Function(arg_types, this->returnType()));
}


ValueRef FunctionDefinition::exec(VMState& vmstate)
{
	// Capture variables at this point, by getting them off the arg and let stack.
	vector<ValueRef> vals;
	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		if(this->captured_vars[i].vartype == CapturedVar::Arg)
		{
			vals.push_back(vmstate.argument_stack[vmstate.func_args_start.back() + this->captured_vars[i].index]);
		}
		else if(this->captured_vars[i].vartype == CapturedVar::Let)
		{
			const int let_frame_offset = this->captured_vars[i].let_frame_offset;
			assert(let_frame_offset < (int)vmstate.let_stack_start.size());

			const int let_stack_start = vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - let_frame_offset];
			vals.push_back(vmstate.let_stack[let_stack_start]);
		}
		else
		{
			assert(0);
		}
	}

	// Put captured values into the variable struct.
	Reference<StructureValue> var_struct(new StructureValue(vals));

	return ValueRef(new FunctionValue(this, var_struct));
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
		std::cout << vmstate.argument_stack[i]->toString() + (i + 1 < vmstate.argument_stack.size() ? string(", ") : string());
	std::cout << "]\n";
}



ValueRef FunctionDefinition::invoke(VMState& vmstate)
{
	if(VERBOSE_EXEC) 
	{
		std::cout << indent(vmstate) << "FunctionDefinition, name=" << this->sig.name << "\n";
		printStack(vmstate);
	}

	if(this->built_in_func_impl)
		return this->built_in_func_impl->invoke(vmstate);

	
	// Evaluate let clauses, which will each push the result onto the let stack
	//vmstate.let_stack_start.push_back(vmstate.let_stack.size()); // Push let frame index
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	// Execute body of function
	ValueRef ret = body->exec(vmstate);

	// Pop things off let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//{
	//	//delete vmstate.let_stack.back();
	//	vmstate.let_stack.pop_back();
	//}
	//// Pop let frame index
	//vmstate.let_stack_start.pop_back();

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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		// Don't try and fold down generic expressions, since we can't evaluate expressions without knowing the types involved.
		if(!this->isGenericFunction())
			checkFoldExpression(body, payload);
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(body, payload);
	}

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

	//if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	//{
		// If this is a generic function, we can't try and bind function expressions yet,
		// because the binding depends on argument type due to function overloading, so we have to wait
		// until we know the concrete type.
//		if(this->isGenericFunction())
//			return; // Don't try and bind functions yet.
	//}

	//bool old_use_captured_vars = payload.capture_variables;
	//if(payload.operation == TraversalPayload::BindVariables)
	//{
	//	if(this->use_captured_vars) // if we are an anon function...
	//		payload.capture_variables = true; // Tell varables in function expression tree to capture
	//}

	payload.func_def_stack.push_back(this);

	stack.push_back(this);

	//for(unsigned int i=0; i<lets.size(); ++i)
	//	lets[i]->traverse(payload, stack);

	if(this->body.nonNull()) // !this->built_in_func_impl)
		this->body->traverse(payload, stack);

	stack.pop_back();

	payload.func_def_stack.pop_back();

	//payload.capture_variables = old_use_captured_vars;

	if(payload.operation == TraversalPayload::BindVariables)
	{
		this->captured_vars = payload.captured_vars;
	}


	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		if(this->declared_return_type.nonNull() &&
			this->declared_return_type->getType() == Type::FloatType && 
			this->body.nonNull() && 
			this->body->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* body_lit = static_cast<IntLiteral*>(this->body.getPointer());
			if(isIntExactlyRepresentableAsFloat(body_lit->value))
			{
				ASTNodeRef new_body(new FloatLiteral((float)body_lit->value));

				this->body = new_body;
				payload.tree_changed = true;
			}
		}

	}

}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef: " << this->sig.toString() << " " << 
		(this->returnType().nonNull() ? this->returnType()->toString() : "[Unknown ret type]");
	if(this->declared_return_type.nonNull())
		s << " (Declared ret type: " + this->declared_return_type->toString() << ")";
	s << "\n";
	//for(unsigned int i=0; i<this->lets.size(); ++i)
	//	lets[i]->print(depth + 1, s);

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
	if(this->captured_vars.empty())
	{

	}

	// This will be called for lambda expressions.
	// Capture variables at this point, by getting them off the arg and let stack.

	//vector<llvm::Value> vals;

	// Load pointer to closure
	llvm::Value* closure_pointer;
	closure_pointer = params.builder->CreateAlloca(
		//this->getClosureStructLLVMType(*params.context)
		this->type()->LLVMType(*params.context)
	);

	{
		// Get type of this function
		llvm::FunctionType* target_func_type = LLVMTypeUtils::llvmFunctionType(
			this->sig.param_types, 
			this->returnType(), 
			*params.context,
			params.hidden_voidptr_arg
		);

		// Get pointer to this function
		llvm::Constant* func = params.module->getOrInsertFunction(
			this->sig.toString(), // Name
			target_func_type // Type
		);

		// Store function pointer in the closure structure
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end()
		);

		// Do the store.
		params.builder->CreateStore(
			func, // value
			field_ptr // ptr
		);

	}

	// for each captured var
	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		llvm::Value* val = NULL;
		// If arg type
		if(this->captured_vars[i].vartype == CapturedVar::Arg)
		{
			// Load arg
			//NOTE: offset if return by ref
			val = LLVMTypeUtils::getNthArg(params.currently_building_func, this->captured_vars[i].index);
		}
		// else if let type
		else if(this->captured_vars[i].vartype == CapturedVar::Let)
		{
			// Load let:
			// Walk up AST until we get to the correct let block
			const int let_frame_offset = this->captured_vars[i].let_frame_offset;

			// TODO: set val
		}
			
		// store in captured var structure field
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
		// Offset by one to allow room for function pointer.
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i + 1, true)));
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end()
		);

		params.builder->CreateStore(
			val, // value
			field_ptr // ptr
		);
	}

	return closure_pointer;
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module,
	const PlatformUtils::CPUInfo& cpu_info,
	bool hidden_voidptr_arg
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	)
{
#if USE_LLVM
	llvm::FunctionType* functype = LLVMTypeUtils::llvmFunctionType(
		this->sig.param_types, 
		returnType(), 
		module->getContext(),
		hidden_voidptr_arg
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



	
	llvm::Function* llvm_func = static_cast<llvm::Function*>(module->getOrInsertFunction(
		this->sig.toString(), // Name
		functype // Type
		));

	llvm_func->setAttributes(attribute_list);

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	llvm_func->setCallingConv(llvm::CallingConv::C);

	//internal_llvm_func->setAttributes(

	// Set names for all arguments.
	/*
	NOTE: for some reason this crashes with optimisations enabled.
	unsigned int i = 0;
	for(llvm::Function::arg_iterator AI = llvm_func->arg_begin(); AI != llvm_func->arg_end(); ++AI, ++i)
	{
		if(this->returnType()->passByValue())
		{					
			if(i >= this->args.size())
				AI->setName("hidden");
			else
				AI->setName(this->args[i].name);
		}
		else
		{
			if(i == 0)
				AI->setName("ret");
			else if(i > this->args.size())
				AI->setName("hidden");
			else
			{
				std::cout << i << std::endl;
				AI->setName(this->args[i-1].name);
			}
		}
	}*/


	llvm::BasicBlock* block = llvm::BasicBlock::Create(
		module->getContext(), 
		"entry", 
		llvm_func
	);
	llvm::IRBuilder<> builder(block);

	// Build body LLVM code
	EmitLLVMCodeParams params;
	params.currently_building_func_def = this;
	params.cpu_info = &cpu_info;
	params.builder = &builder;
	params.module = module;
	params.currently_building_func = llvm_func;
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
			llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

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
			else if(this->returnType()->getType() == Type::FunctionType)
			{
				// Structure types are also passed by ref.

				// Load value through body_code pointer.
				llvm::Value* struct_val = params.builder->CreateLoad(
					body_code
				);

				// Save value out to return_val ptr.
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


	this->built_llvm_function = llvm_func;
	return llvm_func;
#else
	return NULL;
#endif
}


Reference<ASTNode> FunctionDefinition::clone()
{
	assert(0);
	throw BaseException("FunctionDefinition::clone()");
}


bool FunctionDefinition::isGenericFunction() const // true if it is parameterised by type.
{
	for(size_t i=0; i<this->args.size(); ++i)
		if(this->args[i].type->getType() == Type::GenericTypeType)
			return true;
	return false;
}


/*llvm::Value* FunctionDefinition::getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index)
{
	if(let_exprs_llvm_value[let_index] == NULL)
	{
		let_exprs_llvm_value[let_index] = this->lets[let_index]->emitLLVMCode(params);
	}

	return let_exprs_llvm_value[let_index];
}*/


bool FunctionDefinition::isConstant() const
{
	//assert(!"FunctionDefinition::isConstant()");
	return false;//TEMP
}


//llvm::Type* FunctionDefinition::getClosureStructLLVMType(llvm::LLVMContext& context) const
//{
//	vector<const llvm::Type*> field_types;
//
//	// Add pointer to function type
//	field_types.push_back(this->type()->LLVMType(context));
//
//	for(size_t i=0; i<this->captured_vars.size(); ++i)
//	{
//		field_types.push_back(this->captured_vars[i].type->LLVMType(context));
//	}
//
//	return llvm::StructType::get(
//		context,
//		field_types
//	);
//}


//--------------------------------------------------------------------------------


FunctionExpression::FunctionExpression() 
:	target_function(NULL),
	bound_index(-1),
	binding_type(Unbound),
	bound_let_block(NULL),
	bound_function(NULL),
	use_captured_var(false),
	captured_var_index(0)
{
}


FunctionExpression::FunctionExpression(const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1) // 2-arg function
:	target_function(NULL),
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

		ValueRef arg = vmstate.let_stack[vmstate.let_stack_start.back() + this->bound_index];
		assert(dynamic_cast<FunctionValue*>(arg.getPointer()));
		FunctionValue* function_value = dynamic_cast<FunctionValue*>(arg.getPointer());
		function_value_out = function_value;
		return function_value->func_def;
	}
}


ValueRef FunctionExpression::exec(VMState& vmstate)
{
	if(VERBOSE_EXEC) std::cout << indent(vmstate) << "FunctionExpression, target_name=" << this->function_name << "\n";
	
	//assert(target_function);
	if(this->target_function != NULL && this->target_function->external_function.nonNull())
	{
		vector<ValueRef> args;
		for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
			args.push_back(this->argument_expressions[i]->exec(vmstate));

		if(vmstate.hidden_voidptr_arg)
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

	if(VERBOSE_EXEC)
		std::cout << indent(vmstate) << "Calling " << this->function_name << ", func_args_start: " << vmstate.func_args_start.back() << "\n";

	// Execute target function
	vmstate.func_args_start.push_back(initial_arg_stack_size);
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
	int let_frame_offset = 0;


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

						if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
						{
							this->captured_var_index = payload.captured_vars.size();
							this->use_captured_var = true;

							// Add this function argument as a variable that has to be captured for closures.
							CapturedVar var;
							var.vartype = CapturedVar::Arg;
							var.index = i;
							payload.captured_vars.push_back(var);
						}
					}
				}
				in_current_func_def = false;
			}
			else if(LetBlock* let_block = dynamic_cast<LetBlock*>(stack[s]))
			{
				for(unsigned int i=0; i<let_block->lets.size(); ++i)
					if(let_block->lets[i]->variable_name == this->function_name && doesFunctionTypeMatch(let_block->lets[i]->type()))
					{
						this->bound_index = i;
						this->binding_type = Let;
						this->bound_let_block = let_block;
						found_binding = true;

						if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
						{
							this->captured_var_index = payload.captured_vars.size();
							this->use_captured_var = true;

							// Add this function argument as a variable that has to be captured for closures.
							CapturedVar var;
							var.vartype = CapturedVar::Let;
							var.index = i;
							var.let_frame_offset = let_frame_offset;
							payload.captured_vars.push_back(var);
						}

					}
				
				// We only want to count an ancestor let block as an offsetting block if we are not currently in a let clause of it.
				bool is_this_let_clause = false;
				if(s + 1 < (int)stack.size())
					for(size_t z=0; z<let_block->lets.size(); ++z)
						if(let_block->lets[z].getPointer() == stack[s+1])
							is_this_let_clause = true;
				if(!is_this_let_clause)
					let_frame_offset++;
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
						this->argument_expressions[i] = ASTNodeRef(new FloatLiteral((float)static_cast<IntLiteral*>(argument_expressions[i].getPointer())->value));
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
				throw BaseException(s);
			}
		}

		if(this->binding_type == Unbound)
			throw BaseException("Failed to find function '" + sig.toString() + "'");
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
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		for(size_t i=0; i<argument_expressions.size(); ++i)
			convertOverloadedOperators(argument_expressions[i], payload);
	}



	// NOTE: we want to do a post-order traversal here.
	// Thhis is because we want our argument expressions to be linked first.

	stack.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->traverse(payload, stack);

	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables) // LinkFunctions)
	{
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

	llvm::Value* closure_pointer = NULL;

	if(binding_type == Let)
	{
		//target_llvm_func = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index);

		closure_pointer = this->bound_let_block->getLetExpressionLLVMValue(params, bound_index);

		// Load function pointer from closure.

		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end()
		);

		target_llvm_func = params.builder->CreateLoad(field_ptr);
	}
	else if(binding_type == Arg)
	{
		// If the current function returns its result via pointer, then all args are offset by one.
		if(params.currently_building_func_def->returnType()->passByValue())
			closure_pointer = LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index);
		else
			closure_pointer = LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index + 1);

		//target_llvm_func = dynamic_cast<llvm::Function*>(func);

		// Load function pointer from closure.

		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end()
		);

		target_llvm_func = params.builder->CreateLoad(field_ptr);
	}
	else if(binding_type == BoundToGlobalDef)
	{
		// Lookup LLVM function, which should already be created and added to the module.
		/*llvm::Function* target_llvm_func = params.module->getFunction(
			this->target_function->sig.toString() //internalFuncName(call_target_sig)
			);
		assert(target_llvm_func);*/

		FunctionSignature target_sig = this->target_function->sig;

		llvm::FunctionType* target_func_type = LLVMTypeUtils::llvmFunctionType(
			target_sig.param_types, 
			target_ret_type, 
			*params.context,
			params.hidden_voidptr_arg
		);

		target_llvm_func = params.module->getOrInsertFunction(
			target_sig.toString(), // Name
			target_func_type // Type
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


	//------------------
	// Build args list

	if(target_ret_type->passByValue())
	{
		vector<llvm::Value*> args;

		for(unsigned int i=0; i<argument_expressions.size(); ++i)
			args.push_back(argument_expressions[i]->emitLLVMCode(params));

		// Set hidden voidptr argument
		if(params.hidden_voidptr_arg)
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

		// Set hidden voidptr argument
		if(params.hidden_voidptr_arg)
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
	FunctionExpression* e = new FunctionExpression();
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
	// For now, we'll saw a function expression bound to an argument of let var is not constant.
	if(this->binding_type != BoundToGlobalDef)
		return false;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		if(!argument_expressions[i]->isConstant())
			return false;
	return true;
}


//-----------------------------------------------------------------------------------


Variable::Variable(const std::string& name_)
:	name(name_),
	bound_index(-1),
	bound_function(NULL),
	bound_let_block(NULL),
	use_captured_var(false),
	captured_var_index(0)
{
}


void Variable::bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack)
{
	bool in_current_func_def = true;
	int let_frame_offset = 0;
	for(int s = (int)stack.size() - 1; s >= 0; --s) // Walk up the stack of ancestor nodes
	{
		if(FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[s])) // If node is a function definition:
		{
			for(unsigned int i=0; i<def->args.size(); ++i) // For each argument to the function:
				if(def->args[i].name == this->name) // If the argument name matches this variable name:
				{
					// Bind this variable to the argument.
					this->vartype = ArgumentVariable;
					this->bound_index = i;
					this->bound_function = def;


					if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
					{
						this->captured_var_index = payload.captured_vars.size();
						this->use_captured_var = true;

						// Add this function argument as a variable that has to be captured for closures.
						CapturedVar var;
						var.vartype = CapturedVar::Arg;
						var.index = i;
						payload.captured_vars.push_back(var);
					}
					return;
				}

			in_current_func_def = false;
		}
		else if(LetBlock* let_block = dynamic_cast<LetBlock*>(stack[s]))
		{
			for(unsigned int i=0; i<let_block->lets.size(); ++i)
				if(let_block->lets[i]->variable_name == this->name)
				{
					this->vartype = LetVariable;
					this->bound_index = i;
					this->bound_let_block = let_block;

					if(!in_current_func_def && payload.func_def_stack.back()->use_captured_vars)
					{
						this->captured_var_index = payload.captured_vars.size();
						this->use_captured_var = true;

						// Add this function argument as a variable that has to be captured for closures.
						CapturedVar var;
						var.vartype = CapturedVar::Let;
						var.index = i;
						var.let_frame_offset = let_frame_offset;
						payload.captured_vars.push_back(var);
					}
		
					return;
				}

			// We only want to count an ancestor let block as an offsetting block if we are not currently in a let clause of it.
			bool is_this_let_clause = false;
			for(size_t z=0; z<let_block->lets.size(); ++z)
				if(let_block->lets[z].getPointer() == stack[s+1])
					is_this_let_clause = true;
			if(!is_this_let_clause)
				let_frame_offset++;
		}
	}

	// Try and bind to top level function definition
//	BufferRoot* root = static_cast<BufferRoot*>(stack[0]);
//	vector<FunctionDefinitionRef
//	for(size_t i=0; i<stack[0]->get
	Frame::NameToFuncMapType::iterator res = payload.top_lvl_frame->name_to_functions_map.find(this->name);
	if(res != payload.top_lvl_frame->name_to_functions_map.end())
	{
		vector<FunctionDefinitionRef>& matching_functions = res->second;

		assert(matching_functions.size() > 0);

		if(matching_functions.size() > 1)
			throw BaseException("Ambiguous binding for variable '" + this->name + "': multiple functions with name.");

		this->vartype = BoundToGlobalDefVariable;
		this->bound_function = matching_functions[0].getPointer();
		return;
	}


	throw BaseException("Variable::bindVariables(): No such function, function argument or let definition '" + this->name + "'");
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
		this->bindVariables(payload, stack);
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(use_captured_var)
	{
		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		ValueRef captured_struct = vmstate.argument_stack.back();
		assert(dynamic_cast<StructureValue*>(captured_struct.getPointer()));
		StructureValue* s = static_cast<StructureValue*>(captured_struct.getPointer());

		return s->fields[this->captured_var_index];
	}


	if(this->vartype == ArgumentVariable)
	{
		return vmstate.argument_stack[vmstate.func_args_start.back() + bound_index];
	}
	else if(this->vartype == LetVariable)
	{
		return vmstate.let_stack[vmstate.let_stack_start.back() + bound_index];
	}
	else if(this->vartype == BoundToGlobalDefVariable)
	{
		StructureValueRef captured_vars(new StructureValue(vector<ValueRef>()));
		return ValueRef(new FunctionValue(this->bound_function, captured_vars));
	}
	else
	{
		assert(!"invalid vartype.");
		return ValueRef(NULL);
	}
}


TypeRef Variable::type() const
{
	if(this->vartype == LetVariable)
		return this->bound_let_block->lets[this->bound_index]->type();
	else if(this->vartype == ArgumentVariable)
		return this->bound_function->args[this->bound_index].type;
	else if(this->vartype == BoundToGlobalDefVariable)
		return this->bound_function->type();
	else
	{
		assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


inline static const std::string varType(Variable::VariableType t)
{
	return t == Variable::LetVariable ? "Let" : (t == Variable::ArgumentVariable ? "Arg" : "BoundToGlobalDef");
}


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Variable, name=" << this->name << ", " + varType(this->vartype) + ", bound_index=" << bound_index << "\n";
}


llvm::Value* Variable::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(vartype == LetVariable)
	{
		return this->bound_let_block->getLetExpressionLLVMValue(params, this->bound_index);
	}
	else if(vartype == ArgumentVariable)
	{
		assert(this->bound_function);

		//if(shouldPassByValue(*this->type()))
		//{
			// If the current function returns its result via pointer, then all args are offset by one.
			if(params.currently_building_func_def->returnType()->passByValue())
				return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index);
			else
				return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index + 1);
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
	else
	{
		return this->bound_function->emitLLVMCode(params);
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


ValueRef FloatLiteral::exec(VMState& vmstate)
{
	return ValueRef(new FloatValue(value));
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


ValueRef IntLiteral::exec(VMState& vmstate)
{
	return ValueRef(new IntValue(value));
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


ValueRef BoolLiteral::exec(VMState& vmstate)
{
	return ValueRef(new BoolValue(value));
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


ValueRef MapLiteral::exec(VMState& vmstate)
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
	return ValueRef();
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<items.size(); ++i)
		{
			if(shouldFoldExpression(items[i].first, payload))
			{
				items[i].first = foldExpression(items[i].first, payload);
				payload.tree_changed = true;
			}
			if(shouldFoldExpression(items[i].second, payload))
			{
				items[i].second = foldExpression(items[i].second, payload);
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		for(size_t i=0; i<items.size(); ++i)
		{
			convertOverloadedOperators(items[i].first, payload);
			convertOverloadedOperators(items[i].second, payload);
		}
	}


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


bool MapLiteral::isConstant() const
{
	for(size_t i=0; i<items.size(); ++i)
		if(!items[i].first->isConstant() || !items[i].second->isConstant())
			return false;
	return true;
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


ValueRef ArrayLiteral::exec(VMState& vmstate)
{
	vector<ValueRef> elem_values(elements.size());

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		elem_values[i] = this->elements[i]->exec(vmstate);
	}

	return ValueRef(new ArrayValue(elem_values));
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<elements.size(); ++i)
		{
			if(shouldFoldExpression(elements[i], payload))
			{
				elements[i] = foldExpression(elements[i], payload);
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		for(size_t i=0; i<elements.size(); ++i)
			convertOverloadedOperators(elements[i], payload);
	}


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


bool ArrayLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
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


ValueRef VectorLiteral::exec(VMState& vmstate)
{
	vector<ValueRef> elem_values(elements.size());

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		elem_values[i] = this->elements[i]->exec(vmstate);
	}

	return ValueRef(new VectorValue(elem_values));
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<elements.size(); ++i)
		{
			if(shouldFoldExpression(elements[i], payload))
			{
				elements[i] = foldExpression(elements[i], payload);
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		for(size_t i=0; i<elements.size(); ++i)
			convertOverloadedOperators(elements[i], payload);
	}


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


bool VectorLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


//------------------------------------------------------------------------------------------


ValueRef StringLiteral::exec(VMState& vmstate)
{
	return ValueRef(new StringValue(value));
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


class AddOp
{
public:
	float operator() (float x, float y) { return x + y; }
	int operator() (int x, int y) { return x + y; }
};


class SubOp
{
public:
	float operator() (float x, float y) { return x - y; }
	int operator() (int x, int y) { return x - y; }
};


class MulOp
{
public:
	float operator() (float x, float y) { return x * y; }
	int operator() (int x, int y) { return x * y; }
};


template <class Op>
ValueRef execBinaryOp(VMState& vmstate, ASTNodeRef& a, ASTNodeRef& b, Op op)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);

	ValueRef retval;

	switch(a->type()->getType())
	{
	case Type::FloatType:
		retval = ValueRef(new FloatValue(op(
			static_cast<FloatValue*>(aval.getPointer())->value,
			static_cast<FloatValue*>(bval.getPointer())->value
		)));
		break;
	case Type::IntType:
		retval = ValueRef(new IntValue(op(
			static_cast<IntValue*>(aval.getPointer())->value,
			static_cast<IntValue*>(bval.getPointer())->value
		)));
		break;
	case Type::VectorTypeType:
		{
		TypeRef this_type = a->type();
		VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

		VectorValue* aval_vec = static_cast<VectorValue*>(aval.getPointer());
		VectorValue* bval_vec = static_cast<VectorValue*>(bval.getPointer());
		vector<ValueRef> elem_values(aval_vec->e.size());
		switch(vectype->t->getType())
		{
		case Type::FloatType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new FloatValue(op(
					static_cast<FloatValue*>(aval_vec->e[i].getPointer())->value,
					static_cast<FloatValue*>(bval_vec->e[i].getPointer())->value
				)));
			break;
		case Type::IntType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new IntValue(op(
					static_cast<IntValue*>(aval_vec->e[i].getPointer())->value,
					static_cast<IntValue*>(bval_vec->e[i].getPointer())->value
				)));
			break;
		default:
			assert(!"expression vector field type invalid!");
		};
		retval = ValueRef(new VectorValue(elem_values));
		break;
		}
	default:
		assert(!"expression type invalid!");
	}

	return retval;
}


ValueRef AdditionExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, AddOp());
/*
	Value* aval = a->exec(vmstate).getPointer();
	Value* bval = b->exec(vmstate).getPointer();

	ValueRef retval;

	switch(this->type()->getType())
	{
	case Type::FloatType:
		retval = ValueRef(new FloatValue(static_cast<FloatValue*>(aval)->value + static_cast<FloatValue*>(bval)->value));
		break;
	case Type::IntType:
		retval = ValueRef(new IntValue(static_cast<IntValue*>(aval)->value + static_cast<IntValue*>(bval)->value));
		break;
	case Type::VectorTypeType:
		{
		TypeRef this_type = this->type();
		VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);
		vector<ValueRef> elem_values(aval_vec->e.size());
		switch(vectype->t->getType())
		{
		case Type::FloatType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new FloatValue(static_cast<FloatValue*>(aval_vec->e[i].getPointer())->value + static_cast<FloatValue*>(bval_vec->e[i].getPointer())->value));
			break;
		case Type::IntType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new IntValue(static_cast<IntValue*>(aval_vec->e[i].getPointer())->value + static_cast<IntValue*>(bval_vec->e[i].getPointer())->value));
			break;
		default:
			assert(!"additionexpression vector field type invalid!");
		};
		retval = ValueRef(new VectorValue(elem_values));
		break;
		}
	default:
		assert(!"additionexpression type invalid!");
	}

	return retval;
	*/
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(a, payload))
		{
			a = foldExpression(a, payload);
			payload.tree_changed = true;
		}
		if(shouldFoldExpression(b, payload))
		{
			b = foldExpression(b, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload);
		convertOverloadedOperators(b, payload);
	}


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float in addition operation:
		// 3.0 + 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value));
				payload.tree_changed = true;
			}
		}

		// 3 + 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value));
				payload.tree_changed = true;
			}
		}
	}

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
	}
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


bool AdditionExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//-------------------------------------------------------------------------------------------------


ValueRef SubtractionExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, SubOp());
/*
	Value* aval = a->exec(vmstate).getPointer();
	Value* bval = b->exec(vmstate).getPointer();

	ValueRef retval;

	switch(this->type()->getType())
	{
	case Type::FloatType:
		retval = ValueRef(new FloatValue(static_cast<FloatValue*>(aval)->value - static_cast<FloatValue*>(bval)->value));
		break;
	case Type::IntType:
		retval = ValueRef(new IntValue(static_cast<IntValue*>(aval)->value - static_cast<IntValue*>(bval)->value));
		break;
	case Type::VectorTypeType:
		{
		TypeRef this_type = this->type();
		VectorType* vectype = static_cast<VectorType*>(this_type.getPointer());

		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);
		vector<ValueRef> elem_values(aval_vec->e.size());
		switch(vectype->t->getType())
		{
		case Type::FloatType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new FloatValue(static_cast<FloatValue*>(aval_vec->e[i].getPointer())->value - static_cast<FloatValue*>(bval_vec->e[i].getPointer())->value));
			break;
		case Type::IntType:
			for(unsigned int i=0; i<elem_values.size(); ++i)
				elem_values[i] = ValueRef(new IntValue(static_cast<IntValue*>(aval_vec->e[i].getPointer())->value - static_cast<IntValue*>(bval_vec->e[i].getPointer())->value));
			break;
		default:
			assert(!"SubtractionExpression vector field type invalid!");
		};
		retval = ValueRef(new VectorValue(elem_values));
		break;
		}
	default:
		assert(!"SubtractionExpression type invalid!");
	}

	return retval;
	*/
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(a, payload))
		{
			a = foldExpression(a, payload);
			payload.tree_changed = true;
		}
		if(shouldFoldExpression(b, payload))
		{
			b = foldExpression(b, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload);
		convertOverloadedOperators(b, payload);
	}


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 - 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value));
				payload.tree_changed = true;
			}
		}

		// 3 - 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value));
				payload.tree_changed = true;
			}
		}
	}

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
	}

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


bool SubtractionExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//-------------------------------------------------------------------------------------------------------


ValueRef MulExpression::exec(VMState& vmstate)
{
	return execBinaryOp(vmstate, a, b, MulOp());
	/*
	Value* aval = a->exec(vmstate).getPointer();
	Value* bval = b->exec(vmstate).getPointer();
	ValueRef retval;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = ValueRef(new FloatValue(static_cast<FloatValue*>(aval)->value * static_cast<FloatValue*>(bval)->value));
	}
	else if(this->type()->getType() == Type::IntType)
	{
		retval = ValueRef(new IntValue(static_cast<IntValue*>(aval)->value * static_cast<IntValue*>(bval)->value));
	}
	else if(this->type()->getType() == Type::VectorTypeType)
	{
		VectorValue* aval_vec = static_cast<VectorValue*>(aval);
		VectorValue* bval_vec = static_cast<VectorValue*>(bval);

		vector<ValueRef> elem_values(aval_vec->e.size());
		for(unsigned int i=0; i<elem_values.size(); ++i)
		{
			elem_values[i] = ValueRef(new FloatValue(static_cast<FloatValue*>(aval_vec->e[i].getPointer())->value * static_cast<FloatValue*>(bval_vec->e[i].getPointer())->value));
		}

		retval = ValueRef(new VectorValue(elem_values));
	}
	else
	{
		assert(!"mulexpression type invalid!");
	}
	return retval;
	*/
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(a, payload))
		{
			a = foldExpression(a, payload);
			payload.tree_changed = true;
		}
		if(shouldFoldExpression(b, payload))
		{
			b = foldExpression(b, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload);
		convertOverloadedOperators(b, payload);
	}


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 * 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value));
				payload.tree_changed = true;
			}
		}

		// 3 * 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value));
				payload.tree_changed = true;
			}
		}
	}


	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
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


bool MulExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//-------------------------------------------------------------------------------------------------------


ValueRef DivExpression::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);
	ValueRef retval;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = ValueRef(new FloatValue(static_cast<FloatValue*>(aval.getPointer())->value / static_cast<FloatValue*>(bval.getPointer())->value));
	}
	else if(this->type()->getType() == Type::IntType)
	{
		// TODO: catch divide by zero.
		retval = ValueRef(new IntValue(static_cast<IntValue*>(aval.getPointer())->value / static_cast<IntValue*>(bval.getPointer())->value));
	}
	else
	{
		assert(!"divexpression type invalid!");
	}
	return retval;
}


void DivExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(a, payload))
		{
			a = foldExpression(a, payload);
			payload.tree_changed = true;
		}
		if(shouldFoldExpression(b, payload))
		{
			b = foldExpression(b, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload);
		convertOverloadedOperators(b, payload);
	}

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 / 4
		// Only do this if b is != 0.  Otherwise we are messing with divide by zero semantics.
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value) && (b_lit->value != 0))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value));
				payload.tree_changed = true;
			}
		}

		// 3 / 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value));
				payload.tree_changed = true;
			}
		}
	}


	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'");
		}
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


bool DivExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//----------------------------------------------------------------------------------------


ValueRef UnaryMinusExpression::exec(VMState& vmstate)
{
	ValueRef aval = expr->exec(vmstate);
	ValueRef retval;

	if(this->type()->getType() == Type::FloatType)
	{
		retval = ValueRef(new FloatValue(-cast<FloatValue*>(aval)->value));
	}
	else if(this->type()->getType() == Type::IntType)
	{
		retval = ValueRef(new IntValue(-cast<IntValue*>(aval)->value));
	}
	else
	{
		assert(!"UnaryMinusExpression type invalid!");
	}
	return retval;
}


void UnaryMinusExpression::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(expr, payload))
		{
			expr = foldExpression(expr, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload);
	}

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


bool UnaryMinusExpression::isConstant() const
{
	return expr->isConstant();
}


//----------------------------------------------------------------------------------------


ValueRef LetASTNode::exec(VMState& vmstate)
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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(expr, payload))
		{
			expr = foldExpression(expr, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload);
	}

	stack.push_back(this);
	expr->traverse(payload, stack);
	stack.pop_back();
}


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


bool LetASTNode::isConstant() const
{
	return expr->isConstant();
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


ValueRef ComparisonExpression::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);

	ValueRef retval;

	switch(a->type()->getType())
	{
	case Type::FloatType:
		retval = ValueRef(compare<FloatValue>(this->token->getType(), aval.getPointer(), bval.getPointer()));
		break;
	case Type::IntType:
		retval = ValueRef(compare<IntValue>(this->token->getType(), aval.getPointer(), bval.getPointer()));
		break;
	default:
		assert(!"SubtractionExpression type invalid!");
	}

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
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(a, payload))
		{
			a = foldExpression(a, payload);
			payload.tree_changed = true;
		}
		if(shouldFoldExpression(b, payload))
		{
			b = foldExpression(b, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload);
		convertOverloadedOperators(b, payload);
	}

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


bool ComparisonExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//----------------------------------------------------------------------------------------


ValueRef LetBlock::exec(VMState& vmstate)
{
	const size_t let_stack_size = vmstate.let_stack.size();

	// Evaluate let clauses, which will each push the result onto the let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	vmstate.let_stack_start.push_back(let_stack_size); // Push let frame index

	return this->expr->exec(vmstate);

	// Pop things off let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.pop_back();
	
	// Pop let frame index
	vmstate.let_stack_start.pop_back();
}


void LetBlock::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Let Block.  lets:\n";
	for(size_t i=0; i<lets.size(); ++i)
		lets[i]->print(depth + 1, s);
	printMargin(depth, s); s << "in:\n";
	this->expr->print(depth+1, s);
}


void LetBlock::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		if(shouldFoldExpression(expr, payload))
		{
			expr = foldExpression(expr, payload);
			payload.tree_changed = true;
		}
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload);
	}

	stack.push_back(this);

	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->traverse(payload, stack);

	//payload.let_block_stack.push_back(this);

	expr->traverse(payload, stack);

	//payload.let_block_stack.pop_back();

	stack.pop_back();
}


llvm::Value* LetBlock::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return expr->emitLLVMCode(params);
}


Reference<ASTNode> LetBlock::clone()
{
	vector<Reference<LetASTNode> > new_lets(lets.size());
	for(size_t i=0; i<new_lets.size(); ++i)
		new_lets[i] = Reference<LetASTNode>(static_cast<LetASTNode*>(lets[i]->clone().getPointer()));
	return ASTNodeRef(new LetBlock(this->expr->clone(), new_lets));
}


bool LetBlock::isConstant() const
{
	//TODO: check let expressions for constants as well
	for(size_t i=0; i<lets.size(); ++i)
		if(!lets[i]->isConstant())
			return false;

	return expr->isConstant();
}


llvm::Value* LetBlock::getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index)
{
	if(let_exprs_llvm_value[let_index] == NULL)
	{
		let_exprs_llvm_value[let_index] = this->lets[let_index]->emitLLVMCode(params);
	}

	return let_exprs_llvm_value[let_index];
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
