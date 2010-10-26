#pragma once


#include "wnt_Type.h"


namespace Winter
{


class Value;
class VMState;
class EmitLLVMCodeParams;


class BuiltInFunctionImpl
{
public:
	virtual ~BuiltInFunctionImpl(){}

	virtual Value* invoke(VMState& vmstate) = 0;
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const = 0;
};


class Constructor : public BuiltInFunctionImpl
{
public:
	Constructor(Reference<StructureType>& struct_type);
	virtual ~Constructor(){}

	virtual Value* invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;

private:
	Reference<StructureType> struct_type;
};


class GetField : public BuiltInFunctionImpl
{
public:
	GetField(Reference<StructureType>& struct_type_, unsigned int index_) : struct_type(struct_type_), index(index_) {}
	virtual ~GetField(){}

	virtual Value* invoke(VMState& vmstate);
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

	virtual Value* invoke(VMState& vmstate);
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

	virtual Value* invoke(VMState& vmstate);
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

	virtual Value* invoke(VMState& vmstate);
	virtual llvm::Value* emitLLVMCode(EmitLLVMCodeParams& params) const;
private:
	TypeRef T;
};

}
