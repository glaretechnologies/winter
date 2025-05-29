/*=====================================================================
VMState.h
---------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "Value.h"
#include "ValueAllocator.h"
#include <utils/SmallVector.h>
#include <ostream>


namespace Winter
{


class Value;


class VMState
{
public:
	VMState(Reference<ValueAllocator> value_alloc) : trace(false), ostream(NULL), value_allocator(value_alloc ? value_alloc : new ValueAllocator()) {}

	SmallVector<ValueRef, 16> argument_stack;

	// Index at which the function arguments start.  Deepest function is latest on the stack.
	SmallVector<size_t, 8> func_args_start;

	//std::vector<Value*> working_stack;
	//std::vector<ValueRef> let_stack;

	//std::vector<size_t> let_stack_start;
	//Value* return_register;

	const std::string indent() const
	{
		std::string s;
		for(unsigned int i=0; i<func_args_start.size(); ++i)
			s += "  ";
		return s;
	}

	bool trace; // If true, do a verbose trace of the execution, printing out values etc..
	std::ostream* ostream; // Stream to write the trace to.

	Reference<ValueAllocator> value_allocator;
};


};
