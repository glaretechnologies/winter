//Copyright 2009 Nicholas Chapman
#pragma once


#include "FunctionSignature.h"
#include "ASTNode.h"
#include <map>


namespace Winter
{


class Linker
{
public:
	Linker();
	~Linker();

	//void addFunction(

	typedef std::map<FunctionSignature, Reference<FunctionDefinition> > FuncMapType;
	FuncMapType functions;

	void addFunctions(BufferRoot& root);

	void linkFunctions(BufferRoot& root);
};


}
