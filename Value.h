//Copyright 2009 Nicholas Chapman
#pragma once


#include "Type.h"
#include <string>
#include <map>
#include <iostream>
using std::string;


namespace Winter
{


class Value
{
public:
	Value() {} // : refcount(1) {}
	virtual ~Value() {}
	//TypeRef type;
	//int refcount;
	virtual Value* clone() const = 0;
};


class IntValue : public Value
{
public:
	IntValue(int v) : value(v) { std::cout << "IntValue(), this=" << this << ", value = " << value << "\n"; }
	~IntValue() { std::cout << "~IntValue(), this=" << this << ", value = " << value << "\n"; }
	virtual Value* clone() const { return new IntValue(value); }
	int value;
};


class FloatValue : public Value
{
public:
	FloatValue(float v) : value(v) {}
	virtual Value* clone() const { return new FloatValue(value); }
	float value;
};


class BoolValue : public Value
{
public:
	BoolValue(bool v) : value(v) {}
	virtual Value* clone() const { return new BoolValue(value); }
	bool value;
};


class StringValue : public Value
{
public:
	StringValue(const std::string& v) : value(v) {}
	virtual Value* clone() const { return new StringValue(value); }
	string value;
};


class MapValue : public Value
{
public:
	MapValue(const std::map<Value*, Value*>& v) : value(v) {}
	virtual Value* clone() const { return new MapValue(value); }
	std::map<Value*, Value*> value;
};


class FunctionValue : public Value
{
public:
	FunctionValue(FunctionDefinition* func_def_) : func_def(func_def_) { std::cout << "FunctionValue(), this=" << this << "\n"; }
	~FunctionValue() { std::cout << "~FunctionValue(), this=" << this << "\n"; }
	virtual Value* clone() const { return new FunctionValue(func_def); }

	FunctionDefinition* func_def;
};


}
