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
	TypeRef elem_type_ = elements[0]->type();
	if(elem_type_.isNull())
		return NULL;
	TypeVRef elem_type = TypeVRef(elem_type_);

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

				if(cur_elem_type->getType() == Type::DoubleType)
					return new VectorType(new Double(), (int)elements.size()); // double vector type

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
	if(has_int_suffix)
		s += toString(int_suffix);
	return s;
}


std::string VectorLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	// TODO: check vector width is valid for OpenCL C.

	if(!(this->elements[0]->type()->getType() == Type::FloatType || this->elements[0]->type()->getType() == Type::DoubleType || (this->elements[0]->type()->getType() == Type::IntType && this->elements[0]->type().downcastToPtr<Int>()->numBits() == 32)))
		throw BaseException("Only vectors of float or int32 supported for OpenCL currently.");

	const std::string elem_typename = this->elements[0]->type()->OpenCLCType();
	if(has_int_suffix)
	{
		// "((float4)(1.0f))"
		std::string s = "((" + elem_typename + toString(this->int_suffix) + ")(" + 
			elements[0]->emitOpenCLC(params) + "))";
		return s;
	}
	else
	{
		// "((float4)(1.0f, 2.0f, 3.0f, 4.0))"
		// Note that the extra surrounding parentheses seem to be required to avoid syntax errors in some cases.
		std::string s = "((" + elem_typename + toString(elements.size()) + ")(";
		for(size_t i=0; i<elements.size(); ++i)
		{
			s += elements[i]->emitOpenCLC(params);
			if(i + 1 < elements.size())
				s += ", ";
		}
		s += "))";
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
	else */
	if(payload.operation == TraversalPayload::TypeCoercion)
	{
		// Convert e.g. [1.0, 2.0, 3]v to [1.0, 2.0, 3.0]v
		// A vector of elements that contains one or more float-typed elements will be considered to be a float-typed vector.
		// Either all integer elements will be succesfully constant-folded and coerced to a float literal, or type checking will fail.

		// Do we have any double literals in this vector?
		bool have_double = false;
		for(size_t i=0; i<elements.size(); ++i)
			have_double = have_double || (elements[i]->type().nonNull() && elements[i]->type()->getType() == Type::DoubleType);

		// Do we have any float literals in this vector?
		bool have_float = false;
		for(size_t i=0; i<elements.size(); ++i)
			have_float = have_float || (elements[i]->type().nonNull() && elements[i]->type()->getType() == Type::FloatType);

		if(have_double)
		{
			for(size_t i=0; i<elements.size(); ++i)
				if(elements[i]->nodeType() == ASTNode::IntLiteralType)
				{
					const IntLiteral* int_lit = static_cast<const IntLiteral*>(elements[i].getPointer());
					if(isIntExactlyRepresentableAsDouble(int_lit->value))
					{
						elements[i] = new DoubleLiteral((double)int_lit->value, int_lit->srcLocation());
						payload.tree_changed = true;
					}
				}
		}
		else if(have_float)
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

		if(!(elem_type->getType() == Type::IntType || elem_type->getType() == Type::FloatType || elem_type->getType() == Type::DoubleType))
			throw BaseException("Vector types can only contain float, double or int elements." + errorContext(*this, payload));
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


void VectorLiteral::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	for(size_t i=0; i<this->elements.size(); ++i)
		if(this->elements[i].ptr() == old_val)
		{
			this->elements[i] = new_val;
			return;
		}
	assert(0);
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
		// Check if all elements in the array are constant.  If so, use llvm::ConstantVector::get().
		// NOTE: this seems to result in exactly the same ugly IR as below:
		//  %.splatinsert = insertelement <4 x float> undef, float %x, i32 0
		//  %.splat = shufflevector <4 x float> %.splatinsert, <4 x float> undef, <4 x i32> zeroinitializer
		//  %0 = fmul nnan ninf nsz <4 x float> %.splat, <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>
		/*if(this->areAllElementsConstant())
		{
			llvm::SmallVector<llvm::Constant*, 8> llvm_constants(this->elements.size());

			bool are_all_elems_llvm_constants = true;
			for(size_t i=0; i<elements.size(); ++i)
			{
				llvm::Value* v = this->elements[i]->emitLLVMCode(params);
				if(llvm::isa<llvm::Constant>(v))
					llvm_constants[i] = llvm::cast<llvm::Constant>(v);
				else
					are_all_elems_llvm_constants = false;
			}

			if(are_all_elems_llvm_constants)
			{
				llvm::Value* code = llvm::ConstantVector::get(llvm_constants);
				code->dump();
				return code;
			}
		}*/

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


size_t VectorLiteral::getTimeBound(GetTimeBoundParams& params) const
{
	if(has_int_suffix)
		return elements[0]->getTimeBound(params) + int_suffix; // Time to compute elem 0 and then copy time.
	else
	{
		size_t sum = 0;
		for(size_t i=0; i<elements.size(); ++i)
			sum += elements[i]->getTimeBound(params);
		return sum;
	}
}


GetSpaceBoundResults VectorLiteral::getSpaceBound(GetSpaceBoundParams& params) const
{
	// Compute space to compute the element values:
	GetSpaceBoundResults sum(0, 0);
	if(has_int_suffix)
	{
		sum += elements[0]->getSpaceBound(params);
	}
	else
	{
		for(size_t i=0; i<elements.size(); ++i)
			sum += elements[i]->getSpaceBound(params);
	}

	return sum;
}


size_t VectorLiteral::getSubtreeCodeComplexity() const
{
	size_t sum = 0;
	for(size_t i=0; i<elements.size(); ++i)
		sum += elements[i]->getSubtreeCodeComplexity();
	return 1 + sum;
}


} // end namespace Winter
