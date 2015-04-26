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


class BuiltInFunctionImpl : public RefCounted
{
public:
	virtual ~BuiltInFunctionImpl(){}

	virtual ValueRef invoke(VMState& vmstate) = 0;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
	//virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
};


typedef Reference<BuiltInFunctionImpl> BuiltInFunctionImplRef;


class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(Reference<StructureType>& struct_type);
	virtual ~Constructor(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	//virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;

private:
	Reference<StructureType> struct_type;
};


class GetField : public BuiltInFunctionImpl
{
public:
	GetField(Reference<StructureType>& struct_type_, unsigned int index_) : struct_type(struct_type_), index(index_) {}
	virtual ~GetField(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
//private:
	Reference<StructureType> struct_type;
	unsigned int index;
};


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
	UpdateElementBuiltInFunc(const TypeRef& collection_type/*, const TypeRef& value_type*/);
	virtual ~UpdateElementBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	TypeRef collection_type; // Should be one of array, vector, or tuple
	//TypeRef value_type; // T
};


class GetTupleElementBuiltInFunc : public BuiltInFunctionImpl
{
public:
	GetTupleElementBuiltInFunc(const Reference<TupleType>& tuple_type_, unsigned int index_) : tuple_type(tuple_type_), index(index_) {}
	virtual ~GetTupleElementBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	void setIndex(unsigned int i) { index = i; }
//private:
	Reference<TupleType> tuple_type;
	unsigned int index;
};


class GetVectorElement : public BuiltInFunctionImpl
{
public:
	GetVectorElement(Reference<VectorType>& vector_type_, unsigned int index_) : vector_type(vector_type_), index(index_) {}
	virtual ~GetVectorElement(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
	unsigned int index;
};


class ArrayMapBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArrayMapBuiltInFunc(const Reference<ArrayType>& from_type_, const Reference<Function>& func_type_) : from_type(from_type_), func_type(func_type_) {}
	virtual ~ArrayMapBuiltInFunc(){}

	/*
	map(function<T, R>, array<T, N>) array<R, N>

	*/
	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<ArrayType> from_type; // from array type
	Reference<Function> func_type;
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
	ArrayFoldBuiltInFunc(const Reference<Function>& func_type_, const Reference<ArrayType>& array_type_, const TypeRef& state_type_);
	virtual ~ArrayFoldBuiltInFunc(){}

	// Specialise for a particular first argument.
	void specialiseForFunctionArg(FunctionDefinition* f);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<Function> func_type;
	Reference<ArrayType> array_type;
	TypeRef state_type;

	FunctionDefinition* specialised_f;
};


class ArraySubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		Get the i-th element from the array.
	*/
	ArraySubscriptBuiltInFunc(const Reference<ArrayType>& array_type, const TypeRef& index_type);
	virtual ~ArraySubscriptBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<ArrayType> array_type;
	TypeRef index_type;
};


class VectorSubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		Get the i-th element from the vector.
	*/
	VectorSubscriptBuiltInFunc(const Reference<VectorType>& vec_type, const TypeRef& index_type);
	virtual ~VectorSubscriptBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vec_type;
	TypeRef index_type;
};


class ArrayInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		Check if the index is in bounds for this array.
	*/
	ArrayInBoundsBuiltInFunc(const Reference<ArrayType>& array_type, const TypeRef& index_type);
	virtual ~ArrayInBoundsBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<ArrayType> array_type;
	TypeRef index_type;
};


class VectorInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		Check if the index is in bounds for this vector.
	*/
	VectorInBoundsBuiltInFunc(const Reference<VectorType>& vector_type, const TypeRef& index_type);
	virtual ~VectorInBoundsBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
	TypeRef index_type;
};


class IterateBuiltInFunc : public BuiltInFunctionImpl
{
public:
	/*
		
	iterate(function<State, int, tuple<State, bool>> f, State initial_state) State

	or a different version that takes an additional LoopInvariantData arg:

	iterate(function<State, int, LoopInvariantData, tuple<State, bool>> f, State initial_state, LoopInvariantData data) State


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
	IterateBuiltInFunc(const Reference<Function>& func_type_, const TypeRef& state_type_, const TypeRef& invariant_data_type);
	virtual ~IterateBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<Function> func_type;
	TypeRef state_type;
	TypeRef invariant_data_type; // may be NULL
};


class DotProductBuiltInFunc : public BuiltInFunctionImpl
{
public:
	DotProductBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~DotProductBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


class VectorMinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMinBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~VectorMinBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


class VectorMaxBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMaxBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~VectorMaxBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


// Shuffle(vector<T, m>, vector<int, n) -> vector<T, n>
class ShuffleBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ShuffleBuiltInFunc(const Reference<VectorType>& vector_type_, 
		//const std::vector<int>& shuffle_mask_
		const Reference<VectorType>& index_type_
		) : vector_type(vector_type_), 
		//shuffle_mask(shuffle_mask_) 
		index_type(index_type_) 
	{}
	virtual ~ShuffleBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	void setShuffleMask(const std::vector<int>& shuffle_mask);
private:
	Reference<VectorType> vector_type;
	Reference<VectorType> index_type;
	std::vector<int> shuffle_mask;
};


class PowBuiltInFunc : public BuiltInFunctionImpl
{
public:
	PowBuiltInFunc(const TypeRef& type);
	virtual ~PowBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class SqrtBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SqrtBuiltInFunc(const TypeRef& type);
	virtual ~SqrtBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class ExpBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ExpBuiltInFunc(const TypeRef& type);
	virtual ~ExpBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class LogBuiltInFunc : public BuiltInFunctionImpl
{
public:
	LogBuiltInFunc(const TypeRef& type);
	virtual ~LogBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class SinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SinBuiltInFunc(const TypeRef& type);
	virtual ~SinBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class CosBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CosBuiltInFunc(const TypeRef& type);
	virtual ~CosBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class AbsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	AbsBuiltInFunc(const TypeRef& type);
	virtual ~AbsBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class FloorBuiltInFunc : public BuiltInFunctionImpl
{
public:
	FloorBuiltInFunc(const TypeRef& type);
	virtual ~FloorBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class CeilBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CeilBuiltInFunc(const TypeRef& type);
	virtual ~CeilBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class TruncateToIntBuiltInFunc : public BuiltInFunctionImpl
{
public:
	TruncateToIntBuiltInFunc(const TypeRef& type);
	virtual ~TruncateToIntBuiltInFunc(){}

	static TypeRef getReturnType(const TypeRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


// Integer to float conversion
class ToFloatBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToFloatBuiltInFunc(const TypeRef& type);
	virtual ~ToFloatBuiltInFunc(){}

	static TypeRef getReturnType(const TypeRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


//class AllocateRefCountedStructure : public BuiltInFunctionImpl
//{
//public:
//	AllocateRefCountedStructure() {}
//	virtual ~AllocateRefCountedStructure(){}
//
//	virtual ValueRef invoke(VMState& vmstate);
//	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
//private:
//};


}
