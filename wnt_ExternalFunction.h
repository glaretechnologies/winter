/*=====================================================================
wnt_ExternalFunction.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 27 16:44:39 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
#include "wnt_FunctionSignature.h"
#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include "Value.h"


namespace Winter
{


/*=====================================================================
wnt_ExternalFunction
-------------------

=====================================================================*/
class ExternalFunction : public RefCounted
{
public:
	typedef ValueRef (* INTERPRETED_FUNC)(const std::vector<ValueRef>& arg_values);

	static size_t unknownTimeBound() { return std::numeric_limits<size_t>::max(); }
	static size_t unknownSpaceBound() { return std::numeric_limits<size_t>::max(); }

	// Interpreted function must be set, unless the function type is one of
	//
	// (float) -> float
	// (float, float) -> float
	// (double) -> double
	// (double, double) -> double
	// (float) -> bool
	// (double) -> bool
	//
	// in which case NULL can be passed instead.
	ExternalFunction(void* func_, INTERPRETED_FUNC interpreted_func_, const FunctionSignature& sig_, const TypeVRef& return_type_,
		size_t time_bound_ = unknownTimeBound(),
		size_t stack_size_bound_ = unknownSpaceBound(),
		size_t heap_size_bound_ = unknownSpaceBound()
	)
	:	func(func_),
		interpreted_func(interpreted_func_),
		sig(sig_),
		return_type(return_type_),
		has_side_effects(false),
		is_allocation_function(false),
		time_bound(time_bound_),
		stack_size_bound(stack_size_bound_),
		heap_size_bound(heap_size_bound_)
	{}

	~ExternalFunction();

	void* func;
	ValueRef (* interpreted_func)(const std::vector<ValueRef>& arg_values);

	FunctionSignature sig;
	TypeVRef return_type;

	bool has_side_effects; // Such as the function freeString()
	bool is_allocation_function;

	size_t time_bound, stack_size_bound, heap_size_bound;
private:

};


typedef Reference<ExternalFunction> ExternalFunctionRef;


}
