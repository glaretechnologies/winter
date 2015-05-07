/*=====================================================================
wnt_VArrayLiteral.cpp
---------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "wnt_VArrayLiteral.h"


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


VArrayLiteral::VArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix_, int int_suffix_)
:	ASTNode(ArrayLiteralType, loc),
	elements(elems),
	has_int_suffix(has_int_suffix_),
	int_suffix(int_suffix_)
{
	if(has_int_suffix && int_suffix <= 0)
		throw BaseException("VArray literal int suffix must be > 0." + errorContext(*this));
	if(has_int_suffix && elems.size() != 1)
		throw BaseException("VArray literal with int suffix must have only one explicit elem." + errorContext(*this));

	if(elems.empty())
		throw BaseException("VArray literal can't be empty." + errorContext(*this));
}


TypeRef VArrayLiteral::type() const
{
	// if Array literal contains a yet-unbound function, then the type is not known yet and will be NULL.
	const TypeRef e0_type = elements[0]->type();
	if(e0_type.isNull()) return NULL;

	return new VArrayType(elements[0]->type());
}


ValueRef VArrayLiteral::exec(VMState& vmstate)
{
	if(has_int_suffix)
	{
		ValueRef v = this->elements[0]->exec(vmstate);

		vector<ValueRef> elem_values(int_suffix, v);

		return new VArrayValue(elem_values);
	}
	else
	{

		vector<ValueRef> elem_values(elements.size());

		for(unsigned int i=0; i<this->elements.size(); ++i)
			elem_values[i] = this->elements[i]->exec(vmstate);

		return new VArrayValue(elem_values);
	}
}


void VArrayLiteral::print(int depth, std::ostream& s) const
{
	printMargin(depth, s);
	s << "VArray literal\n";

	for(unsigned int i=0; i<this->elements.size(); ++i)
		this->elements[i]->print(depth + 1, s);
}


std::string VArrayLiteral::sourceString() const
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


std::string VArrayLiteral::emitOpenCLC(EmitOpenCLCodeParams& params) const
{
	assert(0);
	return "";
}


void VArrayLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
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
				throw BaseException("VArray element " + ::toString(i) + " did not have required type " + elem_type->toString() + "." + 
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
}


bool VArrayLiteral::areAllElementsConstant() const
{
	for(size_t i=0; i<this->elements.size(); ++i)
		if(!this->elements[i]->isConstant())
			return false;
	return true;
}


llvm::Value* VArrayLiteral::emitLLVMCode(EmitLLVMCodeParams& params, llvm::Value* ret_space_ptr) const
{
	// Emit a call to allocateVArray:
	// allocateVArray(const int elem_size_B, const int num_elems)
	llvm::Function* allocateVArrayLLVMFunc = params.common_functions.allocateVArrayFunc->getOrInsertFunction(
		params.module,
		false // use_cap_var_struct_ptr
	);

	const TypeRef elem_type = elements[0]->type();

	const uint64_t size_B = params.target_data->getTypeAllocSize(elem_type->LLVMType(*params.module)); // Get size of element
	llvm::Value* size_B_constant = llvm::ConstantInt::get(*params.context, llvm::APInt(32, size_B, /*signed=*/false));

	llvm::Value* num_elems = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(32, //TEMP
			this->elements.size(),
			true // signed
		)
	);

	llvm::CallInst* call_inst = params.builder->CreateCall2(allocateVArrayLLVMFunc, size_B_constant, num_elems, "varray");

	// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
	call_inst->setCallingConv(llvm::CallingConv::C);

	// Cast resulting allocated void* down to VArrayRep for the right type, e.g. varray<T>
	llvm::Type* varray_T_type = this->type()->LLVMType(*params.module);
	assert(varray_T_type->isPointerTy());

	llvm::Value* cast_result = params.builder->CreatePointerCast(call_inst, varray_T_type);

	//////////////////////////////////
	// Allocate space for a pointer on the stack, at the entry block of the function

	/*llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());

	this->ptr_alloca = entry_block_builder.CreateAlloca(
		type()->LLVMType(*params.module), // type
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, 1, true)), // num elems
		"varray_ptr"
	);
	// Store a null pointer there
	assert(varray_T_type->isPointerTy());

	llvm::Type* varray_val_type = ((llvm::PointerType*)varray_T_type)->getElementType();

	llvm::PointerType* varray_T_type_ptr = static_cast<llvm::PointerType*>(varray_T_type);

	entry_block_builder.CreateStore( llvm::ConstantPointerNull::get(varray_T_type_ptr), this->ptr_alloca);


	// Emit some code to save to the entry block ptr
	params.builder->CreateStore(cast_result, ptr_alloca);*/

	////////////////////////////




	// Set the reference count to 1
	llvm::Value* ref_ptr = params.builder->CreateStructGEP(cast_result, 0, "ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(64, 1, 
			true // signed
		)
	);
	params.builder->CreateStore(one, ref_ptr);

	//llvm::Value* data_ptr_ptr = params.builder->CreateStructGEP(cast_result, 1, "data_ptr_ptr");
	//llvm::Value* data_ptr = params.builder->CreateLoad(data_ptr_ptr);
	llvm::Value* data_ptr = params.builder->CreateStructGEP(cast_result, 1, "data_ptr");

	//data_ptr->dump();
	//data_ptr->getType()->dump();

	// Store the element values in the array.
	// For each element in the literal
	if(has_int_suffix)
	{
		// NOTE: could optimise this more (share value etc..)
		for(int i=0; i<int_suffix; ++i)
		{
			llvm::Value* element_ptr = params.builder->CreateStructGEP(data_ptr, i);

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
			//llvm::Value* element_ptr = params.builder->CreateStructGEP(data_ptr, i);
			//llvm::Value* element_ptr = params.builder->CreateConstInBoundsGEP1_64(data_ptr, i);
			llvm::Value* element_ptr = params.builder->CreateConstInBoundsGEP2_64(data_ptr, 0, i, "element_ptr");

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

	CleanUpInfo info;
	info.node = this;
	info.value = cast_result;
	params.cleanup_values.push_back(info);

	return cast_result;
}


void VArrayLiteral::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const
{
	// RefCounting::emitVArrayCleanupLLVMCode(params, string_val);

	//TEMP: clean up the ptr_alloca
	//llvm::Value* varray_val = params.builder->CreateLoad(this->ptr_alloca);
	//RefCounting::emitVArrayCleanupLLVMCode(params, varray_val);
}


llvm::Value* VArrayLiteral::getConstantLLVMValue(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


Reference<ASTNode> VArrayLiteral::clone()
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone();
	return new VArrayLiteral(elems, srcLocation(), has_int_suffix, int_suffix);
}


bool VArrayLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


} // end namespace Winter
