/*=====================================================================
ASTNode.h
---------
Copyright Glare Technologies Limited 2019 -
File created by ClassTemplate on Wed Jun 11 03:55:25 2008
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "BaseException.h"
#include "TokenBase.h"
#include "Value.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include <utils/Platform.h>
#include <utils/PlatformUtils.h>
#include <utils/SmallVector.h>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
namespace llvm { class Function; };
namespace llvm { class Value; };
namespace llvm { class Module; };
namespace llvm { class LLVMContext; };
namespace llvm { class TargetData; };
namespace llvm { class Instruction; };
namespace llvm { class ConstantFolder; };
namespace llvm { class DataLayout; };
namespace llvm { class IRBuilderDefaultInserter; };
namespace llvm { template<typename T, typename Inserter > class IRBuilder; };


namespace Winter
{


class VMState;
class Linker;
class LetASTNode;
class AnonFunction;
class Value;
class BuiltInFunctionImpl;
class FunctionDefinition;
class LetBlock;
class ASTNode;
class SourceBuffer;
class ComparisonExpression;
class TraversalPayload;
class LetBlock;
class NamedConstant;
struct ProgramStats;
class SrcLocation;
struct WinterCPUInfo;


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
		//OperatorOverloadConversion, // Converting '+' to op_add etc..
		//GetCleanupNodes,
		CheckInDomain, // Check that calls to elem() etc.. are in bounds.
		InlineFunctionCalls, // inline function calls
		SubstituteVariables, // Used in InlineFunctionCalls passes: replace all variables in the function body with the argument values.
		CustomVisit, // Calls supplied ASTNodeVisitor on each node visited.
		UpdateUpRefs, // When cloning a subtree of nodes, update upwards pointers to point into the new subtree.
		DeadFunctionElimination, // Removes all function definitions that are not reachable (through direct or indirect function calls) from the set of entry functions.
		DeadCodeElimination_ComputeAlive, // Works out which let variables are referenced.
		DeadCodeElimination_RemoveDead, // Removes let variables that are not referenced.
		CountFunctionCalls, // Compute calls_to_func_count.  calls_to_func_count is used in inlining decision.
		CountArgumentRefs, // Count the number of references to each function argument in the body of each function.  Used for inlining decision.
		AddAnonFuncsToLinker, // Adds anonymous functions to linker, to codegen.
		GetAllNamesInScope, // A pass over a function to get the set of names used, so that we can avoid them when generating new names for inlined let vars.
		UnbindVariables, // Unbind certain variables.  Used after cloning an expression during inlining.
		SimplifyIfExpression // Replace "if(true, a, b)" with "a" etc..
	};

	TraversalPayload(Operation e) : 
		linker(nullptr), operation(e), tree_changed(false), current_named_constant(NULL), check_bindings(false)
	{}

	Linker* linker;

	Operation operation;

	std::vector<TypeRef> type_mappings; // for substitute type operation.

	bool tree_changed;

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
	std::map<std::pair<ASTNode*, int>, std::string> new_let_var_name_map;
	int new_order_num;

	Reference<ASTNodeVisitor> custom_visitor;

	bool check_bindings; // If this is true, this is the final binding pass.  Any unbound functions or variables should throw an exception.

	// Types that are captured by a function closure (lambda expression).
	std::set<TypeRef> captured_types;

	// Used in UpdateUpRefs:
	CloneMapType clone_map;

	// Used in DeadFunctionElimination and DeadCodeElimination:
	std::unordered_set<ASTNode*> reachable_nodes;
	std::vector<ASTNode*> nodes_to_process;
	std::unordered_set<ASTNode*> processed_nodes;

	std::map<FunctionDefinition*, int> calls_to_func_count;

	// GetAllNamesInScope, InlineFunctionCalls:
	std::unordered_set<std::string>* used_names;

	SmallVector<Reference<ASTNode>, 4> garbage; // For Storing a ref to a node so it won't get deleted (due to ref count going to zero) while a function on it is still being executed.
};


void printMargin(int depth, std::ostream& s);
bool isIntExactlyRepresentableAsFloat(int64 x);
bool isIntExactlyRepresentableAsDouble(int64 x);
bool checkFoldExpression(Reference<ASTNode>& e, TraversalPayload& payload, std::vector<ASTNode*>& stack); // Returns true if folding took place or e is already a literal.
void doImplicitIntToFloatTypeCoercionForFloatReturn(Reference<ASTNode>& expr, TraversalPayload& payload);
void doImplicitIntToDoubleTypeCoercionForDoubleReturn(Reference<ASTNode>& expr, TraversalPayload& payload);
BufferPosition errorContext(const ASTNode* n);
std::string errorContextString(const ASTNode* n);
BufferPosition errorContext(const ASTNode& n);
std::string errorContextString(const ASTNode& n);
BufferPosition errorContext(const SrcLocation& src_location);
std::string errorContextString(const SrcLocation& src_location);
BufferPosition errorContext(const ASTNode& n, TraversalPayload& payload);
bool isTargetDefinedBeforeAllInStack(const std::vector<FunctionDefinition*>& func_def_stack, int target_function_order_num);
bool expressionIsWellTyped(ASTNode& e, TraversalPayload& payload_);
bool shouldRefCount(EmitLLVMCodeParams& params, const Reference<ASTNode>& expr);
bool shouldRefCount(EmitLLVMCodeParams& params, const ASTNode& expr);
void addMetaDataCommentToInstruction(EmitLLVMCodeParams& params, llvm::Instruction* instr, const std::string& s);
void emitDestructorOrDecrCall(EmitLLVMCodeParams& params, const ASTNode& e, llvm::Value* value, const std::string& comment);
bool mayEscapeCurrentlyBuildingFunction(EmitLLVMCodeParams& params, const TypeRef& type);
void replaceAllUsesWith(Reference<ASTNode>& old_node, Reference<ASTNode>& new_node);
const std::string mapOpenCLCVarName(const std::unordered_set<std::string>& opencl_c_keywords, const std::string& s); // Rename to something that isn't a keyword if is one.


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
	const WinterCPUInfo* cpu_info;
	
	// These template arguments are the defaults from IRBuilder.h.  Written explicitly here so we can forwards declare this type.
	llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter >* builder; 

	llvm::Module* module;
	llvm::Function* currently_building_func;
	llvm::LLVMContext* context;
	const llvm::DataLayout/*TargetData*/* target_data;
	
	std::vector<LetBlock*> let_block_stack; // Pointers to all the let blocks that are parents of the current node

	std::map<const LetASTNode*, llvm::Value* > let_values;

	std::vector<CleanUpInfo> cleanup_values; // A list of ASTNodes that need to emit cleanup code (Ref decrement code) at the end of the function

	CommonFunctions common_functions;

	std::vector<llvm::Value*> argument_values; // Use for function specialisation in Array fold().

	std::set<VRef<const Type>, ConstTypeVRefLessThan>* destructors_called_types;

	bool emit_refcounting_code;

	bool emit_trace_code;

	ProgramStats* stats;
};


class EmitOpenCLCodeParams
{
public:
	EmitOpenCLCodeParams() : uid(0), emit_comments(false), emit_in_bound_asserts(false) {}

	std::string file_scope_code;

	std::string file_scope_func_defs;

	/*
	An expression in Winter, when transformed to OpenCL C, may not be expressible as just as an expression.
	Rather, it may require one or more additional statements.  Such statements will be written to blocks.back().
	
	For example, let blocks are not easily (efficiently) expressible in C:  (we don't want to duplicate sin call)
	let
		x = sin(x)
	in 
		x + x

	In this case the directly returned OpenCL C code will be "x + x", but the code "x = sin(x)" will be put in blocks.back().
	*/
	std::vector<std::string> blocks;

	std::set<TupleTypeRef, TypeRefLessThan> tuple_types_used;

	std::unordered_set<std::string> opencl_c_keywords;

	int uid; // A counter for generating unique names

	bool emit_comments;
	bool emit_in_bound_asserts;
};


struct GetTimeBoundParams
{
	GetTimeBoundParams() : max_bound_computation_steps(1 << 22), steps(0) {}

	// Maximum number of 'steps' (function call expressions encountered currently) to allow when computing the bound,
	// before the computation is terminated and an exception thrown.
	size_t max_bound_computation_steps;

	size_t steps;
};


struct GetSpaceBoundParams
{
	GetSpaceBoundParams() : max_bound_computation_steps(1 << 22), steps(0), is_root_function(true) {}

	// Maximum number of 'steps' (function call expressions encountered currently) to allow when computing the bound,
	// before the computation is terminated and an exception thrown.
	size_t max_bound_computation_steps;

	size_t steps;

	bool is_root_function; // Is this the first function for which getSpaceBound() is called on?
	// (or has getSpaceBound() been called on a function def via a function expression)
};


struct GetSpaceBoundResults
{
	GetSpaceBoundResults(size_t stack_space_, size_t heap_space_) : stack_space(stack_space_), heap_space(heap_space_) {}
	size_t stack_space;
	size_t heap_space;

	void operator += (const GetSpaceBoundResults& b) { stack_space += b.stack_space; heap_space += b.heap_space; }
};


inline GetSpaceBoundResults operator + (const GetSpaceBoundResults& a, const GetSpaceBoundResults& b)
{
	return GetSpaceBoundResults(a.stack_space + b.stack_space, a.heap_space + b.heap_space);
}


struct WinterCPUInfo
{
	enum Arch
	{
		Arch_x64,
		Arch_ARM64
	};

	bool isX64() const { return arch == Arch_x64; }

	Arch arch;
	PlatformUtils::CPUInfo cpu_info; // set if arch == Arch_x64
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
		DoubleLiteralType,
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
		BinaryBitwiseExpressionType,
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
	inline bool isNodeType(ASTNodeType type) const { return node_type == type; }

	virtual TypeRef type() const = 0;

	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack) {}

	// Replace the given child node reference.
	virtual void updateChild(const ASTNode* old_val, Reference<ASTNode>& new_val) { assert(0); }

	// Emit LLVM code to compute the value for this AST node.
	// If the value type is pass-by-pointer, and return_space_pointer is non-null, then code MAY be emitted
	// to store the value at the memory pointed to by return_space_pointer.
	// However, since the value could be stored in a constant global, return_space_pointer doesn't have to be used.
	// Returns a pointer to the mem location where the value is stored.
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* return_space_pointer = NULL) const = 0;

	// Emit cleanup (ref count decrement and delete) code at the end of the function
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;

	virtual Reference<ASTNode> clone(CloneMapType& clone_map) = 0; // clone_map will map from old node to new node.
	
	virtual void print(int depth, std::ostream& s) const = 0;

	virtual std::string sourceString(int depth) const = 0;

	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const = 0;

	// True iff the expression does not depend on any variables
	virtual bool isConstant() const = 0;

	// For constant folding.
	// We can't evaluate an expression for constant folding unless we know the expression is defined.
	// For example, elem(a, i) may not be proven defined yet.
	virtual bool provenDefined() const { return true; }

	virtual size_t getTimeBound(GetTimeBoundParams& params) const = 0;

	// Only FunctionDefinitions need return stack size information, other nodes can return 0.
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const = 0;

	// e.g. number of AST nodes in subtree.
	// For deciding whether to inline a function.
	virtual size_t getSubtreeCodeComplexity() const = 0;


	const SrcLocation& srcLocation() const { return location; }


	// Can this AST node potentially be replaced with a literal node?
	bool can_maybe_constant_fold;

	// std::vector<UpRefBase*> uprefs; // UpRefs that refer to this node.

protected:
	static llvm::Value* emitExternalLinkageCall(const std::string& target_name, EmitLLVMCodeParams& params);

private:
//	GLARE_DISABLE_COPY(ASTNode)

	ASTNodeType node_type;
	SrcLocation location;
};

typedef Reference<ASTNode> ASTNodeRef;


//void updateIndexBounds(TraversalPayload& payload, const ComparisonExpression& comp_expr, const ASTNodeRef& index, int& i_lower, int& i_upper);
bool expressionsHaveSameValue(const ASTNodeRef& a, const ASTNodeRef& b);


class BufferRoot : public ASTNode
{
public:
	BufferRoot(const SrcLocation& loc) : ASTNode(BufferRootType, loc) 
	{}
	
	std::vector<ASTNodeRef> top_level_defs; // Either function definitions or named constants.

	virtual ValueRef exec(VMState& vmstate){ return ValueRef(); }
	virtual TypeRef type() const { throw BaseException("root has no type."); }

	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;
};


class FloatLiteral : public ASTNode
{
public:
	FloatLiteral(float v, const SrcLocation& loc) : ASTNode(FloatLiteralType, loc), value(v) { this->can_maybe_constant_fold = true; }

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Float()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const { return 1; }
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const { return GetSpaceBoundResults(4, 0); }
	virtual size_t getSubtreeCodeComplexity() const { return 1; }

	float value;
};


class DoubleLiteral : public ASTNode
{
public:
	DoubleLiteral(double v, const SrcLocation& loc) : ASTNode(DoubleLiteralType, loc), value(v) { this->can_maybe_constant_fold = true; }

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Double()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const { return 1; }
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const { return GetSpaceBoundResults(8, 0); }
	virtual size_t getSubtreeCodeComplexity() const { return 1; }

	double value;
};


class IntLiteral : public ASTNode
{
public:
	IntLiteral(int64 v, int num_bits_, bool is_signed_, const SrcLocation& loc) : ASTNode(IntLiteralType, loc), value(v), num_bits(num_bits_), is_signed(is_signed_) { assert(num_bits == 16 || num_bits == 32 || num_bits == 64); this->can_maybe_constant_fold = true; }
	
	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new Int(num_bits, is_signed)); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const { return 1; }
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const { return 1; }

	int64 value;
	int num_bits;
	bool is_signed;
};


class StringLiteral : public ASTNode
{
public:
	StringLiteral(const std::string& v, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return TypeRef(new String()); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

	std::string value;
	mutable bool llvm_allocated_on_heap;
};


class CharLiteral : public ASTNode
{
public:
	CharLiteral(const std::string& v, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return new CharType(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	//virtual void emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* val) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const { return 1; }
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const { return GetSpaceBoundResults(1, 0); }
	virtual size_t getSubtreeCodeComplexity() const { return 1; }

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const { return true; }
	virtual size_t getTimeBound(GetTimeBoundParams& params) const { return 1; }
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const { return GetSpaceBoundResults(1, 0); }
	virtual size_t getSubtreeCodeComplexity() const { return 1; }
};


class MapLiteral : public ASTNode
{
public:
	MapLiteral(const SrcLocation& loc) : ASTNode(MapLiteralType, loc) {}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return maptype; }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;


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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual bool provenDefined() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

private:
	void checkNoOverflow(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	void checkNoZeroDivide(TraversalPayload& payload, std::vector<ASTNode*>& stack);
public:
	ASTNodeRef a;
	ASTNodeRef b;
	bool proven_defined;
	mutable TypeRef expr_type; // cached;
};


class BinaryBitwiseExpression : public ASTNode
{
public:
	enum BitwiseType
	{
		BITWISE_AND,
		BITWISE_OR,
		BITWISE_XOR,
		BITWISE_LEFT_SHIFT,
		BITWISE_RIGHT_SHIFT
	};

	BinaryBitwiseExpression(BitwiseType t, const ASTNodeRef& a, const ASTNodeRef& b, const SrcLocation& loc);

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

private:
	const std::string opToken() const;
	BitwiseType t;
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
	virtual TypeRef type() const { return a->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	{}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const { return subscript_expr->type(); }
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;
	
	ASTNodeRef subscript_expr;
};


class NamedConstant : public ASTNode
{
public:
	NamedConstant(const TypeRef& declared_type_, const std::string& name_, const ASTNodeRef& value_expr_, const SrcLocation& loc, int order_num_) : 
	  ASTNode(NamedConstantType, loc), declared_type(declared_type_), name(name_), value_expr(value_expr_), order_num(order_num_), llvm_value(NULL)
	{}

	virtual ValueRef exec(VMState& vmstate);
	virtual TypeRef type() const;
	virtual void print(int depth, std::ostream& s) const;
	virtual std::string sourceString(int depth) const;
	virtual std::string emitOpenCLC(EmitOpenCLCodeParams& params) const;
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual void updateChild(const ASTNode* old_val, ASTNodeRef& new_val);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const;
	virtual Reference<ASTNode> clone(CloneMapType& clone_map);
	virtual bool isConstant() const;
	virtual size_t getTimeBound(GetTimeBoundParams& params) const;
	virtual GetSpaceBoundResults getSpaceBound(GetSpaceBoundParams& params) const;
	virtual size_t getSubtreeCodeComplexity() const;

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
	virtual void traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	vector<FunctionDefinition::FunctionArg> args;
	ASTNodeRef body;
	TypeRef thetype;
};*/


} //end namespace Lang

