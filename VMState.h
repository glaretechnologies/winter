//Copyright 2009 Nicholas Chapman
#pragma once


namespace Winter
{


class Value;


class VMState
{
public:
	std::vector<Value*> argument_stack;
	std::vector<Value*> working_stack;
	std::vector<Value*> let_stack;
	//Value* return_register;
};


};
