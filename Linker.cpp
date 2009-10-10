//Copyright 2009 Nicholas Chapman
#include "Linker.h"


#include "BuiltInFunctionImpl.h"
#include <iostream>

using std::vector;


namespace Winter
{


Linker::Linker()
{}


Linker::~Linker()
{}


void Linker::addFunctions(BufferRoot& root)
{
	for(unsigned int i=0; i<root.func_defs.size(); ++i)
	{
		Reference<FunctionDefinition> def = root.func_defs[i];

		this->functions.insert(std::make_pair(def->sig, def));
	}
}

/*
void Linker::linkFunctions(BufferRoot& root)
{
	root.linkFunctions(*this);
}*/

Reference<FunctionDefinition> Linker::findMatchingFunction(const FunctionSignature& sig)
{
	/*
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
						

	for(Linker::FuncMapType::iterator it = this->functions.begin(); it != functions.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;
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
							(*it).second,
							type_mapping
						));

						return concrete_funcs.back();
					}
					else
						return (*it).second;
				}
			}
		}
	}

	throw BaseException("Could not find " + sig.toString());
/*

	Linker::FuncMapType::iterator res = this->functions.find(sig);
	if(res == this->functions.end())
		throw BaseException("Could not find " + sig.toString());
	else
		return (*res).second;
		*/
}


Reference<FunctionDefinition> Linker::makeConcreteFunction(Reference<FunctionDefinition> generic_func, 
		std::vector<TypeRef> type_mappings)
{
	std::cout << "Making concrete function from " << generic_func->sig.toString() << "\n";
	vector<FunctionDefinition::FunctionArg> args = generic_func->args;
	for(size_t i=0; i<generic_func->args.size(); ++i)
	{
		//args[i] = generic_func->args[i];
		// If generic_func's arg i is generic
		if(generic_func->args[i].type->getType() == Type::GenericTypeType)
		{
			GenericType* gt = dynamic_cast<GenericType*>(generic_func->args[i].type.getPointer());
			assert(gt);
			
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
			GenericType* gt = dynamic_cast<GenericType*>(generic_func->declared_return_type.getPointer());

			//TOPO: check mapping exists
			concrete_declared_ret_type = type_mappings[gt->genericTypeParamIndex()];
		}
		else // Else ret type is already concrete.
			concrete_declared_ret_type = generic_func->declared_return_type;
	}


	FunctionDefinition* def = new FunctionDefinition(
		generic_func->sig.name, // name
		args, // args
		vector<Reference<LetASTNode> >(), // lets
		body,
		concrete_declared_ret_type, // return type
		built_in_impl // built in func impl
	);

	if(body.nonNull())
	{
		// Rebind variables to get new type.
		{
		TraversalPayload payload(TraversalPayload::BindVariables);
		body->traverse(payload, 
			std::vector<ASTNode*>(1, def) // stack
		);
		}

		// Relink, now that conrete types are known
		{
		TraversalPayload payload(TraversalPayload::LinkFunctions);
		payload.linker = this;
		body->traverse(payload, 
			std::vector<ASTNode*>(1, def) // stack
		);
		}

		// Type check again
		{
			TraversalPayload payload(TraversalPayload::TypeCheck);
			body->traverse(payload, 
				std::vector<ASTNode*>(1, def) // stack
			);
		}
	}

	return Reference<FunctionDefinition>(def);
}


}




