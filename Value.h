//Copyright 2009 Nicholas Chapman
#pragma once


#include "Type.h"
#include <string>
#include <map>
using std::string;


namespace Winter
{


class Value
{
public:
	Value() : refcount(1) {}
	virtual ~Value() {}
	//TypeRef type;
	int refcount;
};


class IntValue : public Value
{
public:
	IntValue(int v) : value(v) {}
	int value;
};


class FloatValue : public Value
{
public:
	FloatValue(float v) : value(v) {}
	float value;
};


class BoolValue : public Value
{
public:
	BoolValue(bool v) : value(v) {}
	bool value;
};


class StringValue : public Value
{
public:
	StringValue(const std::string& v) : value(v) {}
	string value;
};


class MapValue : public Value
{
public:
	MapValue(const std::map<Value*, Value*>& v) : value(v) {}
	std::map<Value*, Value*> value;
};


}
