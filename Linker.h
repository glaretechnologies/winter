//Copyright 2009 Nicholas Chapman
#pragma once


#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include <map>
#include <set>
namespace llvm { class Module; /*class TargetData; */class DataLayout; }


namespace Winter
{


class ExternalFunction;


class Linker
{
public:
	Linker(bool hidden_voidptr_arg, void* env);
	~Linker();


	//void addFunctions(BufferRoot& root);
	void addFunctions(const std::vector<FunctionDefinitionRef>& func_defs);
	void addExternalFunctions(std::vector<ExternalFunctionRef>& f);

	//void linkFunctions(BufferRoot& root);

	//ExternalFunctionRef findMatchingExternalFunction(const FunctionSignature& sig);
	FunctionDefinitionRef findMatchingFunction(const FunctionSignature& sig); // Returns null ref if not found
	FunctionDefinitionRef findMatchingFunctionByName(const std::string& name); // NOTE: rather unsafe

	void getFuncsWithMatchingName(const std::string& name, std::vector<FunctionDefinitionRef>& funcs_out);

	void buildLLVMCode(llvm::Module* module, const llvm::DataLayout/*TargetData*/* target_data, const CommonFunctions& common_functions);

	const std::string buildOpenCLCode();

	std::vector<FunctionDefinitionRef> concrete_funcs;
private:
	void addFunction(const FunctionDefinitionRef& f);
	FunctionDefinitionRef makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings);

	typedef std::map<std::string, std::vector<Reference<FunctionDefinition> > > NameToFuncMapType;
	NameToFuncMapType name_to_functions_map;
	typedef std::map<FunctionSignature, Reference<FunctionDefinition> > SigToFuncMapType;
	SigToFuncMapType sig_to_function_map;

	std::vector<Reference<FunctionDefinition> > unique_functions;

	typedef std::map<FunctionSignature, ExternalFunctionRef > ExternalFuncMapType;
	ExternalFuncMapType external_functions;

	bool hidden_voidptr_arg;
	void* env;
};


}
