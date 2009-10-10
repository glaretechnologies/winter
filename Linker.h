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


	void addFunctions(BufferRoot& root);

	//void linkFunctions(BufferRoot& root);

	FunctionDefinitionRef findMatchingFunction(const FunctionSignature& sig);

	vector<FunctionDefinitionRef> concrete_funcs;
private:
	FunctionDefinitionRef makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings);

	typedef std::map<FunctionSignature, Reference<FunctionDefinition> > FuncMapType;
	FuncMapType functions;

};


}
