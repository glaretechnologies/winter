/*=====================================================================
FunctionDefinition.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:40 +0100
=====================================================================*/
#include "wnt_FunctionDefinition.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMTypeUtils.h"
#include "utils/StringUtils.h"
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
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DataLayout.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <iostream>


using std::vector;
using std::string;


namespace Winter
{


static const bool VERBOSE_EXEC = false;


FunctionDefinition::FunctionDefinition(const SrcLocation& src_loc, int order_num_, const std::string& name, const std::vector<FunctionArg>& args_, 
									   //const vector<Reference<LetASTNode> >& lets_,
									   const ASTNodeRef& body_, const TypeRef& declared_rettype, 
									   const BuiltInFunctionImplRef& impl
									   )
:	ASTNode(FunctionDefinitionType, src_loc),
	order_num(order_num_),
	args(args_),
	//lets(lets_),
	body(body_),
	declared_return_type(declared_rettype),
	built_in_func_impl(impl),
	built_llvm_function(NULL),
	jitted_function(NULL),
	use_captured_vars(false),
	closure_type(NULL),
	alloc_func(NULL),
	is_anon_func(false),
	num_uses(0)
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
}


TypeRef FunctionDefinition::returnType() const
{
	//TEMP NEW:
	//if(this->body.nonNull() && this->body->type().nonNull())
	//	return this->body->type();

	if(this->declared_return_type.nonNull())
		return this->declared_return_type;

	//assert(this->body.nonNull());
	//assert(this->body->type().nonNull());
	return this->body->type();
	//return this->body.nonNull() ? this->body->type() : TypeRef(NULL);
}


TypeRef FunctionDefinition::type() const
{
	vector<TypeRef> arg_types(this->args.size());
	for(size_t i=0; i<this->args.size(); ++i)
		arg_types[i] = this->args[i].type;

	//vector<TypeRef> captured_var_types;
	//for(size_t i=0; i<this->captured_vars.size(); ++i)
	//	captured_var_types.push_back(this->captured_vars[i].type());
	const TypeRef return_type = this->returnType();
	if(return_type.isNull()) return NULL;

	return TypeRef(new Function(arg_types, return_type, /*captured_var_types, */this->use_captured_vars));
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
			//const int let_frame_offset = this->captured_vars[i].let_frame_offset;
			//assert(let_frame_offset < (int)vmstate.let_stack_start.size());

			//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - let_frame_offset];
			//vals.push_back(vmstate.let_stack[let_stack_start + this->captured_vars[i].index]);

			ValueRef val = this->captured_vars[i].bound_let_block->lets[this->captured_vars[i].index]->exec(vmstate);
			vals.push_back(val);

			//NOTE: this looks wrong, should use index here as well?
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


const std::string indent(VMState& vmstate)
{
	std::string s;
	for(unsigned int i=0; i<vmstate.func_args_start.size(); ++i)
		s += "  ";
	return s;
}


//static void printStack(VMState& vmstate)
//{
//	std::cout << indent(vmstate) << "arg Stack: [";
//	for(unsigned int i=0; i<vmstate.argument_stack.size(); ++i)
//		std::cout << vmstate.argument_stack[i]->toString() + (i + 1 < vmstate.argument_stack.size() ? string(",\n") : string("\n"));
//	std::cout << "]\n";
//}



ValueRef FunctionDefinition::invoke(VMState& vmstate)
{
	if(VERBOSE_EXEC) 
	{
		std::cout << indent(vmstate) << "FunctionDefinition, name=" << this->sig.name << "\n";
		//printStack(vmstate);
	}

	// Check the types of the arguments that we have received
	for(size_t i=0; i<this->args.size(); ++i)
	{
		ValueRef arg_val = vmstate.argument_stack[vmstate.func_args_start.back() + i];

		if(VERBOSE_EXEC)
		{
			std::cout << "Arg " << i << ": " << arg_val->toString() << std::endl;
		}
	}


	if(this->built_in_func_impl.nonNull())
		return this->built_in_func_impl->invoke(vmstate);

	
	// Evaluate let clauses, which will each push the result onto the let stack
	//vmstate.let_stack_start.push_back(vmstate.let_stack.size()); // Push let frame index
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	// Execute body of function
	ValueRef ret = body->exec(vmstate);

	if(this->declared_return_type.nonNull() && (*body->type() != *this->declared_return_type))
	{
		// This may happen since type checking may not have been done yet.
		throw BaseException("Returned object has invalid type.");
	}

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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		payload.func_def_stack.push_back(this);
		stack.push_back(this);
		convertOverloadedOperators(body, payload, stack);
		stack.pop_back();
		payload.func_def_stack.pop_back();
	}*/
	//if(payload.operation == TraversalPayload::TypeCheck)
	//{
	//	if(this->isGenericFunction())
	//		return; // Don't type check this.  Concrete versions of this func will be type checked individually.

	//	if(this->body.nonNull())
	//	{
	//		if(this->declared_return_type.nonNull())
	//		{
	//			// Check that the return type of the body expression is equal to the declared return type
	//			// of this function.
	//			if(*this->body->type() != *this->declared_return_type)
	//				throw BaseException("Type error for function '" + this->sig.toString() + "': Computed return type '" + this->body->type()->toString() + 
	//					"' is not equal to the declared return type '" + this->declared_return_type->toString() + "'." + errorContext(*this));
	//		}
	//		else
	//		{
	//			// Else return type is NULL, so infer it
	//			//this->return_type = this->body->type();
	//		}
	//	}
	//}

	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
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

	/*TEMP if(payload.operation == TraversalPayload::BindVariables)
	{
		// We want to bind to allocateRefCountedStructure()
		const FunctionSignature sig("allocateRefCountedStructure", std::vector<TypeRef>(1, TypeRef(new Int())));

		FunctionDefinitionRef the_alloc_func = payload.linker->findMatchingFunction(sig);

		assert(the_alloc_func.nonNull());

		this->alloc_func = the_alloc_func.getPointer();

		payload.all_variables_bound = true; // Assume true until we find an unbound variable during the body traversal.

		// Zero all arg ref counts.
		for(size_t i=0; i<args.size(); ++i)
			args[i].ref_count = 0;
	}*/



	payload.func_def_stack.push_back(this);
	stack.push_back(this);

	//for(unsigned int i=0; i<lets.size(); ++i)
	//	lets[i]->traverse(payload, stack);

	//if(this->sig.name == "main")
	//	int a = 9;//TEMP

	if(this->body.nonNull()) // !this->built_in_func_impl)
	{
		if((payload.operation == TraversalPayload::TypeCheck) && this->isGenericFunction())
		{
			// Don't typecheck generic functions.
		}
		else
			this->body->traverse(payload, stack);
	}

	

	//payload.capture_variables = old_use_captured_vars;

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->isGenericFunction())
		{
			// Don't type check this.  Concrete versions of this func will be type checked individually.
		}
		else
		{
			if(this->body.nonNull())
			{
				if(this->declared_return_type.nonNull())
				{
					const TypeRef body_type = this->body->type();
					if(body_type.isNull()) // Will happen if body is a function expression bound to a newly concrete function that has not been type-checked yet.
						throw BaseException("Type error for function '" + this->sig.toString() + "': Computed return type [Unknown] is not equal to the declared return type '" + this->declared_return_type->toString() + "'." + errorContext(*this));

					// Check that the return type of the body expression is equal to the declared return type
					// of this function.
					if(*this->body->type() != *this->declared_return_type)
						throw BaseException("Type error for function '" + this->sig.toString() + "': Computed return type '" + this->body->type()->toString() + 
							"' is not equal to the declared return type '" + this->declared_return_type->toString() + "'." + errorContext(*this));
				}
				else
				{
					// Else return type is NULL, so infer it
					//this->return_type = this->body->type();
				}
			}
		}
	}

	if(payload.operation == TraversalPayload::BindVariables)
	{
		// Do operator overloading for body AST node.
		if(this->body.nonNull()) // Body is NULL for built in functions
		{
			//payload.func_def_stack.push_back(this);
			//stack.push_back(this);
			convertOverloadedOperators(body, payload, stack);
			//stack.pop_back();
			//payload.func_def_stack.pop_back();
		}


		// NOTE: wtf is this code doing?
		//this->captured_vars = payload.captured_vars;

		//this->declared_return_type = 
		/*if(this->declared_return_type.nonNull() && this->declared_return_type->getType() == Type::FunctionType)
		{
			Function* ftype = static_cast<Function*>(this->declared_return_type.getPointer());
		}*/


		/*
		Detecting function arguments that aren't referenced
		===================================================
		Do a pass over function body.
		At end of pass,
		if all variables are bound:
		any argument that is not referenced is not used.
		*/
		/*if(payload.all_variables_bound)
		{
			for(size_t i=0; i<args.size(); ++i)
			{
				args[i].referenced = args[i].ref_count > 0;
			}
		}*/

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
				ASTNodeRef new_body(new FloatLiteral((float)body_lit->value, body_lit->srcLocation()));

				this->body = new_body;
				payload.tree_changed = true;
			}
		}
	}

	stack.pop_back();
	payload.func_def_stack.pop_back();
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

	if(this->built_in_func_impl.nonNull())
	{
		printMargin(depth+1, s);
		s << "Built in Implementation.\n";
	}
	else if(body.nonNull())
	{
		body->print(depth+1, s);
	}
	else
	{
		printMargin(depth+1, s);
		s << "Null body.\n";
	}
}


std::string FunctionDefinition::sourceString() const
{
	std::string s = "def " + sig.name + "(";
	for(unsigned int i=0; i<args.size(); ++i)
	{
		s += args[i].type->toString() + " " + args[i].name;
		if(i + 1 < args.size())
			s += ", ";
	}
	s += ") ";
	if(this->declared_return_type.nonNull())
		s += this->declared_return_type->toString() + " ";
	s += ": ";
	return s + body->sourceString();
}


std::string FunctionDefinition::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	assert(declared_return_type.nonNull());


	// Emit forwards declaration to file scope code:
	std::string opencl_sig = this->declared_return_type->OpenCLCType() + " ";
	opencl_sig += sig.typeMangledName() + "(";
	for(unsigned int i=0; i<args.size(); ++i)
	{
		opencl_sig += args[i].type->OpenCLCType() + " " + args[i].name;
		if(i + 1 < args.size())
			opencl_sig += ", ";
	}
	opencl_sig += ")";

	params.file_scope_code += opencl_sig + ";\n";

	// Emit function definition
	std::string s = opencl_sig + "\n{\n";
	
	// Emit body expression
	params.blocks.push_back("");
	const std::string body_expr = body->emitOpenCLC(params);
	StringUtils::appendTabbed(s, params.blocks.back(), (int)params.blocks.size());
	params.blocks.pop_back();
	
	s += "\treturn " + body_expr + ";\n";

	s += "}\n";

	return s;
}


llvm::Value* FunctionDefinition::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// This will be called for lambda expressions.
	// Capture variables at this point, by getting them off the arg and let stack.

	// Allocate space for the closure on the stack.
	//llvm::Value* closure_pointer;


	//closure_pointer = params.builder->CreateAlloca(
	//	//this->getClosureStructLLVMType(*params.context)
	//	this->type()->LLVMType(*params.context)
	//);

	/*const llvm::StructType* base_cap_var_type = llvm::StructType::get(
		*params.context,
		vector<const llvm::Type*>()
	);*/

	//TEMP: this needs to be in sync with Function::LLVMType()
	const bool simple_func_ptr = true;
	if(simple_func_ptr)
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr: Since we're storing a func ptr, it will be passed the captured var struct on usage.
			//params.hidden_voidptr_arg
		);

		return func;
	}


	assert(this->alloc_func);

	// Get pointer to allocateRefCountedStructure() function.
	llvm::Function* alloc_llvm_func = this->alloc_func->getOrInsertFunction(
		params.module,
		false // use_cap_var_struct_ptr: false as allocateRefCountedStructure() doesn't take it.
		//params.hidden_voidptr_arg
	);

	/////////////////// Create function pointer type /////////////////////
	// Build vector of function args
	vector<llvm::Type*> llvm_arg_types(this->args.size());
	for(size_t i=0; i<this->args.size(); ++i)
		llvm_arg_types[i] = this->args[i].type->LLVMType(*params.context);

	// Add Pointer to captured var struct, if there are any captured vars
	//TEMP since we are returning a closure, the functions will always be passed captured vars.  if(use_captured_vars)
	
//TEMP HACK NO CAPTURED VAR  STURCT ARG
//	llvm_arg_types.push_back(LLVMTypeUtils::getPtrToBaseCapturedVarStructType(*params.context)); //LLVMTypeUtils::pointerType(*base_cap_var_type));

	//TEMP HACK: add hidden void* arg  NOTE: should only do this when hidden_void_arg is true.
	llvm_arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context));

	// Construct the function pointer type
	llvm::Type* func_ptr_type = LLVMTypeUtils::pointerType(*llvm::FunctionType::get(
		this->returnType()->LLVMType(*params.context), // result type
		llvm_arg_types,
		false // is var arg
	));


	/////////////////////// Get full captured var struct type ///////////////
	llvm::Type* cap_var_type_ = this->getCapturedVariablesStructType()->LLVMType(*params.context);
	llvm::StructType* cap_var_type = static_cast<llvm::StructType*>(cap_var_type_);


	
	///////////////// Create closure type //////////////////////////////
	vector<llvm::Type*> closure_field_types(3);
	closure_field_types[0] = llvm::Type::getInt32Ty(*params.context); // Int 32 reference count
	closure_field_types[1] = func_ptr_type;
	closure_field_types[2] = cap_var_type;
	llvm::StructType* closure_type = llvm::StructType::get(
		*params.context,
		closure_field_types
	);
	
	//////////////// Compute size of closure type /////////////////////////
	const llvm::StructLayout* layout = params.target_data->getStructLayout(closure_type);
	
	const uint64_t struct_size = layout->getSizeInBytes();

	vector<llvm::Value*> args;
	args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, struct_size, true)));
	
	// Set hidden voidptr argument
	//if(params.hidden_voidptr_arg)
	//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

	//target_llvm_func->dump();

	// Call our allocateRefCountedStructure function
	llvm::Value* closure_void_pointer = params.builder->CreateCall(alloc_llvm_func, args);

	llvm::Value* closure_pointer = params.builder->CreateBitCast(
		closure_void_pointer,
		LLVMTypeUtils::pointerType(*closure_type),
		"closure_pointer"
	);

	

	// Store function pointer in the closure structure
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			true // use_cap_var_struct_ptr: Since we're storing a func ptr, it will be passed the captured var struct on usage.
			//params.hidden_voidptr_arg
		);

		llvm::Value* func_field_ptr = params.builder->CreateStructGEP(closure_pointer, 1);

		// Do the store.
		params.builder->CreateStore(
			func, // value
			func_field_ptr // ptr
		);
	}

	llvm::Value* captured_var_struct_ptr = params.builder->CreateStructGEP(closure_pointer, 2);

	// for each captured var
	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		llvm::Value* val = NULL;
		if(this->captured_vars[i].vartype == CapturedVar::Arg)
		{
			// Load arg
			//NOTE: offset if return by ref
			//val = LLVMTypeUtils::getNthArg(params.currently_building_func, this->captured_vars[i].index);
			val = LLVMTypeUtils::getNthArg(
				params.currently_building_func,
				params.currently_building_func_def->getLLVMArgIndex(this->captured_vars[i].index)
			);
			//val = LLVMTypeUtils::getNthArg(this
		}
		else if(this->captured_vars[i].vartype == CapturedVar::Let)
		{
			// Load let:
			// Walk up AST until we get to the correct let block
			const int let_frame_offset = this->captured_vars[i].let_frame_offset;

			// TEMP HACK USING 2 instead of 1.
			const int let_index = (int)params.let_block_stack.size() - 1 - let_frame_offset;
			LetBlock* let_block = params.let_block_stack[let_index];
	
			//val = let_block->getLetExpressionLLVMValue(
			//	params,
			//	this->captured_vars[i].index,
			//	ret_space_ptr // TEMP
			//);

			assert(params.let_block_let_values.find(let_block) != params.let_block_let_values.end());
			val = params.let_block_let_values[let_block][let_index];
		}
			
		// store in captured var structure field
		llvm::Value* field_ptr = params.builder->CreateStructGEP(captured_var_struct_ptr, i);

		params.builder->CreateStore(
			val, // value
			field_ptr // ptr
		);
	}

	// Bitcast the closure pointer down to the 'base' closure type.
	llvm::Type* base_closure_type = this->type()->LLVMType(*params.context);

	return params.builder->CreateBitCast(
		closure_pointer,
		base_closure_type,
		"base_closure_ptr"
	);
}


//llvm::Value* FunctionDefinition::getConstantLLVMValue(EmitLLVMCodeParams& params) const
//{
//	if(this->built_in_func_impl)
//	{
//		return this->built_in_func_impl->getConstantLLVMValue(params);
//	}
//	else
//	{
//		return this->body->getConstantLLVMValue(params);
//	}
//}


llvm::Function* FunctionDefinition::getOrInsertFunction(
		llvm::Module* module,
		bool use_cap_var_struct_ptr
		//bool hidden_voidptr_arg
	) const
{
	vector<TypeRef> arg_types = this->sig.param_types;

	/*if(use_captured_vars) // !this->captured_vars.empty())
	{
		// This function has captured variables.
		// So we will make it take a pointer to the closure structure as the last argument.
		//arg_types.push_back(getCapturedVariablesStructType());

		// Add pointer to base type structure.
	}*/


	llvm::FunctionType* functype = LLVMTypeUtils::llvmFunctionType(
		arg_types, 
		use_cap_var_struct_ptr, // this->use_captured_vars, // use captured var struct ptr arg
		returnType(), 
		module->getContext()
	);

	//TEMP:
	//std::cout << "FunctionDefinition::getOrInsertFunction(): " + this->sig.toString() << ": " << std::endl;
	//functype->dump();
	//std::cout << std::endl;

	// Make attribute list
	//llvm::AttrListPtr attribute_list;
	/*if(!this->returnType()->passByValue())
	{
		// Add sret attribute to zeroth argument
		attribute_list = attribute_list.addAttr(
			1, // index (NOTE: starts at one)
			llvm::Attribute::StructRet
		);
	}*/


#if 0
	
#else

	// NOTE: Check this code works somehow!
	// NOTE: It looks like function attributes are being removed from function declarations in the IR, when the function is not called anywhere.

	llvm::AttrBuilder function_attr_builder;
	function_attr_builder.addAttribute(llvm::Attribute::NoUnwind); // Does not throw exceptions
	if(this->returnType()->passByValue())
	{
		bool has_ptr_arg = false;
		for(unsigned int i=0; i<arg_types.size(); ++i)
			if(!arg_types[i]->passByValue())
				has_ptr_arg = true;

		if(external_function.nonNull() && external_function->has_side_effects)
		{}
		else
		{
			if(has_ptr_arg)
				function_attr_builder.addAttribute(llvm::Attribute::ReadOnly); // This attribute indicates that the function does not write through any pointer arguments etc..
			else
				function_attr_builder.addAttribute(llvm::Attribute::ReadNone); // Function computes its result based strictly on its arguments, without dereferencing any pointer arguments etc..
		}
	}


	//TEMP HACK:
	//function_attr_builder.addAttribute(llvm::Attribute::ReadOnly); // This attribute indicates that the function does not write through any pointer arguments etc..

	//function_attr_builder.addAttribute(llvm::Attribute::AlwaysInline);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		this->sig.typeMangledName(), //makeSafeStringForFunctionName(this->sig.toString()), // Name
		functype // Type
	);

	//llvm_func_constant->dump();

	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

	llvm::AttributeSet attributes = llvm::AttributeSet::get(
		module->getContext(),
		llvm::AttributeSet::FunctionIndex, // Index
		function_attr_builder);


	llvm_func->setAttributes(attributes);
	//llvm_func->addAttributes(llvm::AttributeSet::FunctionIndex, attributes);

#endif

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	llvm_func->setCallingConv(llvm::CallingConv::C);

	// Set names for all arguments.
	
	//NOTE: for some reason this crashes with optimisations enabled.
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
			if(i == 0) // Return value pointer arg
			{
				AI->setName("ret");

				// Set SRET and NoAlias attributes.
				llvm::AttrBuilder builder;
				builder.addAttribute(llvm::Attribute::StructRet);
				builder.addAttribute(llvm::Attribute::NoAlias);
				llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1, builder);
				AI->addAttr(set);
			}
			else if(i > this->args.size()) // Hidden pointer to env arg.
			{
				AI->setName("hidden");

				// Mark arg as NoAlias.
				llvm::AttrBuilder builder;
				//builder.addAttribute(llvm::Attribute::NoAlias);
				//builder.addAttribute(llvm::Attribute::ReadOnly); // "On an argument, this attribute indicates that the function does not write through this pointer argument, even though it may write to the memory that the pointer points to."
				//builder.addAttribute(llvm::Attribute::ByVal);
				llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1 + i, builder);
				AI->addAttr(set);
			}
			else // Normal arg.  Due to arg zero being the SRET arg, the index is offset by one.
			{
				//std::cout << i << std::endl;
				AI->setName(this->args[i-1].name);

				if(!this->args[i-1].type->passByValue()) // If pointer arg:
				{
					// Mark arg as NoAlias.
					llvm::AttrBuilder builder;
					builder.addAttribute(llvm::Attribute::NoAlias);

					//NOTE: in trunk, not in LLVM 3.3 yet.
					//builder.addAttribute(llvm::Attribute::ReadOnly); // "On an argument, this attribute indicates that the function does not write through this pointer argument, even though it may write to the memory that the pointer points to."
					llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1 + i, builder);
					AI->addAttr(set);
				}

				if(this->args[i-1].type->getType() == Type::OpaqueTypeType)
				{
					// Mark arg as NoAlias.
					llvm::AttrBuilder builder;
					//builder.addAttribute(llvm::Attribute::NoAlias);

					//builder.addAttribute(llvm::Attribute::ByVal);

					//NOTE: in trunk, not in LLVM 3.3 yet.
					//builder.addAttribute(llvm::Attribute::ReadOnly); // "On an argument, this attribute indicates that the function does not write through this pointer argument, even though it may write to the memory that the pointer points to."
				//	llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1 + i, builder);
				//	AI->addAttr(set);
				}
			}
		}
	}

	return llvm_func;
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module,
	const PlatformUtils::CPUInfo& cpu_info,
	bool hidden_voidptr_arg, 
	const llvm::DataLayout/*TargetData*/* target_data,
	const CommonFunctions& common_functions
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	)
{
#if USE_LLVM

	//NEW: do a pass to get the cleanup nodes first
	/*{
		TraversalPayload payload(TraversalPayload::GetCleanupNodes, false, NULL);
		std::vector<ASTNode*> stack(1, this);
		this->body->traverse(payload, stack);
	}
*/
	//TEMP:
	//this->print(0, std::cout);
	//std::cout << std::endl;


	llvm::Function* llvm_func = this->getOrInsertFunction(
		module,
		this->use_captured_vars // use_cap_var_struct_ptr
		//hidden_voidptr_arg
	);

	llvm::BasicBlock* block = llvm::BasicBlock::Create(
		module->getContext(), 
		"entry", 
		llvm_func
	);
	llvm::IRBuilder<> builder(block);

	// TEMP NEW;
	llvm::FastMathFlags flags;
	flags.setNoNaNs();
	flags.setNoInfs();
	flags.setNoSignedZeros();

	builder.SetFastMathFlags(flags);

	// const bool nsz = builder.getFastMathFlags().noSignedZeros();

	// Build body LLVM code
	EmitLLVMCodeParams params;
	params.currently_building_func_def = this;
	params.cpu_info = &cpu_info;
	//params.hidden_voidptr_arg = hidden_voidptr_arg;
	params.builder = &builder;
	params.module = module;
	params.currently_building_func = llvm_func;
	params.context = &module->getContext();
	params.target_data = target_data;
	params.common_functions = common_functions;

	//llvm::Value* body_code = NULL;
	if(this->built_in_func_impl.nonNull())
	{
		llvm::Value* body_code = this->built_in_func_impl->emitLLVMCode(params);
		if(this->returnType()->passByValue())
			builder.CreateRet(body_code);
		else
			builder.CreateRetVoid();
	}
	else
	{
		if(this->returnType()->passByValue())
		{
			llvm::Value* body_code = this->body->emitLLVMCode(params);

			// Emit cleanup code for reference-counted values.
			for(size_t z=0; z<params.cleanup_values.size(); ++z)
			{
				// Don't want to clean up (decr ref) the return value.
				if(params.cleanup_values[z].node != this->body.getPointer())
					params.cleanup_values[z].node->emitCleanupLLVMCode(params, params.cleanup_values[z].value);
			}

			builder.CreateRet(body_code);
		}
		else
		{
			// Else return type is pass-by-pointer

			// body code will return a pointer to the result of the body expression, allocated on the stack.
			// So load from the stack, and save to the return pointer which will have been passed in as arg zero.
			llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(llvm_func, 0);

			// Emit body code, storing the result directly to the return value ptr.
			llvm::Value* body_code = this->body->emitLLVMCode(params, return_val_ptr);

			// We want to do something here like,
			// If we haven't computed the result, but are merely returning it, then need to copy the arg to return ptr.
			if(true) // this->body->nodeType() == ASTNode::VariableASTNodeType || this->body->nodeType() == ASTNode::IfExpressionType)
			{
				if(return_val_ptr != body_code)
				//if(this->returnType()->getType() == Type::ArrayTypeType)
				//{
				//	llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
				//	params.builder->CreateMemCpy(return_val_ptr, body_code, size, 4);
				//}
				//else
				{
					// Load value
					llvm::Value* val = params.builder->CreateLoad(
						body_code
					);

					// And store at return_val_ptr
					params.builder->CreateStore(
						val, // value
						return_val_ptr // ptr
					);
				}

				/*if(this->body->type()->getType() == Type::ArrayTypeType)
				{
					llvm::Value* size;
					size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(float) * 4, true)); // TEMP HACK
					//size = //this->type().downcast<ArrayType>()->t->LLVMType(*params.context)->getPrimitiveSizeInBits() * 8;

						// Need to copy the value from the mem at arg to the mem at ret_space_ptr
					params.builder->CreateMemCpy(
						return_val_ptr, // dest
						body_code, // src
						size, // size
						4 // align
					);
				}
				else if(this->returnType()->getType() == Type::StructureTypeType)
				{
					// Load struct
					llvm::Value* struct_val = params.builder->CreateLoad(
						body_code
					);

					// And store at return_val_ptr
					params.builder->CreateStore(
						struct_val, // value
						return_val_ptr // ptr
					);

				}
				else
				{
					assert(0);
				}*/
				
			}

			// Emit cleanup code for reference-counted values.
			for(size_t z=0; z<params.cleanup_values.size(); ++z)
			{
				// Don't want to clean up (decr ref) the return value.
				if(params.cleanup_values[z].node != this->body.getPointer())
					params.cleanup_values[z].node->emitCleanupLLVMCode(params, params.cleanup_values[z].value);
			}

			builder.CreateRetVoid();

			//if(*this->returnType() == 
			/*if(this->returnType()->getType() == Type::StructureTypeType)
			{*/
				//StructureType* struct_type = static_cast<StructureType*>(this->returnType().getPointer());

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

			/*	llvm::Value* struct_val = params.builder->CreateLoad(
					body_code
				);

				params.builder->CreateStore(
					struct_val, // value
					return_val_ptr // ptr
				);
			}
			else if(this->returnType()->getType() == Type::ArrayTypeType)
			{
				
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
			}*/

			//builder.CreateRet(return_val_ptr);
			
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
	FunctionDefinitionRef f = new FunctionDefinition(
		this->srcLocation(),
		this->order_num,
		this->sig.name,
		this->args,
		this->body.nonNull() ? this->body->clone() : NULL,
		this->declared_return_type,
		this->built_in_func_impl
	);

	return f;
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
	//return false;//TEMP
	if(body.isNull())
		return false;

	return this->body->isConstant();
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

/*TypeRef FunctionDefinition::getFullClosureType() const
{
	vector<TypeRef> field_types;
	vector<string> field_names;

	field_types.push_back(TypeRef(new Int())); // ref count
	field_types.push_back(


}*/


TypeRef FunctionDefinition::getCapturedVariablesStructType() const
{
	vector<TypeRef> field_types;
	vector<string> field_names;

	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		// Get the type of the captured variable.
		if(this->captured_vars[i].vartype == CapturedVar::Arg)
		{
			assert(this->captured_vars[i].bound_function);

			field_types.push_back(
				this->captured_vars[i].bound_function->args[this->captured_vars[i].index].type
			);
		}
		else if(this->captured_vars[i].vartype == CapturedVar::Let)
		{
			assert(this->captured_vars[i].bound_let_block);

			field_types.push_back(
				this->captured_vars[i].bound_let_block->lets[this->captured_vars[i].index]->type()
			);
		}
		else
		{
			assert(!"Invalid vartype for captured var.");
		}

		field_names.push_back("captured_var_" + toString((uint64)i));
	}
			

	//NOTE: this is pretty heavyweight.
	return TypeRef(new StructureType("captured_var_struct", field_types, field_names));
}


// If the function is return by value, returns winter_index, else returns winter_index + 1
// as the zeroth index will be the sret pointer.
int FunctionDefinition::getLLVMArgIndex(int winter_index)
{
	if(this->returnType()->passByValue())
		return winter_index;
	else
		return winter_index + 1;
}


int FunctionDefinition::getCapturedVarStructLLVMArgIndex()
{
	// The pointer to the captured vars struct goes after the 'normal' arguments.

	assert(!this->captured_vars.empty());

	if(this->returnType()->passByValue())
		return (int)this->args.size();
	else
		return (int)this->args.size() + 1; // allow room for sret argument.
}


const std::string makeSafeStringForFunctionName(const std::string& s)
{
	std::string res = s;

	for(size_t i=0; i<s.size(); ++i)
		if(!(::isAlphaNumeric(s[i]) || s[i] == '_'))
			res[i] = '_';

	return res;
}


} // end namespace Winter

