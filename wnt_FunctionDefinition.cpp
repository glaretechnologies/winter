/*=====================================================================
FunctionDefinition.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-04-25 19:15:40 +0100
=====================================================================*/
#include "wnt_FunctionDefinition.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_Variable.h"
#include "wnt_LetASTNode.h"
#include "VirtualMachine.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMUtils.h"
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


using std::vector;
using std::string;


namespace Winter
{


static const std::vector<TypeVRef> makeArgTypeVector(const std::vector<FunctionDefinition::FunctionArg>& args)
{
	std::vector<TypeVRef> res;
	res.reserve(args.size());
	for(unsigned int i=0; i<args.size(); ++i)
		res.push_back(args[i].type);
	return res;
}

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
	//use_captured_vars(false),
	need_to_emit_captured_var_struct_version(false),
	is_anon_func(false),
	num_uses(0),
	noinline(false),
	sig(name, makeArgTypeVector(args_)),
	llvm_reported_stack_size(-1)
{
	
	// TODO: fix this, make into method
	//function_type = TypeRef(new Function(sig.param_types, declared_rettype));

	//this->let_exprs_llvm_value = std::vector<llvm::Value*>(this->lets.size(), NULL);
}


FunctionDefinition::~FunctionDefinition()
{
	// For any free variables, erase this function definition from the variable's lambda set, as we don't want dangling pointers.
	for(auto it = free_variables.begin(); it != free_variables.end(); ++it)
	{
		(*it)->lambdas.erase(this);
	}
}


TypeRef FunctionDefinition::returnType() const
{
	if(this->declared_return_type.nonNull())
		return this->declared_return_type;
	
	return this->body->type();
}


TypeRef FunctionDefinition::type() const
{
	//vector<TypeRef> captured_var_types;
	//for(size_t i=0; i<this->captured_vars.size(); ++i)
	//	captured_var_types.push_back(this->captured_vars[i].type());
	const TypeRef return_type = this->returnType();
	if(return_type.isNull()) return NULL;

	return new Function(makeArgTypeVector(this->args), TypeVRef(return_type), /*captured_var_types, */true); //this->use_captured_vars);
}


ValueRef FunctionDefinition::exec(VMState& vmstate)
{
	// Capture variables at this point, by getting them off the arg and let stack.
	vector<ValueRef> vals;
	if(vmstate.capture_vars)
	{
		for(auto it = free_variables.begin(); it != free_variables.end(); ++it)
		{
			Variable* var = *it;

			// TODO: need to handle this free variable being bound to another free variable?
			/*if()
			{
				// Get ref to capturedVars structure of values, will be passed in as last arg to function
				ValueRef captured_struct = vmstate.argument_stack.back();
				const StructureValue* s = checkedCast<StructureValue>(captured_struct.getPointer());

				const int free_index = this->getFreeIndexForVar(var);
				ValueRef val = s->fields[free_index];

				vals.push_back(val);
			}
			else */if(var->binding_type == Variable::BindingType_Argument)
			{
				vals.push_back(vmstate.argument_stack[vmstate.func_args_start.back() + var->arg_index]);
			}
			else if(var->binding_type == Variable::BindingType_Let)
			{
				LetASTNode* bound_let_node = var->bound_let_node;
				ValueRef val = var->bound_let_node->exec(vmstate);

				if(bound_let_node->vars.size() != 1)
				{
					// Destructuring assignment, return the particular element from the tuple.
					const TupleValue* t = checkedCast<TupleValue>(val.getPointer());
					val = t->e[var->let_var_index];
				}

				vals.push_back(val);
			}
			else
			{
				assert(0);
				throw BaseException("internal error 136");
			}
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
	else if(payload.operation == TraversalPayload::CountArgumentRefs)
	{
		for(size_t i = 0; i<this->args.size(); ++i)
			this->args[i].ref_count = 0;
	}
	else if(payload.operation == TraversalPayload::UnbindVariables)
	{
		this->free_variables.clear();
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
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		if(body.nonNull())
			checkSubstituteVariable(body, payload);

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
			for(auto it = free_variables.begin(); it != free_variables.end(); ++it)
			{
				payload.captured_types.insert((*it)->type());
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

		if(this->built_in_func_impl.nonNull())
			this->built_in_func_impl->linkInCalledFunctions(payload);


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

		if(this->built_in_func_impl.nonNull())
			this->built_in_func_impl->deadFunctionEliminationTraverse(payload);
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


void FunctionDefinition::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	assert(body.ptr() == old_val);
	this->body = new_val;
}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef (" + toHexString((uint64)this) + "): " << this->sig.toString() << " " << 
		(this->returnType().nonNull() ? this->returnType()->toString() : "[Unknown ret type]");
	if(this->declared_return_type.nonNull())
		s << " (Declared ret type: " + this->declared_return_type->toString() << ")";
	s << "\n";

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
	if(body.isNull())
		return s + " [NULL BODY]";
	else
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
			opencl_sig += "const " + args[i].type->address_space + " " + args[i].type->OpenCLCType() + "* const " + mapOpenCLCVarName(params.opencl_c_keywords, args[i].name);
		else
			opencl_sig += "const " + args[i].type->OpenCLCType() + " " + mapOpenCLCVarName(params.opencl_c_keywords, args[i].name);
			
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
	if((body->nodeType() == ASTNode::VariableASTNodeType) && (body.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument) && body->type()->OpenCLPassByPointer())
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
			"closure"
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
	llvm::Value* closure_ref_count_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, 0, "closure_ref_count_ptr");
	llvm::Value* one = llvm::ConstantInt::get(*params.context, llvm::APInt(64, 1, /*signed=*/true));
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, closure_ref_count_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "Set initial ref count to 1");
	

	// Set the flags
	llvm::Value* flags_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, 1, "closure_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "set initial flags to " + toString(initial_flags));


	// Store function pointer to the anon function in the closure structure
	{
		llvm::Function* func = this->getOrInsertFunction(
			params.module,
			true // use_cap_var_struct_ptr: Since we're storing a func ptr, it will be passed the captured var struct on usage.
		);

		llvm::Value* func_field_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, Function::functionPtrIndex(), "function_field_ptr");
		llvm::StoreInst* store_inst_ = params.builder->CreateStore(func, func_field_ptr); // Do the store.
		addMetaDataCommentToInstruction(params, store_inst_, "Store function pointer in closure");
	}

	// Store captured vars in closure.
	
	llvm::Value* captured_var_struct_ptr = LLVMUtils::createStructGEP(params.builder, closure_pointer, Function::capturedVarStructIndex(), "captured_var_struct_ptr");

	params.stats->num_free_vars += free_variables.size();

	// for each captured var
	size_t free_var_index = 0;
	for(auto z = free_variables.begin(); z != free_variables.end(); ++z)
	{
		Variable* free_var = *z;
	
		llvm::Value* val = NULL;
		// TODO: need to handle this free variable being bound to another free variable?
		/*if()
		{
			// This captured var is bound to a captured var itself.
			// The target captured var is in the captured var struct for the function, which is passed as the last arg to the function body.
			//FunctionDefinition* def = this->captured_vars[i].bound_function;
			llvm::Value* base_current_func_captured_var_struct = LLVMUtils::getLastArg(params.currently_building_func);

			// Now we need to downcast it to the correct type.
			llvm::Type* actual_cap_var_struct_type = params.currently_building_func_def->getCapturedVariablesStructType()->LLVMType(*params.module);

			llvm::Value* current_func_captured_var_struct = params.builder->CreateBitCast(base_current_func_captured_var_struct, LLVMTypeUtils::pointerType(actual_cap_var_struct_type), "actual_cap_var_struct_type");

			llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, current_func_captured_var_struct, this->captured_vars[i].free_index);

			val = params.builder->CreateLoad(field_ptr);
		}
		else */if(free_var->binding_type == Variable::BindingType_Argument)
		{
			// Load arg
			val = LLVMUtils::getNthArg(
				params.currently_building_func,
				params.currently_building_func_def->getLLVMArgIndex(free_var->arg_index)
			);
		}
		else if(free_var->binding_type == Variable::BindingType_Let)
		{
			// Load let:
			LetASTNode* let_node = free_var->bound_let_node;

			assert(params.let_values.find(let_node) != params.let_values.end());
			val = params.let_values[let_node];

			// NOTE: This code is duplicated from Variable.  de-duplicate.
			TypeRef cap_var_type_win = free_var->type();

			if(let_node->vars.size() > 1)
			{
				// Destructuring assignment, we just want to return the individual tuple element.
				// Value should be a pointer to a tuple struct.
				if(cap_var_type_win->passByValue())
				{
					llvm::Value* tuple_elem = params.builder->CreateLoad(LLVMUtils::createStructGEP(params.builder, val, free_var->let_var_index, "tuple_elem_ptr"));
					val = tuple_elem;
				}
				else
				{
					llvm::Value* tuple_elem = LLVMUtils::createStructGEP(params.builder, val, free_var->let_var_index, "tuple_elem_ptr");
					val = tuple_elem;
				}
			}
		}
		else
		{
			assert(0);
		}

		
		// store in captured var structure field
		llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, captured_var_struct_ptr, (unsigned int)free_var_index, "captured_var_" + toString(free_var_index) + "_field_ptr");
		llvm::StoreInst* store_inst_ = params.builder->CreateStore(val, field_ptr);

		addMetaDataCommentToInstruction(params, store_inst_, "Store captured var " + toString(free_var_index) + " in closure");

		// If the captured var is a ref-counted type, we need to increment its reference count, since the struct now holds a reference to it. 
		captured_var_struct_type->component_types[free_var_index]->emitIncrRefCount(params, val, "Capture var ref count increment");

		free_var_index++;
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
		llvm::Value* base_captured_var_struct = LLVMUtils::getNthArg(destructor, 0);

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
		llvm::StoreInst* store_inst_ = params.builder->CreateStore(destructor, LLVMUtils::createStructGEP(params.builder, 
			closure_pointer, 
			Function::destructorPtrIndex(), 
			"destructor ptr"));

		addMetaDataCommentToInstruction(params, store_inst_, "Store destructor in closure");
	}



	// Bitcast the closure pointer down to the 'base' closure type.
	llvm::Type* base_closure_type = this->type()->LLVMType(*params.module);

	return params.builder->CreateBitCast(
		closure_pointer,
		base_closure_type,
		"base_closure_ptr"
	);
}


llvm::Function* FunctionDefinition::getOrInsertFunction(
		llvm::Module* module
	) const
{
	return getOrInsertFunction(module, false);
}


static void setArgumentAttributes(llvm::LLVMContext* context, llvm::Function::arg_iterator it, unsigned int index, llvm::AttrBuilder& attr_builder)
{
#if TARGET_LLVM_VERSION >= 60
		it->addAttrs(attr_builder);
#else
		llvm::AttributeSet set = llvm::AttributeSet::get(*context, index, attr_builder);
		it->addAttr(set);
#endif
}


llvm::Function* FunctionDefinition::getOrInsertFunction(
		llvm::Module* module,
		bool use_cap_var_struct_ptr
	) const
{
	const vector<TypeVRef> arg_types = this->sig.param_types;

	/*if(use_captured_vars) // !this->captured_vars.empty())
	{
		// This function has captured variables.
		// So we will make it take a pointer to the closure structure as the last argument.
		//arg_types.push_back(getCapturedVariablesStructType());

		// Add pointer to base type structure.
	}*/

	TypeRef ret_type = returnType();
	if(ret_type.isNull())
		throw BaseException("Interal error: return type was NULL.");

	llvm::FunctionType* functype = LLVMTypeUtils::llvmFunctionType(
		arg_types, 
		use_cap_var_struct_ptr, // this->use_captured_vars, // use captured var struct ptr arg
		TypeVRef(ret_type),
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
	
	
	llvm::AttrBuilder function_attr_builder;
	function_attr_builder.addAttribute(llvm::Attribute::NoUnwind); // Does not throw exceptions

	if(this->noinline)
		function_attr_builder.addAttribute(llvm::Attribute::NoInline);
	
	// We can only mark a function as readonly (or readnone) if the return type is pass by value, otherwise this function will need to write through the SRET argument.
	/*if(this->returnType()->passByValue())
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
			if(has_ptr_arg)
				function_attr_builder.addAttribute(llvm::Attribute::ReadOnly); // This attribute indicates that the function does not write through any pointer arguments etc..
			else
				function_attr_builder.addAttribute(llvm::Attribute::ReadNone); // Function computes its result based strictly on its arguments, without dereferencing any pointer arguments etc..

			//NOTE: can't have readnone when we're doing heap allocs, like having a varray in function body.  Fix this stuff up?
		}
	}*/

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

#if TARGET_LLVM_VERSION >= 60
	typedef	llvm::AttributeList UseAttributeList;
#else
	typedef llvm::AttributeSet UseAttributeList;
#endif

	UseAttributeList attributes = UseAttributeList::get(
		module->getContext(),
		UseAttributeList::FunctionIndex, // Index
		function_attr_builder);


	llvm_func->setAttributes(attributes);


	// If this is an external allocation function, mark the result as noalias.
	if(external_function.nonNull() && external_function->is_allocation_function)
	{
		llvm_func->addAttribute(UseAttributeList::ReturnIndex, llvm::Attribute::NoAlias);
	}

	
	// Mark return type as nonnull if it's a pointer
	if(this->returnType()->getType() == Type::VArrayTypeType)
	{
	//	llvm_func->addAttribute(llvm::AttributeSet::ReturnIndex, llvm::Attribute::NonNull);
	}

	// Boolean type needs to have zero-extend attribute to be ABI-compatible with C++.
	if(this->returnType()->getType() == Type::BoolType)
		llvm_func->addAttribute(UseAttributeList::ReturnIndex, llvm::Attribute::ZExt);

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
			setArgumentAttributes(&module->getContext(), AI, 1, builder);
		}
		else if(winter_arg_index >= (int)this->args.size())
		{
			// Hidden pointer to env arg.
			AI->setName("hidden");

			llvm::AttrBuilder builder;
			//builder.addAttribute(llvm::Attribute::ByVal);

			builder.addAttribute(llvm::Attribute::ReadOnly); // From LLVM Lang ref: 
			// "On an argument, this attribute indicates that the function does not write through this pointer argument, even though it may write to the memory that the pointer points to."
			
			builder.addAttribute(llvm::Attribute::NoAlias);

			setArgumentAttributes(&module->getContext(), AI, /*index=*/i + 1, builder); // "the attributes for the parameters start at  index `1'." - Attributes.h
		}
		else
		{
			// Normal function arg.
			AI->setName(this->args[winter_arg_index].name);

			if(!this->args[winter_arg_index].type->passByValue()) // If pointer arg:
			{
				llvm::AttrBuilder builder;
				builder.addAttribute(llvm::Attribute::ReadOnly);
				builder.addAttribute(llvm::Attribute::NoAlias);
				setArgumentAttributes(&module->getContext(), AI, /*index=*/i + 1, builder);
			}

			// Boolean type needs to have zero-extend attribute to be ABI-compatible with C++.
			if(this->args[winter_arg_index].type->getType() == Type::BoolType)
			{
				llvm::AttrBuilder builder;
				builder.addAttribute(llvm::Attribute::ZExt);
				setArgumentAttributes(&module->getContext(), AI, /*index=*/i + 1, builder);
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
	const llvm::DataLayout/*TargetData*/* target_data,
	const CommonFunctions& common_functions,
	std::set<VRef<const Type>, ConstTypeVRefLessThan>& destructors_called_types,
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
		with_captured_var_struct_ptr // use_cap_var_struct_ptr
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

#if TARGET_LLVM_VERSION >= 60
	builder.setFastMathFlags(flags);
#else
	builder.SetFastMathFlags(flags);
#endif

	// const bool nsz = builder.getFastMathFlags().noSignedZeros();

	// Build body LLVM code
	EmitLLVMCodeParams params;
	params.currently_building_func_def = this;
	params.cpu_info = &cpu_info;
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
//			sig.param_types[i]->emitIncrRefCount(params, LLVMUtils::getNthArg(llvm_func, i + num_sret_args));


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
//				sig.param_types[i]->emitDecrRefCount(params, LLVMUtils::getNthArg(llvm_func, i));

			builder.CreateRet(body_code);
		}
		else
		{
			// Else return type is pass-by-pointer

			// body code will return a pointer to the result of the body expression, allocated on the stack.
			// So load from the stack, and save to the return pointer which will have been passed in as arg zero.
			llvm::Value* return_val_ptr = LLVMUtils::getNthArg(llvm_func, 0);

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
						const TypeRef body_type = body->type();
						if(body_type.isNull())
							throw BaseException("Internal error: body type was null.");

						// Load value
						LLVMUtils::createCollectionCopy(
							TypeVRef(body_type), 
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
//				sig.param_types[i]->emitDecrRefCount(params, LLVMUtils::getNthArg(llvm_func, i));

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

	// NOTE: We don't need to copy free_variables as they should be cleared after copying anyway, since variables will be rebound for copied trees.
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


size_t FunctionDefinition::getTimeBound(GetTimeBoundParams& params) const
{
	if(built_in_func_impl.nonNull())
	{
		return built_in_func_impl->getTimeBound(params);
	}
	else if(external_function.nonNull())
	{
		if(external_function->time_bound == ExternalFunction::unknownTimeBound())
			throw BaseException("Could not bound time for external function " + external_function->sig.toString());

		return external_function->time_bound;
	}
	else
	{
		return this->body->getTimeBound(params);
	}
	
}


GetSpaceBoundResults FunctionDefinition::getSpaceBound(GetSpaceBoundParams& params) const
{
	// TODO: handle space for lambdas as well

	// We require LLVM 6.0 in order to use the diagnostic callback interface, which allows us to get the stack 
	// size used by functions.  If we can't use this information, just use a large upper bound which should suffice.
#if TARGET_LLVM_VERSION >= 60
	// If llvm_used_stack_size == -1, then this information wasn't set by LLVM.  This probably means that this 
	// function wasn't compiled, due it being inlined and then dead-code removed.
	// It could also mean that it literally used zero stack size, for instance if it just gets compiled down to 
	// a single tail call.
	size_t use_stack_size = (this->llvm_reported_stack_size == -1) ? 0 : (size_t)this->llvm_reported_stack_size;

	use_stack_size += sizeof(void*);// The call instruction pushes the return address onto the stack.
	// Consider this space part of the stack used by functions.
#else
	const size_t use_stack_size = 512;
#endif

	if(built_in_func_impl.nonNull())
	{
		GetSpaceBoundResults bounds = built_in_func_impl->getSpaceBound(params);
		bounds.stack_space += use_stack_size;
		return bounds;
	}
	else if(external_function.nonNull())
	{
		if(external_function->stack_size_bound == ExternalFunction::unknownSpaceBound() ||
			external_function->heap_size_bound == ExternalFunction::unknownSpaceBound())
			throw BaseException("Could not bound space for external function " + external_function->sig.toString());

		return GetSpaceBoundResults(external_function->stack_size_bound, external_function->heap_size_bound);
	}
	else
	{
		GetSpaceBoundResults bounds = this->body->getSpaceBound(params);
		bounds.stack_space += use_stack_size;
		return bounds;
	}
}


size_t FunctionDefinition::getSubtreeCodeComplexity() const
{
	if(built_in_func_impl.nonNull())
	{
		return built_in_func_impl->getSubtreeCodeComplexity();
	}
	else if(external_function.nonNull())
	{
		return 100;
	}
	else
	{
		return body->getSubtreeCodeComplexity();
	}
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


StructureTypeVRef FunctionDefinition::getCapturedVariablesStructType() const
{
	vector<TypeVRef> field_types;
	vector<string> field_names;

	size_t free_var_index = 0;
	for(auto z = free_variables.begin(); z != free_variables.end(); ++z)
	{
		// Get the type of the captured variable.
		const TypeRef field_type = (*z)->type();
		if(field_type.isNull())
			throw BaseException("Error: field type for captured var is null.");

		field_types.push_back(TypeVRef(field_type));
		field_names.push_back("captured_var_" + toString((uint64)free_var_index));
		free_var_index++;
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

	assert(!this->free_variables.empty());

	if(this->returnType()->passByValue())
		return (int)this->args.size();
	else
		return (int)this->args.size() + 1; // allow room for sret argument.
}


int FunctionDefinition::getFreeIndexForVar(const Variable* var)
{
	int i=0;
	for(auto it = free_variables.begin(); it != free_variables.end(); ++it, ++i)
	{
		if(*it == var)
			return i;
	}

	assert(0);
	return 0;
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

