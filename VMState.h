//Copyright 2009 Nicholas Chapman
#pragma once


namespace Winter
{


class Value;


class VMState
{
public:
	std::vector<Value*> argument_stack;

	// Index at which the function arguments start.  Deepest function is latest on the stack.
	std::vector<size_t> func_args_start;

	//std::vector<Value*> working_stack;
	std::vector<Value*> let_stack;

	std::vector<size_t> let_stack_start;
	//Value* return_register;
};


};
