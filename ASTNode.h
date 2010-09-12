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
#include "utils/refcounted.h"
#include "utils/reference.h"
#include "Type.h"
#include "FunctionSignature.h"
#include "BaseException.h"
#if USE_LLVM
#include <llvm/Support/IRBuilder.h>
#endif
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
	enum Operation
	{
		LinkFunctions,
		BindVariables,
		TypeCheck,
		SubstituteType // for making concrete types out of generic types.
	};

	TraversalPayload(Operation e) : operation(e) {}

	Linker* linker;

	Operation operation;

	std::vector<TypeRef> type_mappings; // for substitute type operation.
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
		ArrayLiteralType,
		VectorLiteralType,
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
	virtual Reference<ASTNode> clone() = 0;
	
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
	virtual Reference<ASTNode> clone();

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
		FunctionArg(){}
		FunctionArg(TypeRef type_, const string& n) : type(type_), name(n) {}
		/*enum TypeKind
		{
			GENERIC_TYPE,
			CONCRETE_TYPE
		};

		TypeKind type_kind;*/
		TypeRef type;
		//int generic_type_param_index;
		string name;
	};

	FunctionDefinition(const std::string& name, const std::vector<FunctionArg>& args, 
		const vector<Reference<LetASTNode> >& lets,
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		BuiltInFunctionImpl* impl);
	
	~FunctionDefinition();

	TypeRef returnType() const;

	vector<FunctionArg> args;
	ASTNodeRef body;
	TypeRef declared_return_type;
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
	virtual Reference<ASTNode> clone();

	bool isGenericFunction() const; // true if it is parameterised by type.

	llvm::Function* buildLLVMFunction(
		llvm::Module* module//, 
		//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	) const;

	llvm::Function* built_llvm_function;
};

typedef Reference<FunctionDefinition> FunctionDefinitionRef;


/*
e.g.   f(a, 1)
*/
class FunctionExpression : public ASTNode
{
public:
	FunctionExpression(/*ASTNode* p*/) : /*ASTNode(p),*/ target_function(NULL),argument_index(-1) {}

	bool doesFunctionTypeMatch(TypeRef& type);

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionExpressionType; }
	virtual TypeRef type() const;

	virtual void linkFunctions(Linker& linker, std::vector<ASTNode*>& stack);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	FunctionDefinition* runtimeBind(VMState& vmstate);

	///////

	string function_name;
	vector<Reference<ASTNode> > argument_expressions;

	//Reference<ASTNode> target_function;
	//ASTNode* target_function;
	FunctionDefinition* target_function;
	int argument_index;
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	enum BindingType
	{
		Let,
		Arg,
		Bound
	};
	BindingType binding_type;

	TypeRef target_function_return_type;

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

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VariableASTNodeType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();


	FunctionDefinition* parent_function; // Function for which the variable is an argument of,
	// or for what it is a let of.
	//AnonFunction* parent_anon_function;
	//ASTNode* referenced_var;
	//TypeRef referenced_var_type; // Type of the variable.
	VariableType vartype; // let or arg.

	int argument_index; // index in parent function definition argument list.
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	string name; // variable name.
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(/*ASTNode* parent, */float v) : /*ASTNode(parent), */ value(v) {}

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FloatLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();

	float value;
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
	virtual Reference<ASTNode> clone();
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
	virtual Reference<ASTNode> clone();
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
	virtual Reference<ASTNode> clone();
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
	virtual Reference<ASTNode> clone();



	TypeRef maptype;
	std::vector<std::pair<ASTNodeRef, ASTNodeRef> > items;
};


class ArrayLiteral : public ASTNode
{
public:
	ArrayLiteral(const std::vector<ASTNodeRef>& elems);

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return ArrayLiteralType; }
	virtual TypeRef type() const;// { return array_type; }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();

private:
	//TypeRef array_type;
	std::vector<ASTNodeRef> elements;
};


class VectorLiteral : public ASTNode
{
public:
	VectorLiteral(const std::vector<ASTNodeRef>& elems);

	virtual Value* exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VectorLiteralType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();

private:
	std::vector<ASTNodeRef> elements;
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
	virtual Reference<ASTNode> clone();

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
	virtual Reference<ASTNode> clone();

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
	virtual Reference<ASTNode> clone();

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
	virtual Reference<ASTNode> clone();

	std::string variable_name;
	ASTNodeRef expr;
};


/*class AnonFunction : public ASTNode
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
};*/


} //end namespace Lang


#endif //__ASTNODE_H_666_
