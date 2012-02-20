/*=====================================================================
ASTNode.h`
---------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#pragma once


#include <string>
#include <vector>
using std::string;
using std::vector;
#include <utils/refcounted.h>
#include <utils/reference.h>
#include <utils/platform.h>
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
namespace llvm { class TargetData; };
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
class SourceBuffer;


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


const std::string indent(VMState& vmstate);
void printMargin(int depth, std::ostream& s);
bool isIntExactlyRepresentableAsFloat(int x);
void checkFoldExpression(Reference<ASTNode>& e, TraversalPayload& payload);
void convertOverloadedOperators(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack);
const std::string errorContext(const ASTNode& n);
const std::string errorContext(const ASTNode& n, TraversalPayload& payload);

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
	const llvm::TargetData* target_data;
#endif
	vector<ASTNode*> node_stack;
	vector<LetBlock*> let_block_stack;
};


class SrcLocation
{
public:
	SrcLocation(uint32 char_index_, /*uint32 line_, uint32 column_, */const SourceBuffer* buf) : 
	  char_index(char_index_), /*line(line_), column(column_),*/ source_buffer(buf) {}
	//uint32 line;
	//uint32 column;

	static const SrcLocation invalidLocation() { return SrcLocation(4000000000u, NULL); }

	uint32 char_index;
	//const std::string* text_buffer;
	const SourceBuffer* source_buffer;
};


/*=====================================================================
ASTNode
-------
Abstract syntax tree node.
=====================================================================*/
class ASTNode : public RefCounted
{
public:
	ASTNode(const SrcLocation& loc_) : location(loc_) {}
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
		BinaryBooleanType,
		UnaryMinusExpressionType,
		LetType,
		ComparisonExpressionType,
		AnonFunctionType,
		LetBlockType
	};

	virtual ASTNodeType nodeType() const = 0;
	virtual TypeRef type() const = 0;

	//virtual linearise(std::vector<Reference<ASTNode> >& nodes_out) = 0;

	//virtual void linkFunctions(Linker& linker){}
	//virtual void bindVariables(const std::vector<ASTNode*>& stack){}
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack) {}
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
	virtual Reference<ASTNode> clone() = 0;
	
	virtual void print(int depth, std::ostream& s) const = 0;

	// True iff the expression does not depend on any variables
	virtual bool isConstant() const = 0;

	const SrcLocation& srcLocation() const { return location; }
protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

	//ASTNode* getParent() { return parent; }
	//void setParent(ASTNode* p) { parent = p; }
private:
	//ASTNode* parent;
	SrcLocation location;
};

typedef Reference<ASTNode> ASTNodeRef;


bool shouldFoldExpression(ASTNodeRef& e, TraversalPayload& payload);
ASTNodeRef foldExpression(ASTNodeRef& e, TraversalPayload& payload);
class FunctionDefinition;
class LetBlock;


class BufferRoot : public ASTNode
{
public:
	BufferRoot(const SrcLocation& loc) : ASTNode(loc) 
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


class Variable : public ASTNode
{
public:
	// More accurately, type of binding
	enum VariableType
	{
		UnboundVariable,
		LetVariable,
		ArgumentVariable,
		BoundToGlobalDefVariable,
		CapturedVariable
	};

	Variable(const std::string& name, const SrcLocation& loc);

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

	// Offset of zero means use the latest/deepest set of let values.  Offset 1 means the next oldest, etc.
	int let_frame_offset;

	// bool use_captured_var;
	// int captured_var_index;

	int uncaptured_bound_index;
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(float v, const SrcLocation& loc) : ASTNode(loc), value(v) {}

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
	IntLiteral(int v, const SrcLocation& loc) : ASTNode(loc), value(v) {}
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
	StringLiteral(const std::string& v, const SrcLocation& loc) : ASTNode(loc), value(v) {}
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
	BoolLiteral(bool v, const SrcLocation& loc) : ASTNode(loc), value(v) {}
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
	MapLiteral(const SrcLocation& loc) : ASTNode(loc) {}

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
	ArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc);

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
	VectorLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc);

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
	AdditionExpression(const SrcLocation& loc) : ASTNode(loc) {}

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
	SubtractionExpression(const SrcLocation& loc) : ASTNode(loc) {}

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
	MulExpression(const SrcLocation& loc) : ASTNode(loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return MulExpressionType; }
	virtual TypeRef type() const;
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
	DivExpression(const SrcLocation& loc) : ASTNode(loc) {}

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


class BinaryBooleanExpr : public ASTNode
{
public:
	enum Type
	{
		AND,
		OR
	};

	BinaryBooleanExpr(Type t, const ASTNodeRef& a, const ASTNodeRef& b, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return BinaryBooleanType; }
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	Type t;
	ASTNodeRef a;
	ASTNodeRef b;
};



class UnaryMinusExpression : public ASTNode
{
public:
	UnaryMinusExpression(const SrcLocation& loc) : ASTNode(loc) {}

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
	LetASTNode(const std::string& var_name, const SrcLocation& loc) : 
	  ASTNode(loc), variable_name(var_name) {}

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
	ComparisonExpression(Reference<TokenBase>& token_, ASTNodeRef a_, ASTNodeRef b_, const SrcLocation& loc) : 
	  ASTNode(loc), token(token_), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual ASTNodeType nodeType() const { return ComparisonExpressionType; }
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	const std::string getOverloadedFuncName() const; // returns e.g. op_lt, op_gt   etc..

	Reference<TokenBase> token;
	ASTNodeRef a;
	ASTNodeRef b;
};


class LetBlock : public ASTNode
{
public:
	LetBlock(ASTNodeRef& e, vector<Reference<LetASTNode> > lets_, const SrcLocation& loc) : 
	  ASTNode(loc), expr(e), lets(lets_) 
	{
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

