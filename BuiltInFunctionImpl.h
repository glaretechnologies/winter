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
};


class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(Reference<StructureType>& struct_type);
	virtual ~Constructor(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

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
	ArrayMapBuiltInFunc(TypeRef& from_type_, Reference<Function>& func_type_) : from_type(from_type_), func_type(func_type_) {}
	virtual ~ArrayMapBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef from_type;
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


class IfBuiltInFunc : public BuiltInFunctionImpl
{
public:
	IfBuiltInFunc(TypeRef& T_) : T(T_) {}
	virtual ~IfBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef T;
};


class DotProductBuiltInFunc : public BuiltInFunctionImpl
{
public:
	DotProductBuiltInFunc(Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~DotProductBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


class VectorMinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMinBuiltInFunc(Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~VectorMinBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


class VectorMaxBuiltInFunc : public BuiltInFunctionImpl
{
public:
	VectorMaxBuiltInFunc(Reference<VectorType>& vector_type_) : vector_type(vector_type_) {}
	virtual ~VectorMaxBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	Reference<VectorType> vector_type;
};


class PowBuiltInFunc : public BuiltInFunctionImpl
{
public:
	PowBuiltInFunc() {}
	virtual ~PowBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
};


class SqrtBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SqrtBuiltInFunc() {}
	virtual ~SqrtBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
};


class SinBuiltInFunc : public BuiltInFunctionImpl
{
public:
	SinBuiltInFunc() {}
	virtual ~SinBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
};


class CosBuiltInFunc : public BuiltInFunctionImpl
{
public:
	CosBuiltInFunc() {}
	virtual ~CosBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
};


class TruncateToIntBuiltInFunc : public BuiltInFunctionImpl
{
public:
	TruncateToIntBuiltInFunc() {}
	virtual ~TruncateToIntBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
};


class ToFloatBuiltInFunc : public BuiltInFunctionImpl
{
public:
	ToFloatBuiltInFunc() {}
	virtual ~ToFloatBuiltInFunc(){}

	virtual ValueRef invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
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
