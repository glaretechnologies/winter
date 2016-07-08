/*=====================================================================
Value.h
-------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "BaseException.h"
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
	enum ValueType
	{
		ValueType_Int,
		ValueType_Float,
		ValueType_Double,
		ValueType_Bool,
		ValueType_String,
		ValueType_Char,
		ValueType_Map,
		ValueType_Structure,
		ValueType_Function,
		ValueType_Array,
		ValueType_VArray,
		ValueType_Vector,
		ValueType_Tuple,
		ValueType_VoidPtr
	};

	Value(ValueType value_type_) : value_type(value_type_) {}
	virtual ~Value() {}
	
	virtual Value* clone() const = 0;
	
	virtual const std::string toString() const { return "Value"; }

	ValueType valueType() const { return value_type; }

protected:
	ValueType value_type;
};


typedef Reference<Value> ValueRef;


class IntValue : public Value
{
public:
	IntValue(int64 v, bool is_signed_) : Value(ValueType_Int), value(v), is_signed(is_signed_) {}
	~IntValue() {}
	virtual Value* clone() const { return new IntValue(value, is_signed); }
	
	int64 value;
	bool is_signed;
};


class FloatValue : public Value
{
public:
	FloatValue(float v) : Value(ValueType_Float), value(v) {}
	virtual Value* clone() const { return new FloatValue(value); }
	virtual const std::string toString() const;
	float value;
};
typedef Reference<FloatValue> FloatValueRef;


class DoubleValue : public Value
{
public:
	DoubleValue(double v) : Value(ValueType_Double), value(v) {}
	virtual Value* clone() const { return new DoubleValue(value); }
	virtual const std::string toString() const;
	double value;
};
typedef Reference<DoubleValue> DoubleValueRef;


class BoolValue : public Value
{
public:
	BoolValue(bool v) : Value(ValueType_Bool), value(v) {}
	virtual Value* clone() const { return new BoolValue(value); }
	bool value;
};


class StringValue : public Value
{
public:
	StringValue(const std::string& v) : Value(ValueType_String), value(v) {}
	virtual Value* clone() const { return new StringValue(value); }

	std::string value;
};


class CharValue : public Value
{
public:
	CharValue(const std::string& v) : Value(ValueType_Char), value(v) {}
	virtual Value* clone() const { return new CharValue(value); }

	std::string value;
};


class MapValue : public Value
{
public:
	MapValue(const std::map<ValueRef, ValueRef>& v) : Value(ValueType_Map), value(v) {}
	virtual Value* clone() const { return new MapValue(value); }
	std::map<ValueRef, ValueRef> value;
};


class StructureValue : public Value
{
public:
	StructureValue(const std::vector<ValueRef>& fields_) : Value(ValueType_Structure), fields(fields_) {}
	~StructureValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;

	std::vector<ValueRef> fields;
};


typedef Reference<StructureValue> StructureValueRef;


class FunctionValue : public Value
{
public:
	FunctionValue(FunctionDefinition* func_def_, const Reference<StructureValue>& values_) : Value(ValueType_Function), func_def(func_def_), captured_vars(values_) {}
	~FunctionValue() {}
	virtual Value* clone() const { return new FunctionValue(func_def, captured_vars); }

	virtual const std::string toString() const { return "Function"; }

	FunctionDefinition* func_def;

	Reference<StructureValue> captured_vars;
};


class ArrayValue : public Value
{
public:
	ArrayValue() : Value(ValueType_Array) {}
	ArrayValue(const std::vector<ValueRef>& e_) : Value(ValueType_Array), e(e_) {}
	~ArrayValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;

	std::vector<ValueRef> e;
};
typedef Reference<ArrayValue> ArrayValueRef;


class VArrayValue : public Value
{
public:
	VArrayValue() : Value(ValueType_VArray) {}
	VArrayValue(const std::vector<ValueRef>& e_) : Value(ValueType_VArray), e(e_) {}
	~VArrayValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;

	std::vector<ValueRef> e;
};
typedef Reference<VArrayValue> VArrayValueRef;


class VectorValue : public Value
{
public:
	VectorValue() : Value(ValueType_Vector) {}
	VectorValue(const std::vector<ValueRef>& e_) : Value(ValueType_Vector), e(e_) {}
	~VectorValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;

	std::vector<ValueRef> e;
};


class TupleValue : public Value
{
public:
	TupleValue() : Value(ValueType_Tuple) {}
	TupleValue(const std::vector<ValueRef>& e_) : Value(ValueType_Tuple), e(e_) {}
	~TupleValue();
	virtual Value* clone() const;
	virtual const std::string toString() const;

	std::vector<ValueRef> e;
};


class VoidPtrValue : public Value
{
public:
	VoidPtrValue(void* v) : Value(ValueType_VoidPtr), value(v) {}
	virtual Value* clone() const { return new VoidPtrValue(value); }
	virtual const std::string toString() const;

	void* value;
};


template <class T> inline Value::ValueType getValueTypeForClass();
template <> inline Value::ValueType getValueTypeForClass<IntValue>() { return Value::ValueType_Int; }
template <> inline Value::ValueType getValueTypeForClass<FloatValue>() { return Value::ValueType_Float; }
template <> inline Value::ValueType getValueTypeForClass<DoubleValue>() { return Value::ValueType_Double; }
template <> inline Value::ValueType getValueTypeForClass<BoolValue>() { return Value::ValueType_Bool; }
template <> inline Value::ValueType getValueTypeForClass<StringValue>() { return Value::ValueType_String; }
template <> inline Value::ValueType getValueTypeForClass<CharValue>() { return Value::ValueType_Char; }
template <> inline Value::ValueType getValueTypeForClass<MapValue>() { return Value::ValueType_Map; }
template <> inline Value::ValueType getValueTypeForClass<StructureValue>() { return Value::ValueType_Structure; }
template <> inline Value::ValueType getValueTypeForClass<FunctionValue>() { return Value::ValueType_Function; }
template <> inline Value::ValueType getValueTypeForClass<ArrayValue>() { return Value::ValueType_Array; }
template <> inline Value::ValueType getValueTypeForClass<VArrayValue>() { return Value::ValueType_VArray; }
template <> inline Value::ValueType getValueTypeForClass<VectorValue>() { return Value::ValueType_Vector; }
template <> inline Value::ValueType getValueTypeForClass<TupleValue>() { return Value::ValueType_Tuple; }
template <> inline Value::ValueType getValueTypeForClass<VoidPtrValue>() { return Value::ValueType_VoidPtr; }
template <> inline Value::ValueType getValueTypeForClass<const IntValue>() { return Value::ValueType_Int; }
template <> inline Value::ValueType getValueTypeForClass<const FloatValue>() { return Value::ValueType_Float; }
template <> inline Value::ValueType getValueTypeForClass<const DoubleValue>() { return Value::ValueType_Double; }
template <> inline Value::ValueType getValueTypeForClass<const BoolValue>() { return Value::ValueType_Bool; }
template <> inline Value::ValueType getValueTypeForClass<const StringValue>() { return Value::ValueType_String; }
template <> inline Value::ValueType getValueTypeForClass<const CharValue>() { return Value::ValueType_Char; }
template <> inline Value::ValueType getValueTypeForClass<const MapValue>() { return Value::ValueType_Map; }
template <> inline Value::ValueType getValueTypeForClass<const StructureValue>() { return Value::ValueType_Structure; }
template <> inline Value::ValueType getValueTypeForClass<const FunctionValue>() { return Value::ValueType_Function; }
template <> inline Value::ValueType getValueTypeForClass<const ArrayValue>() { return Value::ValueType_Array; }
template <> inline Value::ValueType getValueTypeForClass<const VArrayValue>() { return Value::ValueType_VArray; }
template <> inline Value::ValueType getValueTypeForClass<const VectorValue>() { return Value::ValueType_Vector; }
template <> inline Value::ValueType getValueTypeForClass<const TupleValue>() { return Value::ValueType_Tuple; }
template <> inline Value::ValueType getValueTypeForClass<const VoidPtrValue>() { return Value::ValueType_VoidPtr; }

// Downcast from a value object, to a value object subclass.
// Check the type is correct with valueType().  If it's not, throw an exception.
template <class T> 
const T* checkedCast(const ValueRef& v)
{
	if(v->valueType() == getValueTypeForClass<T>())
		return static_cast<const T*>(v.getPointer());
	else
		throw BaseException("Type error.");
}


} // end namespace Winter

