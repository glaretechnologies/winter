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
	vector<TypeRef> component_types(elements.size());
	for(size_t i=0; i<component_types.size(); ++i)
	{
		component_types[i] = elements[i]->type();
		if(component_types[i].isNull())
			return NULL;
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
		printMargin(depth+1, s);
		this->elements[i]->print(depth+2, s);
	}
}


std::string TupleLiteral::sourceString() const
{
	assert(0);
	return "";
}


std::string TupleLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	TypeRef t = this->type();

	const std::string constructor_name = t.downcast<TupleType>()->OpenCLCType() + "_cnstr";

	std::string s = constructor_name + "(";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->emitOpenCLC(params);
		if(i + 1 < elements.size())
			s += ", ";
	}
	s += ")";
	return s;
}


void TupleLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::ConstantFolding)
	{
		for(size_t i=0; i<elements.size(); ++i)
			checkFoldExpression(elements[i], payload);
	}
	else if(payload.operation == TraversalPayload::OperatorOverloadConversion)
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
			tuple_type->LLVMType(*params.context), // This type (tuple type)
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			"Tuple literal space"
		);
	}

	// For each field in the structure
	for(unsigned int i=0; i<tuple_type->component_types.size(); ++i)
	{
		// Get the pointer to the structure field.
		llvm::Value* field_ptr = params.builder->CreateStructGEP(result_struct_val, i);

		llvm::Value* arg_value = this->elements[i]->emitLLVMCode(params);

		if(!tuple_type->component_types[i]->passByValue())
		{
			// Load the value from memory
			arg_value = params.builder->CreateLoad(
				arg_value // ptr
			);
		}

		params.builder->CreateStore(
			arg_value, // value
			field_ptr // ptr
		);

		// If the field is of string type, we need to increment its reference count
		if(tuple_type->component_types[i]->getType() == Type::StringType)
			RefCounting::emitIncrementStringRefCount(params, arg_value);
	}

	return result_struct_val;
}


Reference<ASTNode> TupleLiteral::clone()
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone();
	return ASTNodeRef(new TupleLiteral(elems, srcLocation()));
}


bool TupleLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
