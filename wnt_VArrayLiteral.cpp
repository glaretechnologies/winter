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
#include "CompiledValue.h"
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


VArrayLiteral::VArrayLiteral(const std::vector<ASTNodeRef>& elems, const SrcLocation& loc, bool has_int_suffix_, int int_suffix_)
:	ASTNode(VArrayLiteralType, loc),
	elements(elems),
	has_int_suffix(has_int_suffix_),
	int_suffix(int_suffix_),
	make_varray_func_def(NULL),
	llvm_heap_allocated(false)
{
	if(has_int_suffix && int_suffix <= 0)
		throw ExceptionWithPosition("VArray literal int suffix must be > 0.", errorContext(*this));
	if(has_int_suffix && elems.size() != 1)
		throw ExceptionWithPosition("VArray literal with int suffix must have only one explicit elem.", errorContext(*this));

	// Need to have at least one element, so we can determine the element type.
	if(elems.empty())
		throw ExceptionWithPosition("VArray literal can't be empty.", errorContext(*this));
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


std::string VArrayLiteral::sourceString(int depth) const
{
	std::string s = "[";
	for(size_t i=0; i<elements.size(); ++i)
	{
		s += elements[i]->sourceString(depth);
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


static const int MAX_INT_SUFFIX_NO_MAKE_VARRAY = 16;


void VArrayLiteral::traverse(TraversalPayload& payload, std::vector<ASTNode*>& stack)
{
	if(payload.operation == TraversalPayload::BindVariables)
	{
		TypeRef elem_0_type = elements[0]->type();
		if(elem_0_type.nonNull())
		{
			if(has_int_suffix && (int_suffix > MAX_INT_SUFFIX_NO_MAKE_VARRAY)) // If we want to call makeVArray() to construct this varray:
			{
				const FunctionSignature makeVArray_sig("makeVArray", typePair(TypeVRef(elements[0]->type()), new Int(64)));
				FunctionDefinitionRef def = payload.linker->findMatchingFunction(makeVArray_sig, Winter::SrcLocation::invalidLocation(),
					/*effective_callsite_order_num=*/-1);
				make_varray_func_def = def.getPointer();
			}
		}
	}


	stack.push_back(this);
	for(unsigned int i=0; i<this->elements.size(); ++i)
	{
		this->elements[i]->traverse(payload, stack);
	}
	stack.pop_back();


	if(payload.operation == TraversalPayload::TypeCheck)
	{
		// Check all the element expression types match the computed element type.
		const TypeRef elem_type = this->elements[0]->type();
		for(unsigned int i=0; i<this->elements.size(); ++i)
			if(*elem_type != *this->elements[i]->type())
				throw ExceptionWithPosition("VArray element " + ::toString(i) + " did not have required type " + elem_type->toString() + ".",
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
			const bool elem_is_literal = checkFoldExpression(elements[i], payload, stack);
			this->can_maybe_constant_fold = this->can_maybe_constant_fold && elem_is_literal;
		}
	}
	else if(payload.operation == TraversalPayload::DeadFunctionElimination)
	{
		// Mark makeVArray() as reachable.
		if(make_varray_func_def)
			payload.reachable_nodes.insert(make_varray_func_def);
	}
}


void VArrayLiteral::updateChild(const ASTNode* old_val, ASTNodeRef& new_val)
{
	for(size_t i=0; i<this->elements.size(); ++i)
		if(this->elements[i].ptr() == old_val)
		{
			this->elements[i] = new_val;
			return;
		}
	assert(0);
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
	const size_t max_num_stack_elems = 1 << 16;
	const size_t max_on_stack_size = 1 << 16;

	const uint64_t num_elems = has_int_suffix ? int_suffix : elements.size();
	const TypeRef elem_type = elements[0]->type();
	const uint64_t elem_size_B = params.target_data->getTypeAllocSize(elem_type->LLVMType(*params.module)); // Get size of element
	const uint64_t all_elems_size_B = elem_size_B * num_elems;

	// We need to allocate on heap if size of varray is too large for stack
	const bool alloc_on_heap = mayEscapeCurrentlyBuildingFunction(params, this->type()) ||
		(num_elems > max_num_stack_elems) ||
		(all_elems_size_B > max_on_stack_size);
	this->llvm_heap_allocated = alloc_on_heap;

	llvm::Value* varray_ptr;
	uint64 initial_flags;

	if(alloc_on_heap)
	{
		llvm::Value* num_elems_llvm_val = llvm::ConstantInt::get(*params.context, 
			llvm::APInt(/*num bits=*/64, num_elems, /*signed=*/true));

		// If we have an int suffix with a large value, then we don't want to emit assignment instructions for every element,
		// rather emit a call to makeVArray() which uses a loop.
		if(has_int_suffix && (num_elems > MAX_INT_SUFFIX_NO_MAKE_VARRAY))
		{
			llvm::Function* make_varray_llvm_func = make_varray_func_def->getOrInsertFunction(params.module);

			llvm::Value* element_0_value = this->elements[0]->emitLLVMCode(params);

			llvm::Value* args[] = { element_0_value, num_elems_llvm_val };
			return params.builder->CreateCall(make_varray_llvm_func, args);
		}

		params.stats->num_heap_allocation_calls++;

		// Emit a call to allocateVArray:
		// allocateVArray(const int elem_size_B, const int num_elems)
		llvm::Function* allocateVArrayLLVMFunc = params.common_functions.allocateVArrayFunc->getOrInsertFunction(
			params.module,
			false // use_cap_var_struct_ptr
		);

		llvm::Value* size_B_constant = llvm::ConstantInt::get(*params.context, llvm::APInt(64, elem_size_B, /*signed=*/false));

		

		llvm::Value* args[] = { size_B_constant, num_elems_llvm_val };
		llvm::CallInst* call_inst = params.builder->CreateCall(allocateVArrayLLVMFunc, args, "varray_literal");

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

		const uint64 total_varray_size_B = sizeof(uint64)*3 + elem_size_B * num_elems;
		const uint64 total_varray_size_uint64s = Maths::roundedUpDivide<uint64>(total_varray_size_B, sizeof(uint64));

		// NOTE: use int64s as the allocation type so we get the necessary alignment for the uint64s in VArrayRep.
		llvm::Value* alloca_ptr = entry_block_builder.CreateAlloca(
			llvm::Type::getInt64Ty(*params.context), // type - int64
			llvm::ConstantInt::get(*params.context, llvm::APInt(64, total_varray_size_uint64s, true)), // num elems
			this->type()->toString() + " stack space"
		);

		// Cast resulting allocated int64* down to VArrayRep for the right type, e.g. varray<T>
		llvm::Type* varray_T_type = this->type()->LLVMType(*params.module);
		assert(varray_T_type->isPointerTy());

		varray_ptr = params.builder->CreatePointerCast(alloca_ptr, varray_T_type);
		initial_flags = 0; // flag = 0 = not heap allocated

		params.cleanup_values.push_back(CleanUpInfo(this, varray_ptr));
	}


	// Set the reference count to 1
	llvm::Value* ref_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 0, "varray_literal_ref_ptr");
	llvm::Value* one = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(64, 1, 
			true // signed
		)
	);
	llvm::StoreInst* store_inst = params.builder->CreateStore(one, ref_ptr);
	addMetaDataCommentToInstruction(params, store_inst, "VArray literal set initial ref count to 1");

	// Set VArray length
	llvm::Value* length_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 1, "varray_literal_length_ptr");
	llvm::Value* length_constant_int = llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(64, num_elems, 
			true // signed
		)
	);
	llvm::StoreInst* store_length_inst = params.builder->CreateStore(length_constant_int, length_ptr);
	addMetaDataCommentToInstruction(params, store_length_inst, "VArray literal set initial length count to " + ::toString(num_elems));

	// Set the flags
	llvm::Value* flags_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 2, "varray_literal_flags_ptr");
	llvm::Value* flags_contant_val = llvm::ConstantInt::get(*params.context, llvm::APInt(64, initial_flags));
	llvm::StoreInst* store_flags_inst = params.builder->CreateStore(flags_contant_val, flags_ptr);
	addMetaDataCommentToInstruction(params, store_flags_inst, "VArray literal set initial flags to " + toString(initial_flags));


	llvm::Value* data_ptr = LLVMUtils::createStructGEP(params.builder, varray_ptr, 3, "varray_literal_data_ptr");

	//data_ptr->dump();
	//data_ptr->getType()->dump();

	// Store the element values in the array.
	// For each element in the literal
	if(has_int_suffix)
	{
		assert(int_suffix <= 16);

		if(this->elements[0]->type()->passByValue())
		{
			llvm::Value* elem_0_value = this->elements[0]->emitLLVMCode(params);
			for(int i=0; i<int_suffix; ++i)
			{
				// Store the element in the array
				llvm::Value* element_ptr = LLVMUtils::createStructGEP(params.builder, data_ptr, i);
				params.builder->CreateStore(/*value=*/elem_0_value, /*ptr=*/element_ptr);
			}
		}
		else
		{
			// Element is pass-by-pointer, for example a structure.
			// So just emit code that will store it directly in the array.
			for(int i=0; i<int_suffix; ++i)
			{
				llvm::Value* element_ptr = LLVMUtils::createStructGEP(params.builder, data_ptr, i);
				this->elements[0]->emitLLVMCode(params, element_ptr);
			}
		}
	}
	else
	{
		for(unsigned int i=0; i<this->elements.size(); ++i)
		{
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


size_t VArrayLiteral::getTimeBound(GetTimeBoundParams& params) const
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


GetSpaceBoundResults VArrayLiteral::getSpaceBound(GetSpaceBoundParams& params) const
{
	TypeRef this_type = this->type();
	assert(this_type->getType() == Type::VArrayTypeType);
	VArrayType* varray_type = static_cast<VArrayType*>(this_type.getPointer());

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

	if(llvm_heap_allocated)
	{
		const size_t single_elem_heap_size = varray_type->elem_type->isHeapAllocated() ? sizeof(void*) : varray_type->elem_type->memSize();
		const size_t header_and_data_size = sizeof(VArrayRep) + single_elem_heap_size * numElementsInValue();
		
		// We have to take into account the stack space that the C++ function allocateVArray(), which will be called, will take.
		sum += GetSpaceBoundResults(1024, /*heap space=*/header_and_data_size);
	}

	return sum;
}


size_t VArrayLiteral::getSubtreeCodeComplexity() const
{
	size_t sum = 0;
	for(size_t i=0; i<elements.size(); ++i)
		sum += elements[i]->getSubtreeCodeComplexity();
	return 1 + sum;
}


} // end namespace Winter
