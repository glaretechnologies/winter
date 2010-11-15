//Copyright 2009 Nicholas Chapman
#pragma once


#include "Value.h"


namespace Winter
{


class Value;


class VMState
{
public:
	VMState(bool hidden_voidptr_arg_) : hidden_voidptr_arg(hidden_voidptr_arg_) {}

	std::vector<ValueRef> argument_stack;

	// Index at which the function arguments start.  Deepest function is latest on the stack.
	std::vector<size_t> func_args_start;

	//std::vector<Value*> working_stack;
	std::vector<ValueRef> let_stack;

	std::vector<size_t> let_stack_start;
	//Value* return_register;

	bool hidden_voidptr_arg;
};


};
