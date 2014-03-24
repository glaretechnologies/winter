//Copyright 2009 Nicholas Chapman
#include "Linker.h"


#include "BuiltInFunctionImpl.h"
#include "utils/StringUtils.h"
#include "utils/PlatformUtils.h"
#include "wnt_ExternalFunction.h"
#include "wnt_LLVMVersion.h"


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


void Linker::buildLLVMCode(llvm::Module* module, const llvm::DataLayout/*TargetData*/* target_data, const CommonFunctions& common_functions)
{
	PlatformUtils::CPUInfo cpu_info;
	PlatformUtils::getCPUInfo(cpu_info);

	/*CommonFunctions common_functions;
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new VoidPtrType()));
		common_functions.allocateStringFunc = findMatchingFunction(allocateStringSig).getPointer();
		assert(common_functions.allocateStringFunc);

		const FunctionSignature freeStringSig("freeString", vector<TypeRef>(1, new String()));
		common_functions.freeStringFunc = findMatchingFunction(freeStringSig).getPointer();
		assert(common_functions.freeStringFunc);
	}*/

	for(Linker::SigToFuncMapType::iterator it = sig_to_function_map.begin(); it != sig_to_function_map.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;

		if(!f.isGenericFunction() && !f.isExternalFunction())
		{
			f.buildLLVMFunction(module, cpu_info, hidden_voidptr_arg, target_data, common_functions);
		}
	}

	// Build concrete funcs
	for(unsigned int i=0; i<concrete_funcs.size(); ++i)
	{
		assert(!concrete_funcs[i]->isGenericFunction());

		concrete_funcs[i]->buildLLVMFunction(module, cpu_info, hidden_voidptr_arg, target_data, common_functions);
	}

	// Build 'unique' functions (like shuffle())
	for(unsigned int i=0; i<unique_functions.size(); ++i)
	{
		unique_functions[i]->buildLLVMFunction(module, cpu_info, hidden_voidptr_arg, target_data, common_functions);
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


template <class BuiltInFuncType>
static FunctionDefinitionRef makeBuiltInFuncDef(const std::string& name, const TypeRef& type, const TypeRef& return_type)
{
	vector<FunctionDefinition::FunctionArg> args(1);
	args[0].name = "x";
	args[0].type = type;

	FunctionDefinitionRef def = new FunctionDefinition(
		SrcLocation::invalidLocation(),
		name, // name
		args, // args
		NULL, // body expr
		return_type, // return type
		new BuiltInFuncType(type) // built in impl.
	);

	return def;
}


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

	// If the function matching this signature is in the map, return it
	SigToFuncMapType::iterator sig_lookup_res = sig_to_function_map.find(sig);
	if(sig_lookup_res != sig_to_function_map.end())
		return sig_lookup_res->second;

	// Handle float->float, or vector<float, N> -> vector<float, N> functions
	if(sig.param_types.size() == 1)
	{
		if(
			(sig.param_types[0]->getType() == Type::FloatType || // If float
			(sig.param_types[0]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType))) // or vector of floats
		{

			if(sig.name == "floor")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<FloorBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "ceil")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<CeilBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "sqrt")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<SqrtBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "sin")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<SinBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "cos")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<CosBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "exp")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<ExpBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "log")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<LogBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "abs")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<AbsBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "truncateToInt")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<TruncateToIntBuiltInFunc>(sig.name, sig.param_types[0], TruncateToIntBuiltInFunc::getReturnType(sig.param_types[0]));
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}
		else if(
			(sig.param_types[0]->getType() == Type::IntType || // If Int
			(sig.param_types[0]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::IntType))) // or vector of ints
		{
			if(sig.name == "toFloat")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<ToFloatBuiltInFunc>(sig.name, sig.param_types[0], ToFloatBuiltInFunc::getReturnType(sig.param_types[0]));
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}
	}
	else if(sig.param_types.size() == 2)
	{
		if(sig.param_types[0]->getType() == Type::FunctionType && sig.param_types[1]->getType() == Type::ArrayTypeType)
		{
			const Reference<ArrayType> array_type = sig.param_types[1].downcast<ArrayType>();
			const TypeRef elem_type = array_type->elem_type;

			const Reference<Function> func_type = sig.param_types[0].downcast<Function>();
			
			// TODO: typecheck elems and function arg and return types

			// Function from elemType -> elemType
			//TypeRef func_type = new Function(
			//	vector<TypeRef>(1, elem_type), // arg types
			//	elem_type, // return type
			//	false // use captured vars.  TEMP
			//);
			vector<FunctionDefinition::FunctionArg> args(2);
			args[0].type = func_type;
			args[0].name = "f";
			args[1].type = array_type;
			args[1].name = "array";

			FunctionDefinitionRef def = new FunctionDefinition(
				SrcLocation::invalidLocation(),
				"map",
				args,
				ASTNodeRef(NULL), // body expr
				array_type, // return type
				new ArrayMapBuiltInFunc(
					array_type, // from array type
					func_type // func type
				)
			);

			this->sig_to_function_map.insert(std::make_pair(sig, def));
			return def;
		}

		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "array";
				args[0].type = sig.param_types[0];
				args[1].name = "index";
				args[1].type = sig.param_types[1];

				TypeRef ret_type = sig.param_types[0].downcast<ArrayType>()->elem_type;

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"elem", // name
					args, // args
					NULL, // body expr
					ret_type, // return type
					new ArraySubscriptBuiltInFunc(sig.param_types[0].downcast<ArrayType>(), sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		// inBounds(array, index)
		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "inBounds")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "array";
				args[0].type = sig.param_types[0];
				args[1].name = "index";
				args[1].type = sig.param_types[1];

				TypeRef ret_type = new Bool();

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"inBounds", // name
					args, // args
					NULL, // body expr
					ret_type, // return type
					new ArrayInBoundsBuiltInFunc(sig.param_types[0].downcast<ArrayType>(), sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		// inBounds(vector, index)
		if(sig.param_types[0]->getType() == Type::VectorTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "inBounds")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "vector";
				args[0].type = sig.param_types[0];
				args[1].name = "index";
				args[1].type = sig.param_types[1];

				TypeRef ret_type = new Bool();

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"inBounds", // name
					args, // args
					NULL, // body expr
					ret_type, // return type
					new VectorInBoundsBuiltInFunc(sig.param_types[0].downcast<VectorType>(), sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(sig.param_types[0]->getType() == Type::VectorTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "vector";
				args[0].type = sig.param_types[0];
				args[1].name = "index";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"elem", // name
					args, // args
					NULL, // body expr
					sig.param_types[0].downcast<VectorType>()->elem_type, // return type
					new VectorSubscriptBuiltInFunc(sig.param_types[0].downcast<VectorType>(), sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		// Gather elem :      elem(array<T, n>, vector<int, m>) -> vector<T, m>
		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::VectorTypeType && sig.param_types[1].downcast<VectorType>()->elem_type->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "vector";
				args[0].type = sig.param_types[0];
				args[1].name = "index";
				args[1].type = sig.param_types[1];
				
				Reference<ArrayType> a_type = sig.param_types[0].downcast<ArrayType>();

				TypeRef return_type = new VectorType(a_type->elem_type, sig.param_types[1].downcast<VectorType>()->num);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"elem", // name
					args, // args
					NULL, // body expr
					return_type, // return type
					new ArraySubscriptBuiltInFunc(a_type, sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(sig.param_types[0]->getType() == Type::FloatType && sig.param_types[1]->getType() == Type::FloatType)
		{
			// There is a problem with LLVM 3.3 and earlier with the pow intrinsic getting turned into exp2f() when the first argument is 2.
			// So for now just use our own pow() external function.
#if USE_LLVM_3_4
			if(sig.name == "pow")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "x";
				args[0].type = sig.param_types[0];
				args[1].name = "y";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"pow", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new PowBuiltInFunc(sig.param_types[0]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
#endif
		}

		if(
			sig.param_types[0]->getType() == Type::VectorTypeType && // vector 
			sig.param_types[1]->getType() == Type::VectorTypeType) // and vector
		{
			// Shuffle(vector<T, m>, vector<int, n) -> vector<T, n>
			if(sig.param_types[1].downcast<VectorType>()->elem_type->getType() == Type::IntType)
			{
				if(sig.name == "shuffle")
				{
					vector<FunctionDefinition::FunctionArg> args(2);
					args[0].name = "x";
					args[0].type = sig.param_types[0];
					args[1].name = "y";
					args[1].type = sig.param_types[1];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						"shuffle_" + toString(unique_functions.size()) + "_", // name
						args, // args
						NULL, // body expr
						new VectorType(sig.param_types[0].downcast<VectorType>()->elem_type, sig.param_types[1].downcast<VectorType>()->num), // return type
						new ShuffleBuiltInFunc(sig.param_types[0].downcast<VectorType>(), sig.param_types[1].downcast<VectorType>()) // built in impl.
					);

					// NOTE: because shuffle is unusual in that it has the shuffle mask 'baked into it', we need a unique ShuffleBuiltInFunc impl each time.
					// So don't add to function map, so that it isn't reused.
					// However, we need to add it to unique_functions to prevent it from being deleted, as calling function expr doesn't hold a ref to it.
					unique_functions.push_back(def);
					return def;
				}
			}



			if(
				(static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType && // vector of floats
				static_cast<const VectorType*>(sig.param_types[1].getPointer())->elem_type->getType() == Type::FloatType) || // and vector of floats
				(static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::IntType && // vector of ints
				static_cast<const VectorType*>(sig.param_types[1].getPointer())->elem_type->getType() == Type::IntType)) // and vector of ints
			{
				if(sig.name == "min")
				{
					vector<FunctionDefinition::FunctionArg> args(2);
					args[0].name = "x";
					args[0].type = sig.param_types[0];
					args[1].name = "y";
					args[1].type = sig.param_types[1];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						"min", // name
						args, // args
						NULL, // body expr
						sig.param_types[0], // return type
						new VectorMinBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "max")
				{
					vector<FunctionDefinition::FunctionArg> args(2);
					args[0].name = "x";
					args[0].type = sig.param_types[0];
					args[1].name = "y";
					args[1].type = sig.param_types[1];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						"max", // name
						args, // args
						NULL, // body expr
						sig.param_types[0], // return type
						new VectorMaxBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
			}


			if(
				static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType && // vector of floats
				static_cast<const VectorType*>(sig.param_types[1].getPointer())->elem_type->getType() == Type::FloatType) // and vector of floats
			{
				if(sig.name == "pow")
				{
					vector<FunctionDefinition::FunctionArg> args(2);
					args[0].name = "x";
					args[0].type = sig.param_types[0];
					args[1].name = "y";
					args[1].type = sig.param_types[1];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						"pow", // name
						args, // args
						NULL, // body expr
						sig.param_types[0], // return type
						new PowBuiltInFunc(sig.param_types[0]) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot")
				{
					vector<FunctionDefinition::FunctionArg> args(2);
					args[0].name = "x";
					args[0].type = sig.param_types[0];
					args[1].name = "y";
					args[1].type = sig.param_types[1];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						"dot", // name
						args, // args
						NULL, // body expr
						new Float(), // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
			} // End if (vector of floats, vector of floats)
		} // End if (vector, vector) params


		if(
			(sig.param_types[0]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType) && // vector of floats
			(sig.param_types[1]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[1].getPointer())->elem_type->getType() == Type::FloatType)) // and vector of floats
		{
			if(sig.name == "pow")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "x";
				args[0].type = sig.param_types[0];
				args[1].name = "y";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"pow", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new PowBuiltInFunc(sig.param_types[0]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "min")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "x";
				args[0].type = sig.param_types[0];
				args[1].name = "y";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"min", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new VectorMinBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "max")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "x";
				args[0].type = sig.param_types[0];
				args[1].name = "y";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"max", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new VectorMaxBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "dot")
			{
				vector<FunctionDefinition::FunctionArg> args(2);
				args[0].name = "x";
				args[0].type = sig.param_types[0];
				args[1].name = "y";
				args[1].type = sig.param_types[1];

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					"dot", // name
					args, // args
					NULL, // body expr
					new Float(), // return type
					new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		} // End if (vector, vector) params
	} // End if two params
	else if(sig.param_types.size() == 3)
	{
	} // End if three params


	// Match against vector element access functions of name 'eN' where N is an integer.
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
			
				// NOTE: this function def gets created multiple times if eN() is called multiple times with same N?

				FunctionDefinitionRef new_func_def(new FunctionDefinition(
					SrcLocation::invalidLocation(),
					sig.name, // name
					args,
					//vector<Reference<LetASTNode> >(), // lets
					ASTNodeRef(NULL), // body expr
					vec_type->elem_type, // declared return type
					new GetVectorElement(
						vec_type,
						index
					)// built in func impl
				));

				//if(sig_to_function_map.find(sig) == sig_to_function_map.end())
				//{
					/*sig_to_function_map.insert(std::make_pair(
						sig,
						new_func_def
					));*/
					addFunction(new_func_def);
					return new_func_def;
				//}
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


Reference<FunctionDefinition> Linker::findMatchingFunctionByName(const std::string& name)
{
	/*for(Linker::FuncMapType::iterator it = this->functions.begin(); it != functions.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;
		if(f.sig.name == name)
			return it->second;
	}

	//throw BaseException("Could not find function '" + name + "'");
	return FunctionDefinitionRef();*/

	vector<FunctionDefinitionRef> funcs;
	getFuncsWithMatchingName(name, funcs);
	return funcs.empty() ? NULL : funcs[0];
}


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
			//built_in_impl = new IfBuiltInFunc(type_mappings[0]);
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
			TraversalPayload payload(TraversalPayload::BindVariables);
			payload.func_def_stack.push_back(def.getPointer());
			payload.linker = this;
			
			std::vector<ASTNode*> stack;
			def->traverse(
				payload,
				stack
			);
		}

		// Type check again
		/*{
			TraversalPayload payload(TraversalPayload::TypeCheck);
			
			std::vector<ASTNode*> stack;
			def->traverse(
				payload,
				stack
			);
		}*/
	}

	return def;
}


}




