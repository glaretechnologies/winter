/*=====================================================================
wnt_TupleLiteral.cpp
--------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_TupleLiteral.h"


#include "wnt_ASTNode.h"
#include "wnt_SourceBuffer.h"
#include "wnt_RefCounting.h"
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


using std::vector;
using std::string;


namespace Winter
{


TupleLiteral::TupleLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc)
:	ASTNode(TupleLiteralType, loc),
	elements(elems)
{
	if(elems.empty())
		throw BaseException("Tuple literal can't be empty." + errorContext(*this));
}


TypeRef TupleLiteral::type() const
{
	vector<TypeVRef> component_types;
	component_types.reserve(elements.size());
	for(size_t i=0; i<elements.size(); ++i)
	{
		TypeRef t = elements[i]->type();
		if(t.isNull())
			return NULL;
		component_types.push_back(TypeVRef(t));
	}

	return new TupleType(component_types);
}


ValueRef TupleLiteral::exec(VMState& vmstate)
{
	vector<ValueRef> elem_values(elements.size());

	for(unsigned int i=0; i<this->elements.size(); ++i)
		elem_values[i] = this->elements[i]->exec(vmstate);

	return new TupleValue(elem_values);
}


void TupleLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Tuple literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->print(depth + 1, s);
	}
}


std::string TupleLiteral::sourceString() const
{
	std::string s = "[";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->sourceString();
		if(i + 1 < elements.size())
			s += ", ";
	}
	s += "]t";
	return s;
}

/*
Generate something like

struct
{
	float field_0;
	float field_1;
} temp_xx;

temp_xx.field_0 = 1;
temp_xx.field_1 = 2;

*/
std::string TupleLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	const Reference<TupleType> t = this->type().downcast<TupleType>();

	params.tuple_types_used.insert(t);

	const std::string struct_var_name = "tuple_" + toString(params.uid++);
	
	// struct { float e0; float e1; } temp_xx;
	/*std::string s = "struct { ";

	for(size_t i=0; i<t->component_types.size(); ++i)
	{
		s += t->component_types[i]->OpenCLCType() + " field_" + ::toString(i) + "; ";
	}

	s += "} " + struct_var_name + ";\n";*/
	std::string s = t->OpenCLCType() + " " + struct_var_name + ";\n";


	for(size_t i=0; i<elements.size(); ++i)
	{
		// Emit code for let variable
		params.blocks.push_back("");
		const std::string elem_expression = this->elements[i]->emitOpenCLC(params);
		StringUtils::appendTabbed(s, params.blocks.back(), 1);
		params.blocks.pop_back();

		s += struct_var_name + ".field_" + toString(i) + " = " + elem_expression + ";\n";
	}
	params.blocks.back() += s;

	return struct_var_name;

	/*const std::string constructor_name = t.downcast<TupleType>()->OpenCLCType() + "_cnstr";

	std::string s = constructor_name + "(";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->emitOpenCLC(params);
		if(i + 1 < elements.size())
			s += ", ";
	}
	s += ")";
	return s;*/
}


void TupleLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	/*if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<elements.size(); ++i)
			checkFoldExpression(elements[i], payload);
	}
	else */if(payload.operation == TraversalPayload::BindVariables)
	{
		for(size_t i=0; i<elements.size(); ++i)
			convertOverloadedOperators(elements[i], payload, stack);
	}


	stack.push_back(this);
	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->traverse(payload, stack);
	}
	stack.pop_back();


	if(payload.operation == TraversalPayload::InlineFunctionCalls)
	{
		for(size_t i=0; i<elements.size(); ++i)
			checkInlineExpression(elements[i], payload, stack);
	}
	else if(payload.operation == TraversalPayload::SubstituteVariables)
	{
		for(size_t i=0; i<elements.size(); ++i)
			checkSubstituteVariable(elements[i], payload);
	}
	else if(payload.operation == TraversalPayload::TypeCheck)
	{
		// Nothing in particular to do here
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		/*this->can_constant_fold = true;
		for(unsigned int i=0; i<elements.size(); ++i)
			if(!elements[i]->can_constant_fold)
			{
				this->can_constant_fold = false;
				break;
			}
		this->can_constant_fold = this->can_constant_fold && expressionIsWellTyped(*this, payload);*/
		this->can_maybe_constant_fold = true;
		for(size_t i=0; i<elements.size(); ++i)
		{
			const bool elem_is_literal = checkFoldExpression(elements[i], payload);
			this->can_maybe_constant_fold = this->can_maybe_constant_fold && elem_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		for(size_t i=0; i<elements.size(); ++i)
			doDeadCodeElimination(elements[i], payload, stack);
	}
}


bool TupleLiteral::areAllElementsConstant() const
{
	for(size_t i=0; i<this->elements.size(); ++i)
		if(!this->elements[i]->isConstant())
			return false;
	return true;
}


llvm::Value* TupleLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	TypeRef t_ = type();
	const TupleType* tuple_type = static_cast<const TupleType*>(t_.getPointer());


	llvm::Value* result_struct_val;
	if(ret_space_ptr)
		result_struct_val = ret_space_ptr;
	else
	{
		// Allocate space on stack for result structure/tuple
		
		// Emit the alloca in the entry block for better code-gen.
		// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
		llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

		result_struct_val = entry_block_builder.CreateAlloca(
			tuple_type->LLVMType(*params.module), // This type (tuple type)
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"Tuple literal space"
		);
	}

	// For each field in the structure
	for(unsigned int i=0; i<tuple_type->component_types.size(); ++i)
	{
		// Get the pointer to the structure field.
		llvm::Value* field_ptr = LLVMUtils::createStructGEP(params.builder, result_struct_val, i);

		llvm::Value* arg_value_or_ptr = this->elements[i]->emitLLVMCode(params);

		if(tuple_type->component_types[i]->passByValue())
		{
			params.builder->CreateStore(
				arg_value_or_ptr, // value
				field_ptr // ptr
			);
		}
		else
		{
			LLVMUtils::createCollectionCopy(
				tuple_type->component_types[i], 
				field_ptr, // dest ptr
				arg_value_or_ptr, // src ptr
				params
			);
		}


		// If the field is a ref-counted type, we need to increment its reference count, since the newly constructed tuple now holds a reference to it.
		//tuple_type->component_types[i]->emitIncrRefCount(params, arg_value_or_ptr, "TupleLiteral::emitLLVMCode() for type " + tuple_type->toString());

		// If the field is of string type, we need to increment its reference count
		//if(tuple_type->component_types[i]->getType() == Type::StringType)
		//	RefCounting::emitIncrementStringRefCount(params, arg_value_or_ptr);
	}

	return result_struct_val;
}


Reference<ASTNode> TupleLiteral::clone(CloneMapType& clone_map)
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone(clone_map);

	TupleLiteral* res = new TupleLiteral(elems, srcLocation());
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool TupleLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
