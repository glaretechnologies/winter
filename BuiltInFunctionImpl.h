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


class BuiltInFunctionImpl : public RefCounted
{
public:
	virtual ~BuiltInFunctionImpl(){}

	virtual ValueRef invoke(VMState& vmstate) = 0;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
	virtual bool callIsExpensive() const { return true; } // Should this call be considered expensive, when considering possible duplication during inlining?
};


typedef Reference<BuiltInFunctionImpl> BuiltInFunctionImplRef;


// Construct a struct value
class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(Reference<StructureType>& struct_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	Reference<StructureType> struct_type;
};


// Return a field value or pointer to a field from a structure.
class GetField : public BuiltInFunctionImpl
{
public:
	GetField(Reference<StructureType>& struct_type_, unsigned int index_) : struct_type(struct_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
//private:
	Reference<StructureType> struct_type;
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
	UpdateElementBuiltInFunc(const TypeRef& collection_type/*, const TypeRef& value_type*/);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	TypeRef collection_type; // Should be one of array, vector, or tuple
	//TypeRef value_type; // T
};


// Return a field value or pointer to a field from a tuple.
class GetTupleElementBuiltInFunc : public BuiltInFunctionImpl
{
public:
	GetTupleElementBuiltInFunc(const Reference<TupleType>& tuple_type_, unsigned int index_) : tuple_type(tuple_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }

	void setIndex(unsigned int i) { index = i; }
//private:
	Reference<TupleType> tuple_type;
	unsigned int index;
};


// Return an element with given fixed index from a vector.
class GetVectorElement : public BuiltInFunctionImpl
{
public:
	GetVectorElement(Reference<VectorType>& vector_type_, unsigned int index_) : vector_type(vector_type_), index(index_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	Reference<VectorType> vector_type;
	unsigned int index;
};


class ArrayMapBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArrayMapBuiltInFunc(const Reference<ArrayType>& from_type_, const Reference<Function>& func_type_) : from_type(from_type_), func_type(func_type_), specialised_f(NULL) {}

	/*
	map(function<T, R>, array<T, N>) array<R, N>

	*/
	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	// Specialise for a particular first argument.
	void specialiseForFunctionArg(FunctionDefinition* f);
private:
	llvm::Value* insertWorkFunction(EmitLLVMCodeParams& params) const;
	Reference<ArrayType> from_type; // from array type
	Reference<Function> func_type;

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
	ArrayFoldBuiltInFunc(const Reference<Function>& func_type_, const Reference<ArrayType>& array_type_, const TypeRef& state_type_);

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


// Get the i-th element from an array.
class ArraySubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArraySubscriptBuiltInFunc(const Reference<ArrayType>& array_type, const TypeRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	Reference<ArrayType> array_type;
	TypeRef index_type;
};


// Get the i-th element from a varray.
class VArraySubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VArraySubscriptBuiltInFunc(const Reference<VArrayType>& array_type, const TypeRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	Reference<VArrayType> array_type;
	TypeRef index_type;
};


// Make a varray of count elements.
class MakeVArrayBuiltInFunc : public BuiltInFunctionImpl
{
public:
	MakeVArrayBuiltInFunc(const Reference<VArrayType>& array_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VArrayType> array_type;
};


// Get the i-th element from a vector.
class VectorSubscriptBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorSubscriptBuiltInFunc(const Reference<VectorType>& vec_type, const TypeRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
	virtual bool callIsExpensive() const { return false; }
private:
	Reference<VectorType> vec_type;
	TypeRef index_type;
};


// Check if the index is in bounds for this array.
class ArrayInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ArrayInBoundsBuiltInFunc(const Reference<ArrayType>& array_type, const TypeRef& index_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<ArrayType> array_type;
	TypeRef index_type;
};


// Check if the index is in bounds for this vector.
class VectorInBoundsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorInBoundsBuiltInFunc(const Reference<VectorType>& vector_type, const TypeRef& index_type);

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
	IterateBuiltInFunc(const Reference<Function>& func_type_, const TypeRef& state_type_, const std::vector<TypeRef>& invariant_data_types);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	const std::string emitOpenCLForFunctionArg(EmitOpenCLCodeParams& params,
		const FunctionDefinition* f, // arg 0
		const std::vector<Reference<ASTNode> >& argument_expressions
	);

private:
	Reference<Function> func_type;
	TypeRef state_type;
	std::vector<TypeRef> invariant_data_types; // may be empty.
};


// Take the dot product of two vectors.
class DotProductBuiltInFunc : public BuiltInFunctionImpl
{
public:
	DotProductBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


// Element-wise min of two vectors
class VectorMinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMinBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


// Element-wise max of two vectors
class VectorMaxBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMaxBuiltInFunc(const Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}

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

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class SqrtBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SqrtBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class ExpBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ExpBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class LogBuiltInFunc : public BuiltInFunctionImpl
{
public:
	LogBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class SinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SinBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class CosBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CosBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class AbsBuiltInFunc : public BuiltInFunctionImpl
{
public:
	AbsBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class FloorBuiltInFunc : public BuiltInFunctionImpl
{
public:
	FloorBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class CeilBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CeilBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class SignBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SignBuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


class TruncateToIntBuiltInFunc : public BuiltInFunctionImpl
{
public:
	TruncateToIntBuiltInFunc(const TypeRef& type);

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

	static TypeRef getReturnType(const TypeRef& arg_type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


// int32 -> int64 conversion
class ToInt64BuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToInt64BuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


// int64 -> int32 conversion
class ToInt32BuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToInt32BuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef type;
};


// Opaque/voidptr to int64 conversion
class VoidPtrToInt64BuiltInFunc : public BuiltInFunctionImpl
{
public:
	VoidPtrToInt64BuiltInFunc(const TypeRef& type);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
};


class LengthBuiltInFunc : public BuiltInFunctionImpl
{
public:
	LengthBuiltInFunc(const TypeRef& type_);

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

	TypeRef type;
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
