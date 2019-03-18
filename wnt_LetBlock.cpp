/*=====================================================================
wnt_LetBlock.cpp
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "wnt_LetBlock.h"


#include "wnt_ASTNode.h"
#include "wnt_LetASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
#include "wnt_Variable.h"
#include "VMState.h"
#include "Value.h"
#include "Linker.h"
#include "BuiltInFunctionImpl.h"
#include "LLVMUtils.h"
#include "LLVMTypeUtils.h"
#include "ProofUtils.h"
#include "utils/StringUtils.h"
#include "maths/mathstypes.h"
#include <ostream>
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using namespace std;


namespace Winter
{


ValueRef LetBlock::exec(VMState& vmstate)
{
	//const size_t let_stack_size = vmstate.let_stack.size();
	//vmstate.let_stack_start.push_back(let_stack_size); // Push let frame index

	// Evaluate let clauses, which will each push the result onto the let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.push_back(lets[i]->exec(vmstate));


	ValueRef retval = this->expr->exec(vmstate);

	// Pop things off let stack
	//for(unsigned int i=0; i<lets.size(); ++i)
	//	vmstate.let_stack.pop_back();
	
	// Pop let frame index
	//vmstate.let_stack_start.pop_back();

	return retval;
}


void LetBlock::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Let Block.  (" + toHexString((uint64)this) + ") lets:\n";
	for(size_t i=0; i<lets.size(); ++i)
		lets[i]->print(depth + 1, s);
	printMargin(depth, s); s << "in:\n";
	this->expr->print(depth+1, s);
}


std::string LetBlock::sourceString() const
{
	std::string s;
	s += "let\n";
	for(size_t i=0; i<lets.size(); ++i)
		s += "\t" + lets[i]->sourceString() + "\n";
	s += "in\n\t";
	s += expr->sourceString();
	return s;
}


/*
let
	x = 1
	y = 2
in
	x + y

=>


int let_result_xx;
{
	int x = 1;
	int y = 2;
	
	let_result_xx = x + y;
}


for destructuring assignment:
----------------------------

let
	x, y = (1, 2)
in
	x + y

=>

int let_result_xx;
{
	let_var_value_xx = //
	int x = let_var_value_xx.field_0;
	int y = let_var_value_xx.field_1;
	
	let_result_xx = x + y;
}

*/

std::string LetBlock::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	const std::string result_var_name = "let_result_" + toString(params.uid++);

	std::string s = this->type()->OpenCLCType() + " " + result_var_name + ";\n";
	s += "{\n";

	for(size_t i=0; i<lets.size(); ++i)
	{
		// Emit code for let variable
		params.blocks.push_back("");
		std::string let_expression = this->lets[i]->emitOpenCLC(params);
		StringUtils::appendTabbed(s, params.blocks.back(), 1);
		params.blocks.pop_back();

		if(this->lets[i]->vars.size() == 1)
		{
			// If let expression is a pass-by-pointer argument, need to dereference it.
			if(this->lets[i]->expr->type()->OpenCLPassByPointer() && (this->lets[i]->expr->nodeType() == ASTNode::VariableASTNodeType) && (this->lets[i]->expr.downcastToPtr<Variable>()->binding_type == Variable::BindingType_Argument))
				let_expression = "*" + let_expression;

			s += "\t" + this->lets[i]->type()->OpenCLCType() + " " + mapOpenCLCVarName(params.opencl_c_keywords, this->lets[i]->vars[0].name) + " = " + let_expression + ";\n";
		}
		else
		{
			// Destructuring:
			assert(this->lets[i]->type()->getType() == Type::TupleTypeType);
			const std::string let_var_value_name = "let_var_value_" + toString(params.uid++);
			s += "\t" + this->lets[i]->type()->OpenCLCType() + " " + let_var_value_name + " = " + let_expression + ";\n";
			for(size_t z=0; z<this->lets[i]->vars.size(); ++z)
			{
				const TypeRef elem_type = this->lets[i]->type().downcastToPtr<TupleType>()->component_types[z];
				s += "\t" + elem_type->OpenCLCType() + " " + mapOpenCLCVarName(params.opencl_c_keywords, this->lets[i]->vars[z].name) + " = " + let_var_value_name + ".field_" + toString(z) + ";\n";
			}
		}
	}

	// Emit code for let value expression
	params.blocks.push_back("");
	const std::string let_value_expr = expr->emitOpenCLC(params);
	StringUtils::appendTabbed(s, params.blocks.back(), 1);
	params.blocks.pop_back();

	s += "\t" + result_var_name + " = " + let_value_expr + ";\n";

	s += "}\n";

	params.blocks.back() += s;

	return result_var_name;
}


// Return a new name for the let variable, that is not in the set of used names.
// See "Secrets of the Glasgow Haskell Compiler inliner", 
// http://research.microsoft.com/en-us/um/people/simonpj/Papers/inlining/inline.pdf
// Section 3.3: "Choosing a new name" for a discussion of the issues.
static std::string getNewName(const std::string& old_name, const std::unordered_set<std::string>& used_names)
{
	// Try and pick a new name for "x" like "x_0", "x_1".  These names might be used though, so give up after a while.
	for(int i=0; i<10; ++i)
	{
		const std::string n = old_name + "_" + toString(i);
		if(used_names.find(n) == used_names.end()) // If this new name is not used:
			return n;
	}

	// Try and pick a new name for "x" like "x_n", "x_{n+1}", where n = num elements in used_name set.
	for(int i=0; i<100; ++i)
	{
		const std::string n = old_name + "_" + toString(used_names.size() + i);
		if(used_names.find(n) == used_names.end()) // If this new name is not used:
			return n;
	}

	throw BaseException("Failed to find new name in a reasonable time.");
}


void LetBlock::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	stack.push_back(this);

	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		checkFoldExpression(expr, payload);
	}*/

	if(payload.operation == TraversalPayload::DeadCodeElimination_ComputeAlive)
	{
		// The let value expression is alive, but the let vars are not necessarily alive.  So just traverse to the let value expression.
		expr->traverse(payload, stack);
		stack.pop_back();
		return;
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		// Remove unused let nodes
		for(auto i=lets.begin(); i!=lets.end();)
		{
			LetASTNode* node = i->getPointer();
			if(payload.reachable_nodes.find(node) == payload.reachable_nodes.end()) // If this let node is not reachable:
			{
				// std::cout << "Removing unused let node '" + node->vars[0].name + "'.\n";
				i = lets.erase(i); // Remove it
				payload.tree_changed = true;
			}
			else
				i++;
		}
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		// Rename all let vars
		for(size_t i=0; i<lets.size(); ++i)
			for(size_t z=0; z<lets[i]->vars.size(); ++z)
			{
				if(payload.used_names->count(lets[i]->vars[z].name)) // If var name is in set of used names:
				{
					// Pick a new name for the let variable
					const std::string new_name = getNewName(lets[i]->vars[z].name, *payload.used_names);
					payload.used_names->insert(new_name);

					lets[i]->vars[z].name = new_name;
					payload.new_let_var_name_map[std::make_pair(lets[i].getPointer(), (int)z)] = new_name;
				}
			}

		checkSubstituteVariable(expr, payload);
	}


	for(unsigned int i=0; i<lets.size(); ++i)
		lets[i]->traverse(payload, stack);

	//payload.let_block_stack.push_back(this);

	expr->traverse(payload, stack);

	//payload.let_block_stack.pop_back();

	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		checkInlineExpression(expr, payload, stack);
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		/*this->can_constant_fold = true;
		for(unsigned int i=0; i<lets.size(); ++i)
			this->can_constant_fold = this->can_constant_fold && lets[i]->can_constant_fold;
		this->can_constant_fold = this->can_constant_fold && expr->can_constant_fold;
		this->can_constant_fold = this->can_constant_fold && expressionIsWellTyped(*this, payload);*/

		this->can_maybe_constant_fold = checkFoldExpression(expr, payload);

		for(size_t i=0; i<lets.size(); ++i)
		{
			const bool let_is_literal = checkFoldExpression(lets[i]->expr, payload); // NOTE: this correct?
			this->can_maybe_constant_fold = this->can_maybe_constant_fold && let_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		for(unsigned int i=0; i<lets.size(); ++i)
			doDeadCodeElimination(lets[i]->expr, payload, stack);

		doDeadCodeElimination(expr, payload, stack);
	}

	stack.pop_back();
}


void LetBlock::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	if(expr.ptr() == old_val)
		expr = new_val;
	else
		assert(0);
}


bool shouldRefCount(EmitLLVMCodeParams& params, const ASTNodeRef& expr)
{
	return shouldRefCount(params, *expr);
}


bool shouldRefCount(EmitLLVMCodeParams& params, const ASTNode& expr)
{
	/*if(expr.nodeType() == ASTNode::VariableASTNodeType && static_cast<const Variable&>(expr).vartype == Variable::ArgumentVariable)
	{
		// If this is a variable bound to a function argument, only need to do ref counting for if it has the same type as the enclosing function return type.
		const bool same_as_ret_type = *expr.type() == *params.currently_building_func_def->returnType();
		return same_as_ret_type;
	}
	else*/
		return true;
}


static const std::string makeSafeMetaDataString(const std::string& s)
{
	if(s.empty())
		return s;

	std::string res = s;
	
	if(!isAlphabetic(s[0]))
		res[0] = 'z';

	for(size_t i=0; i<s.size(); ++i)
		if(!(::isAlphaNumeric(s[i]) || s[i] == '_'))
			res[i] = '_';
	return res;
}
	

void addMetaDataCommentToInstruction(EmitLLVMCodeParams& params, llvm::Instruction* instr, const std::string& s)
{
	const std::string safe_comment = makeSafeMetaDataString(s);
	llvm::MDNode* mdnode = llvm::MDNode::get(*params.context, llvm::MDString::get(*params.context, s));
	instr->setMetadata(safe_comment, mdnode);
}


static llvm::Function* getOrInsertTracePrintFloatCall(llvm::Module* module)
{
	// void tracePrintFloat(const char* var_name, float val)

	llvm::Type* arg_types[2] = { 
		llvm::Type::getInt8PtrTy(module->getContext()), 
		llvm::Type::getFloatTy(module->getContext())
	};

	llvm::FunctionType* functype = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module->getContext()), // return type
		arg_types,
		false // varargs
	);

	llvm::Constant* llvm_func_constant = module->getOrInsertFunction(
		"tracePrintFloat", // Name
		functype // Type
	);

	assert(llvm::isa<llvm::Function>(llvm_func_constant));
	return static_cast<llvm::Function*>(llvm_func_constant);
}


static void emitTracePrintCall(EmitLLVMCodeParams& params, const string& var_name, llvm::Value* float_value)
{
	// Make a global constant character array for the string data.
	llvm::Value* string_global = params.builder->CreateGlobalString(var_name);

	// Get a pointer to the zeroth elem
	llvm::Value* elem_0 = LLVMUtils::createStructGEP(params.builder, string_global, 0);

	llvm::Function* f = getOrInsertTracePrintFloatCall(params.module);
	llvm::Value* args[2] = { elem_0, float_value };
	params.builder->CreateCall(f, args);
}


llvm::Value* LetBlock::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// NEW: Emit code for the let statements now.
	// We need to do this now, otherwise we will get "instruction does not dominate all uses", if a let statement has its code emitted in a if statement block.
	
	//for(size_t i=0; i<lets.size(); ++i)
	//	let_exprs_llvm_value[i] = this->lets[i]->emitLLVMCode(params, ret_space_ptr);

	//params.let_block_let_values.insert(std::make_pair(this, std::vector<llvm::Value*>()));
	//params.let_block_let_values[this] = std::vector<llvm::Value*>();


	params.let_block_stack.push_back(const_cast<LetBlock*>(this));

	//std::vector<llvm::Value*> let_values(lets.size());
	for(size_t i=0; i<lets.size(); ++i)
	{
		llvm::Value* let_value = this->lets[i]->emitLLVMCode(params, ret_space_ptr);

		if(params.emit_trace_code && this->lets[i]->type()->getType() == Type::FloatType)
			emitTracePrintCall(params, this->lets[i]->vars[0].name, let_value);

		//params.let_block_let_values[this].push_back(let_value);
		params.let_values[this->lets[i].getPointer()] = let_value;
	}

	//params.let_block_let_values.insert(std::make_pair(this, let_values));


	llvm::Value* expr_value = expr->emitLLVMCode(params, ret_space_ptr);

	params.let_block_stack.pop_back();

	// Decrement ref counts on all let blocks
	for(size_t i=0; i<lets.size(); ++i)
	{
		if(shouldRefCount(params, this->lets[i]->expr))
			emitDestructorOrDecrCall(params, *this->lets[i]->expr, params.let_values[this->lets[i].getPointer()],  "Let block for let var " + this->lets[i]->vars[0].name + " decrement/destructor");
	}

	return expr_value;
}


Reference<ASTNode> LetBlock::clone(CloneMapType& clone_map)
{
	vector<Reference<LetASTNode> > new_lets(lets.size());
	for(size_t i=0; i<new_lets.size(); ++i)
		new_lets[i] = Reference<LetASTNode>(static_cast<LetASTNode*>(lets[i]->clone(clone_map).getPointer()));
	Winter::ASTNodeRef clone = this->expr->clone(clone_map);
	LetBlock* res = new LetBlock(clone, new_lets, this->srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool LetBlock::isConstant() const
{
	//TODO: check let expressions for constants as well
	for(size_t i=0; i<lets.size(); ++i)
		if(!lets[i]->isConstant())
			return false;

	return expr->isConstant();
}


size_t LetBlock::getTimeBound(GetTimeBoundParams& params) const
{
	size_t sum = 0;
	for(size_t i=0; i<lets.size(); ++i)
		sum += lets[i]->getTimeBound(params);

	return sum + expr->getTimeBound(params);
}


GetSpaceBoundResults LetBlock::getSpaceBound(GetSpaceBoundParams& params) const
{
	GetSpaceBoundResults sum_bound(0, 0);
	for(size_t i=0; i<lets.size(); ++i)
		sum_bound += lets[i]->getSpaceBound(params);

	return sum_bound + expr->getSpaceBound(params);
}


size_t LetBlock::getSubtreeCodeComplexity() const
{
	size_t sum = 0;
	for(size_t i=0; i<lets.size(); ++i)
		sum += lets[i]->getSubtreeCodeComplexity();
	return 1 + sum;
}


//llvm::Value* LetBlock::getLetExpressionLLVMValue(EmitLLVMCodeParams& params, unsigned int let_index, llvm::Value* ret_space_ptr)
//{
	/*if(let_exprs_llvm_value[let_index] == NULL)
	{
		let_exprs_llvm_value[let_index] = this->lets[let_index]->emitLLVMCode(params, ret_space_ptr);
	}*/

	//return let_exprs_llvm_value[let_index];
//}


} // end namespace Winter
