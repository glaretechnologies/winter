/*=====================================================================
ASTNode.h
---------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ASTNODE_H_666_
#define __ASTNODE_H_666_


#include <string>
#include <vector>
using std::string;
using std::vector;
#include "../indigo/trunk/utils/refcounted.h"
#include "../indigo/trunk/utils/reference.h"
#include "Type.h"
#include "FunctionSignature.h"
#include "BaseException.h"


namespace Winter
{

class VMState;
class Linker;
class LetASTNode;


/*=====================================================================
ASTNode
-------
Abstract syntax tree node.
=====================================================================*/
class ASTNode : public RefCounted
{
public:
	ASTNode(/*ASTNode* p*/) /*: parent(p)*/ {}
	virtual ~ASTNode() {}

	virtual void exec(VMState& vmstate) = 0;

	enum ASTNodeType
	{
		BufferRootType,
		FunctionDefinitionType,
		FunctionExpressionType,
		VariableASTNodeType,
		FloatLiteralType,
		IntLiteralType,
		BoolLiteralType,
		StringLiteralType,
		MapLiteralType,
		AdditionExpressionType,
		SubtractionExpressionType,
		MulExpressionType,
		LetType
	};

	virtual ASTNodeType nodeType() const = 0;
	virtual TypeRef type() const = 0;

	//virtual linearise(std::vector<Reference<ASTNode> >& nodes_out) = 0;

	virtual void linkFunctions(Linker& linker){}
	virtual void bindVariables(const std::vector<ASTNode*>& stack){}

	virtual void print(int depth, std::ostream& s) const = 0;

	//ASTNode* getParent() { return parent; }
	//void setParent(ASTNode* p) { parent = p; }
private:
	//ASTNode* parent;
};

typedef Reference<ASTNode> ASTNodeRef;


class FunctionDefinition;


class BufferRoot : public ASTNode
{
public:
	BufferRoot()// : ASTNode(NULL) 
	{}
	string function_name;
	vector<Reference<FunctionDefinition> > func_defs;

	virtual void exec(VMState& vmstate){}
	virtual ASTNodeType nodeType() const { return BufferRootType; }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
};


/*class TypeASTNode : public ASTNode
{
public:
	TypeRef type;
};*/

/*
e.g.	def f(double a, double b) -> double : a + g(b)
*/
class FunctionDefinition : public ASTNode
{
public:
	class FunctionArg
	{
	public:
		TypeRef type;
		string name;
	};

	FunctionDefinition(/*ASTNode* parent, */const std::string& name, const std::vector<FunctionArg>& args, 
		//ASTNodeRef& body, 
		TypeRef& rettype);

	//string function_name;

	vector<FunctionArg> args;
	Reference<ASTNode> body;
	TypeRef return_type;
	vector<Reference<LetASTNode> > lets;

	FunctionSignature sig;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionDefinitionType; }
	virtual TypeRef type() const { return return_type; }

	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;

};


/*
e.g.   f(a, 1)
*/
class FunctionExpression : public ASTNode
{
public:
	FunctionExpression(/*ASTNode* p*/) : /*ASTNode(p),*/ target_function(NULL) {}
	string function_name;
	vector<Reference<ASTNode> > argument_expressions;

	//Reference<ASTNode> target_function;
	//ASTNode* target_function;
	FunctionDefinition* target_function;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionExpressionType; }
	virtual TypeRef type() const;

	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
};


class Variable : public ASTNode
{
public:
	enum VariableType
	{
		LetVariable,
		ArgumentVariable
	};

	Variable(ASTNode* parent, const std::string& name);
	string name;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VariableASTNodeType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	void bindVariables(const std::vector<ASTNode*>& stack);

	//FunctionDefinition* parent_function; // Function for which the variable is an argument of.
	//ASTNode* referenced_var;
	TypeRef referenced_var_type;
	VariableType vartype;

	int argument_offset; // Currently, a variable must be an argument to the enclosing function
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(/*ASTNode* parent, */float v) : /*ASTNode(parent), */ value(v) {}
	float value;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FloatLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(/*ASTNode* parent, */int v) : /*ASTNode(parent),*/ value(v) {}
	int value;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return IntLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Int()); }
	virtual void print(int depth, std::ostream& s) const;
};


class StringLiteral : public ASTNode
{
public:
	StringLiteral(/*ASTNode* parent, */const std::string& v) : /*ASTNode(parent), */value(v) {}
	string value;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return StringLiteralType; }
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
};


class BoolLiteral : public ASTNode
{
public:
	BoolLiteral(/*ASTNode* parent, */bool v) : /*ASTNode(parent), */value(v) {}
	bool value;

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return BoolLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
};


class MapLiteral : public ASTNode
{
public:
	MapLiteral(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MapLiteralType; }
	virtual TypeRef type() const { return maptype; }
	virtual void print(int depth, std::ostream& s) const;
	virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);


	TypeRef maptype;
	std::vector<std::pair<ASTNodeRef, ASTNodeRef> > items;
};


class AdditionExpression : public ASTNode
{
public:
	AdditionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return AdditionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);

	ASTNodeRef a;
	ASTNodeRef b;
};


class SubtractionExpression : public ASTNode
{
public:
	SubtractionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return SubtractionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);

	ASTNodeRef a;
	ASTNodeRef b;
};


class MulExpression : public ASTNode
{
public:
	MulExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MulExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);

	ASTNodeRef a;
	ASTNodeRef b;
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(/*ASTNode* parent, */const std::string var_name) : 
	  /*ASTNode(parent), */ variable_name(var_name) {}

	virtual void exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return LetType; }
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void linkFunctions(Linker& linker);
	virtual void bindVariables(const std::vector<ASTNode*>& stack);

	std::string variable_name;
	ASTNodeRef expr;
};


} //end namespace Lang


#endif //__ASTNODE_H_666_
