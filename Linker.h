//Copyright 2009 Nicholas Chapman
#pragma once


#include "wnt_FunctionSignature.h"
#include "wnt_ASTNode.h"
#include <map>
#include <set>
namespace llvm { class Module; }


namespace Winter
{


class ExternalFunction;


class Linker
{
public:
	Linker();
	~Linker();


	void addFunctions(BufferRoot& root);
	void addExternalFunctions(std::vector<ExternalFunction>& f);

	//void linkFunctions(BufferRoot& root);

	FunctionDefinitionRef findMatchingFunction(const FunctionSignature& sig);

	void buildLLVMCode(llvm::Module* module);

	vector<FunctionDefinitionRef> concrete_funcs;
private:
	FunctionDefinitionRef makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings);

	typedef std::map<FunctionSignature, Reference<FunctionDefinition> > FuncMapType;
	FuncMapType functions;

	//FuncMapType external_functions;

};


}
