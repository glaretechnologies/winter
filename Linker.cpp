//Copyright 2009 Nicholas Chapman
#include "Linker.h"


#include "BuiltInFunctionImpl.h"
#include <iostream>
#include "utils/stringutils.h"
#include "utils/platformutils.h"
#include "wnt_ExternalFunction.h"


using std::vector;


namespace Winter
{


Linker::Linker(bool hidden_voidptr_arg_, void* env_)
:	hidden_voidptr_arg(hidden_voidptr_arg_),
	env(env_)
{}


Linker::~Linker()
{}


//void Linker::addFunctions(BufferRoot& root)
//{
//	for(unsigned int i=0; i<root.func_defs.size(); ++i)
//	{
//		Reference<FunctionDefinition> def = root.func_defs[i];
//
//		addFunction(def);
//		//this->name_to_functions_map[def->sig.name].push_back(def);
//		//this->sig_to_function_map.insert(std::make_pair(def->sig, def));
//	}
//}


void Linker::addFunctions(const vector<FunctionDefinitionRef>& func_defs)
{
	for(unsigned int i=0; i<func_defs.size(); ++i)
		addFunction(func_defs[i]);
}


void Linker::addFunction(const FunctionDefinitionRef& def)
{
	this->name_to_functions_map[def->sig.name].push_back(def);
	this->sig_to_function_map.insert(std::make_pair(def->sig, def));
}


void Linker::addExternalFunctions(vector<ExternalFunctionRef>& funcs)
{
	for(unsigned int i=0; i<funcs.size(); ++i)
	{
		ExternalFunctionRef& f = funcs[i];

		vector<FunctionDefinition::FunctionArg> args;
		for(size_t z=0; z<f->sig.param_types.size(); ++z)
			args.push_back(FunctionDefinition::FunctionArg(f->sig.param_types[z], "arg_" + ::toString((uint64)z)));

		Reference<FunctionDefinition> def(new FunctionDefinition(
			SrcLocation::invalidLocation(),
			f->sig.name,
			args,
			ASTNodeRef(NULL), // body
			f->return_type, // declared return type
			NULL
		));

		def->external_function = f;
		//this->external_functions.insert(f[i].sig);
		addFunction(def);

		//this->functions.insert(std::make_pair(funcs[i].sig, def));*/
		//this->external_functions.insert(std::make_pair(funcs[i]->sig, funcs[i]));
	}
}


void Linker::buildLLVMCode(llvm::Module* module, const llvm::TargetData* target_data)
{
	PlatformUtils::CPUInfo cpu_info;
	PlatformUtils::getCPUInfo(cpu_info);

	for(Linker::SigToFuncMapType::iterator it = sig_to_function_map.begin(); it != sig_to_function_map.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;

		if(!f.isGenericFunction() && !f.isExternalFunction())
		{
			f.buildLLVMFunction(module, cpu_info, hidden_voidptr_arg, target_data);
		}
	}

	// Build concrete funcs
	for(unsigned int i=0; i<concrete_funcs.size(); ++i)
	{
		assert(!concrete_funcs[i]->isGenericFunction());

		concrete_funcs[i]->buildLLVMFunction(module, cpu_info, hidden_voidptr_arg, target_data);
	}
}


/*
void Linker::linkFunctions(BufferRoot& root)
{
	root.linkFunctions(*this);
}*/


/*ExternalFunctionRef Linker::findMatchingExternalFunction(const FunctionSignature& sig)
{
	ExternalFuncMapType::iterator res = external_functions.find(sig);
	if(res != external_functions.end())
	{
		return res->second;
	}
	return ExternalFunctionRef();
}*/


Reference<FunctionDefinition> Linker::findMatchingFunction(const FunctionSignature& sig)
{
	/*
	if sig.name matches eN
		create or insert eN function
	For each function f
		If f.name == sig.name
			If it takes the correct number of args
				new empty association
				for each arg type in f T_i
					if T_i is a generic type
						if T_i is already associated with a type
							if T_i associated_type != sig.T_i, fail match
						else let T_i = sig.T_i
					else if T_i is a concrete type
						if T_i associated_type != sig.T_i, fail match
						if T_i has children, then, for each child C_i
							if sig.T_i is concrete type
	*/	

	if(sig.name.size() > 1 && sig.name[0] == 'e')
	{
		bool numeric = true;
		for(unsigned int i=1; i<sig.name.size(); ++i)
			if(!::isNumeric(sig.name[i]))
				numeric = false;
		if(numeric)
		{
			const int index = ::stringToInt(::eatPrefix(sig.name, "e"));

			if(sig.param_types.size() != 1)
				throw BaseException("eN() functions must take one argument.");

			//if(sig.param_types[0]->getType() != Type::VectorTypeType)
			//	throw BaseException("eN() functions must take a vector as their argument.  Call: " + sig.name + ", found arg type: " + sig.param_types[0]->toString());
			if(sig.param_types[0]->getType() == Type::VectorTypeType)
			{

				Reference<VectorType> vec_type(
					(VectorType*)(sig.param_types[0].getPointer()) // NOTE: dirty cast
					);

				if(index >= (int)vec_type->num)
					throw BaseException("eN function has N >= vector size.");


				vector<FunctionDefinition::FunctionArg> args(1,
					FunctionDefinition::FunctionArg(
						sig.param_types[0], //TypeRef(new Int()), // type
						"vec" // name
					)
				);
			
				FunctionDefinitionRef new_func_def(new FunctionDefinition(
					SrcLocation::invalidLocation(),
					sig.name, // name
					args,
					//vector<Reference<LetASTNode> >(), // lets
					ASTNodeRef(NULL), // body expr
					vec_type->t, // declared return type
					new GetVectorElement(
						vec_type,
						index
					)// built in func impl
				));

				if(sig_to_function_map.find(sig) == sig_to_function_map.end())
				{
					/*sig_to_function_map.insert(std::make_pair(
						sig,
						new_func_def
					));*/
					addFunction(new_func_def);
				}
			}
		}
	}

	NameToFuncMapType::iterator res = this->name_to_functions_map.find(sig.name);
	if(res != this->name_to_functions_map.end())
	{
		vector<FunctionDefinitionRef> funcs = res->second;
		for(size_t z=0; z<funcs.size(); ++z)
		{
			FunctionDefinition& f = *funcs[z];
			//bool match = true;
			if(f.sig.name == sig.name)
			{
				if(f.sig.param_types.size() == sig.param_types.size())
				{
					bool match = true;
					//std::map<int, TypeRef> types; // generic types
					vector<TypeRef> type_mapping;

					for(unsigned int i=0; match && (i<f.sig.param_types.size()); ++i)
					{
						/*if(f.sig.param_types[i]->getType() == Type::GenericTypeType)
						{
							GenericType* gt = dynamic_cast<GenericType*>(f.sig.param_types[i].getPointer());
							assert(gt);
							if(gt->genericTypeParamIndex() < (int)types.size() && types[gt->genericTypeParamIndex()].nonNull())
							{
								if(*types[gt->genericTypeParamIndex()] != *sig.param_types[i])
									match = false;
							}
							else
							{
								if(gt->genericTypeParamIndex() >= (int)types.size())
									types.resize(gt->genericTypeParamIndex() + 1);
								types[gt->genericTypeParamIndex()] = sig.param_types[i];
							}
						}
						else // else concrete type
						{
							if(*f.sig.param_types[i] != *sig.param_types[i])
								match = false;
						}*/
						const bool arg_match = f.sig.param_types[i]->matchTypes(*sig.param_types[i], type_mapping);
						if(!arg_match)
							match = false;
					}

					if(match)
					{
						if(f.isGenericFunction())
						{
							concrete_funcs.push_back(makeConcreteFunction(
								funcs[z],
								type_mapping
							));

							return concrete_funcs.back();
						}
						else
							return funcs[z];
					}
				}
			}
		}
	}

//	throw BaseException("Could not find " + sig.toString());
	return FunctionDefinitionRef();
/*

	Linker::FuncMapType::iterator res = this->functions.find(sig);
	if(res == this->functions.end())
		throw BaseException("Could not find " + sig.toString());
	else
		return (*res).second;
		*/
}


void Linker::getFuncsWithMatchingName(const std::string& name, vector<FunctionDefinitionRef>& funcs_out)
{
	/*for(Linker::FuncMapType::iterator it = this->functions.begin(); it != functions.end(); ++it)
	{
		FunctionDefinitionRef& f = (*it).second;
		if(f->sig.name == name)
			funcs_out.push_back(f);
	}*/
	NameToFuncMapType::iterator res = name_to_functions_map.find(name);
	if(res != name_to_functions_map.end())
		funcs_out = res->second;
}


//Reference<FunctionDefinition> Linker::findMatchingFunctionByName(const std::string& name)
//{
//	for(Linker::FuncMapType::iterator it = this->functions.begin(); it != functions.end(); ++it)
//	{
//		FunctionDefinition& f = *(*it).second;
//		if(f.sig.name == name)
//			return it->second;
//	}
//
//	//throw BaseException("Could not find function '" + name + "'");
//	return FunctionDefinitionRef();
//}


Reference<FunctionDefinition> Linker::makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings)
{
	//std::cout << "Making concrete function from " << generic_func->sig.toString() << "\n";
	vector<FunctionDefinition::FunctionArg> args = generic_func->args;
	for(size_t i=0; i<generic_func->args.size(); ++i)
	{
		//args[i] = generic_func->args[i];
		// If generic_func's arg i is generic
		if(generic_func->args[i].type->getType() == Type::GenericTypeType)
		{
			assert(generic_func->args[i].type->getType() == Type::GenericTypeType);
			GenericType* gt = static_cast<GenericType*>(generic_func->args[i].type.getPointer());
			
			// Then replace with the bound concrete type.
			args[i].type = type_mappings[gt->genericTypeParamIndex()];
		}
	}

	/*TypeRef ret_type = generic_func->returnType(); 
	// If return type is a generic type...
	if(generic_func->returnType()->getType() == Type::GenericTypeType)
	{
		GenericType* gt = dynamic_cast<GenericType*>(generic_func->returnType().getPointer());
		assert(gt);
		// Replace it with the mapped type.
		ret_type = type_mappings[gt->genericTypeParamIndex()];
		assert(ret_type.nonNull());
	}*/

	// Make copy of the body expression, with concrete types substituted for generic types.

	ASTNodeRef body(NULL);
	BuiltInFunctionImpl* built_in_impl = NULL;

	if(generic_func->body.nonNull())
		body = generic_func->body->clone();
	else
	{
		if(generic_func->sig.name == "fold")
		{
			assert(type_mappings.size() == 1);
			built_in_impl = new ArrayFoldBuiltInFunc(type_mappings[0]);
		}
		else if(generic_func->sig.name == "if")
		{
			assert(type_mappings.size() >= 1);
			built_in_impl = new IfBuiltInFunc(type_mappings[0]);
		}
		else
		{
			assert(!"missing match for generic func built in body.");
		}
	}

	// Map across declared return type
	TypeRef concrete_declared_ret_type(NULL);
	if(generic_func->declared_return_type.nonNull())
	{
		if(generic_func->declared_return_type->getType() == Type::GenericTypeType)
		{
			GenericType* gt = static_cast<GenericType*>(generic_func->declared_return_type.getPointer());

			//TODO: check mapping exists
			concrete_declared_ret_type = type_mappings[gt->genericTypeParamIndex()];
		}
		else // Else ret type is already concrete.
			concrete_declared_ret_type = generic_func->declared_return_type;
	}


	Reference<FunctionDefinition> def(new FunctionDefinition(
		generic_func->srcLocation(), // Use the generic function's location in src for the location
		generic_func->sig.name, // name
		args, // args
		body,
		concrete_declared_ret_type, // return type
		built_in_impl // built in func impl
	));

	if(body.nonNull())
	{
		// Rebind variables to get new type.
		{
			TraversalPayload payload(TraversalPayload::BindVariables, hidden_voidptr_arg, env);
			payload.func_def_stack.push_back(def.getPointer());
			payload.linker = this;
			
			std::vector<ASTNode*> stack;
			def->traverse(
				payload,
				stack
			);
		}

		// Type check again
		{
			TraversalPayload payload(TraversalPayload::TypeCheck, hidden_voidptr_arg, env);
			
			std::vector<ASTNode*> stack;
			def->traverse(
				payload,
				stack
			);
		}
	}

	return def;
}


}




