#pragma once


#include "wnt_Type.h"
#include "Value.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
namespace llvm { class Value; }


namespace Winter
{


class Value;
class VMState;
class EmitLLVMCodeParams;
class EmitOpenCLCodeParams;
class ASTNode;
class TraversalPayload;


class BuiltInFunctionImpl : public RefCounted
{
public:
	enum BuiltInFunctionImplType
	{
		BuiltInType_Constructor,
		BuiltInType_GetField,
		BuiltInType_UpdateElementBuiltInFunc,
		BuiltInType_GetTupleElementBuiltInFunc,
		BuiltInType_GetVectorElement,
		BuiltInType_ArrayMapBuiltInFunc,
		BuiltInType_ArrayFoldBuiltInFunc,
		BuiltInType_ArraySubscriptBuiltInFunc,
		BuiltInType_VArraySubscriptBuiltInFunc,
		BuiltInType_MakeVArrayBuiltInFunc,
		BuiltInType_VectorSubscriptBuiltInFunc,
		BuiltInType_ArrayInBoundsBuiltInFunc,
		BuiltInType_VectorInBoundsBuiltInFunc,
		BuiltInType_IterateBuiltInFunc,
		BuiltInType_DotProductBuiltInFunc,
		BuiltInType_VectorMinBuiltInFunc,
		BuiltInType_VectorMaxBuiltInFunc,
		BuiltInType_ShuffleBuiltInFunc,
		BuiltInType_PowBuiltInFunc,
		BuiltInType_SqrtBuiltInFunc,
		BuiltInType_ExpBuiltInFunc,
		BuiltInType_LogBuiltInFunc,
		BuiltInType_SinBuiltInFunc,
		BuiltInType_CosBuiltInFunc,
		BuiltInType_AbsBuiltInFunc,
		BuiltInType_FloorBuiltInFunc,
		BuiltInType_CeilBuiltInFunc,
		BuiltInType_SignBuiltInFunc,
		BuiltInType_TruncateToIntBuiltInFunc,
		BuiltInType_ToFloatBuiltInFunc,
		BuiltInType_ToDoubleBuiltInFunc,
		BuiltInType_ToInt64BuiltInFunc,
		BuiltInType_ToInt32BuiltInFunc,
		BuiltInType_VoidPtrToInt64BuiltInFunc,
		BuiltInType_LengthBuiltInFunc,
		BuiltInType_CompareEqualBuiltInFunc
	};

	BuiltInFunctionImpl(BuiltInFunctionImplType builtin_type_) : builtin_type(builtin_type_) {}
	virtual ~BuiltInFunctionImpl(){}

	virtual ValueRef invoke(VMState& vmstate) = 0;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;

	// Should this call be considered expensive, when considering possible duplication during inlining?
	virtual bool callIsExpensive() const { return true; } 

	 // Built-in functions may call other functions, so we need to be able to traverse them when getting set of alive functions.
	virtual void deadFunctionEliminationTraverse(TraversalPayload& payload) const {}

	virtual void linkInCalledFunctions(TraversalPayload& payload) const {}

	const BuiltInFunctionImplType builtInType() const { return builtin_type; }
private:
	BuiltInFunctionImplType builtin_type;
};


typedef Reference<BuiltInFunctionImpl> BuiltInFunctionImplRef;


// Construct a struct value
class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(VRef<StructureType>& struct_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	VRef<StructureType> struct_type;
};


// Return a field value or pointer to a field from a structure.
class GetField : public BuiltInFunctionImpl
{
public:
	GetField(VRef<StructureType>& struct_type_, unsigned int index_) : BuiltInFunctionImpl(BuiltInType_GetField), struct_type(struct_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
//private:
	VRef<StructureType> struct_type;
	unsigned int index;
};


// Returns a new collection value with the value at the given index updated to the new value.
class UpdateElementBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		def update(CollectionType c, int index, T newval) CollectionType


		is equivalent to 

		def update(CollectionType c, int index, T newval) CollectionType
			new_c = clone(c)
			new_c[index] = newval
			return new_c
		end
	*/
	UpdateElementBuiltInFunc(const TypeVRef& collection_type/*, const TypeRef& value_type*/);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	TypeVRef collection_type; // Should be one of array, vector, or tuple
	//TypeRef value_type; // T
};


// Return a field value or pointer to a field from a tuple.
class GetTupleElementBuiltInFunc : public BuiltInFunctionImpl
{
public:
	GetTupleElementBuiltInFunc(const VRef<TupleType>& tuple_type_, unsigned int index_) : BuiltInFunctionImpl(BuiltInType_GetTupleElementBuiltInFunc), tuple_type(tuple_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }

	void setIndex(unsigned int i) { index = i; }
//private:
	VRef<TupleType> tuple_type;
	unsigned int index;
};


// Return an element with given fixed index from a vector.
class GetVectorElement : public BuiltInFunctionImpl
{
public:
	GetVectorElement(VRef<VectorType>& vector_type_, unsigned int index_) : BuiltInFunctionImpl(BuiltInType_GetVectorElement), vector_type(vector_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	VRef<VectorType> vector_type;
	unsigned int index;
};


class ArrayMapBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArrayMapBuiltInFunc(const VRef<ArrayType>& from_type_, const VRef<Function>& func_type_) : BuiltInFunctionImpl(BuiltInType_ArrayMapBuiltInFunc), from_type(from_type_), func_type(func_type_), specialised_f(NULL) {}

	/*
	map(function<T, R>, array<T, N>) array<R, N>

	*/
	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	// Specialise for a particular first argument.
	void specialiseForFunctionArg(FunctionDefinition* f);
private:
	llvm::Value* insertWorkFunction(EmitLLVMCodeParams& params) const;
	VRef<ArrayType> from_type; // from array type
	VRef<Function> func_type;

	FunctionDefinition* specialised_f;
};


class ArrayFoldBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		suppose T = array_elem_type
		
		then fold is

		fold(function<State, T, State> f, array<T> array, State initial_state) State

		Where f is
		def f(State current_state, T array_element) : State

		and returns the new state.


		Fold is equivalent to:

		State state = initial_state;
		for each element elem in array:
		{
			state = f(state, elem);
		}
	*/
	ArrayFoldBuiltInFunc(const VRef<Function>& func_type_, const VRef<ArrayType>& array_type_, const TypeVRef& state_type_);

	// Specialise for a particular first argument.
	void specialiseForFunctionArg(FunctionDefinition* f);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<Function> func_type;
	VRef<ArrayType> array_type;
	TypeVRef state_type;

	FunctionDefinition* specialised_f;
};


// Get the i-th element from an array.
class ArraySubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArraySubscriptBuiltInFunc(const VRef<ArrayType>& array_type, const TypeVRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	VRef<ArrayType> array_type;
	TypeVRef index_type;
};


// Get the i-th element from a varray.
class VArraySubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VArraySubscriptBuiltInFunc(const VRef<VArrayType>& array_type, const TypeVRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	VRef<VArrayType> array_type;
	TypeVRef index_type;
};


// Make a varray of count elements.
class MakeVArrayBuiltInFunc : public BuiltInFunctionImpl
{
public:
	MakeVArrayBuiltInFunc(const VRef<VArrayType>& array_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<VArrayType> array_type;
};


// Get the i-th element from a vector.
class VectorSubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorSubscriptBuiltInFunc(const VRef<VectorType>& vec_type, const TypeVRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	VRef<VectorType> vec_type;
	TypeVRef index_type;
};


// Check if the index is in bounds for this array.
class ArrayInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArrayInBoundsBuiltInFunc(const VRef<ArrayType>& array_type, const TypeVRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<ArrayType> array_type;
	TypeVRef index_type;
};


// Check if the index is in bounds for this vector.
class VectorInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorInBoundsBuiltInFunc(const VRef<VectorType>& vector_type, const TypeVRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<VectorType> vector_type;
	TypeVRef index_type;
};


class IterateBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		
	iterate(function<State, int, tuple<State, bool>> f, State initial_state) State

	or a different version that takes an additional LoopInvariantData argument(s):

	iterate(function<State, int, LoopInvariantData0, LoopInvariantData1, ..., LoopInvariantDataN, tuple<State, bool>> f, State initial_state, LoopInvariantData0, LoopInvariantData1, ..., LoopInvariantDataN) State


	Where f is
	def f(State current_state, int iteration) : tuple<State, bool>

	and returns the new state, and if iteration should continue.


	equivalent to

	State state = initial_state;
	iteration = 0;
	while(1)
	{
		res = f(state, iteration);
		if(res.second == false)
			return res.first;
		iteration++;
		state = res.first;
	}
	

	*/
	IterateBuiltInFunc(const VRef<Function>& func_type_, const TypeVRef& state_type_, const std::vector<TypeVRef>& invariant_data_types);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	const std::string emitOpenCLForFunctionArg(EmitOpenCLCodeParams& params,
		const FunctionDefinition* f, // arg 0
		const std::vector<Reference<ASTNode> >& argument_expressions
	);

private:
	VRef<Function> func_type;
	TypeVRef state_type;
	std::vector<TypeVRef> invariant_data_types; // may be empty.
};


// Take the dot product of two vectors.
class DotProductBuiltInFunc : public BuiltInFunctionImpl
{
public:
	DotProductBuiltInFunc(const VRef<VectorType>& vector_type_, int num_components_ = -1) :
		BuiltInFunctionImpl(BuiltInType_DotProductBuiltInFunc), vector_type(vector_type_), 
		num_components(num_components_)
	{}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<VectorType> vector_type;
	int num_components; // -1 if default
};


// Element-wise min of two vectors
class VectorMinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMinBuiltInFunc(const VRef<VectorType>& vector_type_) : BuiltInFunctionImpl(BuiltInType_VectorMinBuiltInFunc), vector_type(vector_type_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<VectorType> vector_type;
};


// Element-wise max of two vectors
class VectorMaxBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMaxBuiltInFunc(const VRef<VectorType>& vector_type_) : BuiltInFunctionImpl(BuiltInType_VectorMaxBuiltInFunc), vector_type(vector_type_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	VRef<VectorType> vector_type;
};


// Shuffle(vector<T, m>, vector<int, n) -> vector<T, n>
class ShuffleBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ShuffleBuiltInFunc(const VRef<VectorType>& vector_type_, 
		//const std::vector<int>& shuffle_mask_
		const VRef<VectorType>& index_type_
		) : BuiltInFunctionImpl(BuiltInType_ShuffleBuiltInFunc), 
		vector_type(vector_type_), 
		//shuffle_mask(shuffle_mask_) 
		index_type(index_type_) 
	{}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	void setShuffleMask(const std::vector<int>& shuffle_mask);
private:
	VRef<VectorType> vector_type;
	VRef<VectorType> index_type;
	std::vector<int> shuffle_mask;
};


class PowBuiltInFunc : public BuiltInFunctionImpl
{
public:
	PowBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class SqrtBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SqrtBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class ExpBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ExpBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class LogBuiltInFunc : public BuiltInFunctionImpl
{
public:
	LogBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class SinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SinBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class CosBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CosBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class AbsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	AbsBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class FloorBuiltInFunc : public BuiltInFunctionImpl
{
public:
	FloorBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class CeilBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CeilBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class SignBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SignBuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


class TruncateToIntBuiltInFunc : public BuiltInFunctionImpl
{
public:
	TruncateToIntBuiltInFunc(const TypeVRef& type);

	static TypeVRef getReturnType(const TypeVRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


// Integer to float conversion
class ToFloatBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToFloatBuiltInFunc(const TypeVRef& type);

	static TypeVRef getReturnType(const TypeVRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


// Integer to double conversion
class ToDoubleBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToDoubleBuiltInFunc(const TypeVRef& type);

	static TypeVRef getReturnType(const TypeVRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


// int32 -> int64 conversion
class ToInt64BuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToInt64BuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


// int64 -> int32 conversion
class ToInt32BuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToInt32BuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeVRef type;
};


// Opaque/voidptr to int64 conversion
class VoidPtrToInt64BuiltInFunc : public BuiltInFunctionImpl
{
public:
	VoidPtrToInt64BuiltInFunc(const TypeVRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class LengthBuiltInFunc : public BuiltInFunctionImpl
{
public:
	LengthBuiltInFunc(const TypeVRef& type_);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	TypeVRef type;
};


class CompareEqualBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CompareEqualBuiltInFunc(const TypeVRef& arg_type_, bool is_compare_not_equal);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual void deadFunctionEliminationTraverse(TraversalPayload& payload) const;
	virtual void linkInCalledFunctions(TraversalPayload& payload) const;

	TypeVRef arg_type;
	bool is_compare_not_equal;
};


//class AllocateRefCountedStructure : public BuiltInFunctionImpl
//{
//public:
//	AllocateRefCountedStructure() {}
//
//	virtual ValueRef invoke(VMState& vmstate);
//	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
//private:
//};


}
