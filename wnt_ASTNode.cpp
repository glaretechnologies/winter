/*=====================================================================
ASTNode.cpp
-----------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "wnt_ASTNode.h"


#include "wnt_FunctionExpression.h"
#include "wnt_SourceBuffer.h"
#include "wnt_Diagnostics.h"
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


static const bool VERBOSE_EXEC = false;


namespace Winter
{


void printMargin(int depth, std::ostream& s)
{
	for(int i=0; i<depth; ++i)
		s << "  ";
}


bool isIntExactlyRepresentableAsFloat(int x)
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


bool shouldFoldExpression(ASTNodeRef& e, TraversalPayload& payload)
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
	

ASTNodeRef foldExpression(ASTNodeRef& e, TraversalPayload& payload)
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

		return ASTNodeRef(new FloatLiteral(val->value, e->srcLocation()));
	}
	else if(e->type()->getType() == Type::IntType)
	{
		assert(dynamic_cast<IntValue*>(retval.getPointer()));
		IntValue* val = static_cast<IntValue*>(retval.getPointer());

		return ASTNodeRef(new IntLiteral(val->value, e->srcLocation()));
	}
	else if(e->type()->getType() == Type::BoolType)
	{
		assert(dynamic_cast<BoolValue*>(retval.getPointer()));
		BoolValue* val = static_cast<BoolValue*>(retval.getPointer());

		return ASTNodeRef(new BoolLiteral(val->value, e->srcLocation()));
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


void convertOverloadedOperators(ASTNodeRef& e, TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(e.isNull())
		return;

	switch(e->nodeType())
	{
	case ASTNode::AdditionExpressionType:
	{
		AdditionExpression* expr = static_cast<AdditionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_add function call.
					e = ASTNodeRef(new FunctionExpression(expr->srcLocation(), "op_add", expr->a, expr->b));
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables, payload.hidden_voidptr_arg, payload.env);
					new_payload.top_lvl_frame = payload.top_lvl_frame;
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::SubtractionExpressionType:
	{
		SubtractionExpression* expr = static_cast<SubtractionExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
				if(expr->a->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_sub function call.
					e = ASTNodeRef(new FunctionExpression(expr->srcLocation(), "op_sub", expr->a, expr->b));
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables, payload.hidden_voidptr_arg, payload.env);
					new_payload.top_lvl_frame = payload.top_lvl_frame;
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::MulExpressionType:
	{
		MulExpression* expr = static_cast<MulExpression*>(e.getPointer());
		assert(expr->a->type().nonNull() && expr->b->type().nonNull());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
			{
				// Replace expr with an op_mul function call.
				e = ASTNodeRef(new FunctionExpression(expr->srcLocation(), "op_mul", expr->a, expr->b));
				payload.tree_changed = true;

				// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
				// This is needed now because we need to know the type of op_X, which is only available once bound.
				TraversalPayload new_payload(TraversalPayload::BindVariables, payload.hidden_voidptr_arg, payload.env);
				new_payload.top_lvl_frame = payload.top_lvl_frame;
				new_payload.linker = payload.linker;
				new_payload.func_def_stack = payload.func_def_stack;
				e->traverse(new_payload, stack);
			}
		break;
	}
	case ASTNode::DivExpressionType:
	{
		DivExpression* expr = static_cast<DivExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
				{
					// Replace expr with an op_div function call.
					e = ASTNodeRef(new FunctionExpression(expr->srcLocation(), "op_div", expr->a, expr->b));
					payload.tree_changed = true;

					// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
					// This is needed now because we need to know the type of op_X, which is only available once bound.
					TraversalPayload new_payload(TraversalPayload::BindVariables, payload.hidden_voidptr_arg, payload.env);
					new_payload.top_lvl_frame = payload.top_lvl_frame;
					new_payload.linker = payload.linker;
					new_payload.func_def_stack = payload.func_def_stack;
					e->traverse(new_payload, stack);
				}
		break;
	}
	case ASTNode::ComparisonExpressionType:
	{
		ComparisonExpression* expr = static_cast<ComparisonExpression*>(e.getPointer());
		if(expr->a->type().nonNull() && expr->b->type().nonNull())
			if(	expr->a->type()->getType() == Type::StructureTypeType ||
				expr->b->type()->getType() == Type::StructureTypeType)
			{
				// Replace expr with a function call.
				e = ASTNodeRef(new FunctionExpression(expr->srcLocation(), expr->getOverloadedFuncName(), expr->a, expr->b));
				payload.tree_changed = true;

				// Do a bind traversal of the new subtree now, in order to bind the new op_X function.
				// This is needed now because we need to know the type of op_X, which is only available once bound.
				TraversalPayload new_payload(TraversalPayload::BindVariables, payload.hidden_voidptr_arg, payload.env);
				new_payload.top_lvl_frame = payload.top_lvl_frame;
				new_payload.linker = payload.linker;
				new_payload.func_def_stack = payload.func_def_stack;
				e->traverse(new_payload, stack);
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


CapturedVar::CapturedVar()
:	bound_function(NULL),
	bound_let_block(NULL)
{}


TypeRef CapturedVar::type() const
{
	if(this->vartype == Let)
	{
		assert(this->bound_let_block);
		return this->bound_let_block->lets[this->index]->type();
	}
	else if(this->vartype == Arg)
	{
		assert(this->bound_function);
		return this->bound_function->args[this->index].type;
	}
	else
	{
		assert(!"Invalid vartype");
		return TypeRef();
	}
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
const std::string errorContext(const ASTNode& n)
{
	//if(!payload.func_def_stack.empty())
	//	return "In function " + payload.func_def_stack[payload.func_def_stack.size() - 1]->sig.toString();
	//return "";

	/*for(int i=(int)payload.func_def_stack.size() - 1; i >= 0; --i)
	{
	s +=  "In function " + payload.func_def_stack[i]->sig.toString();
	}*/

	const SourceBuffer* source_buffer = n.srcLocation().source_buffer;
	if(source_buffer == NULL)
		return "Invalid Location";

	return Diagnostics::positionString(*source_buffer, n.srcLocation().char_index);
}


const std::string errorContext(const ASTNode& n, TraversalPayload& payload)
{
	return errorContext(n);
}


Variable::Variable(const std::string& name_, const SrcLocation& loc)
:	ASTNode(loc),
	name(name_),
	bound_index(-1),
	bound_function(NULL),
	bound_let_block(NULL)
	//use_captured_var(false),
	//captured_var_index(0)
{
}


void Variable::bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack)
{
	bool in_current_func_def = true;
	int use_let_frame_offset = 0;
	for(int s = (int)stack.size() - 1; s >= 0; --s) // Walk up the stack of ancestor nodes
	{
		if(FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[s])) // If node is a function definition:
		{
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

					return;
				}

			in_current_func_def = false;
		}
		else if(LetBlock* let_block = dynamic_cast<LetBlock*>(stack[s]))
		{
			for(unsigned int i=0; i<let_block->lets.size(); ++i)
				if(let_block->lets[i]->variable_name == this->name)
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
					}
		
					return;
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
	Frame::NameToFuncMapType::iterator res = payload.top_lvl_frame->name_to_functions_map.find(this->name);
	if(res != payload.top_lvl_frame->name_to_functions_map.end())
	{
		vector<FunctionDefinitionRef>& matching_functions = res->second;

		assert(matching_functions.size() > 0);

		if(matching_functions.size() > 1)
			throw BaseException("Ambiguous binding for variable '" + this->name + "': multiple functions with name." + 
				errorContext(*this, payload));

		this->vartype = BoundToGlobalDefVariable;
		this->bound_function = matching_functions[0].getPointer();
		return;
	}


	throw BaseException("Variable::bindVariables(): No such function, function argument or let definition '" + this->name + "'." + 
		errorContext(*this, payload));
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
		this->bindVariables(payload, stack);
}


ValueRef Variable::exec(VMState& vmstate)
{
	if(this->vartype == ArgumentVariable)
	{
		return vmstate.argument_stack[vmstate.func_args_start.back() + bound_index];
	}
	else if(this->vartype == LetVariable)
	{
		const int let_stack_start = (int)vmstate.let_stack_start[vmstate.let_stack_start.size() - 1 - this->let_frame_offset];

		return vmstate.let_stack[let_stack_start + this->bound_index];
	}
	else if(this->vartype == BoundToGlobalDefVariable)
	{
		StructureValueRef captured_vars(new StructureValue(vector<ValueRef>()));
		return ValueRef(new FunctionValue(this->bound_function, captured_vars));
	}
	else if(this->vartype == CapturedVariable)
	{
		// Get ref to capturedVars structure of values, will be passed in as last arg to function
		ValueRef captured_struct = vmstate.argument_stack.back();
		assert(dynamic_cast<StructureValue*>(captured_struct.getPointer()));
		StructureValue* s = static_cast<StructureValue*>(captured_struct.getPointer());

		return s->fields[this->bound_index];
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
	else if(this->vartype == CapturedVariable)
	{
		if(this->bound_function != NULL)
			return this->bound_function->args[this->uncaptured_bound_index].type;
		else
			return this->bound_let_block->lets[this->uncaptured_bound_index]->type();
	}
	else
	{
		assert(!"invalid vartype.");
		return TypeRef(NULL);
	}
}


inline static const std::string varType(Variable::VariableType t)
{
	if(t == Variable::LetVariable)
		return "Let";
	else if(t == Variable::ArgumentVariable)
		return "Arg";
	else if(t == Variable::BoundToGlobalDefVariable)
		return "BoundToGlobalDef";
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
			//if(params.currently_building_func_def->returnType()->passByValue())
			//	return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index);
			//else
			//	return LLVMTypeUtils::getNthArg(params.currently_building_func, this->bound_index + 1);

		return LLVMTypeUtils::getNthArg(
			params.currently_building_func,
			params.currently_building_func_def->getLLVMArgIndex(this->bound_index)
		);
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
		return this->bound_function->emitLLVMCode(params);
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
			*params.currently_building_func_def->getCapturedVariablesStructType()->LLVMType(*params.context)
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

		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true))); // array index
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->bound_index, true))); // field index
		
		//TEMP
		//params.currently_building_func->dump();
		//base_cap_var_structure->dump();
		//cap_var_structure->dump();

		llvm::Value* field_ptr = params.builder->CreateGEP(
			cap_var_structure, // ptr
			indices
		);

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
			convertOverloadedOperators(items[i].first, payload, stack);
			convertOverloadedOperators(items[i].second, payload, stack);
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
	MapLiteral* m = new MapLiteral(srcLocation());
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


ArrayLiteral::ArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc)
:	ASTNode(loc),
	elements(elems)
{
	//this->t
	if(elems.empty())
		throw BaseException("Array literal can't be empty." + errorContext(*this));
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
			convertOverloadedOperators(elements[i], payload, stack);
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
				throw BaseException("Array element " + ::toString(i) + " did not have required type " + elem_type->toString() + "." + 
				errorContext(*this, payload));
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
	return ASTNodeRef(new ArrayLiteral(elems, srcLocation()));
}


bool ArrayLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


//------------------------------------------------------------------------------------------


VectorLiteral::VectorLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc)
:	ASTNode(loc),
	elements(elems)
{
	if(elems.empty())
		throw BaseException("Vector literal can't be empty." + errorContext(*this));
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
			convertOverloadedOperators(elements[i], payload, stack);
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
				throw BaseException("Vector element " + ::toString(i) + " did not have required type " + elem_type->toString() + "." + errorContext(*this, payload));
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
		//llvm_vec_type, 
		std::vector<llvm::Constant*>(
			this->elements.size(),
			this->elements[0]->type()->defaultLLVMValue(*params.context)
			//llvm::ConstantFP::get(*params.context, llvm::APFloat(0.0))
		)
	);
	/*llvm::Value* v = llvm::ConstantVector::get(
		llvm_vec_type, 
		std::vector<llvm::Constant*>(
			this->elements.size(),
			this->elements[0]->type()->defaultLLVMValue(*params.context)
			//llvm::ConstantFP::get(*params.context, llvm::APFloat(0.0))
		)
	);*/

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
	return ASTNodeRef(new VectorLiteral(elems, srcLocation()));
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float in addition operation:
		// 3.0 + 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 + 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'" + errorContext(*this, payload));
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("AdditionExpression: Binary operator '+' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
	}
}


llvm::Value* AdditionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(this->type()->getType() == Type::VectorTypeType || this->type()->getType() == Type::FloatType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FAdd, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Add, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for AdditionExpression code emission");
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> AdditionExpression::clone()
{
	AdditionExpression* e = new AdditionExpression(this->srcLocation());
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 - 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 - 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}
	}

	if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '-' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
	}

}


llvm::Value* SubtractionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(this->type()->getType() == Type::VectorTypeType || this->type()->getType() == Type::FloatType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FSub, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Sub, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for AdditionExpression code emission");
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> SubtractionExpression::clone()
{
	SubtractionExpression* e = new SubtractionExpression(this->srcLocation());
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


TypeRef MulExpression::type() const
{
	return a->type();
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/


	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();


	// NEW: moved to after child traversal
	/*if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 * 4
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 * 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '*' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
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
	if(this->type()->getType() == Type::VectorTypeType || this->type()->getType() == Type::FloatType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::FMul, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Mul, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
		);
	}
	else
	{
		throw BaseException("Unknown type for MulExpression code emission");
	}
#else
	return NULL;
#endif
}


Reference<ASTNode> MulExpression::clone()
{
	MulExpression* e = new MulExpression(this->srcLocation());
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// implicit conversion from int to float
		// 3.0 / 4
		// Only do this if b is != 0.  Otherwise we are messing with divide by zero semantics.
		if(a->nodeType() == ASTNode::FloatLiteralType && b->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* b_lit = static_cast<IntLiteral*>(b.getPointer());
			if(isIntExactlyRepresentableAsFloat(b_lit->value) && (b_lit->value != 0))
			{
				b = ASTNodeRef(new FloatLiteral((float)b_lit->value, b->srcLocation()));
				payload.tree_changed = true;
			}
		}

		// 3 / 4.0
		if(b->nodeType() == ASTNode::FloatLiteralType && a->nodeType() == ASTNode::IntLiteralType)
		{
			IntLiteral* a_lit = static_cast<IntLiteral*>(a.getPointer());
			if(isIntExactlyRepresentableAsFloat(a_lit->value))
			{
				a = ASTNodeRef(new FloatLiteral((float)a_lit->value, a->srcLocation()));
				payload.tree_changed = true;
			}
		}
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(this->type()->getType() == Type::GenericTypeType || *this->type() == Int() || *this->type() == Float())
		{
			if(*a->type() != *b->type())
				throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
		}
		else if(a->type()->getType() == Type::VectorTypeType && b->type()->getType() == Type::VectorTypeType)
		{
			// this is alright.
			// NOTE: need to do more checking tho.
			// Need to check number of elements is same in both vectors, and field types are the same.
		}
		else
		{
			throw BaseException("Binary operator '/' not defined for types '" +  a->type()->toString() + "' and '" +  b->type()->toString() + "'." + errorContext(*this, payload));
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
	DivExpression* e = new DivExpression(this->srcLocation());
	e->a = this->a->clone();
	e->b = this->b->clone();
	return ASTNodeRef(e);
}


bool DivExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


//-------------------------------------------------------------------------------------------------------


BinaryBooleanExpr::BinaryBooleanExpr(Type t_, const ASTNodeRef& a_, const ASTNodeRef& b_, const SrcLocation& loc)
:	ASTNode(loc),
	t(t_), a(a_), b(b_)
{
}


ValueRef BinaryBooleanExpr::exec(VMState& vmstate)
{
	ValueRef aval = a->exec(vmstate);
	ValueRef bval = b->exec(vmstate);
	ValueRef retval;

	if(t == OR)
	{
		return ValueRef(new BoolValue(
			static_cast<BoolValue*>(aval.getPointer())->value || 
			static_cast<BoolValue*>(bval.getPointer())->value
		));
	}
	else if(t == AND)
	{
		return ValueRef(new BoolValue(
			static_cast<BoolValue*>(aval.getPointer())->value &&
			static_cast<BoolValue*>(bval.getPointer())->value
		));
	}
	else
	{
		assert(!"invalid t");
		return ValueRef();
	}
}


void BinaryBooleanExpr::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	
	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(a->type()->getType() != Winter::Type::BoolType)
			throw BaseException("First child does not have boolean type." + errorContext(*this, payload));

		if(b->type()->getType() != Winter::Type::BoolType)
			throw BaseException("Second child does not have boolean type." + errorContext(*this, payload));
	}
}


void BinaryBooleanExpr::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Binary boolean Expression ";
	if(t == OR)
		s << " (OR)";
	else if(t == AND)
		s << " (AND)";
	s << "\n";

	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


llvm::Value* BinaryBooleanExpr::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM
	if(t == AND)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::And, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
			);
	}
	else if(t == OR)
	{
		return params.builder->CreateBinOp(
			llvm::Instruction::Or, 
			a->emitLLVMCode(params), 
			b->emitLLVMCode(params)
			);
	}
	else
	{
		assert(!"t type invalid!");
		return NULL;
	}

#else
	return NULL;
#endif
}


Reference<ASTNode> BinaryBooleanExpr::clone()
{
	return ASTNodeRef(new BinaryBooleanExpr(t, a, b, srcLocation()));
}


bool BinaryBooleanExpr::isConstant() const
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload, stack);
	}*/

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

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
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
	UnaryMinusExpression* e = new UnaryMinusExpression(this->srcLocation());
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload, stack);
	}*/

	stack.push_back(this);
	expr->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
}


llvm::Value* LetASTNode::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return expr->emitLLVMCode(params);
}


Reference<ASTNode> LetASTNode::clone()
{
	LetASTNode* e = new LetASTNode(this->variable_name, this->srcLocation());
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
	case Type::BoolType:
		retval = ValueRef(compare<BoolValue>(this->token->getType(), aval.getPointer(), bval.getPointer()));
		break;
	default:
		assert(!"ComparisonExpression type invalid!");
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}*/

	stack.push_back(this);
	a->traverse(payload, stack);
	b->traverse(payload, stack);
	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(a, payload, stack);
		convertOverloadedOperators(b, payload, stack);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		if(a->type()->getType() == Type::GenericTypeType || a->type()->getType() == Type::IntType || a->type()->getType() == Type::FloatType || a->type()->getType() == Type::BoolType)
		{}
		else
		{
			throw BaseException("Child type '" + this->type()->toString() + "' does not define Comparison operators. (First child type: " + a->type()->toString() + ")." + errorContext(*this, payload));
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
			default: assert(0); throw BaseException("Unsupported token type for comparison.");
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
	case Type::BoolType:
		{
			switch(this->token->getType())
			{
			case DOUBLE_EQUALS_TOKEN: return params.builder->CreateICmpEQ(a_code, b_code);
			case NOT_EQUALS_TOKEN: return params.builder->CreateICmpNE(a_code, b_code);
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
	return Reference<ASTNode>(new ComparisonExpression(token, a, b, this->srcLocation()));
}


bool ComparisonExpression::isConstant() const
{
	return a->isConstant() && b->isConstant();
}


const std::string ComparisonExpression::getOverloadedFuncName() const // returns e.g. op_lt, op_gt   etc..
{
	switch(this->token->getType())
	{
	case LEFT_ANGLE_BRACKET_TOKEN: return "op_lt";
	case RIGHT_ANGLE_BRACKET_TOKEN: return "op_gt";
	case DOUBLE_EQUALS_TOKEN: return "op_eq";
	case NOT_EQUALS_TOKEN: return "op_neq";
	case LESS_EQUAL_TOKEN: return "op_lte";
	case GREATER_EQUAL_TOKEN: return "op_gte";
	default: assert(0); throw BaseException("Unsupported token type for comparison");
	}
}


//----------------------------------------------------------------------------------------


ValueRef LetBlock::exec(VMState& vmstate)
{
	const size_t let_stack_size = vmstate.let_stack.size();
	vmstate.let_stack_start.push_back(let_stack_size); // Push let frame index

	// Evaluate let clauses, which will each push the result onto the let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.push_back(lets[i]->exec(vmstate));


	ValueRef retval = this->expr->exec(vmstate);

	// Pop things off let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.pop_back();
	
	// Pop let frame index
	vmstate.let_stack_start.pop_back();

	return retval;
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
	/*else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
	{
		convertOverloadedOperators(expr, payload, stack);
	}*/

	stack.push_back(this);

	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->traverse(payload, stack);

	//payload.let_block_stack.push_back(this);

	expr->traverse(payload, stack);

	//payload.let_block_stack.pop_back();

	stack.pop_back();

	if(payload.operation == TraversalPayload::BindVariables)
	{
		convertOverloadedOperators(expr, payload, stack);
	}
}


llvm::Value* LetBlock::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	params.let_block_stack.push_back(const_cast<LetBlock*>(this));

	return expr->emitLLVMCode(params);

	params.let_block_stack.pop_back();

}


Reference<ASTNode> LetBlock::clone()
{
	vector<Reference<LetASTNode> > new_lets(lets.size());
	for(size_t i=0; i<new_lets.size(); ++i)
		new_lets[i] = Reference<LetASTNode>(static_cast<LetASTNode*>(lets[i]->clone().getPointer()));
	Winter::ASTNodeRef clone = this->expr->clone();
	return ASTNodeRef(new LetBlock(clone, new_lets, this->srcLocation()));
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
