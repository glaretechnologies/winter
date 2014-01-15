#pragma once


#include "wnt_Type.h"
#include "Value.h"
namespace llvm { class Value; }


namespace Winter
{


class Value;
class VMState;
class EmitLLVMCodeParams;


class BuiltInFunctionImpl
{
public:
	virtual ~BuiltInFunctionImpl(){}

	virtual ValueRef invoke(VMState& vmstate) = 0;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
	//virtual llvm::Value* getConstantLLVMValue(EmitLLVMCodeParams& params) const;
};


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
private:
	Reference<StructureType> struct_type;
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
		fold(function<T, T, T> func, array<T> array, T initial val) T

	*/
	ArrayFoldBuiltInFunc(TypeRef& T_) : T(T_) {}
	virtual ~ArrayFoldBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef T;
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


class IfBuiltInFunc : public BuiltInFunctionImpl
{
public:
	IfBuiltInFunc(const TypeRef& T_) : T(T_) {}
	virtual ~IfBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef T;
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
