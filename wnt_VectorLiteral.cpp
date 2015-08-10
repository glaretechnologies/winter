/*=====================================================================
wnt_VectorLiteral.cpp
---------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_VectorLiteral.h"


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


VectorLiteral::VectorLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix_, int int_suffix_)
:	ASTNode(VectorLiteralType, loc),
	elements(elems),
	has_int_suffix(has_int_suffix_),
	int_suffix(int_suffix_)
{
	if(has_int_suffix && int_suffix <= 0)
		throw BaseException("Vector literal int suffix must be > 0." + errorContext(*this));
	if(has_int_suffix && elems.size() != 1)
		throw BaseException("Vector literal with int suffix must have only one explicit elem." + errorContext(*this));

	if(elems.empty())
		throw BaseException("Vector literal can't be empty." + errorContext(*this));
}


TypeRef VectorLiteral::type() const
{
	TypeRef elem_type = elements[0]->type();
	if(elem_type.isNull())
		return NULL;

	if(has_int_suffix)
		return new VectorType(elem_type, this->int_suffix);
	else
	{
		if(elem_type->getType() == Type::IntType)
		{
			// Consider type coercion.
			// A vector of elements that contains one or more float-typed elements will be considered to be a float-typed vector.
			// Either all integer elements will be succesfully constant-folded and coerced to a float literal, or type checking will fail.
			for(size_t i=0; i<elements.size(); ++i)
			{
				const TypeRef& cur_elem_type = elements[i]->type();

				if(cur_elem_type.isNull())
					return NULL;

				if(cur_elem_type->getType() == Type::FloatType)
					return new VectorType(new Float(), (int)elements.size()); // float vector type
			}
		}

		return new VectorType(elem_type, (int)elements.size());
	}
}


ValueRef VectorLiteral::exec(VMState& vmstate)
{
	if(has_int_suffix)
	{
		ValueRef v = this->elements[0]->exec(vmstate);

		vector<ValueRef> elem_values(int_suffix, v);

		return new VectorValue(elem_values);
	}
	else
	{
		vector<ValueRef> elem_values(elements.size());

		for(unsigned int i=0; i<this->elements.size(); ++i)
			elem_values[i] = this->elements[i]->exec(vmstate);

		return new VectorValue(elem_values);
	}
}


void VectorLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "Vector literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->print(depth + 1, s);
	}
}


std::string VectorLiteral::sourceString() const
{
	std::string s = "[";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->sourceString();
		if(i + 1 < elements.size())
			s += ", ";
	}
	s += "]v";
	return s;
}


std::string VectorLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	//return "";
	/*if(elements.size() == 4)
	{
		return "(float4)(" + elements[0]->emitOpenCLC(params) + ", " + elements[1]->emitOpenCLC(params) + ", " + elements[2]->emitOpenCLC(params) + ", " + elements[3]->emitOpenCLC(params) + ")";
	}
	else
	{
		assert(0);
		return "";
	}*/

	// TODO: check vector width

	if(!(this->elements[0]->type()->getType() == Type::FloatType || (this->elements[0]->type()->getType() == Type::IntType && this->elements[0]->type().downcastToPtr<Int>()->numBits() == 32)))
		throw BaseException("Only vectors of float or int32 supported for OpenCL currently.");

	const std::string elem_typename = this->elements[0]->type()->getType() == Type::FloatType ? "float" : "int";
	if(has_int_suffix)
	{
		// "(float4)(1.0f, 2.0f, 3.0f, 4.0)"
		std::string s = "(" + elem_typename + toString(this->int_suffix) + ")(";
		for(size_t i=0; i<this->int_suffix; ++i)
		{
			s += elements[0]->emitOpenCLC(params);
			if(i + 1 < this->int_suffix)
				s += ", ";
		}
		s += ")";
		return s;
	}
	else
	{
		std::string s = "(" + elem_typename + toString(elements.size()) + ")(";
		for(size_t i=0; i<elements.size(); ++i)
		{
			s += elements[i]->emitOpenCLC(params);
			if(i + 1 < elements.size())
				s += ", ";
		}
		s += ")";
		return s;
	}
}


void VectorLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
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
	else if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// Convert e.g. [1.0, 2.0, 3]v to [1.0, 2.0, 3.0]v
		// A vector of elements that contains one or more float-typed elements will be considered to be a float-typed vector.
		// Either all integer elements will be succesfully constant-folded and coerced to a float literal, or type checking will fail.

		// Do we have any float literals in this vector?
		bool have_float = false;
		for(size_t i=0; i<elements.size(); ++i)
			have_float = have_float || (elements[i]->type().nonNull() && elements[i]->type()->getType() == Type::FloatType);

		if(have_float)
		{
			for(size_t i=0; i<elements.size(); ++i)
				if(elements[i]->nodeType() == ASTNode::IntLiteralType)
				{
					const IntLiteral* int_lit = static_cast<const IntLiteral*>(elements[i].getPointer());
					if(isIntExactlyRepresentableAsFloat(int_lit->value))
					{
						elements[i] = new FloatLiteral((float)int_lit->value, int_lit->srcLocation());
						payload.tree_changed = true;
					}
				}
		}
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
		const TypeRef this_type = this->type();
		if(this_type.isNull() || this_type->getType() != Type::VectorTypeType)
			throw BaseException("Vector type error." + errorContext(*this, payload));

		const Type* elem_type = this_type.downcastToPtr<VectorType>()->elem_type.getPointer();

		for(size_t i=0; i<this->elements.size(); ++i)
			if(*elem_type != *this->elements[i]->type())
				throw BaseException("Vector element did not have required type " + elem_type->toString() + "." + errorContext(*this->elements[i], payload));

		if(!(elem_type->getType() == Type::IntType || elem_type->getType() == Type::FloatType))
			throw BaseException("Vector types can only contain float or int elements." + errorContext(*this, payload));
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
}


bool VectorLiteral::areAllElementsConstant() const
{
	for(size_t i=0; i<this->elements.size(); ++i)
		if(!this->elements[i]->isConstant())
			return false;
	return true;
}


llvm::Value* VectorLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// Check if all elements in the array are constant.  If so, use a constant global array.
	/*if(this->areAllElementsConstant())
	{
		vector<llvm::Constant*> llvm_values(this->elements.size());

		for(size_t i=0; i<elements.size(); ++i)
		{
			VMState vm_state(true); // hidden_voidptr_arg
			vm_state.func_args_start.push_back(0);
			vm_state.argument_stack.push_back(ValueRef(new VoidPtrValue(NULL)));
			ValueRef value = this->elements[i]->exec(vm_state);
			llvm_values[i] = value->getConstantLLVMValue(params, this->type().downcast<VectorType>()->t);
		}

		assert(this->type()->LLVMType(*params.context)->isVectorTy());

		llvm::GlobalVariable* global = new llvm::GlobalVariable(
			*params.module,
			this->type()->LLVMType(*params.context), // This type (vector type)
			true, // is constant
			llvm::GlobalVariable::InternalLinkage,
			llvm::ConstantVector::get(
				llvm_values
			)
		);

		global->dump();//TEMP

		return params.builder->CreateLoad(global);
	}*/

	if(has_int_suffix)
	{
		return params.builder->CreateVectorSplat(
			this->int_suffix, // num elements
			this->elements[0]->emitLLVMCode(params), // value
			"vector literal"
		);
	}
	else
	{
		//NOTE TODO: Can just use get() here to create the constant vector immediately?

		// Start with a vector of Undefs.
		llvm::Value* v = llvm::ConstantVector::getSplat(
			(unsigned int)this->elements.size(),
			llvm::UndefValue::get(this->elements[0]->type()->LLVMType(*params.module))
		);

		// Insert elements one-by-one.
		llvm::Value* vec = v;
		for(unsigned int i=0; i<this->elements.size(); ++i)
		{
			llvm::Value* elem_llvm_code = this->elements[i]->emitLLVMCode(params);

			vec = params.builder->CreateInsertElement(
				vec, // vec
				elem_llvm_code, // new element
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, i)) // index
			);
		}

		return vec;
	}
}


Reference<ASTNode> VectorLiteral::clone(CloneMapType& clone_map)
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone(clone_map);
	VectorLiteral* res = new VectorLiteral(elems, srcLocation(), has_int_suffix, int_suffix);
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool VectorLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
