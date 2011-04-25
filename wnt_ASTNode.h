/*=====================================================================
ASTNode.h`
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
#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_Frame.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#if USE_LLVM
#include <llvm/Support/IRBuilder.h>
#endif
namespace llvm { class Function; };
namespace llvm { class Value; };
namespace llvm { class Module; };
namespace llvm { class LLVMContext; };
namespace PlatformUtils { class CPUInfo; }

namespace Winter
{

class VMState;
class Linker;
class LetASTNode;
class AnonFunction;
class Value;
class BuiltInFunctionImpl;
class FunctionDefinition;
class Frame;
class LetBlock;
class ASTNode;


class CapturedVar
{
public:
	CapturedVar();

	enum CapturedVarType
	{
		Let,
		Arg
	};

	TypeRef type() const;


	CapturedVarType vartype;
	int index;
	int let_frame_offset;
	//TypeRef type;


	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;
};


class TraversalPayload
{
public:
	enum Operation
	{
		//LinkFunctions,
		BindVariables,
		TypeCoercion,
		TypeCheck,
		ConstantFolding,
		SubstituteType, // for making concrete types out of generic types.
		OperatorOverloadConversion // Converting '+' to op_add
	};

	TraversalPayload(Operation e, bool hidden_voidptr_arg_, void* env_) : 
		operation(e), tree_changed(false), hidden_voidptr_arg(hidden_voidptr_arg_), env(env_) {}

	Linker* linker;

	Operation operation;

	std::vector<TypeRef> type_mappings; // for substitute type operation.

	bool tree_changed;

	bool hidden_voidptr_arg;
	void* env;

	FrameRef top_lvl_frame;

	//bool capture_variables; // If true, variables and function expressions will capture variable and add to captured_vars
	//vector<CapturedVar> captured_vars; // For when parsing anon functions

	//vector<LetBlock*> let_block_stack;
	vector<FunctionDefinition*> func_def_stack;
};

class EmitLLVMCodeParams
{
public:
	FunctionDefinition* currently_building_func_def;
	const PlatformUtils::CPUInfo* cpu_info;
	bool hidden_voidptr_arg;
#if USE_LLVM
	llvm::IRBuilder<>* builder;
	llvm::Module* module;
	llvm::Function* currently_building_func;
	llvm::LLVMContext* context;
#endif
	vector<ASTNode*> node_stack;
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

	virtual ValueRef exec(VMState& vmstate) = 0;

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
		DivExpressionType,
		UnaryMinusExpressionType,
		LetType,
		ComparisonExpressionType,
		AnonFunctionType,
		LetBlockType
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

	// True iff the expression does not depend on any variables
	virtual bool isConstant() const = 0;

protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

	//ASTNode* getParent() { return parent; }
	//void setParent(ASTNode* p) { parent = p; }
private:
	//ASTNode* parent;
};

typedef Reference<ASTNode> ASTNodeRef;


class FunctionDefinition;
class LetBlock;


class BufferRoot : public ASTNode
{
public:
	BufferRoot()// : ASTNode(NULL) 
	{}
	vector<Reference<FunctionDefinition> > func_defs;

	virtual ValueRef exec(VMState& vmstate){ return ValueRef(); }
	virtual ASTNodeType nodeType() const { return BufferRootType; }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

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
		//const vector<Reference<LetASTNode> >& lets,
		const ASTNodeRef& body, 
		const TypeRef& declared_rettype, // May be null, if return type is to be inferred.
		BuiltInFunctionImpl* impl);
	
	~FunctionDefinition();

	TypeRef returnType() const;

	vector<FunctionArg> args;
	ASTNodeRef body;
	TypeRef declared_return_type;
	//TypeRef function_type;
	//vector<Reference<LetASTNode> > lets;

	FunctionSignature sig;
	BuiltInFunctionImpl* built_in_func_impl;
	ExternalFunctionRef external_function;

	bool use_captured_vars;
	vector<CapturedVar> captured_vars; // For when parsing anon functions


	virtual ValueRef invoke(VMState& vmstate);
	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionDefinitionType; }
	virtual TypeRef type() const;// { return function_type; }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	bool isGenericFunction() const; // true if it is parameterised by type.
	bool isExternalFunction() const { return external_function.nonNull(); }

	llvm::Function* buildLLVMFunction(
		llvm::Module* module,
		const PlatformUtils::CPUInfo& cpu_info,
		bool hidden_voidptr_arg
		//std::map<Lang::FunctionSignature, llvm::Function*>& external_functions
	);

	// llvm::Type* getClosureStructLLVMType(llvm::LLVMContext& context) const;
	TypeRef getCapturedVariablesStructType() const;

	// If the function is return by value, returns winter_index, else returns winter_index + 1
	// as the zeroth index will be the sret pointer.
	int getLLVMArgIndex(int winter_index);

	int getCapturedVarStructLLVMArgIndex();


	llvm::Type* closure_type;

	llvm::Function* built_llvm_function;
	void* jitted_function;

//	llvm::Value* getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index);

//	std::vector<llvm::Value*> let_exprs_llvm_value;
};

typedef Reference<FunctionDefinition> FunctionDefinitionRef;


/*
e.g.   f(a, 1)
*/
class FunctionExpression : public ASTNode
{
public:
	FunctionExpression();
	FunctionExpression(const std::string& func_name, const ASTNodeRef& arg0, const ASTNodeRef& arg1); // 2-arg function

	bool doesFunctionTypeMatch(TypeRef& type);

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FunctionExpressionType; }
	virtual TypeRef type() const;

	virtual void linkFunctions(Linker& linker, TraversalPayload& payload, std::vector<ASTNode*>& stack);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;
	FunctionDefinition* runtimeBind(VMState& vmstate, FunctionValue*& function_value_out);

	///////

	string function_name;
	vector<Reference<ASTNode> > argument_expressions;

	//Reference<ASTNode> target_function;
	//ASTNode* target_function;
	FunctionDefinition* target_function; // May be NULL
	//Reference<ExternalFunction> target_external_function; // May be NULL
	int bound_index;
	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	enum BindingType
	{
		Unbound,
		Let,
		Arg,
		BoundToGlobalDef
	};
	BindingType binding_type;

	//TypeRef target_function_return_type;
	bool use_captured_var;
	int captured_var_index;
};


class Variable : public ASTNode
{
public:
	// More accurately, type of binding
	enum VariableType
	{
		LetVariable,
		ArgumentVariable,
		BoundToGlobalDefVariable,
		CapturedVariable
	};

	Variable(const std::string& name);

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VariableASTNodeType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	void bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return false; }

	// or for what it is a let of.
	//AnonFunction* parent_anon_function;
	//ASTNode* referenced_var;
	//TypeRef referenced_var_type; // Type of the variable.
	VariableType vartype; // let or arg.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;

	int bound_index; // index in parent function definition argument list.
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	string name; // variable name.

	// bool use_captured_var;
	// int captured_var_index;
	int uncaptured_bound_index;
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(/*ASTNode* parent, */float v) : /*ASTNode(parent), */ value(v) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return FloatLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }

	float value;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(/*ASTNode* parent, */int v) : /*ASTNode(parent),*/ value(v) {}
	int value;

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return IntLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Int()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }
};


class StringLiteral : public ASTNode
{
public:
	StringLiteral(/*ASTNode* parent, */const std::string& v) : /*ASTNode(parent), */value(v) {}
	string value;

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return StringLiteralType; }
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }
};


class BoolLiteral : public ASTNode
{
public:
	BoolLiteral(/*ASTNode* parent, */bool v) : /*ASTNode(parent), */value(v) {}
	bool value;

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return BoolLiteralType; }
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }
};


class MapLiteral : public ASTNode
{
public:
	MapLiteral(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MapLiteralType; }
	virtual TypeRef type() const { return maptype; }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;



	TypeRef maptype;
	std::vector<std::pair<ASTNodeRef, ASTNodeRef> > items;
};


class ArrayLiteral : public ASTNode
{
public:
	ArrayLiteral(const std::vector<ASTNodeRef>& elems);

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return ArrayLiteralType; }
	virtual TypeRef type() const;// { return array_type; }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

private:
	//TypeRef array_type;
	std::vector<ASTNodeRef> elements;
};


class VectorLiteral : public ASTNode
{
public:
	VectorLiteral(const std::vector<ASTNodeRef>& elems);

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return VectorLiteralType; }
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

private:
	std::vector<ASTNodeRef> elements;
};


class AdditionExpression : public ASTNode
{
public:
	AdditionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return AdditionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class SubtractionExpression : public ASTNode
{
public:
	SubtractionExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return SubtractionExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class MulExpression : public ASTNode
{
public:
	MulExpression(/*ASTNode* parent*/) /*: ASTNode(parent)*/ {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MulExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class DivExpression : public ASTNode
{
public:
	DivExpression()  {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return DivExpressionType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class UnaryMinusExpression : public ASTNode
{
public:
	UnaryMinusExpression() {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return UnaryMinusExpressionType; }
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef expr;
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(/*ASTNode* parent, */const std::string var_name) : 
	  /*ASTNode(parent), */ variable_name(var_name) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return LetType; }
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	std::string variable_name;
	ASTNodeRef expr;
};


class ComparisonExpression : public ASTNode
{
public:
	ComparisonExpression(Reference<TokenBase>& token_, ASTNodeRef a_, ASTNodeRef b_) : 
	  token(token_), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return ComparisonExpressionType; }
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	Reference<TokenBase> token;
	ASTNodeRef a;
	ASTNodeRef b;
};


class LetBlock : public ASTNode
{
public:
	LetBlock(ASTNodeRef& e, vector<Reference<LetASTNode> > lets_) : expr(e), lets(lets_) {
		let_exprs_llvm_value = vector<llvm::Value*>(lets_.size(), NULL); }

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return LetBlockType; }
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	vector<Reference<LetASTNode> > lets;

	llvm::Value* getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index);

	std::vector<llvm::Value*> let_exprs_llvm_value;

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
