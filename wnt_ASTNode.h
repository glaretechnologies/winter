/*=====================================================================
ASTNode.h
---------
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
Code By Nicholas Chapman.

Copyright 2009 Nicholas Chapman
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_Frame.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include <utils/platform.h>
#pragma warning(push, 0) // Disable warnings
#include <llvm/IR/IRBuilder.h>
#pragma warning(pop) // Re-enable warnings
#include <string>
#include <vector>
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
class ComparisonExpression;


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
		OperatorOverloadConversion, // Converting '+' to op_add
		GetCleanupNodes,
		CheckInDomain, // Check that calls to elem() etc.. are in bounds.
		AddOpaqueEnvArg,
		InlineFunctionCalls, // inline function calls
		SubstituteVariables // Used in InlineFunctionCalls passes: replace all variables in the function body with the argument values.
	};

	TraversalPayload(Operation e) : 
		operation(e), tree_changed(false) {}

	Linker* linker;

	Operation operation;

	std::vector<TypeRef> type_mappings; // for substitute type operation.

	bool tree_changed;

	FrameRef top_lvl_frame;

	//bool capture_variables; // If true, variables and function expressions will capture variable and add to captured_vars
	//vector<CapturedVar> captured_vars; // For when parsing anon functions

	//vector<LetBlock*> let_block_stack;
	std::vector<FunctionDefinition*> func_def_stack;

	bool all_variables_bound; // Are all variables in a given function body bound?  Used in BindVariables pass.

	std::vector<Reference<ASTNode> > variable_substitutes; // Used in SubstituteVariables pass
};


const std::string indent(VMState& vmstate);
void printMargin(int depth, std::ostream& s);
bool isIntExactlyRepresentableAsFloat(int x);
void checkFoldExpression(Reference<ASTNode>& e, TraversalPayload& payload);
void checkSubstituteVariable(Reference<ASTNode>& e, TraversalPayload& payload);
void checkInlineExpression(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack);
void convertOverloadedOperators(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack);
const std::string errorContext(const ASTNode& n);
const std::string errorContext(const ASTNode& n, TraversalPayload& payload);


class CleanUpInfo
{
public:
	CleanUpInfo(){}
	CleanUpInfo(const ASTNode* node_, llvm::Value* value_) : node(node_), value(value_) {}
	const ASTNode* node;
	llvm::Value* value;
};


class CommonFunctions
{
public:
	FunctionDefinition* allocateStringFunc;
	FunctionDefinition* freeStringFunc;

	llvm::Function* decrStringRefCountLLVMFunc;
	llvm::Function* incrStringRefCountLLVMFunc;
};


class EmitLLVMCodeParams
{
public:
	FunctionDefinition* currently_building_func_def;
	const PlatformUtils::CPUInfo* cpu_info;
	//bool hidden_voidptr_arg;
#if USE_LLVM
	llvm::IRBuilder<>* builder;
	llvm::Module* module;
	llvm::Function* currently_building_func;
	llvm::LLVMContext* context;
	const llvm::DataLayout/*TargetData*/* target_data;
#endif
	std::vector<ASTNode*> node_stack;
	std::vector<LetBlock*> let_block_stack;

	std::map<const LetBlock*, std::vector<llvm::Value*> > let_block_let_values;

	std::vector<CleanUpInfo> cleanup_values; // A list of ASTNodes that need to emit cleanup code (Ref decrement code) at the end of the function

	CommonFunctions common_functions;
};


class SrcLocation
{
public:
	SrcLocation(uint32 char_index_, /*uint32 line_, uint32 column_, */const SourceBuffer* buf) : 
	  char_index(char_index_), /*line(line_), column(column_),*/ source_buffer(buf) {}
	//uint32 line;
	//uint32 column;

	static const SrcLocation invalidLocation() { return SrcLocation(4000000000u, NULL); }

	bool isValid() const { return char_index != 4000000000u; }

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
		CharLiteralType,
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
		LetBlockType,
		ArraySubscriptType
	};

	ASTNode(ASTNodeType node_type_, const SrcLocation& loc_) : node_type(node_type_), location(loc_) {}
	virtual ~ASTNode() {}

	virtual ValueRef exec(VMState& vmstate) = 0;


	inline ASTNodeType nodeType() const { return node_type; }

	virtual TypeRef type() const = 0;

	//virtual linearise(std::vector<Reference<ASTNode> >& nodes_out) = 0;

	//virtual void linkFunctions(Linker& linker){}
	//virtual void bindVariables(const std::vector<ASTNode*>& stack){}
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack) {}

	// Emit LLVM code to compute the value for this AST node.
	// If the value type is pass-by-pointer, and return_space_pointer is non-null, then code MAY be emitted
	// to store the value at the memory pointed to by return_space_pointer.
	// However, since the value could be stored in a constant global, return_space_pointer doesn't have to be used.
	// Returns a pointer to the mem location where the value is stored.
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* return_space_pointer = NULL) const = 0;

	// Emit cleanup (ref count decrement and delete) code at the end of the function
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;

	// For the global const array optimisation: Return the AST node as a LLVM value directly.
	virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;

	virtual Reference<ASTNode> clone() = 0;
	
	virtual void print(int depth, std::ostream& s) const = 0;

	virtual std::string sourceString() const = 0;

	// True iff the expression does not depend on any variables
	virtual bool isConstant() const = 0;

	// For constant folding.
	// We can't evaluate an expression for constant folding unless we know the expression is defined.
	// For example, elem(a, i) may not be proven defined yet.
	virtual bool provenDefined() const { return true; }


	const SrcLocation& srcLocation() const { return location; }
protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

	//ASTNode* getParent() { return parent; }
	//void setParent(ASTNode* p) { parent = p; }
private:
	//ASTNode* parent;
	ASTNodeType node_type;
	SrcLocation location;
};

typedef Reference<ASTNode> ASTNodeRef;


bool shouldFoldExpression(ASTNodeRef& e, TraversalPayload& payload);
ASTNodeRef foldExpression(ASTNodeRef& e, TraversalPayload& payload);
//void updateIndexBounds(TraversalPayload& payload, const ComparisonExpression& comp_expr, const ASTNodeRef& index, int& i_lower, int& i_upper);
bool expressionsHaveSameValue(const ASTNodeRef& a, const ASTNodeRef& b);
class FunctionDefinition;
class LetBlock;


class BufferRoot : public ASTNode
{
public:
	BufferRoot(const SrcLocation& loc) : ASTNode(BufferRootType, loc) 
	{}
	vector<Reference<FunctionDefinition> > func_defs;

	virtual ValueRef exec(VMState& vmstate){ return ValueRef(); }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
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
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	void bindVariables(TraversalPayload& payload, const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	// or for what it is a let of.
	//AnonFunction* parent_anon_function;
	//ASTNode* referenced_var;
	//TypeRef referenced_var_type; // Type of the variable.
	VariableType vartype; // let or arg.

	FunctionDefinition* bound_function; // Function for which the variable is an argument of,
	LetBlock* bound_let_block;

	int bound_index; // index in parent function definition argument list.
	//int argument_offset; // Currently, a variable must be an argument to the enclosing function
	std::string name; // variable name.

	// Offset of zero means use the latest/deepest set of let values.  Offset 1 means the next oldest, etc.
	int let_frame_offset;

	// bool use_captured_var;
	// int captured_var_index;

	int uncaptured_bound_index;
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(float v, const SrcLocation& loc) : ASTNode(FloatLiteralType, loc), value(v) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }

	float value;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(int v, const SrcLocation& loc) : ASTNode(IntLiteralType, loc), value(v) {}
	int value;

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Int()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }
};


class StringLiteral : public ASTNode
{
public:
	StringLiteral(const std::string& v, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }

	std::string value;
};


class CharLiteral : public ASTNode
{
public:
	CharLiteral(const std::string& v, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }

	std::string value;
};


class BoolLiteral : public ASTNode
{
public:
	BoolLiteral(bool v, const SrcLocation& loc) : ASTNode(BoolLiteralType, loc), value(v) {}
	bool value;

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const { return true; }
};


class MapLiteral : public ASTNode
{
public:
	MapLiteral(const SrcLocation& loc) : ASTNode(MapLiteralType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return maptype; }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
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
	virtual TypeRef type() const;// { return array_type; }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

private:
	bool areAllElementsConstant() const;
	//TypeRef array_type;
	std::vector<ASTNodeRef> elements;
};


class VectorLiteral : public ASTNode
{
public:
	VectorLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix, int int_suffix);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	const std::vector<ASTNodeRef>& getElements() const { return elements; }
private:
	bool areAllElementsConstant() const;
	std::vector<ASTNodeRef> elements;
	bool has_int_suffix;
	int int_suffix;
};


class AdditionExpression : public ASTNode
{
public:
	AdditionExpression(const SrcLocation& loc) : ASTNode(AdditionExpressionType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class SubtractionExpression : public ASTNode
{
public:
	SubtractionExpression(const SrcLocation& loc) : ASTNode(SubtractionExpressionType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class MulExpression : public ASTNode
{
public:
	MulExpression(const SrcLocation& loc) : ASTNode(MulExpressionType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef a;
	ASTNodeRef b;
};


class DivExpression : public ASTNode
{
public:
	DivExpression(const SrcLocation& loc) : ASTNode(DivExpressionType, loc), proven_defined(false) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;
	virtual bool provenDefined() const;

private:
	void checkNoOverflow(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void checkNoZeroDivide(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:
	ASTNodeRef a;
	ASTNodeRef b;
	bool proven_defined;
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
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	Type t;
	ASTNodeRef a;
	ASTNodeRef b;
};



class UnaryMinusExpression : public ASTNode
{
public:
	UnaryMinusExpression(const SrcLocation& loc) : ASTNode(UnaryMinusExpressionType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	ASTNodeRef expr;
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(const std::string& var_name, const SrcLocation& loc) : 
	  ASTNode(LetType, loc), variable_name(var_name) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	std::string variable_name;
	ASTNodeRef expr;
};


class ComparisonExpression : public ASTNode
{
public:
	ComparisonExpression(const Reference<TokenBase>& token_, const ASTNodeRef a_, const ASTNodeRef b_, const SrcLocation& loc) : 
	  ASTNode(ComparisonExpressionType, loc), token(token_), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
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
	LetBlock(const ASTNodeRef& e, const vector<Reference<LetASTNode> > lets_, const SrcLocation& loc) : 
	  ASTNode(LetBlockType, loc), expr(e), lets(lets_) 
	{
		//let_exprs_llvm_value = vector<llvm::Value*>(lets_.size(), NULL); 
	}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;

	vector<Reference<LetASTNode> > lets;

	//llvm::Value* getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index, llvm::Value* ret_space_ptr);

	//std::vector<llvm::Value*> let_exprs_llvm_value;

	ASTNodeRef expr;
};


class ArraySubscript : public ASTNode
{
public:
	ArraySubscript(const ASTNodeRef& subscript_expr_, const SrcLocation& loc) : 
	  ASTNode(ArraySubscriptType, loc), subscript_expr(subscript_expr_)
	{
	}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return subscript_expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone();
	virtual bool isConstant() const;
	

	ASTNodeRef subscript_expr;
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

