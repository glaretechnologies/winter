/*=====================================================================
Linker.h
--------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "wnt_FunctionSignature.h"
#include "wnt_ExternalFunction.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include <map>
namespace llvm { class Module; class DataLayout; }


namespace Winter
{


class ExternalFunction;


class Linker
{
public:
	Linker(bool try_coerce_int_to_double_first, bool emit_in_bound_asserts, 
		bool real_is_double);
	~Linker();

	void addFunctions(const std::vector<FunctionDefinitionRef>& func_defs);
	void addExternalFunctions(std::vector<ExternalFunctionRef>& f);

	void addTopLevelDefs(const std::vector<ASTNodeRef>& defs);


	// Don't match with built-in functions like elem, don't instantiate generic functions, just return from sig_to_function_map.
	FunctionDefinitionRef findMatchingFunctionSimple(const FunctionSignature& sig); // Returns null ref if not found

	// If func_def_stack is present, makes sure found function is defined before all functions in func_def_stack.
	FunctionDefinitionRef findMatchingFunction(const FunctionSignature& sig, const SrcLocation& call_src_location, int effective_callsite_order_num = 1000000000/*, const std::vector<FunctionDefinition*>* func_def_stack = NULL*/); // Returns null ref if not found
	FunctionDefinitionRef findMatchingFunctionByName(const std::string& name); // NOTE: rather unsafe

	void getFuncsWithMatchingName(const std::string& name, std::vector<FunctionDefinitionRef>& funcs_out);

	void buildLLVMCode(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, ProgramStats& stats, bool emit_trace_code);

	const std::string buildOpenCLCode();

	std::vector<ASTNodeRef> top_level_defs; // Either function definitions or named constants.

	typedef std::map<std::string, Reference<NamedConstant> > NamedConstantMap;
	NamedConstantMap named_constant_map;

//private:
	void addFunction(const FunctionDefinitionRef& f);
	FunctionDefinitionRef makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeVRef> type_mappings);

	typedef std::map<std::string, std::vector<FunctionDefinitionRef> > NameToFuncMapType;
	NameToFuncMapType name_to_functions_map;
	typedef std::map<FunctionSignature, FunctionDefinitionRef> SigToFuncMapType;
	SigToFuncMapType sig_to_function_map;

	std::vector<FunctionDefinitionRef> anon_functions_to_codegen;

	std::vector<FunctionDefinitionRef> unique_functions;

	std::vector<FunctionDefinitionRef> unique_functions_no_codegen; // hang on to them, don't generate code for them though.

	typedef std::map<FunctionSignature, ExternalFunctionRef> ExternalFuncMapType;
	ExternalFuncMapType external_functions;

	bool try_coerce_int_to_double_first;
	bool emit_in_bound_asserts;
	bool real_is_double;
};


}
