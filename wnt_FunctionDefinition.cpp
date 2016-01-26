/*=====================================================================
FunctionDefinition.cpp
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:40 +0100
=====================================================================*/
#include "wnt_FunctionDefinition.h"


#include "wnt_LLVMVersion.h"
#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_Variable.h"
#include "VirtualMachine.h"
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


CapturedVar::CapturedVar()
:	bound_function(NULL),
	bound_let_node(NULL),
	enclosing_lambda(NULL)
	//substituted_expr(NULL)
{}


TypeRef CapturedVar::type() const
{
	if(this->vartype == Let)
	{
		// NOTE: duplicated from var, de-duplicate

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
	else if(this->vartype == Arg)
	{
		assert(this->bound_function);
		return this->bound_function->args[this->arg_index].type;
	}
	else if(this->vartype == Captured)
	{
		assert(this->enclosing_lambda);
		return this->enclosing_lambda->captured_vars[this->free_index].type();
	}
	else
	{
		assert(!"Invalid vartype");
		return TypeRef();
	}
}


//----------------------------------------------------------------------------------


FunctionDefinition::FunctionDefinition(const SrcLocation& src_loc, int order_num_, const std::string& name, const std::vector<FunctionArg>& args_, 
									   const ASTNodeRef& body_, const TypeRef& declared_rettype, 
									   const BuiltInFunctionImplRef& impl
									   )
:	ASTNode(FunctionDefinitionType, src_loc),
	order_num(order_num_),
	args(args_),
	body(body_),
	declared_return_type(declared_rettype),
	built_in_func_impl(impl),
	built_llvm_function(NULL),
	jitted_function(NULL),
	//use_captured_vars(false),
	need_to_emit_captured_var_struct_version(false),
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
	if(this->declared_return_type.nonNull())
		return this->declared_return_type;
	
	return this->body->type();
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

	return new Function(arg_types, return_type, /*captured_var_types, */true); //this->use_captured_vars);
}


ValueRef FunctionDefinition::exec(VMState& vmstate)
{
	// Capture variables at this point, by getting them off the arg and let stack.
	vector<ValueRef> vals;
	if(vmstate.capture_vars)
		for(size_t i=0; i<this->captured_vars.size(); ++i)
		{
			if(this->captured_vars[i].vartype == CapturedVar::Arg)
			{
				vals.push_back(vmstate.argument_stack[vmstate.func_args_start.back() + this->captured_vars[i].arg_index]);
			}
			else if(this->captured_vars[i].vartype == CapturedVar::Let)
			{
				//const int let_frame_offset = this->captured_vars[i].let_frame_offset;
				//assert(let_frame_offset < (int)vmstate.let_stack_start.size());

				//const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - let_frame_offset];
				//vals.push_back(vmstate.let_stack[let_stack_start + this->captured_vars[i].index]);

				LetASTNode* bound_let_node = this->captured_vars[i].bound_let_node;
				ValueRef val = this->captured_vars[i].bound_let_node->exec(vmstate);

				if(bound_let_node->vars.size() == 1)
				{}
				else
				{
					// Destructuring assignment, return the particular element from the tuple.
					const TupleValue* t = checkedCast<TupleValue>(val.getPointer());
					val = t->e[this->captured_vars[i].let_var_index];
				}

				vals.push_back(val);
			}
			else if(this->captured_vars[i].vartype == CapturedVar::Captured)
			{
				// Get ref to capturedVars structure of values, will be passed in as last arg to function
				ValueRef captured_struct = vmstate.argument_stack.back();
				const StructureValue* s = checkedCast<StructureValue>(captured_struct.getPointer());

				ValueRef val = s->fields[this->captured_vars[i].free_index];

				vals.push_back(val);
			}
			else
			{
				assert(0);
				throw BaseException("internal error 136");
			}
		}

	// Put captured values into the variable struct.
	Reference<StructureValue> var_struct = new StructureValue(vals);

	return new FunctionValue(this, var_struct);
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
	if(vmstate.trace) 
	{
		//*vmstate.ostream << vmstate.indent() << "" << this->sig.name << "()\n";
		//printStack(vmstate);
	}

	// Check the types of the arguments that we have received
	for(size_t i=0; i<this->args.size(); ++i)
	{
		ValueRef arg_val = vmstate.argument_stack[vmstate.func_args_start.back() + i];

		if(vmstate.trace)
		{
			//*vmstate.ostream << "Arg " << i << ": " << arg_val->toString() << std::endl;
		}
	}


	if(this->built_in_func_impl.nonNull())
		return this->built_in_func_impl->invoke(vmstate);

	
	// Execute body of function
	ValueRef ret = body->exec(vmstate);

	if(this->declared_return_type.nonNull() && (*body->type() != *this->declared_return_type))
	{
		// This may happen since type checking may not have been done yet.
		throw BaseException("Returned object has invalid type.");
	}

	return ret;
}


void FunctionDefinition::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	// Don't traverse function defn from top level directly, wait until it is traversed as the child of the enclosing function
	if(is_anon_func && stack.empty() && (payload.operation != TraversalPayload::UpdateUpRefs))
		return;

	if(this->isGenericFunction() && (payload.operation != TraversalPayload::CustomVisit))
		return;


	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(body, payload);
	}*/
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
	if(payload.operation == TraversalPayload::TypeCheck)
	{
		payload.captured_types.clear();
	}
	else if(payload.operation == TraversalPayload::CustomVisit)
	{
		if(payload.custom_visitor.nonNull())
			payload.custom_visitor->visit(*this, payload);
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_ComputeAlive)
	{
		if(stack.empty()) // If this is a top-level global func (not a lambda expression):
		{
			// Clear this data in preparation for the analysis.
			payload.reachable_nodes.clear();
			payload.nodes_to_process.clear();
			payload.processed_nodes.clear();
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

	if(this->body.nonNull())
	{
		this->body->traverse(payload, stack);
	}

	

	//payload.capture_variables = old_use_captured_vars;

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		if(body.nonNull())
			checkInlineExpression(body, payload, stack);
	}
	else if(payload.operation == TraversalPayload::UpdateUpRefs)
	{
		for(size_t i=0; i<captured_vars.size(); ++i)
		{
			switch(captured_vars[i].vartype)
			{
			case CapturedVar::Let:
				{
					ASTNode* updated_node = payload.clone_map[captured_vars[i].bound_let_node];
					if(updated_node)
						captured_vars[i].bound_let_node = (LetASTNode*)updated_node;
					break;
				}
			case CapturedVar::Arg:
				break;
			case CapturedVar::Captured:
				{
					break;
				}
			}
		}
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		if(body.nonNull())
			checkSubstituteVariable(body, payload);

		// Clear captured vars
		this->captured_vars.resize(0);

		this->order_num = payload.new_order_num;
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
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

		// If this is an anon func, add any types that it captures to the current set for the top-level function we are in.
		if(this->is_anon_func)
		{
			for(size_t i=0; i<this->captured_vars.size(); ++i)
			{
				payload.captured_types.insert(this->captured_vars[i].type());
			}
		}

		if(stack.size() == 1)
		{
			assert(stack[0] == this);

			// If this is a top-level func.
			this->captured_var_types = payload.captured_types;

			//std::cout << "captured var types for function " + this->sig.toString() + ": " << std::endl;
			//for(auto i=captured_var_types.begin(); i != captured_var_types.end(); ++i)
			//	std::cout << (*i)->toString() << std::endl;
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
		else if(this->declared_return_type.nonNull() &&
			this->declared_return_type->getType() == Type::DoubleType && 
			this->body.nonNull() && 
			this->body->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* body_lit = static_cast<IntLiteral*>(this->body.getPointer());
			if(isIntExactlyRepresentableAsDouble(body_lit->value))
			{
				ASTNodeRef new_body(new DoubleLiteral((double)body_lit->value, body_lit->srcLocation()));

				this->body = new_body;
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		const bool body_is_literal = checkFoldExpression(body, payload);
			
		this->can_maybe_constant_fold = body_is_literal;
	}
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		if(this->is_anon_func)
		{
			payload.reachable_nodes.insert(this);
		}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_ComputeAlive)
	{
		if(!this->is_anon_func) // if this is a top-level func:
		{
			while(!payload.nodes_to_process.empty())
			{
				ASTNode* n = payload.nodes_to_process.back();
				payload.nodes_to_process.pop_back();

				if(payload.processed_nodes.find(n) == payload.processed_nodes.end()) // If not already processed:
				{
					payload.processed_nodes.insert(n); // Mark node as processed.

					//std::cout << "Processing node " << n << std::endl;

					n->traverse(payload, stack); // stack will be wrong, but it shouldn't matter.
				}
			}
		}
		else
		{
			for(size_t i=0; i<captured_vars.size(); ++i)
			{
				if(captured_vars[i].vartype == CapturedVar::Let)
				{
					payload.reachable_nodes.insert(captured_vars[i].bound_let_node); // Mark as alive
					if(payload.processed_nodes.find(captured_vars[i].bound_let_node) == payload.processed_nodes.end()) // If has not been processed yet:
						payload.nodes_to_process.push_back(captured_vars[i].bound_let_node); // Add to to-process list.
				}
			}
		}
	}
	else if(payload.operation == TraversalPayload::AddAnonFuncsToLinker)
	{
		if(this->is_anon_func)
			payload.linker->anon_functions_to_codegen.push_back(this);
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		if(body.nonNull())
			doDeadCodeElimination(body, payload, stack);
	}
	

	stack.pop_back();
	payload.func_def_stack.pop_back();
}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef (" + toHexString((uint64)this) + "): " << this->sig.toString() << " " << 
		(this->returnType().nonNull() ? this->returnType()->toString() : "[Unknown ret type]");
	if(this->declared_return_type.nonNull())
		s << " (Declared ret type: " + this->declared_return_type->toString() << ")";
	s << "\n";

	for(size_t i=0; i<this->captured_vars.size(); ++i)
		captured_vars[i].print(depth + 1, s);


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
	std::string s;

	if(is_anon_func)
	{
		s += "\\";
	}
	else
	{
		s += "def " + sig.name;
	}

	if(!generic_type_param_names.empty())
	{
		s += "<";
		s += StringUtils::join(generic_type_param_names, ", ");
		s += ">";
	}
	
	s += "(";
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
	assert(returnType().nonNull());


	// Emit forwards declaration to file scope code:
	std::string opencl_sig = this->returnType()->OpenCLCType() + " ";
	opencl_sig += sig.typeMangledName() + "(";
	for(unsigned int i=0; i<args.size(); ++i)
	{
		if(args[i].type->OpenCLPassByPointer())
			opencl_sig += "const " + args[i].type->address_space + " " + args[i].type->OpenCLCType() + "* const " + args[i].name;
		else
			opencl_sig += "const " + args[i].type->OpenCLCType() + " " + args[i].name;
			
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

	// If the body expression is just an argument, and it is pass by pointer, then it will need to be dereferenced.
	if((body->nodeType() == ASTNode::VariableASTNodeType) && (body.downcastToPtr<Variable>()->binding_type == Variable::ArgumentVariable) && body->type()->OpenCLPassByPointer())
		s += "\treturn *" + body_expr + ";\n";// Deref
	else
		s += "\treturn " + body_expr + ";\n";

	s += "}\n";

	return s;
}


llvm::Value* FunctionDefinition::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// This will be called for lambda expressions.

	// We want to return a function closure, which is allocated on the heap (usually) and contains a function pointer to the anonymous function, 
	// a structure containing the captured variables, etc..

	params.stats->num_closure_allocations++;

	TypeRef this_closure_type = this->type();

	// NOTE: this type doesn't have the actual captured var struct.
	llvm::Type* this_closure_llvm_type = this_closure_type->LLVMType(*params.module);
	assert(this_closure_llvm_type->isPointerTy());
	assert(this_closure_llvm_type->getPointerElementType()->isStructTy());
	llvm::StructType* this_closure_llvm_struct_type = (llvm::StructType*)this_closure_llvm_type->getPointerElementType();
	
	// llvm::StructType::elements() function is not present in LLVM 3.4.
	llvm::StructType::element_iterator it = this_closure_llvm_struct_type->element_begin();
	for(int i=0; i<Function::functionPtrIndex(); ++i)
		it++;

	llvm::Type* func_ptr_type = *it; // this_closure_llvm_struct_type->elements()[Function::functionPtrIndex()];


	/////////////////////// Get closure destructor type ///////////////
	llvm::Type* destructor_arg_types[1] = { LLVMTypeUtils::getPtrToBaseCapturedVarStructType(*params.module) };

	llvm::FunctionType* destructor_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(params.module->getContext()), // return type
		destructor_arg_types,
		false // varargs
	);


	/////////////////////// Get full captured var struct type ///////////////
	const StructureTypeRef captured_var_struct_type = this->getCapturedVariablesStructType();
	llvm::Type* cap_var_type_ = captured_var_struct_type->LLVMType(*params.module);
	assert(cap_var_type_->isStructTy());
	llvm::StructType* cap_var_type = static_cast<llvm::StructType*>(cap_var_type_);

	
	///////////////// Create full closure type //////////////////////////////
	// NOTE: the number and type of captured variables depends on the call site, so isn't actually reflected in the function type.
	llvm::Type* closure_field_types[] = {
		llvm::Type::getInt64Ty(*params.context), // Int 64 reference count
		llvm::Type::getInt64Ty(*params.context), // flags
		func_ptr_type,
		LLVMTypeUtils::pointerType(destructor_type),
		cap_var_type
	};
	llvm::StructType* closure_type = llvm::StructType::create(
		*params.context,
		llvm::makeArrayRef(closure_field_types),
		this->sig.typeMangledName() + "_closure"
	);

	//////////////// Compute size of complete closure type /////////////////////////
	const llvm::StructLayout* layout = params.target_data->getStructLayout(closure_type);
	
	const uint64_t struct_size_B = layout->getSizeInBytes();


	const bool alloc_on_heap = mayEscapeCurrentlyBuildingFunction(params, this->type());

	llvm::Value* closure_pointer;
	uint64 initial_flags;

	if(alloc_on_heap)
	{
		params.stats->num_heap_allocation_calls++;

		// Get pointer to allocateClosureFunc() function.
		llvm::Function* alloc_closure_func = params.common_functions.allocateClosureFunc->getOrInsertFunction(params.module);

		llvm::Value* alloc_closure_func_args[] = { llvm::ConstantInt::get(*params.context, llvm::APInt(64, struct_size_B, true)) };
	
		// Set hidden voidptr argument
		//if(params.hidden_voidptr_arg)
		//	args.push_back(LLVMTypeUtils::getLastArg(params.currently_building_func));

		// Call our allocateClosureFunc function
	
		llvm::CallInst* alloc_closure_func_call = params.builder->CreateCall(alloc_closure_func, alloc_closure_func_args, "base_closure_ptr");
		addMetaDataCommentToInstruction(params, alloc_closure_func_call, "alloc closure for " + this->sig.typeMangledName());
		llvm::Value* closure_void_pointer = alloc_closure_func_call;

		// Cast the pointer returned from the alloc_closure_func, from dummy closure type to the actual closure type.
		closure_pointer = params.builder->CreateBitCast(
			closure_void_pointer,
			LLVMTypeUtils::pointerType(*closure_type),
			"closure_pointer"
		);
		initial_flags = 1; // flag = 1 = heap allocated
	}
	else
	{
		// Emit the alloca in the entry block for better code-gen.
		// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
		llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

		llvm::Value* alloca_ptr = entry_block_builder.CreateAlloca(
			closure_type, // byte
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, true)), // array size
			this->type()->toString() + " closure stack space"
		);

		closure_pointer = alloca_ptr;
		initial_flags = 0; // flag = 0 = not heap allocated
	}

	

	// Capture variables at this point, by getting them off the arg and let stack.

	//TEMP: this needs to be in sync with Function::LLVMType()
	/*const bool simple_func_ptr = false;
	if(simple_func_ptr)
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr: Since we're storing a func ptr, it will be passed the captured var struct on usage.
		);
		return func;
	}*/


	

	/////////////////// Create function pointer type /////////////////////
	// Build vector of function args
	/*vector<llvm::Type*> llvm_arg_types(this->args.size());
	for(size_t i=0; i<this->args.size(); ++i)
		llvm_arg_types[i] = this->args[i].type->LLVMType(*params.module);

	// Add Pointer to captured var struct, if there are any captured vars
	//TEMP since we are returning a closure, the functions will always be passed captured vars.  if(use_captured_vars)
	
	llvm_arg_types.push_back(LLVMTypeUtils::getPtrToBaseCapturedVarStructType(*params.module));

	// Add hidden void* arg  NOTE: should only do this when hidden_void_arg is true.
	// llvm_arg_types.push_back(LLVMTypeUtils::voidPtrType(*params.context));

	// Construct the function pointer type
	llvm::Type* func_ptr_type = LLVMTypeUtils::pointerType(*llvm::FunctionType::get(
		this->returnType()->LLVMType(*params.module), // result type
		llvm_arg_types,
		false // is var arg
	));*/


	// Set the reference count in the closure to 1
	llvm::Value* closure_ref_count_ptr = params.builder->CreateStructGEP(closure_pointer, 0, "closure_ref_count_ptr");
	llvm::Value* one = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, /*signed=*/true));
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, closure_ref_count_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "Set initial ref count to 1");
	

	// Set the flags
	llvm::Value* flags_ptr = params.builder->CreateStructGEP(closure_pointer, 1, "closure_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "set initial flags to " + toString(initial_flags));


	// Store function pointer to the anon function in the closure structure
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			true // use_cap_var_struct_ptr: Since we're storing a func ptr, it will be passed the captured var struct on usage.
		);

		llvm::Value* func_field_ptr = params.builder->CreateStructGEP(closure_pointer, Function::functionPtrIndex(), "function_field_ptr");
		llvm::StoreInst* store_inst = params.builder->CreateStore(func, func_field_ptr); // Do the store.
		addMetaDataCommentToInstruction(params, store_inst, "Store function pointer in closure");
	}

	// Store captured vars in closure.
	
	llvm::Value* captured_var_struct_ptr = params.builder->CreateStructGEP(closure_pointer, Function::capturedVarStructIndex(), "captured_var_struct_ptr");

	// for each captured var
	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		llvm::Value* val = NULL;
		if(this->captured_vars[i].vartype == CapturedVar::Arg)
		{
			// Load arg
			val = LLVMTypeUtils::getNthArg(
				params.currently_building_func,
				params.currently_building_func_def->getLLVMArgIndex(this->captured_vars[i].arg_index)
			);
		}
		else if(this->captured_vars[i].vartype == CapturedVar::Let)
		{
			// Load let:
			// Walk up AST until we get to the correct let block
	/*		const int let_frame_offset = this->captured_vars[i].let_frame_offset;

			const int let_block_index = (int)params.let_block_stack.size() - 1 - let_frame_offset;
			LetBlock* let_block = params.let_block_stack[let_block_index];*/

			LetASTNode* let_node = this->captured_vars[i].bound_let_node;
	
			assert(params.let_values.find(let_node) != params.let_values.end());
			val = params.let_values[let_node];

			// NOTE: This code is duplicated from Variable.  de-duplicate.
			TypeRef cap_var_type = this->captured_vars[i].type();

			if(let_node->vars.size() > 1)
			{
				// Destructuring assignment, we just want to return the individual tuple element.
				// Value should be a pointer to a tuple struct.
				if(cap_var_type->passByValue())
				{
					llvm::Value* tuple_elem = params.builder->CreateLoad(params.builder->CreateStructGEP(val, this->captured_vars[i].let_var_index, "tuple_elem_ptr"));
					val = tuple_elem;
				}
				else
				{
					llvm::Value* tuple_elem = params.builder->CreateStructGEP(val, this->captured_vars[i].let_var_index, "tuple_elem_ptr");
					val = tuple_elem;
				}
			}
		}
		else if(this->captured_vars[i].vartype == CapturedVar::Captured)
		{
			// This captured var is bound to a captured var itself.
			// The target captured var is in the captured var struct for the function, which is passed as the last arg to the function body.
			//FunctionDefinition* def = this->captured_vars[i].bound_function;
			llvm::Value* base_current_func_captured_var_struct = LLVMTypeUtils::getLastArg(params.currently_building_func);
			
			// Now we need to downcast it to the correct type.
			llvm::Type* actual_cap_var_struct_type = params.currently_building_func_def->getCapturedVariablesStructType()->LLVMType(*params.module);
			
			llvm::Value* current_func_captured_var_struct = params.builder->CreateBitCast(base_current_func_captured_var_struct, LLVMTypeUtils::pointerType(actual_cap_var_struct_type), "actual_cap_var_struct_type");

			llvm::Value* field_ptr = params.builder->CreateStructGEP(current_func_captured_var_struct, this->captured_vars[i].free_index);
			
			val = params.builder->CreateLoad(field_ptr);
		}
		else
		{
			assert(0);
		}

		
		// store in captured var structure field
		llvm::Value* field_ptr = params.builder->CreateStructGEP(captured_var_struct_ptr, (unsigned int)i, "captured_var_" + toString(i) + "_field_ptr");
		llvm::StoreInst* store_inst = params.builder->CreateStore(val, field_ptr);

		addMetaDataCommentToInstruction(params, store_inst, "Store captured var " + toString(i) + " in closure");

		// If the captured var is a ref-counted type, we need to increment its reference count, since the struct now holds a reference to it. 
		captured_var_struct_type->component_types[i]->emitIncrRefCount(params, val, "Capture var ref count increment");
	}



	// Emit a destructor function for this closure.
	// Will be something like (C++ equiv):
	// void anon_func_xx_captured_var_struct_destructor(base_captured_var_struct* s)
	{
		const std::string destructor_name = this->sig.typeMangledName() + toHexString((uint64)this) + "_captured_var_struct_destructor";

		assert(params.module->getFunction(destructor_name) == NULL);

		llvm::Constant* llvm_func_constant = params.module->getOrInsertFunction(
			destructor_name, // Name
			destructor_type // Type
		);

		assert(llvm::isa<llvm::Function>(llvm_func_constant));
		llvm::Function* destructor = static_cast<llvm::Function*>(llvm_func_constant);
		llvm::BasicBlock* block = llvm::BasicBlock::Create(params.module->getContext(), "entry", destructor);
		llvm::IRBuilder<> builder(block);

		// Get zeroth arg to func
		llvm::Value* base_captured_var_struct = LLVMTypeUtils::getNthArg(destructor, 0);

		// Downcast to known actual captured var struct type.
		// Bitcast the closure pointer down to the 'base' closure type.
		llvm::Value* actual_captured_var_struct = builder.CreateBitCast(
			base_captured_var_struct, // value
			LLVMTypeUtils::pointerType(cap_var_type), // dest type
			"actual_captured_var_struct"
		);

		// Emit a call to the destructor for this captured var struct, if it has one.
		EmitLLVMCodeParams temp_params = params;
		temp_params.builder = &builder;
		captured_var_struct_type->emitDestructorCall(temp_params, actual_captured_var_struct, "captured var struct destructor call");
		
		builder.CreateRetVoid();

		// Store a pointer to the destructor in the closure.
		llvm::StoreInst* store_inst = params.builder->CreateStore(destructor, params.builder->CreateStructGEP(closure_pointer, Function::destructorPtrIndex(), "destructor ptr"));

		addMetaDataCommentToInstruction(params, store_inst, "Store destructor in closure");
	}



	// Bitcast the closure pointer down to the 'base' closure type.
	llvm::Type* base_closure_type = this->type()->LLVMType(*params.module);

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
		llvm::Module* module
	) const
{
	return getOrInsertFunction(module, false);
}


llvm::Function* FunctionDefinition::getOrInsertFunction(
		llvm::Module* module,
		bool use_cap_var_struct_ptr
		//bool hidden_voidptr_arg
	) const
{
	const vector<TypeRef> arg_types = this->sig.param_types;

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
		*module
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


	// NOTE: It looks like function attributes are being removed from function declarations in the IR, when the function is not called anywhere.
	
	// We can only mark a function as readonly (or readnone) if the return type is pass by value, otherwise this function will need to write through the SRET argument.
	llvm::AttrBuilder function_attr_builder;
	function_attr_builder.addAttribute(llvm::Attribute::NoUnwind); // Does not throw exceptions
	if(this->returnType()->passByValue())
	{
		bool has_ptr_arg = false;
		for(unsigned int i=0; i<arg_types.size(); ++i)
			if(!arg_types[i]->passByValue() || arg_types[i]->isHeapAllocated())
				has_ptr_arg = true;

		if(use_cap_var_struct_ptr)
			has_ptr_arg = true;

		if(external_function.nonNull() && external_function->has_side_effects)
		{}
		else
		{
			//if(has_ptr_arg)
			//	function_attr_builder.addAttribute(llvm::Attribute::ReadOnly); // This attribute indicates that the function does not write through any pointer arguments etc..
			//else
			//TEMP	function_attr_builder.addAttribute(llvm::Attribute::ReadNone); // Function computes its result based strictly on its arguments, without dereferencing any pointer arguments etc..

			//NOTE: can't have readnone when we're doing heap allocs
		}
	}

	std::string use_name = this->sig.typeMangledName();
	if(use_cap_var_struct_ptr)
		use_name += "_with_cap_var_struct_arg";


	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		use_name, //makeSafeStringForFunctionName(this->sig.toString()), // Name
		functype // Type
	);

	if(!llvm::isa<llvm::Function>(llvm_func_constant))
	{
		//std::cout << std::endl;
		//llvm_func_constant->dump();
		assert(0);
		throw BaseException("Internal error while building function '" + sig.toString() + "', result was not a function.");
	}

	llvm::Function* llvm_func = static_cast<llvm::Function*>(llvm_func_constant);

	llvm::AttributeSet attributes = llvm::AttributeSet::get(
		module->getContext(),
		llvm::AttributeSet::FunctionIndex, // Index
		function_attr_builder);


	llvm_func->setAttributes(attributes);


	// If this is an external allocation function, mark the result as noalias.
	if(external_function.nonNull() && external_function->is_allocation_function)
	{
		llvm_func->addAttribute(llvm::AttributeSet::ReturnIndex, llvm::Attribute::NoAlias);
	}

	
	// Mark return type as nonnull if it's a pointer
	if(this->returnType()->getType() == Type::VArrayTypeType)
	{
	//	llvm_func->addAttribute(llvm::AttributeSet::ReturnIndex, llvm::Attribute::NonNull);
	}

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	llvm_func->setCallingConv(llvm::CallingConv::C);

	// Set names and attributes for function arguments.
	int i = 0;
	for(llvm::Function::arg_iterator AI = llvm_func->arg_begin(); AI != llvm_func->arg_end(); ++AI, ++i)
	{
		// If return type is pass-by-pointer, there is an extra SRET argument as the first arg for the LLVM function.
		const int winter_arg_index = this->returnType()->passByValue() ? i : i - 1;

		if(winter_arg_index < 0)
		{
			assert(!this->returnType()->passByValue() && i == 0);
			// This is the SRET arg for when return type is pass-by-pointer.
			AI->setName("ret");

			// Set SRET and NoAlias attributes.
			llvm::AttrBuilder builder;
			builder.addAttribute(llvm::Attribute::StructRet);
			builder.addAttribute(llvm::Attribute::NoAlias);
			llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1, builder);
			AI->addAttr(set);
		}
		else if(winter_arg_index >= (int)this->args.size())
		{
			// Hidden pointer to env arg.
			AI->setName("hidden");

			llvm::AttrBuilder builder;
			//builder.addAttribute(llvm::Attribute::ByVal);
#if TARGET_LLVM_VERSION >= 34
			builder.addAttribute(llvm::Attribute::ReadOnly); // From LLVM Lang ref: 
			// "On an argument, this attribute indicates that the function does not write through this pointer argument, even though it may write to the memory that the pointer points to."
#endif			
			builder.addAttribute(llvm::Attribute::NoAlias);
			llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1 + i, builder); // "the attributes for the parameters start at  index `1'." - Attributes.h
			AI->addAttr(set);
		}
		else
		{
			// Normal function arg.
			AI->setName(this->args[winter_arg_index].name);

			if(!this->args[winter_arg_index].type->passByValue()) // If pointer arg:
			{
				llvm::AttrBuilder builder;
#if TARGET_LLVM_VERSION >= 34
				builder.addAttribute(llvm::Attribute::ReadOnly);
#endif
				builder.addAttribute(llvm::Attribute::NoAlias);

				llvm::AttributeSet set = llvm::AttributeSet::get(module->getContext(), 1 + i, builder);
				AI->addAttr(set);
			}

			//if(this->args[i-1].type->getType() == Type::OpaqueTypeType)
			//	//builder.addAttribute(llvm::Attribute::ByVal);
		}
	}

	return llvm_func;
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module,
	const PlatformUtils::CPUInfo& cpu_info,
	bool hidden_voidptr_arg, 
	const llvm::DataLayout/*TargetData*/* target_data,
	const CommonFunctions& common_functions,
	std::set<Reference<const Type>, ConstTypeRefLessThan>& destructors_called_types,
	ProgramStats& stats,
	bool emit_trace_code,
	bool with_captured_var_struct_ptr
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	)
{
	//NEW: do a pass to get the cleanup nodes first
	/*{
		TraversalPayload payload(TraversalPayload::GetCleanupNodes, false, NULL);
		std::vector<ASTNode*> stack(1, this);
		this->body->traverse(payload, stack);
	}
*/

	llvm::Function* llvm_func = this->getOrInsertFunction(
		module,
		with_captured_var_struct_ptr//this->use_captured_vars // use_cap_var_struct_ptr
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
	params.destructors_called_types = &destructors_called_types;
	params.emit_refcounting_code = true;
	params.stats = &stats;
	params.emit_trace_code = emit_trace_code;

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
		// Increment ref counts on all (ref counted) arguments:
//		const size_t num_sret_args = this->returnType()->passByValue() ? 0 : 1;
//		for(size_t i=0; i<this->sig.param_types.size(); ++i)
//			sig.param_types[i]->emitIncrRefCount(params, LLVMTypeUtils::getNthArg(llvm_func, i + num_sret_args));


		if(this->returnType()->passByValue())
		{
			llvm::Value* body_code = this->body->emitLLVMCode(params);

			// Emit cleanup code for reference-counted values.
			for(size_t z=0; z<params.cleanup_values.size(); ++z)
			{
				// Don't want to clean up (decr ref) the return value.
				//if(params.cleanup_values[z].node != this->body.getPointer())
				//	params.cleanup_values[z].node->emitCleanupLLVMCode(params, params.cleanup_values[z].value);
			//	CleanUpInfo& cleanup_info = params.cleanup_values[z];
			//	cleanup_info.node->type()->emitDestructorCall(params, cleanup_info.value, "stack allocated cleanup");
			}

			// Decrement ref counts on all (ref counted) arguments:
//			for(size_t i=0; i<this->sig.param_types.size(); ++i)
//				sig.param_types[i]->emitDecrRefCount(params, LLVMTypeUtils::getNthArg(llvm_func, i));

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
				{
					//if(this->returnType()->getType() == Type::ArrayTypeType)
					//{
					//	llvm::Value* size = llvm::ConstantInt::get(*params.context, llvm::APInt(32, sizeof(int) * state_type.downcast<ArrayType>()->num_elems, true)); // TEMP HACK
					//	params.builder->CreateMemCpy(return_val_ptr, body_code, size, 4);
					//}
					//else
					{
						// Load value
						LLVMTypeUtils::createCollectionCopy(
							body->type(), 
							return_val_ptr, // dest ptr
							body_code, // src ptr
							params
						);
					}
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
			/*for(size_t z=0; z<params.cleanup_values.size(); ++z)
			{
				// Don't want to clean up (decr ref) the return value.
				if(params.cleanup_values[z].node != this->body.getPointer())
					params.cleanup_values[z].node->emitCleanupLLVMCode(params, params.cleanup_values[z].value);
			}*/

			// Decrement ref counts on all (ref counted) arguments:
//			for(size_t i=0; i<this->sig.param_types.size(); ++i)
//				sig.param_types[i]->emitDecrRefCount(params, LLVMTypeUtils::getNthArg(llvm_func, i));

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
}


Reference<ASTNode> FunctionDefinition::clone(CloneMapType& clone_map)
{
	FunctionDefinitionRef f = new FunctionDefinition(
		this->srcLocation(),
		this->order_num,
		this->sig.name,
		this->args,
		this->body.nonNull() ? this->body->clone(clone_map) : NULL,
		this->declared_return_type,
		this->built_in_func_impl
	);

	f->captured_vars = this->captured_vars;
	f->captured_var_types = this->captured_var_types;
	f->is_anon_func = this->is_anon_func;
	f->need_to_emit_captured_var_struct_version = this->need_to_emit_captured_var_struct_version;

	clone_map.insert(std::make_pair(this, f.getPointer()));
	return f;
}


bool FunctionDefinition::isGenericFunction() const // true if it is parameterised by type.
{
	return !generic_type_param_names.empty();
	//for(size_t i=0; i<this->args.size(); ++i)
	//	if(this->args[i].type->getType() == Type::GenericTypeType)
	//		return true;
	//return false;
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
	/*if(body.isNull())
		return false;

	return this->body->isConstant();*/

	// A FunctionDefinition node always returns the same anonymous function, so it's constant.
	return true;
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


StructureTypeRef FunctionDefinition::getCapturedVariablesStructType() const
{
	vector<TypeRef> field_types;
	vector<string> field_names;

	for(size_t i=0; i<this->captured_vars.size(); ++i)
	{
		// Get the type of the captured variable.
		field_types.push_back(this->captured_vars[i].type());
		field_names.push_back("captured_var_" + toString((uint64)i));
	}
			

	//NOTE: this is pretty heavyweight.
	return new StructureType(this->sig.typeMangledName() + "_captured_var_struct", field_types, field_names);
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


void CapturedVar::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	std::string vartype_s;
	switch(vartype)
	{
	case Let:
		vartype_s = "Let"; break;
	case Arg:
		vartype_s = "Arg"; break;
	case Captured:
		vartype_s = "Captured"; break;
	};

	s << "captured var, vartype=" + vartype_s + ", free_index=" << free_index;
	if(bound_function)
		s << " bound func: " + bound_function->sig.toString() + ", arg_index=" << arg_index;
	if(bound_let_node)
		s << " bound let node: " + toHexString((uint64)bound_let_node);
	s << "\n";
}


} // end namespace Winter

