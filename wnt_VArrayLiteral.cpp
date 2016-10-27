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
#include "VirtualMachine.h"
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
:	ASTNode(VArrayLiteralType, loc),
	elements(elems),
	has_int_suffix(has_int_suffix_),
	int_suffix(int_suffix_)
{
	if(has_int_suffix && int_suffix <= 0)
		throw BaseException("VArray literal int suffix must be > 0." + errorContext(*this));
	if(has_int_suffix && elems.size() != 1)
		throw BaseException("VArray literal with int suffix must have only one explicit elem." + errorContext(*this));

	// Need to have at least one element, so we can determine the element type.
	if(elems.empty())
		throw BaseException("VArray literal can't be empty." + errorContext(*this));
}


TypeRef VArrayLiteral::type() const
{
	// if Array literal contains a yet-unbound function, then the type is not known yet and will be NULL.
	const TypeRef e0_type = elements[0]->type();
	if(e0_type.isNull()) return NULL;

	return new VArrayType(TypeVRef(elements[0]->type()));
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
	s += "]va";
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
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		//FunctionDefinitionRef def = payload.linker->findMatchingFunctionSimple(FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int(64))));
		//payload.reachable_defs.insert(def.getPointer());
	}
	else if(payload.operation == TraversalPayload::DeadCodeElimination_RemoveDead)
	{
		for(size_t i=0; i<elements.size(); ++i)
			doDeadCodeElimination(elements[i], payload, stack);
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
	const bool alloc_on_heap = mayEscapeCurrentlyBuildingFunction(params, this->type());

	llvm::Value* varray_ptr;
	uint64 initial_flags;

	if(alloc_on_heap)
	{
		params.stats->num_heap_allocation_calls++;

		// Emit a call to allocateVArray:
		// allocateVArray(const int elem_size_B, const int num_elems)
		llvm::Function* allocateVArrayLLVMFunc = params.common_functions.allocateVArrayFunc->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr
		);

		const TypeRef elem_type = elements[0]->type();

		const uint64_t size_B = params.target_data->getTypeAllocSize(elem_type->LLVMType(*params.module)); // Get size of element
		llvm::Value* size_B_constant = llvm::ConstantInt::get(*params.context, llvm::APInt(64, size_B, /*signed=*/false));

		llvm::Value* num_elems = llvm::ConstantInt::get(
			*params.context,
			llvm::APInt(64, //TEMP
				this->elements.size(),
				true // signed
			)
		);

		llvm::CallInst* call_inst = params.builder->CreateCall2(allocateVArrayLLVMFunc, size_B_constant, num_elems, "varray_literal");

		// Set calling convention.  NOTE: LLVM claims to be C calling conv. by default, but doesn't seem to be.
		call_inst->setCallingConv(llvm::CallingConv::C);

		// Cast resulting allocated void* down to VArrayRep for the right type, e.g. varray<T>
		llvm::Type* varray_T_type = this->type()->LLVMType(*params.module);
		assert(varray_T_type->isPointerTy());

		varray_ptr = params.builder->CreatePointerCast(call_inst, varray_T_type);
		initial_flags = 1; // flag = 1 = heap allocated
	}
	else
	{
		// Allocate space on stack for array.
		// Allocate as just an array of bytes.
		// Then cast to the needed type.  We do this because our LLVM Varray type has only zero length for the actual data, so can't be used for the alloca.
		
		// Emit the alloca in the entry block for better code-gen.
		// We will emit the alloca at the start of the block, so that it doesn't go after any terminator instructions already created which have to be at the end of the block.
		llvm::IRBuilder<> entry_block_builder(&params.currently_building_func->getEntryBlock(), params.currently_building_func->getEntryBlock().getFirstInsertionPt());


		const TypeRef elem_type = elements[0]->type();
		const uint64 elem_size_B = params.target_data->getTypeAllocSize(elem_type->LLVMType(*params.module)); // Get size of element
		const uint64 total_varray_size_B = sizeof(uint64)*3 + elem_size_B * elements.size();

		llvm::Value* alloca_ptr = entry_block_builder.CreateAlloca(
			llvm::Type::getInt8Ty(*params.context), // byte
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, total_varray_size_B, true)), // number of bytes needed.
			this->type()->toString() + " stack space"
		);

		// Cast resulting allocated uint8* down to VArrayRep for the right type, e.g. varray<T>
		llvm::Type* varray_T_type = this->type()->LLVMType(*params.module);
		assert(varray_T_type->isPointerTy());

		varray_ptr = params.builder->CreatePointerCast(alloca_ptr, varray_T_type);
		initial_flags = 0; // flag = 0 = not heap allocated

		params.cleanup_values.push_back(CleanUpInfo(this, varray_ptr));
	}


	// Set the reference count to 1
	llvm::Value* ref_ptr = params.builder->CreateStructGEP(varray_ptr, 0, "varray_literal_ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(64, 1, 
			true // signed
		)
	);
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, ref_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "VArray literal set initial ref count to 1");

	// Set VArray length
	llvm::Value* length_ptr = params.builder->CreateStructGEP(varray_ptr, 1, "varray_literal_length_ptr");
	llvm::Value* length_constant_int = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(64, this->elements.size(), 
			true // signed
		)
	);
	llvm::StoreInst* store_length_inst = params.builder->CreateStore(length_constant_int, length_ptr);
	addMetaDataCommentToInstruction(params, store_length_inst, "VArray literal set initial length count to " + ::toString(this->elements.size()));

	// Set the flags
	llvm::Value* flags_ptr = params.builder->CreateStructGEP(varray_ptr, 2, "varray_literal_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "VArray literal set initial flags to " + toString(initial_flags));


	llvm::Value* data_ptr = params.builder->CreateStructGEP(varray_ptr, 3, "varray_literal_data_ptr");

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
			llvm::Value* element_ptr = params.builder->CreateConstInBoundsGEP2_64(data_ptr, 0, i, "varray_literal_element_ptr");

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

	/*CleanUpInfo info;
	info.node = this;
	info.value = varray_ptr;
	params.cleanup_values.push_back(info);*/

	return varray_ptr;
}


void VArrayLiteral::emitCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val) const
{
	// RefCounting::emitVArrayCleanupLLVMCode(params, string_val);

	//TEMP: clean up the ptr_alloca
	//llvm::Value* varray_val = params.builder->CreateLoad(this->ptr_alloca);
	//RefCounting::emitVArrayCleanupLLVMCode(params, varray_val);
}


Reference<ASTNode> VArrayLiteral::clone(CloneMapType& clone_map)
{
	std::vector<ASTNodeRef> elems(this->elements.size());
	for(size_t i=0; i<elements.size(); ++i)
		elems[i] = this->elements[i]->clone(clone_map);

	VArrayLiteral* res = new VArrayLiteral(elems, srcLocation(), has_int_suffix, int_suffix);
	clone_map.insert(std::make_pair(this, res));
	return res;
}


bool VArrayLiteral::isConstant() const
{
	for(size_t i=0; i<elements.size(); ++i)
		if(!elements[i]->isConstant())
			return false;
	return true;
}


size_t VArrayLiteral::numElementsInValue() const
{
	return has_int_suffix ? int_suffix : elements.size();
}


} // end namespace Winter
