/*=====================================================================
ASTNode.h
---------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
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
#include <utils/Platform.h>
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include <llvm/IR/IRBuilder.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <string>
#include <vector>
#include <set>
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
class TraversalPayload;
struct ProgramStats;


typedef std::map<ASTNode*, ASTNode*> CloneMapType;


class ASTNodeVisitor : public RefCounted
{
public:
	virtual ~ASTNodeVisitor(){}

	virtual void visit(ASTNode& node, TraversalPayload& payload) = 0;
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
		ComputeCanConstantFold,
		ConstantFolding,
		//SubstituteType, // for making concrete types out of generic types.
		OperatorOverloadConversion, // Converting '+' to op_add
		//GetCleanupNodes,
		CheckInDomain, // Check that calls to elem() etc.. are in bounds.
		InlineFunctionCalls, // inline function calls
		SubstituteVariables, // Used in InlineFunctionCalls passes: replace all variables in the function body with the argument values.
		CustomVisit, // Calls supplied ASTNodeVisitor on each node visited.
		UpdateUpRefs,
		DeadFunctionElimination,
		DeadCodeElimination_ComputeAlive,
		DeadCodeElimination_RemoveDead
	};

	TraversalPayload(Operation e) : 
		operation(e), tree_changed(false), current_named_constant(NULL), check_bindings(false) {}

	Linker* linker;

	Operation operation;

	std::vector<TypeRef> type_mappings; // for substitute type operation.

	bool tree_changed;

	//FrameRef top_lvl_frame;

	//bool capture_variables; // If true, variables and function expressions will capture variable and add to captured_vars
	//vector<CapturedVar> captured_vars; // For when parsing anon functions

	//vector<LetBlock*> let_block_stack;
	std::vector<FunctionDefinition*> func_def_stack;

	//std::vector<NamedConstant*> named_constant_stack;
	NamedConstant* current_named_constant;

	//bool all_variables_bound; // Are all variables in a given function body bound?  Used in BindVariables pass.

	// Used in SubstituteVariables pass:
	FunctionDefinition* func_args_to_sub; // This is the function whose body is getting inlined into the call site.
	std::vector<Reference<ASTNode> > variable_substitutes; // Used in SubstituteVariables pass

	Reference<ASTNodeVisitor> custom_visitor;

	bool check_bindings; // If this is true, this is the final binding pass.  Any unbound functions or variables should throw an exception.

	// Types that are captured by a function closure (lambda expression).
	std::set<TypeRef> captured_types;

	// Used in UpdateUpRefs:
	CloneMapType clone_map;

	// Used in DeadFunctionElimination and DeadCodeElimination:
	std::set<ASTNode*> reachable_nodes;
	std::vector<ASTNode*> nodes_to_process;
	std::set<ASTNode*> processed_nodes;
};


void printMargin(int depth, std::ostream& s);
bool isIntExactlyRepresentableAsFloat(int64 x);
bool checkFoldExpression(Reference<ASTNode>& e, TraversalPayload& payload); // Returns true if folding took place or e is already a literal.
void checkSubstituteVariable(Reference<ASTNode>& e, TraversalPayload& payload);
void checkInlineExpression(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack);
void convertOverloadedOperators(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack);
void doImplicitIntToFloatTypeCoercionForFloatReturn(Reference<ASTNode>& expr, TraversalPayload& payload);
const std::string errorContext(const ASTNode* n);
const std::string errorContext(const ASTNode& n);
const std::string errorContext(const ASTNode& n, TraversalPayload& payload);
bool isTargetDefinedBeforeAllInStack(const std::vector<FunctionDefinition*>& func_def_stack, int target_function_order_num);
bool expressionIsWellTyped(ASTNode& e, TraversalPayload& payload_);
bool shouldRefCount(EmitLLVMCodeParams& params, const Reference<ASTNode>& expr);
bool shouldRefCount(EmitLLVMCodeParams& params, const ASTNode& expr);
void addMetaDataCommentToInstruction(EmitLLVMCodeParams& params, llvm::Instruction* instr, const std::string& s);
void emitDestructorOrDecrCall(EmitLLVMCodeParams& params, const ASTNode& e, llvm::Value* value, const std::string& comment);
bool mayEscapeCurrentlyBuildingFunction(EmitLLVMCodeParams& params, const TypeRef& type);
void replaceAllUsesWith(Reference<ASTNode>& old_node, Reference<ASTNode>& new_node);

// Clones sub-tree, and updates up-refs to point into new subtree where possible.
Reference<ASTNode> cloneASTNodeSubtree(Reference<ASTNode>& n);

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
	
	FunctionDefinition* allocateVArrayFunc;
	FunctionDefinition* freeVArrayFunc;

	FunctionDefinition* allocateClosureFunc;
	FunctionDefinition* freeClosureFunc;

	llvm::Function* incrStringRefCountLLVMFunc;
	llvm::Function* incrVArrayRefCountLLVMFunc;
	llvm::Function* incrClosureRefCountLLVMFunc;
};


class EmitLLVMCodeParams
{
public:
	FunctionDefinition* currently_building_func_def;
	const PlatformUtils::CPUInfo* cpu_info;
	//bool hidden_voidptr_arg;
	
	llvm::IRBuilder<>* builder;
	llvm::Module* module;
	llvm::Function* currently_building_func;
	llvm::LLVMContext* context;
	const llvm::DataLayout/*TargetData*/* target_data;
	
	std::vector<ASTNode*> node_stack;
	std::vector<LetBlock*> let_block_stack; // Pointers to all the let blocks that are parents of the current node

	std::map<const LetASTNode*, llvm::Value* > let_values;

	std::vector<CleanUpInfo> cleanup_values; // A list of ASTNodes that need to emit cleanup code (Ref decrement code) at the end of the function

	CommonFunctions common_functions;

	std::vector<llvm::Value*> argument_values; // Use for function specialisation in Array fold().

	std::set<Reference<const Type>, ConstTypeRefLessThan>* destructors_called_types;

	bool emit_refcounting_code;

	bool emit_trace_code;

	ProgramStats* stats;
};


class EmitOpenCLCodeParams
{
public:
	std::string file_scope_code;

	std::vector<std::string> blocks;

	std::set<TupleTypeRef, TypeRefLessThan> tuple_types_used;

	int uid;
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
		VArrayLiteralType,
		VectorLiteralType,
		TupleLiteralType,
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
		ArraySubscriptType,
		IfExpressionType,
		NamedConstantType,
		LogicalNegationExprType
	};

	ASTNode(ASTNodeType node_type_, const SrcLocation& loc_) : node_type(node_type_), location(loc_), can_maybe_constant_fold(false) {}
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
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;

	// For the global const array optimisation: Return the AST node as a LLVM value directly.
	virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;

	
	virtual Reference<ASTNode> clone(CloneMapType& clone_map) = 0; // clone_map will map from old node to new node.
	
	virtual void print(int depth, std::ostream& s) const = 0;

	virtual std::string sourceString() const = 0;

	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const = 0;

	// True iff the expression does not depend on any variables
	virtual bool isConstant() const = 0;

	// For constant folding.
	// We can't evaluate an expression for constant folding unless we know the expression is defined.
	// For example, elem(a, i) may not be proven defined yet.
	virtual bool provenDefined() const { return true; }


	const SrcLocation& srcLocation() const { return location; }


	// Can this AST node potentially be replaced with a literal node?
	bool can_maybe_constant_fold;

	// std::vector<UpRefBase*> uprefs; // UpRefs that refer to this node.

protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

	//ASTNode* getParent() { return parent; }
	//void setParent(ASTNode* p) { parent = p; }

private:
//	INDIGO_DISABLE_COPY(ASTNode)

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
class NamedConstant;


class BufferRoot : public ASTNode
{
public:
	BufferRoot(const SrcLocation& loc) : ASTNode(BufferRootType, loc) 
	{}
	
	//std::vector<Reference<FunctionDefinition> > func_defs;
	//std::vector<Reference<NamedConstant> > named_constants;
	std::vector<ASTNodeRef> top_level_defs; // Either function definitions or named constants.

	virtual ValueRef exec(VMState& vmstate){ return ValueRef(); }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(float v, const SrcLocation& loc) : ASTNode(FloatLiteralType, loc), value(v) { this->can_maybe_constant_fold = true; }

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }

	float value;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(int64 v, int num_bits_, const SrcLocation& loc) : ASTNode(IntLiteralType, loc), value(v), num_bits(num_bits_) { assert(num_bits == 32 || num_bits == 64); this->can_maybe_constant_fold = true; }
	int64 value;
	int num_bits;

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Int(num_bits)); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
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
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }

	std::string value;
};


class CharLiteral : public ASTNode
{
public:
	CharLiteral(const std::string& v, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return new CharType(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }

	std::string value; // utf-8 encoded char.
};


class BoolLiteral : public ASTNode
{
public:
	BoolLiteral(bool v, const SrcLocation& loc) : ASTNode(BoolLiteralType, loc), value(v) { can_maybe_constant_fold = true; }
	bool value;

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Bool()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
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
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;



	TypeRef maptype;
	std::vector<std::pair<ASTNodeRef, ASTNodeRef> > items;
};


class AdditionExpression : public ASTNode
{
public:
	AdditionExpression(const SrcLocation& loc, const ASTNodeRef& a_, const ASTNodeRef& b_) : ASTNode(AdditionExpressionType, loc), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	bool typeCheck(TraversalPayload& payload) const;

	ASTNodeRef a;
	ASTNodeRef b;

	mutable TypeRef expr_type; // cached;
};


class SubtractionExpression : public ASTNode
{
public:
	SubtractionExpression(const SrcLocation& loc, const ASTNodeRef& a_, const ASTNodeRef& b_) : ASTNode(SubtractionExpressionType, loc), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	bool typeCheck(TraversalPayload& payload) const;

	ASTNodeRef a;
	ASTNodeRef b;

	mutable TypeRef expr_type; // cached;
};


class MulExpression : public ASTNode
{
public:
	MulExpression(const SrcLocation& loc, const ASTNodeRef& a_, const ASTNodeRef& b_) : ASTNode(MulExpressionType, loc), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	bool typeCheck(TraversalPayload& payload) const;

	ASTNodeRef a;
	ASTNodeRef b;

	mutable TypeRef expr_type; // cached;
};


class DivExpression : public ASTNode
{
public:
	DivExpression(const SrcLocation& loc, const ASTNodeRef& a_, const ASTNodeRef& b_) : ASTNode(DivExpressionType, loc), a(a_), b(b_), proven_defined(false) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual bool provenDefined() const;

	bool typeCheck(TraversalPayload& payload) const;

private:
	void checkNoOverflow(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void checkNoZeroDivide(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:
	ASTNodeRef a;
	ASTNodeRef b;
	bool proven_defined;
	mutable TypeRef expr_type; // cached;
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
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	Type t;
	ASTNodeRef a;
	ASTNodeRef b;
};


class UnaryMinusExpression : public ASTNode
{
public:
	UnaryMinusExpression(const SrcLocation& loc, const ASTNodeRef& expr_) : ASTNode(UnaryMinusExpressionType, loc), expr(expr_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	ASTNodeRef expr;
};


// e.g. !x
class LogicalNegationExpr : public ASTNode
{
public:
	LogicalNegationExpr(const SrcLocation& loc, const ASTNodeRef& expr_) : ASTNode(LogicalNegationExprType, loc), expr(expr_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	ASTNodeRef expr;
};


/*
In a let block,
like:
	let
		a = f()
		b, c = g()
	in
		a + b + c

Then 'a = f()' and 'b, c = g()' are nodes of type LetASTNode.
*/
class LetNodeVar
{
public:
	std::string name;
	TypeRef declared_type; // may be NULL
};


class LetASTNode : public ASTNode
{
public:
	LetASTNode(const std::vector<LetNodeVar>& vars, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	//virtual void linkFunctions(Linker& linker);
	//virtual void bindVariables(const std::vector<ASTNode*>& stack);
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	ASTNodeRef expr;
	std::vector<LetNodeVar> vars; // One or more variable names (And possible associated declared types).  Will be more than one in the case of destructuring assignment.
	//mutable llvm::Value* llvm_value;
	bool traced;
};


class LetBlock : public ASTNode
{
public:
	LetBlock(const ASTNodeRef& e, const std::vector<Reference<LetASTNode> > lets_, const SrcLocation& loc) : 
	  ASTNode(LetBlockType, loc), expr(e), lets(lets_) 
	{
		//let_exprs_llvm_value = vector<llvm::Value*>(lets_.size(), NULL); 
	}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	std::vector<Reference<LetASTNode> > lets;

	//llvm::Value* getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index, llvm::Value* ret_space_ptr);

	//std::vector<llvm::Value*> let_exprs_llvm_value;

	ASTNodeRef expr;
};


class ComparisonExpression : public ASTNode
{
public:
	ComparisonExpression(const Reference<TokenBase>& token_, const ASTNodeRef a_, const ASTNodeRef b_, const SrcLocation& loc) : 
	  ASTNode(ComparisonExpressionType, loc), token(token_), a(a_), b(b_) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return new Bool(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	const std::string getOverloadedFuncName() const; // returns e.g. op_lt, op_gt   etc..

	Reference<TokenBase> token;
	ASTNodeRef a;
	ASTNodeRef b;
};


// Not used currently.
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
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	
	ASTNodeRef subscript_expr;
};


class NamedConstant : public ASTNode
{
public:
	NamedConstant(const TypeRef& declared_type_, const std::string& name_, const ASTNodeRef& value_expr_, const SrcLocation& loc, int order_num_) : 
	  ASTNode(NamedConstantType, loc), declared_type(declared_type_), name(name_), value_expr(value_expr_), order_num(order_num_), llvm_value(NULL)
	{
	}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString() const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;

	TypeRef declared_type; // May be NULL if no type was declared.
	std::string name;
	ASTNodeRef value_expr;
	int order_num;
	mutable llvm::Value* llvm_value;
};

typedef Reference<NamedConstant> NamedConstantRef;


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

