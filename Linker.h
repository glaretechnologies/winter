//Copyright 2009 Nicholas Chapman
#pragma once


#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
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
	Linker(bool hidden_voidptr_arg, void* env);
	~Linker();


	//void addFunctions(BufferRoot& root);
	void addFunctions(const vector<FunctionDefinitionRef>& func_defs);
	void addExternalFunctions(std::vector<ExternalFunctionRef>& f);

	//void linkFunctions(BufferRoot& root);

	ExternalFunctionRef findMatchingExternalFunction(const FunctionSignature& sig);
	FunctionDefinitionRef findMatchingFunction(const FunctionSignature& sig);
	//FunctionDefinitionRef findMatchingFunctionByName(const std::string& name); // NOTE: rather unsafe

	void getFuncsWithMatchingName(const std::string& name, vector<FunctionDefinitionRef>& funcs_out);

	void buildLLVMCode(llvm::Module* module);

	vector<FunctionDefinitionRef> concrete_funcs;
private:
	void addFunction(const FunctionDefinitionRef& f);
	FunctionDefinitionRef makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings);

	typedef std::map<std::string, vector<Reference<FunctionDefinition> > > NameToFuncMapType;
	NameToFuncMapType name_to_functions_map;
	typedef std::map<FunctionSignature, Reference<FunctionDefinition> > SigToFuncMapType;
	SigToFuncMapType sig_to_function_map;

	typedef std::map<FunctionSignature, ExternalFunctionRef > ExternalFuncMapType;
	ExternalFuncMapType external_functions;

	bool hidden_voidptr_arg;
	void* env;
};


}
