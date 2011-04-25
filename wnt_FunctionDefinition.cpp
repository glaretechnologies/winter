/*=====================================================================
FunctionDefinition.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:40 +0100
=====================================================================*/
#include "wnt_FunctionDefinition.h"


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
	//TEMP NEW:
	if(this->body.nonNull() && this->body->type().nonNull())
		return this->body->type();

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

	vector<TypeRef> captured_var_types;
	for(size_t i=0; i<this->captured_vars.size(); ++i)
		captured_var_types.push_back(this->captured_vars[i].type());

	return TypeRef(new Function(arg_types, this->returnType(), captured_var_types, this->use_captured_vars));
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


const std::string indent(VMState& vmstate)
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

	if(this->sig.name == "main")
		int a = 9;//TEMP

	if(this->body.nonNull()) // !this->built_in_func_impl)
		this->body->traverse(payload, stack);

	stack.pop_back();

	payload.func_def_stack.pop_back();

	//payload.capture_variables = old_use_captured_vars;

	if(payload.operation == TraversalPayload::BindVariables)
	{
		//this->captured_vars = payload.captured_vars;

		//this->declared_return_type = 
		if(this->declared_return_type.nonNull() && this->declared_return_type->getType() == Type::FunctionType)
		{
			Function* ftype = static_cast<Function*>(this->declared_return_type.getPointer());
		}
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
	// This will be called for lambda expressions.
	// Capture variables at this point, by getting them off the arg and let stack.

	// Allocate space for the closure on the stack.
	llvm::Value* closure_pointer;
	closure_pointer = params.builder->CreateAlloca(
		//this->getClosureStructLLVMType(*params.context)
		this->type()->LLVMType(*params.context)
	);

	// Store function pointer in the closure structure
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			params.hidden_voidptr_arg
		);

		//std::cout << "closure_pointer: " << std::endl;
		//TEMP
		//closure_pointer->dump();

		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // field index
		
		llvm::Value* func_field_ptr = params.builder->CreateGEP(
			closure_pointer, // ptr
			indices.begin(),
			indices.end()
		);

		// Do the store.
		params.builder->CreateStore(
			func, // value
			func_field_ptr // ptr
		);
	}

	llvm::Value* captured_var_struct_ptr = NULL;
	{
	vector<llvm::Value*> indices;
	indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
	indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true))); // field index
		
	captured_var_struct_ptr = params.builder->CreateGEP(
		closure_pointer, // ptr
		indices.begin(),
		indices.end()
	);
	}

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

		

			// TODO: set val
			assert(0);
		}
			
		// store in captured var structure field
		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)));
		
		llvm::Value* field_ptr = params.builder->CreateGEP(
			captured_var_struct_ptr, // ptr
			indices.begin(),
			indices.end()
		);

		//TEMP:
		std::cout << "val: " << std::endl;
		val->dump();
		std::cout << "field_ptr: " << std::endl;
		field_ptr->dump();

		params.builder->CreateStore(
			val, // value
			field_ptr // ptr
		);
	}

	return closure_pointer;
}


llvm::Function* FunctionDefinition::getOrInsertFunction(
		llvm::Module* module,
		bool hidden_voidptr_arg
	) const
{
	vector<TypeRef> arg_types = this->sig.param_types;

	if(use_captured_vars) // !this->captured_vars.empty())
	{
		// This function has captured variables.
		// So we will make it take a pointer to the closure structure as the last argument.
		arg_types.push_back(getCapturedVariablesStructType());
	}


	llvm::FunctionType* functype = LLVMTypeUtils::llvmFunctionType(
		arg_types, 
		returnType(), 
		module->getContext(),
		hidden_voidptr_arg
	);

	//TEMP:
	std::cout << "FunctionDefinition::getOrInsertFunction(): " + this->sig.toString() << std::endl;
	functype->dump();

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
		for(unsigned int i=0; i<arg_types/*this->args*/.size(); ++i)
		{
			if(!arg_types[i]/*this->args[i].type*/->passByValue())
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

	return llvm_func;
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module,
	const PlatformUtils::CPUInfo& cpu_info,
	bool hidden_voidptr_arg
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	)
{
#if USE_LLVM

	llvm::Function* llvm_func = this->getOrInsertFunction(
		module,
		hidden_voidptr_arg
	);

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

		field_names.push_back("captured_var_" + toString(i));
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


} // end namespace Winter

