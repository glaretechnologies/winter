/*=====================================================================
ValueAllocator.h
----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "BaseException.h"
#include <utils/RefCounted.h>
#include <utils/Reference.h>
#include <utils/PoolAllocator.h>
#include <string>
#include <vector>
#include <map>


namespace Winter
{

class FloatValue;


class ValueAllocator : public RefCounted
{
public:
	ValueAllocator();

	FloatValue* allocFloatValue(float v);
	void freeFloatValue(FloatValue* value);

	glare::PoolAllocator float_allocator;
};

}
