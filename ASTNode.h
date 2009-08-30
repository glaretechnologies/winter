/*=====================================================================
ASTNode.h
---------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#ifndef __ASTNODE_H_666_
#define __ASTNODE_H_666_


#include <string>
#include <vector>
using std::string;
using std::vector;
#include "../../indigosvn/trunk/utils/refcounted.h"
#include "../../indigosvn/trunk/utils/reference.h"
#include "Type.h"
#include "FunctionSignature.h"
#include "BaseException.h"
//#include <llvm/ExecutionEngine/JIT.h>
#if USE_LLVM
#include <llvm/Support/IRBuilder.h>
#endif
//#include <llvm/Intrinsics.h>
namespace llvm { class Function; };
namespace llvm { class Value; };
namespace llvm { class Module; };
namespace llvm { class LLVMContext; };


namespace Winter
{

class VMState;
class Linker;
class LetASTNode;
class AnonFunction;
class Value;
class BuiltInFunctionImpl;


class TraversalPayload
{
public:
	Linker* linker;

	enum Operation
	{
		LinkFunctions,
		BindVariables,
		TypeCheck
	};
	Operation operation;
};

class EmitLLVMCodeParams
{
public:
#if USE_LLVM
	llvm::IRBuilder<>* builder;
	llvm::Module* module;
	llvm::Function* currently_building_func;
	llvm::LLVMContext* context;
#endif
};


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

	virtual Value* exec(VMState& vmstate) = 0;

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
		LetType,
		AnonFunctionType
	};

	virtual ASTNodeType nodeType() const = 0;
	virtual TypeRef type() const = 0;

	//virtual linearise(std::vector<Reference<ASTNode> >& nodes_out) = 0;

	virtual void linkFunctions(Linker& linker){}
	virtual void bindVariables(const std::vector<ASTNode*>& stack){}
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack) {}
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
	
	virtual void print(int depth, std::ostream& s) const = 0;

protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

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
	vector<Reference<FunctionDefinition> > func_defs;

	virtual Value* exec(VMState& vmstate){ return NULL; }
	virtual ASTNodeType nodeType() const { return BufferRootType; }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
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

	FunctionDefinition(const std::string& name, const std::vector<FunctionArg>& args, 
		const vector<Reference<LetASTNode> >& lets,
		const ASTNodeRef& body, 
		const TypeRef& rettype,
		BuiltInFunctionImpl* impl);
	
	~FunctionDefinition();

	vector<FunctionArg> args;
	ASTNodeRef body;
	TypeRef return_type;
	TypeRef function_type;
	vector<Reference<LetASTNode> > lets;

	FunctionSignature sig;
	BuiltInFunctionImpl* built_in_func_impl;

	virtual Value* invoke(VMState& vmstate);
	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionDefinitionType; }
	virtual TypeRef type() const { return function_type; }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	llvm::Function* buildLLVMFunction(
		llvm::Module* module//, 
		//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	) const;
};


/*
e.g.   f(a, 1)
*/
class FunctionExpression : public ASTNode
{
public:
	FunctionExpression(/*ASTNode* p*/) : /*ASTNode(p),*/ target_function(NULL),argument_index(-1),argument_offset(-1) {}
	string function_name;
	vector<Reference<ASTNode> > argument_expressions;

	//Reference<ASTNode> target_function;
	//ASTNode* target_function;
	FunctionDefinition* target_function;
	int argument_index;
	int argument_offset; // Currently, a variable must be an argument to the enclosing function
	enum BindingType
	{
		Let,
		Arg,
		Bound
	};
	BindingType binding_type;

	TypeRef target_function_return_type;

	FunctionDefinition* runtimeBind(VMState& vmstate);

	bool doesFunctionTypeMatch(TypeRef& type);

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionExpressionType; }
	virtual TypeRef type() const;

	virtual void linkFunctions(Linker& linker, std::vector<ASTNode*>& stack);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class Variable : public ASTNode
{
public:
	enum VariableType
	{
		LetVariable,
		ArgumentVariable
	};

	Variable(const std::string& name);
	string name;

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VariableASTNodeType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;


	FunctionDefinition* parent_function; // Function for which the variable is an argument of.
	AnonFunction* parent_anon_function;
	//ASTNode* referenced_var;
	TypeRef referenced_var_type;
	VariableType vartype;

	int argument_index;
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(/*ASTNode* parent, */float v) : /*ASTNode(parent), */ value(v) {}
	float value;

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FloatLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(/*ASTNode* parent, */int v) : /*ASTNode(parent),*/ value(v) {}
	int value;

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return IntLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Int()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class StringLiteral : public ASTNode
{
public:
	StringLiteral(/*ASTNode* parent, */const std::string& v) : /*ASTNode(parent), */value(v) {}
	string value;

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return StringLiteralType; }
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class BoolLiteral : public ASTNode
{
public:
	BoolLiteral(/*ASTNode* parent, */bool v) : /*ASTNode(parent), */value(v) {}
	bool value;

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return BoolLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class MapLiteral : public ASTNode
{
public:
	MapLiteral(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MapLiteralType; }
	virtual TypeRef type() const { return maptype; }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;



	TypeRef maptype;
	std::vector<std::pair<ASTNodeRef, ASTNodeRef> > items;
};


class AdditionExpression : public ASTNode
{
public:
	AdditionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return AdditionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class SubtractionExpression : public ASTNode
{
public:
	SubtractionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return SubtractionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class MulExpression : public ASTNode
{
public:
	MulExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MulExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(/*ASTNode* parent, */const std::string var_name) : 
	  /*ASTNode(parent), */ variable_name(var_name) {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return LetType; }
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	std::string variable_name;
	ASTNodeRef expr;
};


class AnonFunction : public ASTNode
{
public:
	AnonFunction() {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return AnonFunctionType; }
	virtual TypeRef type() const { return thetype; }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	vector<FunctionDefinition::FunctionArg> args;
	ASTNodeRef body;
	TypeRef thetype;
};


} //end namespace Lang


#endif //__ASTNODE_H_666_
