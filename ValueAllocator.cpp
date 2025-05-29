/*=====================================================================
ValueAllocator.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "ValueAllocator.h"


#include "Value.h"


namespace Winter
{


ValueAllocator::ValueAllocator()
:	float_allocator(/*ob alloc size=*/sizeof(Winter::FloatValue), 16, /*block capacity=*/64)
{
}


FloatValue* ValueAllocator::allocFloatValue(float v)
{
	glare::FastPoolAllocator::AllocResult alloc_res = float_allocator.alloc();

	Winter::FloatValue* val = new(alloc_res.ptr) FloatValue(v); // placement new
	val->value_allocator = this;
	val->allocation_index = alloc_res.index;
	return val;
}


void ValueAllocator::freeFloatValue(FloatValue* value)
{
	assert(value->value_allocator == this);

	const int idx = value->allocation_index;

	// destroy object
	value->~FloatValue();

	// Free mem
	float_allocator.free(idx);
}


} // end namespace Winter
