/*=====================================================================
wnt_ArrayLiteral.cpp
--------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_ArrayLiteral.h"


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


ArrayLiteral::ArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix_, int int_suffix_)
:	ASTNode(ArrayLiteralType, loc),
	elements(elems),
	has_int_suffix(has_int_suffix_),
	int_suffix(int_suffix_)
{
	if(has_int_suffix && int_suffix <= 0)
		throw BaseException("Array literal int suffix must be > 0." + errorContext(*this));
	if(has_int_suffix && elems.size() != 1)
		throw BaseException("Array literal with int suffix must have only one explicit elem." + errorContext(*this));

	if(elems.empty())
		throw BaseException("Array literal can't be empty." + errorContext(*this));
}


TypeRef ArrayLiteral::type() const
{
	// if Array literal contains a yet-unbound function, then the type is not known yet and will be NULL.
	const TypeRef e0_type = elements[0]->type();
	if(e0_type.isNull()) return NULL;

	TypeRef array_type;
	if(has_int_suffix)
		array_type = new ArrayType(e0_type, this->int_suffix);
	else
		array_type = new ArrayType(e0_type, elements.size());

	array_type->address_space = "__constant";
	return array_type;
}


ValueRef ArrayLiteral::exec(VMState& vmstate)
{
	if(has_int_suffix)
	{
		ValueRef v = this->elements[0]->exec(vmstate);

		vector<ValueRef> elem_values(int_suffix, v);

		return new ArrayValue(elem_values);
	}
	else
	{
		vector<ValueRef> elem_values(elements.size());

		for(unsigned int i=0; i<this->elements.size(); ++i)
			elem_values[i] = this->elements[i]->exec(vmstate);

		return new ArrayValue(elem_values);
	}
}


void ArrayLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Array literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->print(depth + 1, s);
	}
}


std::string ArrayLiteral::sourceString() const
{
	std::string s = "[";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->sourceString();
		if(i + 1 < elements.size())
			s += ", ";
	}
	s += "]a";
	return s;
}


std::string ArrayLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	// Work out which type this is
	/*Type::TypeType type;
	for(size_t i=0; i<this->elements.size(); ++i)
	{
		if(this->elements[i]->nodeType() == ASTNode::FloatLiteralType)
		{
			type = Type::FloatType;
			break;
		}
		else if(this->elements[i]->nodeType() == ASTNode::IntLiteralType)
		{
			type = Type::IntType;
			// don't break, keep going to see if we hit a float literal.
		}
	}*/
	TypeRef this_type = this->type();
	assert(this_type->getType() == Type::ArrayTypeType);

	ArrayType* array_type = static_cast<ArrayType*>(this_type.getPointer());

	std::string s = "__constant ";
	if(array_type->elem_type->getType() == Type::FloatType)
		s += "float ";
	else if(array_type->elem_type->getType() == Type::IntType)
		s += "int ";
	else
		throw BaseException("Array literal must be of int or float type for OpenCL emission currently.");
	

	const std::string name = "array_literal_" + toString(params.uid++);
	s += name + "[] = {";
	for(size_t i=0; i<this->elements.size(); ++i)
	{
		s += this->elements[i]->emitOpenCLC(params);
		if(i + 1 < this->elements.size())
			s += ", ";
	}
	s += "};\n";

	params.file_scope_code += s;

	return name;
}


void ArrayLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
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
		// Check all the element expression types match the computed element type.
		const TypeRef elem_type = this->elements[0]->type();
		for(unsigned int i=0; i<this->elements.size(); ++i)
			if(*elem_type != *this->elements[i]->type())
				throw BaseException("Array element " + ::toString(i) + " did not have required type " + elem_type->toString() + "." + 
				errorContext(*this, payload));
	}
	else if(payload.operation == TraversalPayload::ComputeCanConstantFold)
	{
		/*this->can_constant_fold = true;
		for(size_t i=0; i<elements.size(); ++i)
			can_constant_fold = can_constant_fold && elements[i]->can_constant_fold;
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


static bool areAllElementsIntLiterals(const std::vector<ASTNodeRef>& elements)
{
	for(size_t i=0; i<elements.size(); ++i)
		if(elements[i]->nodeType() != ASTNode::IntLiteralType)
			return false;
	return true;
}


static bool areAllElementsFloatLiterals(const std::vector<ASTNodeRef>& elements)
{
	for(size_t i=0; i<elements.size(); ++i)
		if(elements[i]->nodeType() != ASTNode::FloatLiteralType)
			return false;
	return true;
}


static bool areAllElementsDoubleLiterals(const std::vector<ASTNodeRef>& elements)
{
	for(size_t i=0; i<elements.size(); ++i)
		if(elements[i]->nodeType() != ASTNode::DoubleLiteralType)
			return false;
	return true;
}


llvm::Value* ArrayLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	//if(!ret_space_ptr)
	//{

	// Check if all elements in the array are literals, of a type that LLVM supports in constant global arrays.  If so, use a constant global array.
	// TODO: can probably allow more types here.
	if(areAllElementsIntLiterals(this->elements) || areAllElementsFloatLiterals(this->elements) || areAllElementsDoubleLiterals(this->elements))
	{
		vector<llvm::Constant*> array_llvm_values;

		if(has_int_suffix)
		{
			llvm::Value* element_0_value = this->elements[0]->emitLLVMCode(params);
			if(!llvm::isa<llvm::Constant>(element_0_value))
				throw BaseException("Internal error: expected constant.");

			array_llvm_values.resize(int_suffix);
			for(size_t i=0; i<int_suffix; ++i)
				array_llvm_values[i] = static_cast<llvm::Constant*>(element_0_value);
		}
		else
		{
			array_llvm_values.resize(elements.size());

			for(size_t i=0; i<elements.size(); ++i)
			{
				llvm::Value* element_value = this->elements[i]->emitLLVMCode(params);
				if(!llvm::isa<llvm::Constant>(element_value))
					throw BaseException("Internal error: expected constant.");

				array_llvm_values[i] = static_cast<llvm::Constant*>(element_value);
			}
		}

		assert(this->type()->LLVMType(*params.module)->isArrayTy());

		llvm::GlobalVariable* global = new llvm::GlobalVariable(
			*params.module,
			this->type()->LLVMType(*params.module), // This type (array type)
			true, // is constant
			llvm::GlobalVariable::PrivateLinkage, // llvm::GlobalVariable::InternalLinkage,
			llvm::ConstantArray::get(
				(llvm::ArrayType*)this->type()->LLVMType(*params.module),
				array_llvm_values
			)
		);
		global->setUnnamedAddr(true); // Mark as unnamed_addr - this means the address is not significant, so multiple arrays with the same contents can be combined.

		return global;
	}


	llvm::Value* array_addr;
	if(ret_space_ptr)
		array_addr = ret_space_ptr;
	else
	{
		// Allocate space on stack for array
		
		// Emit the alloca in the entry block for better code-gen.
		// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
		llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

		array_addr = entry_block_builder.CreateAlloca(
			this->type()->LLVMType(*params.module), // This type (array type)
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
			//this->elements[0]->type()->LLVMType(*params.context),
			//llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->elements.size(), true)), // num elems
			"Array literal space"
		);
	}

	// For each element in the literal
	if(has_int_suffix)
	{
		// NOTE: could optimise this more (share value etc..)
		for(int i=0; i<int_suffix; ++i)
		{
			llvm::Value* element_ptr = params.builder->CreateStructGEP(array_addr, i);

			if(this->elements[0]->type()->passByValue())
			{
				llvm::Value* element_value = this->elements[0]->emitLLVMCode(params);

				// Store the element in the array
				params.builder->CreateStore(
					element_value, // value
					element_ptr // ptr
				);
			}
			else
			{
				// Element is pass-by-pointer, for example a structure.
				// So just emit code that will store it directly in the array.
				this->elements[0]->emitLLVMCode(params, element_ptr);
			}
		}
	}
	else
	{
		for(unsigned int i=0; i<this->elements.size(); ++i)
		{
			llvm::Value* element_ptr = params.builder->CreateStructGEP(array_addr, i);

			if(this->elements[i]->type()->passByValue())
			{
				llvm::Value* element_value = this->elements[i]->emitLLVMCode(params);

				// Store the element in the array
				params.builder->CreateStore(
					element_value, // value
					element_ptr // ptr
				);
			}
			else
			{
				// Element is pass-by-pointer, for example a structure.
				// So just emit code that will store it directly in the array.
				this->elements[i]->emitLLVMCode(params, element_ptr);
			}
		}
	}

	return array_addr;//NOTE: this correct?
}


Reference<ASTNode> ArrayLiteral::clone(CloneMapType& clone_map)
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone(clone_map);
	ArrayLiteral* a = new ArrayLiteral(elems, srcLocation(), has_int_suffix, int_suffix);
	clone_map.insert(std::make_pair(this, a));
	return a;
}


bool ArrayLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
