/*=====================================================================
ASTNode.cpp
-----------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#include "ASTNode.h"


#include "VMState.h"
#include "Value.h"
#include "Linker.h"

#include "llvm/Type.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/ModuleProvider.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CallingConv.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>


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


//----------------------------------------------------------------------------------


FunctionDefinition::FunctionDefinition(/*ASTNode* parent, */const std::string& name, const std::vector<FunctionArg>& args_, 
									   /*ASTNodeRef& body_, */TypeRef& rettype)
:	//ASTNode(parent), 
	args(args_),
	//body(body_),
	return_type(rettype)
{
	sig.name = name;
	for(unsigned int i=0; i<args_.size(); ++i)
		sig.param_types.push_back(args_[i].type);
}


Value* FunctionDefinition::exec(VMState& vmstate)
{
	// Evaluate let clauses, which will each push the result onto the let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.push_back(lets[i]->exec(vmstate));

	Value* ret = body->exec(vmstate);

	// Pop things off let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.pop_back();

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
	stack.push_back(this);

	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->traverse(payload, stack);

	this->body->traverse(payload, stack);

	stack.pop_back();
}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef: " << this->sig.toString() << " " << this->return_type->toString() << "\n";
	for(unsigned int i=0; i<this->lets.size(); ++i)
		lets[i]->print(depth + 1, s);
	body->print(depth+1, s);
}


llvm::Value* FunctionDefinition::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	//params.currently_building_func = this;
	return body->emitLLVMCode(params);
}


static llvm::FunctionType* llvmInternalFunctionType(const vector<TypeRef>& arg_types, const Type& return_type)
{
	std::vector<const llvm::Type*> the_types;

	for(unsigned int i=0; i<arg_types.size(); ++i)
		the_types.push_back(arg_types[i]->LLVMType());

	return llvm::FunctionType::get(
		return_type.LLVMType(),
		the_types,
		false // varargs
		);
}


llvm::Function* FunctionDefinition::buildLLVMFunction(
	llvm::Module* module
	//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	) const
{
	llvm::FunctionType* functype = llvmInternalFunctionType(this->sig.param_types, *this->return_type);

	llvm::Function *internal_llvm_func = static_cast<llvm::Function*>(module->getOrInsertFunction(
		this->sig.toString(), // internalFuncName(this->getSig()), // Name
		functype // Type
		));


	llvm::BasicBlock* block = llvm::BasicBlock::Create("entry", internal_llvm_func);
	llvm::IRBuilder<> builder(block);

	// Build body LLVM code
	EmitLLVMCodeParams params;
	params.builder = &builder;
	params.module = module;
	params.currently_building_func = internal_llvm_func;
	//params.external_functions = &external_functions;
	//params.compiling_function = this;
	//params.compiling_internal_function = true;

	llvm::Value* body_code = this->body->emitLLVMCode(params);

	builder.CreateRet(body_code);

	return internal_llvm_func;
}


//--------------------------------------------------------------------------------


Value* FunctionExpression::exec(VMState& vmstate)
{
	assert(target_function);

	// Push arguments onto argument stack
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		vmstate.argument_stack.push_back(this->argument_expressions[i]->exec(vmstate));

	// Execute target function
	Value* ret = target_function->exec(vmstate);

	// Remove arguments from stack
	vmstate.argument_stack.resize(vmstate.argument_stack.size() - argument_expressions.size());

	return ret;
}


void FunctionExpression::linkFunctions(Linker& linker, std::vector<ASTNode*>& stack)
{
	// We want to find a function that matches our argument expression types, and the function name

	// First, walk up tree, and see if such a target function has been given a name with a let.
	/*for(int i = (int)stack.size() - 1; i >= 0; --i)
	{
		{
			FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[i]);
			if(def != NULL)
			{
				for(unsigned int i=0; i<def->lets.size(); ++i)
					if(def->lets[i]->type() == this->name)
					{
						this->argument_index = i;
						this->argument_offset = (int)def->lets.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = LetVariable;
						this->parent_function = def;
						return;
					}

				for(unsigned int i=0; i<def->args.size(); ++i)
					if(def->args[i].name == this->name)
					{
						this->argument_index = i;
						this->argument_offset = (int)def->args.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = ArgumentVariable;
						this->parent_function = def;
						return;
					}

				if(this->argument_offset == -1)
					throw BaseException("No such function argument '" + this->name + "'");
			}
		}
*/

	vector<TypeRef> argtypes;
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		argtypes.push_back(this->argument_expressions[i]->type());


	FunctionSignature sig(this->function_name, argtypes);

	Linker::FuncMapType::iterator res = linker.functions.find(sig);
	if(res == linker.functions.end())
		throw BaseException("Failed to find function with signature " + sig.toString());
	
	this->target_function = (*res).second.getPointer();
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
	if(payload.linker)
		linkFunctions(*payload.linker, stack);

	stack.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->traverse(payload, stack);

	stack.pop_back();
}


void FunctionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionExpr";
	if(this->target_function)
		s << "; target: " << this->target_function->sig.toString();
	s << "\n";
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->print(depth + 1, s);
}


TypeRef FunctionExpression::type() const
{
	if(!target_function)
	{
		assert(0);
		throw BaseException("Tried to get type from an unlinked function expression.");
	}
	return this->target_function->type();
}


llvm::Value* FunctionExpression::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Lookup LLVM function, which should already be created and added to the module.
	llvm::Function* target_llvm_func = params.module->getFunction(
		this->target_function->sig.toString() //internalFuncName(call_target_sig)
		);
	assert(target_llvm_func);

	//------------------
	// Build args list
	std::vector<llvm::Value*> args;

	for(unsigned int i=0; i<argument_expressions.size(); ++i)
		args.push_back(argument_expressions[i]->emitLLVMCode(params));

	return params.builder->CreateCall(target_llvm_func, args.begin(), args.end());
}


//-----------------------------------------------------------------------------------


Variable::Variable(ASTNode* parent, const std::string& name_)
:	//ASTNode(parent),
	//referenced_var(NULL),
	name(name_),
	argument_offset(-1),
	argument_index(-1),
	parent_function(NULL),
	parent_anon_function(NULL)
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
						this->argument_offset = (int)def->lets.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = LetVariable;
						this->parent_function = def;
						return;
					}

				for(unsigned int i=0; i<def->args.size(); ++i)
					if(def->args[i].name == this->name)
					{
						this->argument_index = i;
						this->argument_offset = (int)def->args.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = ArgumentVariable;
						this->parent_function = def;
						return;
					}

				if(this->argument_offset == -1)
					throw BaseException("No such function argument '" + this->name + "'");
			}
		}

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
						this->argument_offset = (int)def->args.size() - i;
						this->referenced_var_type = def->args[i].type;
						this->vartype = ArgumentVariable;
						this->parent_anon_function = def;
						return;
					}

				if(this->argument_offset == -1)
					throw BaseException("No such function argument '" + this->name + "'");
			}
		}
	}
	throw BaseException("Variable::bindVariables(): No such function argument '" + this->name + "'");
}


void Variable::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	this->bindVariables(stack);
}


Value* Variable::exec(VMState& vmstate)
{
	assert(this->argument_offset >= 0);

	if(this->vartype == ArgumentVariable)
	{
		return vmstate.argument_stack[vmstate.argument_stack.size() - argument_offset];
	}
	else
	{
		return vmstate.let_stack[vmstate.let_stack.size() - argument_offset];
	}
}


TypeRef Variable::type() const
{
	assert(this->argument_offset >= 0);
	assert(referenced_var_type.nonNull());

	//if(!this->referenced_var)
	//	throw BaseException("referenced_var == NULL");
	//return this->referenced_var->type();
	return this->referenced_var_type;
}


inline static const std::string varType(Variable::VariableType t)
{
	return t == Variable::LetVariable ? "Let" : "Arg";
}


void Variable::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Variable, name=" << this->name << ", " + varType(this->vartype) + ", argument_offset=" << argument_offset << "\n";
}


static llvm::Value* getNthArg(llvm::Function *func, int n)
{
	llvm::Function::arg_iterator args = func->arg_begin();
	for(int i=0; i<n; ++i)
		args++;
	return args;
}


static bool shouldPassByValue(const Type& type)
{
	return true;
}


llvm::Value* Variable::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	// Emit a call to constructStringOnHeap(this->value);
	//return emitExternalLinkageCall(
	//	//true, // implicit void arg?
	//	"constructStringOnHeap", // target name
	//	params
	//	);

	if(vartype == LetVariable)
	{
		return this->parent_function->lets[this->argument_index]->emitLLVMCode(params);
	}
	else
	{
		assert(this->parent_function);

		if(shouldPassByValue(*this->type()))
		{
			return getNthArg(params.currently_building_func, this->argument_index);
		}
		else
		{
			return params.builder->CreateLoad(
				getNthArg(params.currently_building_func, this->argument_index),
				false, // true,// TEMP: volatile = true to pick up returned vector);
				"argument" // name
			);

				/*return params.builder->Insert(new llvm::LoadInst(
					getNthArg(params.func, actual_arg_index),
					"arg_" + toString(actual_arg_index),
					VOLATILE, //isVolatile
					16 // Align
					));*/
		}
	}
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
	return llvm::ConstantFP::get(*params.context, llvm::APFloat(this->value));
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
	return llvm::ConstantInt::get(*params.context, llvm::APInt(
		32, // num bits
		this->value, // value
		true // signed
	));
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
	return NULL;
}


//----------------------------------------------------------------------------------------------


Value* MapLiteral::exec(VMState& vmstate)
{
	std::map<Value*, Value*> m;
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


//-----------------------------------------------------------------------------------------------


Value* AdditionExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	if(this->type()->getType() == Type::FloatType)
	{
		return new FloatValue(static_cast<FloatValue*>(aval)->value + static_cast<FloatValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return new IntValue(static_cast<IntValue*>(aval)->value + static_cast<IntValue*>(bval)->value);
	}
	return NULL;
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
	return params.builder->CreateBinOp(
		llvm::Instruction::Add, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
}


//-------------------------------------------------------------------------------------------------


Value* SubtractionExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	if(this->type()->getType() == Type::FloatType)
	{
		return new FloatValue(static_cast<FloatValue*>(aval)->value - static_cast<FloatValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return new IntValue(static_cast<IntValue*>(aval)->value - static_cast<IntValue*>(bval)->value);
	}
	return NULL;
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
	return params.builder->CreateBinOp(
		llvm::Instruction::Sub, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
}


//-------------------------------------------------------------------------------------------------------


Value* MulExpression::exec(VMState& vmstate)
{
	Value* aval = a->exec(vmstate);
	Value* bval = b->exec(vmstate);

	if(this->type()->getType() == Type::FloatType)
	{
		return new FloatValue(static_cast<FloatValue*>(aval)->value * static_cast<FloatValue*>(bval)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		return new IntValue(static_cast<IntValue*>(aval)->value * static_cast<IntValue*>(bval)->value);
	}
	return NULL;
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
	return params.builder->CreateBinOp(
		llvm::Instruction::Mul, 
		a->emitLLVMCode(params), 
		b->emitLLVMCode(params)
	);
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


//---------------------------------------------------------------------------------


Value* AnonFunction::exec(VMState& vmstate)
{
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


} //end namespace Lang
