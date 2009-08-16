/*=====================================================================
ASTNode.cpp
-----------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "ASTNode.h"


#include "VMState.h"
#include "Value.h"
#include "Linker.h"


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

void BufferRoot::linkFunctions(Linker& linker)
{
	for(unsigned int i=0; i<func_defs.size(); ++i)
		func_defs[i]->linkFunctions(linker);
}


void BufferRoot::bindVariables(const std::vector<ASTNode*>& stack)
{
	//std::vector<ASTNode*> s;
	for(unsigned int i=0; i<func_defs.size(); ++i)
		func_defs[i]->bindVariables(stack);
}



void BufferRoot::print(int depth, std::ostream& s) const
{
	for(unsigned int i=0; i<func_defs.size(); ++i)
	{
		func_defs[i]->print(depth+1, s);
		s << "\n";
	}
}


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


void FunctionDefinition::exec(VMState& vmstate)
{
	// Evaluate let clauses, which will each push the result onto the let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->exec(vmstate);

	body->exec(vmstate);

	// Pop things off let stack
	for(unsigned int i=0; i<lets.size(); ++i)
		vmstate.let_stack.pop_back();

	// Pop resulting value to return register
	vmstate.return_register = vmstate.working_stack.back();
	vmstate.working_stack.pop_back();
}


void FunctionDefinition::linkFunctions(Linker& linker)
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
}


void FunctionDefinition::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "FunctionDef: " << this->sig.toString() << " " << this->return_type->toString() << "\n";
	for(unsigned int i=0; i<this->lets.size(); ++i)
		lets[i]->print(depth + 1, s);
	body->print(depth+1, s);
}


void FunctionExpression::exec(VMState& vmstate)
{
	assert(target_function);

	// Push arguments onto working stack
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->exec(vmstate);

	// Copy to argument stack
	const unsigned int oldargstacksize = (unsigned int)vmstate.argument_stack.size();
	vmstate.argument_stack.resize(oldargstacksize + this->argument_expressions.size());
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		vmstate.argument_stack[oldargstacksize + i] = vmstate.working_stack[vmstate.working_stack.size() - this->argument_expressions.size() + i];

	vmstate.working_stack.resize(vmstate.working_stack.size() - this->argument_expressions.size());

	// Execute target function
	target_function->exec(vmstate);

	// Remove arguments from stack
	vmstate.argument_stack.resize(vmstate.argument_stack.size() - argument_expressions.size());

	// Push returned value from return register onto working stack.
	vmstate.working_stack.push_back(vmstate.return_register);
}


void FunctionExpression::linkFunctions(Linker& linker)
{
	// We want to find a function that matches our argument expression types, and the function name

	vector<TypeRef> argtypes;
	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		argtypes.push_back(this->argument_expressions[i]->type());


	FunctionSignature sig(this->function_name, argtypes);

	Linker::FuncMapType::iterator res = linker.functions.find(sig);
	if(res == linker.functions.end())
		throw BaseException("Failed to find function with signature " + sig.toString());
	
	this->target_function = (*res).second.getPointer();
}


void FunctionExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);

	for(unsigned int i=0; i<this->argument_expressions.size(); ++i)
		this->argument_expressions[i]->bindVariables(s);
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


Variable::Variable(ASTNode* parent, const std::string& name_)
:	//ASTNode(parent),
	//referenced_var(NULL),
	name(name_),
	argument_offset(-1)
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
		FunctionDefinition* def = dynamic_cast<FunctionDefinition*>(stack[i]);
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
		}
	}
	throw BaseException("No such function argument '" + this->name + "'");
}


void Variable::exec(VMState& vmstate)
{
	assert(this->argument_offset >= 0);

	if(this->vartype == ArgumentVariable)
	{
		// Copy argument from argument stack to working stack
		vmstate.working_stack.push_back(
			vmstate.argument_stack[vmstate.argument_stack.size() - argument_offset]
		);
	}
	else
	{
		vmstate.working_stack.push_back(
			vmstate.let_stack[vmstate.let_stack.size() - argument_offset]
		);
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


void FloatLiteral::exec(VMState& vmstate)
{
	vmstate.working_stack.push_back(new FloatValue(value));
}


void FloatLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Float literal, value=" << this->value << "\n";
}


void IntLiteral::exec(VMState& vmstate)
{
	vmstate.working_stack.push_back(new IntValue(value));
}


void IntLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Int literal, value=" << this->value << "\n";
}


void BoolLiteral::exec(VMState& vmstate)
{
	vmstate.working_stack.push_back(new BoolValue(value));
}


void BoolLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Bool literal, value=" << this->value << "\n";
}


void MapLiteral::exec(VMState& vmstate)
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

	vmstate.working_stack.push_back(new MapValue(m));
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


void MapLiteral::linkFunctions(Linker& linker)
{
	for(unsigned int i=0; i<this->items.size(); ++i)
	{
		this->items[i].first->linkFunctions(linker);
		this->items[i].second->linkFunctions(linker);
	}
}


void StringLiteral::exec(VMState& vmstate)
{
	vmstate.working_stack.push_back(new StringValue(value));
}


void StringLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "String literal, value='" << this->value << "'\n";
}


void AdditionExpression::exec(VMState& vmstate)
{
	this->a->exec(vmstate);
	this->b->exec(vmstate);

	Value* a = vmstate.working_stack[vmstate.working_stack.size() - 2];
	Value* b = vmstate.working_stack[vmstate.working_stack.size() - 1];

	Value* res;
	if(this->type()->getType() == Type::FloatType)
	{
		res = new FloatValue(static_cast<FloatValue*>(a)->value + static_cast<FloatValue*>(b)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		res = new IntValue(static_cast<IntValue*>(a)->value + static_cast<IntValue*>(b)->value);
	}

	//delete a;
	//delete b;

	vmstate.working_stack.resize(vmstate.working_stack.size() - 1);
	vmstate.working_stack.back() = res;
}


void AdditionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Addition Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


void AdditionExpression::linkFunctions(Linker& linker)
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
}


void SubtractionExpression::exec(VMState& vmstate)
{
	this->a->exec(vmstate);
	this->b->exec(vmstate);

	Value* a = vmstate.working_stack[vmstate.working_stack.size() - 2];
	Value* b = vmstate.working_stack[vmstate.working_stack.size() - 1];

	Value* res;
	if(this->type()->getType() == Type::FloatType)
	{
		res = new FloatValue(static_cast<FloatValue*>(a)->value - static_cast<FloatValue*>(b)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		res = new IntValue(static_cast<IntValue*>(a)->value - static_cast<IntValue*>(b)->value);
	}

	//delete a;
	//delete b;

	vmstate.working_stack.resize(vmstate.working_stack.size() - 1);
	vmstate.working_stack.back() = res;
}


void SubtractionExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Subtraction Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


void SubtractionExpression::linkFunctions(Linker& linker)
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
}


void MulExpression::exec(VMState& vmstate)
{
	this->a->exec(vmstate);
	this->b->exec(vmstate);

	Value* a = vmstate.working_stack[vmstate.working_stack.size() - 2];
	Value* b = vmstate.working_stack[vmstate.working_stack.size() - 1];

	Value* res;
	if(this->type()->getType() == Type::FloatType)
	{
		res = new FloatValue(static_cast<FloatValue*>(a)->value * static_cast<FloatValue*>(b)->value);
	}
	else if(this->type()->getType() == Type::IntType)
	{
		res = new IntValue(static_cast<IntValue*>(a)->value * static_cast<IntValue*>(b)->value);
	}

	//delete a;
	//delete b;

	vmstate.working_stack.resize(vmstate.working_stack.size() - 1);
	vmstate.working_stack.back() = res;
}


void MulExpression::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	this->a->bindVariables(s);
	this->b->bindVariables(s);
}


void MulExpression::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Mul Expression\n";
	this->a->print(depth+1, s);
	this->b->print(depth+1, s);
}


void MulExpression::linkFunctions(Linker& linker)
{
	a->linkFunctions(linker);
	b->linkFunctions(linker);
}


void LetASTNode::exec(VMState& vmstate)
{
	// Evaluate expression, and push result onto let stack

	// Eval expression
	this->expr->exec(vmstate);

	vmstate.let_stack.push_back(vmstate.working_stack.back()); // Copy value from working stack to let stack
	vmstate.working_stack.pop_back(); // Pop off working stack
}


void LetASTNode::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Let, var_name = '" + this->variable_name + "'\n";
	this->expr->print(depth+1, s);
}


void LetASTNode::linkFunctions(Linker& linker)
{
	expr->linkFunctions(linker);
}


void LetASTNode::bindVariables(const std::vector<ASTNode*>& stack)
{
	std::vector<ASTNode*> s(stack);
	s.push_back(this);
	expr->bindVariables(s);
}


} //end namespace Lang
