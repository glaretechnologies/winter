//Copyright 2009 Nicholas Chapman
#pragma once


#include "wnt_Type.h"
#include <utils/RefCounted.h>
#include <string>
#include <vector>
#include <map>


namespace Winter
{
class FunctionDefinition;
class EmitLLVMCodeParams;


class Value : public RefCounted
{
public:
	Value() {} // : refcount(1) {}
	virtual ~Value() {}
	//TypeRef type;
	//int refcount;
	virtual Value* clone() const = 0;
	
	virtual const std::string toString() const { return "Value"; }

	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const = 0;
};


typedef Reference<Value> ValueRef;


class IntValue : public Value
{
public:
	IntValue(int v) : value(v) { /*std::cout << "IntValue(), this=" << this << ", value = " << value << "\n";*/ }
	~IntValue() { /*std::cout << "~IntValue(), this=" << this << ", value = " << value << "\n";*/ }
	virtual Value* clone() const { return new IntValue(value); }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;
	int value;
};


class FloatValue : public Value
{
public:
	FloatValue(float v) : value(v) {}
	virtual Value* clone() const { return new FloatValue(value); }
	virtual const std::string toString() const;
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;
	float value;
};


class BoolValue : public Value
{
public:
	BoolValue(bool v) : value(v) {}
	virtual Value* clone() const { return new BoolValue(value); }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;
	bool value;
};


//class StringRep;
class StringValue : public Value
{
public:
	StringValue(const std::string& v) : value(v) {}
	virtual Value* clone() const { return new StringValue(value); }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	std::string value;
	//StringRep* string_rep;
};


class CharValue : public Value
{
public:
	CharValue(const std::string& v) : value(v) {}
	virtual Value* clone() const { return new CharValue(value); }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	std::string value;
};


class MapValue : public Value
{
public:
	MapValue(const std::map<ValueRef, ValueRef>& v) : value(v) {}
	virtual Value* clone() const { return new MapValue(value); }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;
	std::map<ValueRef, ValueRef> value;
};


class StructureValue : public Value
{
public:
	StructureValue(const std::vector<ValueRef>& fields_) : fields(fields_) {}
	~StructureValue();
	virtual Value* clone() const;
	virtual const std::string toString() const { return "struct"; }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	std::vector<ValueRef> fields;
};


typedef Reference<StructureValue> StructureValueRef;


class FunctionValue : public Value
{
public:
	FunctionValue(FunctionDefinition* func_def_, const Reference<StructureValue>& values_) : func_def(func_def_), captured_vars(values_) { /* std::cout << "FunctionValue(), this=" << this << "\n";*/ }
	~FunctionValue() { /* std::cout << "~FunctionValue(), this=" << this << "\n"; */ }
	virtual Value* clone() const { return new FunctionValue(func_def, captured_vars); }

	virtual const std::string toString() const { return "Function"; }
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	FunctionDefinition* func_def;

	// vector<ValueRef> values;
	Reference<StructureValue> captured_vars;
};


class ArrayValue : public Value
{
public:
	ArrayValue(){}
	ArrayValue(const std::vector<ValueRef>& e_) : e(e_) {}
	~ArrayValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	std::vector<ValueRef> e;
};


class VectorValue : public Value
{
public:
	VectorValue(){}
	VectorValue(const std::vector<ValueRef>& e_) : e(e_) {}
	~VectorValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	std::vector<ValueRef> e;
};


class VoidPtrValue : public Value
{
public:
	VoidPtrValue(void* v) : value(v) {}
	virtual Value* clone() const { return new VoidPtrValue(value); }
	virtual const std::string toString() const;
	virtual llvm::Constant* getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const;

	void* value;
};


}
