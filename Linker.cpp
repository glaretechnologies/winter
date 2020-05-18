/*=====================================================================
Linker.cpp
----------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "Linker.h"


#include "BuiltInFunctionImpl.h"
#include "utils/StringUtils.h"
#include "utils/PlatformUtils.h"
#include "wnt_ExternalFunction.h"
#include "wnt_RefCounting.h"
#include <limits>


using std::vector;


namespace Winter
{


Linker::Linker(bool try_coerce_int_to_double_first_, bool emit_in_bound_asserts_, 
	bool real_is_double_, bool optimise_for_opencl_)
:	try_coerce_int_to_double_first(try_coerce_int_to_double_first_),
	emit_in_bound_asserts(emit_in_bound_asserts_),
	real_is_double(real_is_double_),
	optimise_for_opencl(optimise_for_opencl_)
{}


Linker::~Linker()
{}


void Linker::addFunctions(const vector<FunctionDefinitionRef>& new_func_defs)
{
	for(unsigned int i=0; i<new_func_defs.size(); ++i)
		addFunction(new_func_defs[i]);
}


void Linker::addFunction(const FunctionDefinitionRef& def)
{
	if(this->sig_to_function_map.find(def->sig) != this->sig_to_function_map.end())
		throw ExceptionWithPosition("Function " + def->sig.toString() + " already defined: " + errorContextString(def.getPointer()) + "\nalready defined here: ",
		errorContext(this->sig_to_function_map[def->sig].getPointer()));

	this->name_to_functions_map[def->sig.name].push_back(def);
	this->sig_to_function_map.insert(std::make_pair(def->sig, def));
	top_level_defs.push_back(def);
}


void Linker::addTopLevelDefs(const vector<ASTNodeRef>& defs)
{
	for(unsigned int i=0; i<defs.size(); ++i)
	{
		if(defs[i]->nodeType() == ASTNode::FunctionDefinitionType)
			addFunction(defs[i].downcast<FunctionDefinition>());
		else if(defs[i]->nodeType() == ASTNode::NamedConstantType)
		{
			const NamedConstantRef named_constant = defs[i].downcast<NamedConstant>();
			
			if(named_constant_map.find(named_constant->name) != named_constant_map.end())
				throw ExceptionWithPosition("Named constant with name '" + named_constant->name + "' already defined." + errorContextString(*named_constant) +
				"\nalready defined here: ", errorContext(named_constant_map[named_constant->name].getPointer()));

			named_constant_map[named_constant->name] = named_constant;

			top_level_defs.push_back(named_constant);
		}
		else
		{
			assert(0);
		}
	}
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
			-1, // order number
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


void Linker::buildLLVMCode(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, ProgramStats& stats, bool emit_trace_code)
{
	PlatformUtils::CPUInfo cpu_info;
	PlatformUtils::getCPUInfo(cpu_info);

	std::set<VRef<const Type>, ConstTypeVRefLessThan> destructors_called_types;

	for(Linker::SigToFuncMapType::iterator it = sig_to_function_map.begin(); it != sig_to_function_map.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;

		if(!f.isGenericFunction() && !f.isExternalFunction())
		{
			if(!f.is_anon_func)
				f.buildLLVMFunction(module, cpu_info, target_data, common_functions, destructors_called_types, stats, emit_trace_code, false);
			if(f.is_anon_func || f.need_to_emit_captured_var_struct_version)
				f.buildLLVMFunction(module, cpu_info, target_data, common_functions, destructors_called_types, stats, emit_trace_code, true);
		}
	}

	// Emit code for anonymous functions
	for(size_t i=0; i<anon_functions_to_codegen.size(); ++i)
	{
		anon_functions_to_codegen[i]->buildLLVMFunction(module, cpu_info, target_data, common_functions, destructors_called_types, stats, emit_trace_code, 
			true // with_captured_var_struct_ptr
		);
	}

	// Build concrete funcs
	/*for(unsigned int i=0; i<concrete_funcs.size(); ++i)
	{
		assert(!concrete_funcs[i]->isGenericFunction());

		concrete_funcs[i]->buildLLVMFunction(module, cpu_info, target_data, common_functions);
	}*/

	// Build 'unique' functions (like shuffle())
	for(unsigned int i=0; i<unique_functions.size(); ++i)
	{
		unique_functions[i]->buildLLVMFunction(module, cpu_info, target_data, common_functions, destructors_called_types, stats, emit_trace_code, false);
	}


	// Emit destructors
	for(auto i = destructors_called_types.begin(); i != destructors_called_types.end(); ++i)
	{
		if((*i)->hasDestructor())
		{
			RefCounting::emitDecrementorForType(module, target_data, common_functions, *i);
			RefCounting::emitDestructorForType(module, target_data, common_functions, *i);
		}
	}
}


const std::string Linker::buildOpenCLCode()
{
	//NOTE: not called right now
	assert(0);

	std::string s;

	EmitOpenCLCodeParams params;
	params.uid = 0;
	params.emit_comments = true;
	params.emit_in_bound_asserts = emit_in_bound_asserts;

	for(Linker::SigToFuncMapType::iterator it = sig_to_function_map.begin(); it != sig_to_function_map.end(); ++it)
	{
		FunctionDefinition& f = *(*it).second;

		if(!f.isGenericFunction() && !f.isExternalFunction() && f.built_in_func_impl.isNull())
		{
			s += f.emitOpenCLC(params) + "\n";
		}
	}

	// Build concrete funcs
	/*for(unsigned int i=0; i<concrete_funcs.size(); ++i)
	{
		assert(!concrete_funcs[i]->isGenericFunction());

		s += concrete_funcs[i]->emitOpenCLC(params) + "\n";
	}*/

	// Build 'unique' functions (like shuffle())
	//for(unsigned int i=0; i<unique_functions.size(); ++i)
	//{
	//	s += unique_functions[i]->emitOpenCLC() + "\n";
	//}

	return params.file_scope_code + "\n\n" + s;
}


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
static FunctionDefinitionRef makeBuiltInFuncDef(const std::string& name, const TypeVRef& type, const TypeVRef& return_type)
{
	vector<FunctionDefinition::FunctionArg> args;
	args.push_back(FunctionDefinition::FunctionArg(type, "x"));

	FunctionDefinitionRef def = new FunctionDefinition(
		SrcLocation::invalidLocation(),
		-1, // order number
		name, // name
		args, // args
		NULL, // body expr
		return_type, // return type
		new BuiltInFuncType(type) // built in impl.
	);

	return def;
}


FunctionDefinitionRef Linker::findMatchingFunctionSimple(const FunctionSignature& sig)
{
	SigToFuncMapType::iterator sig_lookup_res = sig_to_function_map.find(sig);
	if(sig_lookup_res != sig_to_function_map.end())
		return sig_lookup_res->second;
	else
		return NULL;
}


static vector<FunctionDefinition::FunctionArg> makeFunctionArgPair(const std::string& arg0_name, const TypeVRef& type0, const std::string& arg1_name, const TypeVRef& type1)
{
	vector<FunctionDefinition::FunctionArg> args;
	args.reserve(2);
	args.push_back(FunctionDefinition::FunctionArg(type0, arg0_name));
	args.push_back(FunctionDefinition::FunctionArg(type1, arg1_name));
	return args;
}


Reference<FunctionDefinition> Linker::findMatchingFunction(const FunctionSignature& sig, const SrcLocation& call_src_location, int effective_callsite_order_num) // , const std::vector<FunctionDefinition*>* func_def_stack)
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
	{
		if(sig_lookup_res->second->order_num >= effective_callsite_order_num && effective_callsite_order_num != -1)
			throw ExceptionWithPosition("Tried to refer to a function defined later: " + sig.toString() + errorContextString(call_src_location) + "\ntried to call function defined later: ", errorContext(*sig_lookup_res->second));
		//if(sig_lookup_res->second->order_num < effective_callsite_order_num || effective_callsite_order_num == -1) //  !func_def_stack || isTargetDefinedBeforeAllInStack(*func_def_stack, sig_lookup_res->second->order_num))
		return sig_lookup_res->second;
	}

	
	if(sig.param_types.size() == 0)
	{
		if(sig.name == "floatNaN")
		{
			FunctionDefinitionRef def = new FunctionDefinition(
				SrcLocation::invalidLocation(),
				-1, // order number
				sig.name, // name
				std::vector<FunctionDefinition::FunctionArg>(), // args
				NULL, // body expr
				new Float(), // return type
				new NaNBuiltInFunc(new Float()) // built in impl.
			);
			this->sig_to_function_map.insert(std::make_pair(sig, def));
			return def;
		}
		if(sig.name == "doubleNaN")
		{
			FunctionDefinitionRef def = new FunctionDefinition(
				SrcLocation::invalidLocation(),
				-1, // order number
				sig.name, // name
				std::vector<FunctionDefinition::FunctionArg>(), // args
				NULL, // body expr
				new Double(), // return type
				new NaNBuiltInFunc(new Double()) // built in impl.
			);
			this->sig_to_function_map.insert(std::make_pair(sig, def));
			return def;
		}
		if(sig.name == "realNaN")
		{
			if(real_is_double)
			{
				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					sig.name, // name
					std::vector<FunctionDefinition::FunctionArg>(), // args
					NULL, // body expr
					new Double(), // return type
					new NaNBuiltInFunc(new Double()) // built in impl.
				);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else
			{
				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					sig.name, // name
					std::vector<FunctionDefinition::FunctionArg>(), // args
					NULL, // body expr
					new Float(), // return type
					new NaNBuiltInFunc(new Float()) // built in impl.
				);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}
	}
	else if(sig.param_types.size() == 1)
	{
		// Handle float->float, or vector<float, N> -> vector<float, N> functions
		if(sig.param_types[0]->getType() == Type::FloatType || sig.param_types[0]->getType() == Type::DoubleType || // if float or double
			(sig.param_types[0]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType) || // or vector of floats
			(sig.param_types[0]->getType() == Type::VectorTypeType && static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::DoubleType) // or vector of doubles
			)
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
			else if(sig.name == "sign")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<SignBuiltInFunc>(sig.name, sig.param_types[0], sig.param_types[0]);
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

			if(sig.name == "toDouble")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<ToDoubleBuiltInFunc>(sig.name, sig.param_types[0], ToDoubleBuiltInFunc::getReturnType(sig.param_types[0]));
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}

			if(sig.name == "toReal")
			{
				FunctionDefinitionRef def;
				if(real_is_double)
					def = makeBuiltInFuncDef<ToDoubleBuiltInFunc>(sig.name, sig.param_types[0], ToDoubleBuiltInFunc::getReturnType(sig.param_types[0]));
				else
					def = makeBuiltInFuncDef<ToFloatBuiltInFunc>(sig.name, sig.param_types[0], ToFloatBuiltInFunc::getReturnType(sig.param_types[0]));

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}

			if(sig.param_types[0]->getType() == Type::IntType && sig.param_types[0].downcastToPtr<const Int>()->numBits() == 32 && sig.name == "toInt64")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<ToInt64BuiltInFunc>(sig.name, sig.param_types[0], new Int(64));
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}

			if(sig.param_types[0]->getType() == Type::IntType && sig.param_types[0].downcastToPtr<const Int>()->numBits() == 64 && sig.name == "toInt32")
			{
				FunctionDefinitionRef def = makeBuiltInFuncDef<ToInt32BuiltInFunc>(sig.name, sig.param_types[0], new Int(32));
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}
		else if(sig.param_types[0]->getType() == Type::OpaqueTypeType)
		{
			if(sig.name == "toInt")
			{
				TypeVRef ret_type = new Int(64);
				FunctionDefinitionRef def = makeBuiltInFuncDef<VoidPtrToInt64BuiltInFunc>(sig.name, sig.param_types[0], ret_type);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(sig.name == "length")
		{
			if(sig.param_types[0]->getType() == Type::ArrayTypeType || sig.param_types[0]->getType() == Type::VArrayTypeType ||
				sig.param_types[0]->getType() == Type::TupleTypeType || sig.param_types[0]->getType() == Type::VectorTypeType)
			{
				TypeVRef ret_type = new Int(64);
				FunctionDefinitionRef def = makeBuiltInFuncDef<LengthBuiltInFunc>(sig.name, sig.param_types[0], ret_type);
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

	}
	else if(sig.param_types.size() == 2)
	{
		if(sig.name == "__compare_equal")
		{
			if(sig.param_types[0]->requiresCompareEqualFunction())
			{
				//if(type->getType() == Type::FunctionType)
				//	continue; // TODO: implement function comparison

				vector<FunctionDefinition::FunctionArg> compare_args;
				compare_args.push_back(FunctionDefinition::FunctionArg(sig.param_types[0], "a"));
				compare_args.push_back(FunctionDefinition::FunctionArg(sig.param_types[0], "b"));

				FunctionDefinitionRef compare_eq_func = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"__compare_equal", // name
					compare_args, // arguments
					ASTNodeRef(), // body expr
					new Bool(), // declard return type
					new CompareEqualBuiltInFunc(sig.param_types[0], /*is_compare_not_equal=*/false) // built in func impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, compare_eq_func));
				this->top_level_defs.push_back(compare_eq_func); // Add to top_level_defs so binding is done on this function as well.
				return compare_eq_func;
			}
		}
		else if(sig.name == "__compare_not_equal")
		{
			if(sig.param_types[0]->requiresCompareEqualFunction())
			{
				//if(type->getType() == Type::FunctionType)
				//	continue; // TODO: implement function comparison

				vector<FunctionDefinition::FunctionArg> compare_args;
				compare_args.push_back(FunctionDefinition::FunctionArg(sig.param_types[0], "a"));
				compare_args.push_back(FunctionDefinition::FunctionArg(sig.param_types[0], "b"));

				FunctionDefinitionRef compare_neq_func = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"__compare_not_equal", // name
					compare_args, // arguments
					ASTNodeRef(), // body expr
					new Bool(), // declard return type
					new CompareEqualBuiltInFunc(sig.param_types[0], /*is_compare_not_equal=*/true) // built in func impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, compare_neq_func));
				this->top_level_defs.push_back(compare_neq_func); // Add to top_level_defs so binding is done on this function as well.
				return compare_neq_func;
			}
		}

		if(sig.param_types[0]->getType() == Type::FunctionType && sig.param_types[1]->getType() == Type::ArrayTypeType)
		{
			if(sig.name == "map")
			{
				const VRef<ArrayType> array_type = sig.param_types[1].downcast<ArrayType>();
				const VRef<Type> array_elem_type = array_type->elem_type;

				const VRef<Function> func_type = sig.param_types[0].downcast<Function>();
				const VRef<Type> R(func_type->return_type);
			
				// map(function<T, R>, array<T, N>) array<R, N>
				if(func_type->arg_types.size() != 1)
					throw ExceptionWithPosition("Function argument to map must take one argument.", errorContext(call_src_location));

				if(*func_type->arg_types[0] != *array_elem_type)
				{
					throw ExceptionWithPosition(std::string("Function argument to map must take same argument type as array element.\n") +
						"Function type: " + func_type->toString() + ",\n array_elem_type: " + array_elem_type->toString(), errorContext(call_src_location));
				}

				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("f", func_type, "array", array_type);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"map",
					args,
					ASTNodeRef(NULL), // body expr
					new ArrayType(R, array_type->num_elems), // return type
					new ArrayMapBuiltInFunc(
						array_type, // from array type
						func_type // func type
					)
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("array", sig.param_types[0], "index", sig.param_types[1]);

				VRef<Type> ret_type = sig.param_types[0].downcast<ArrayType>()->elem_type;

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"elem", // name
					args, // args
					NULL, // body expr
					ret_type, // return type
					new ArraySubscriptBuiltInFunc(sig.param_types[0].downcast<ArrayType>(), sig.param_types[1]) // built in impl.
				);

				assert(this->sig_to_function_map.find(sig) == this->sig_to_function_map.end()); // Check not already inserted
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(sig.param_types[0]->getType() == Type::VArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("varray", sig.param_types[0], "index", sig.param_types[1]);

				TypeVRef ret_type = sig.param_types[0].downcast<const VArrayType>()->elem_type;

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"elem", // name
					args, // args
					NULL, // body expr
					ret_type, // return type
					new VArraySubscriptBuiltInFunc(sig.param_types[0].downcast<VArrayType>(), sig.param_types[1]) // built in impl.
				);

				assert(this->sig_to_function_map.find(sig) == this->sig_to_function_map.end()); // Check not already inserted
				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		// inBounds(array, index)
		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "inBounds")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("array", sig.param_types[0], "index", sig.param_types[1]);

				TypeRef ret_type = new Bool();

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
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
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("vector", sig.param_types[0], "index", sig.param_types[1]);

				TypeRef ret_type = new Bool();

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
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
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("vector", sig.param_types[0], "index", sig.param_types[1]);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
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

		if(sig.param_types[0]->getType() == Type::TupleTypeType && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("tuple", sig.param_types[0], "index", sig.param_types[1]);

				FunctionDefinitionRef def = new FunctionDefinition(
					call_src_location,
					-1, // order number - Consider Before everything else
					"elem", // name
					args, // args
					NULL, // body expr
					NULL, // sig.param_types[0].downcast<TupleType>()->component_types, // return type
					new GetTupleElementBuiltInFunc(sig.param_types[0].downcast<TupleType>(), std::numeric_limits<unsigned int>::max()) // built in impl.
				);

				// This isn't really a proper function, and cannot be, because the return type depends on the index.
				// So it will just be special cased in the FunctionExpression node code emission, and no actual func should be generated for it.
				unique_functions_no_codegen.push_back(def);
				return def;
			}
		}

		// Gather elem :      elem(array<T, n>, vector<int, m>) -> vector<T, m>
		if(sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::VectorTypeType && sig.param_types[1].downcast<VectorType>()->elem_type->getType() == Type::IntType)
		{
			if(sig.name == "elem")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("array", sig.param_types[0], "index_vector", sig.param_types[1]);

				VRef<ArrayType> array_type = sig.param_types[0].downcast<ArrayType>();

				TypeRef return_type = new VectorType(array_type->elem_type, sig.param_types[1].downcast<VectorType>()->num);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"elem", // name
					args, // args
					NULL, // body expr
					return_type, // return type
					new ArraySubscriptBuiltInFunc(array_type, sig.param_types[1]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
		}

		if(	(sig.param_types[0]->getType() == Type::FloatType && sig.param_types[1]->getType() == Type::FloatType) ||
			(sig.param_types[0]->getType() == Type::DoubleType && sig.param_types[1]->getType() == Type::DoubleType))
		{
			if(sig.name == "pow")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"pow", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new PowBuiltInFunc(sig.param_types[0]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
			else if(sig.name == "_frem_")
			{
				const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

				FunctionDefinitionRef def = new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
					"_frem_", // name
					args, // args
					NULL, // body expr
					sig.param_types[0], // return type
					new FRemBuiltInFunc(sig.param_types[0]) // built in impl.
				);

				this->sig_to_function_map.insert(std::make_pair(sig, def));
				return def;
			}
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
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						call_src_location,
						-1, // order number
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



			if((
				(static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType) || // if vector of floats
				(static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::DoubleType) || // or vector of doubles
				(static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::IntType)  // or vector of ints
				) && (*sig.param_types[0] == *sig.param_types[1])) // and argument types are the same
			{
				assert(*sig.param_types[0] == *sig.param_types[1]);

				if(sig.name == "min")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
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
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					const TypeRef ret_type = sig.param_types[0];

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
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


			if((static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::FloatType || // vector of floats
				static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type->getType() == Type::DoubleType) && // or vector of doubles
				(*sig.param_types[0] == *sig.param_types[1])) // and argument types are the same
			{
				if(sig.name == "pow")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"pow", // name
						args, // args
						NULL, // body expr
						sig.param_types[0], // return type
						new PowBuiltInFunc(sig.param_types[0]) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "_frem_")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"_frem_", // name
						args, // args
						NULL, // body expr
						sig.param_types[0], // return type
						new FRemBuiltInFunc(sig.param_types[0]) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"dot", // name
						args, // args
						NULL, // body expr
						static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type, // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>()) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot1")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"dot1", // name
						args, // args
						NULL, // body expr
						static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type, // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>(), /*num_components=*/1) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot2")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"dot2", // name
						args, // args
						NULL, // body expr
						static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type, // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>(), /*num_components=*/2) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot3")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"dot3", // name
						args, // args
						NULL, // body expr
						static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type, // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>(), /*num_components=*/3) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
				else if(sig.name == "dot4")
				{
					const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("x", sig.param_types[0], "y", sig.param_types[1]);

					FunctionDefinitionRef def = new FunctionDefinition(
						SrcLocation::invalidLocation(),
						-1, // order number
						"dot4", // name
						args, // args
						NULL, // body expr
						static_cast<const VectorType*>(sig.param_types[0].getPointer())->elem_type, // return type
						new DotProductBuiltInFunc(sig.param_types[0].downcast<VectorType>(), /*num_components=*/4) // built in impl.
					);

					this->sig_to_function_map.insert(std::make_pair(sig, def));
					return def;
				}
			} // End if (vector of floats, vector of floats)
		} // End if (vector, vector) params

		if(sig.name == "makeVArray" && sig.param_types[1]->getType() == Type::IntType)
		{
			if(sig.param_types[1].downcastToPtr<Int>()->numBits() != 64)
				throw ExceptionWithPosition("second argument to makeVArray() must have type int64.", errorContext(call_src_location));

			const vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("element", sig.param_types[0], "count", sig.param_types[1]);

			VRef<VArrayType> ret_type = new VArrayType(args[0].type);

			FunctionDefinitionRef def = new FunctionDefinition(
				SrcLocation::invalidLocation(),
				-1, // order number
				"makeVArray", // name
				args, // args
				NULL, // body expr
				ret_type, // return type
				new MakeVArrayBuiltInFunc(ret_type) // built in impl.
			);

			assert(this->sig_to_function_map.find(sig) == this->sig_to_function_map.end()); // Check not already inserted
			this->sig_to_function_map.insert(std::make_pair(sig, def));
			return def;
		}

	} // End if two params
	else if(sig.param_types.size() == 3)
	{
	} // End if three params


	if(sig.name == "iterate" && (sig.param_types.size() >= 2) && sig.param_types[0]->getType() == Type::FunctionType)
	{
		const VRef<Function> func_type = sig.param_types[0].downcast<Function>();
		const TypeVRef state_type = sig.param_types[1];

		// Remaining args are invariant data args
		vector<TypeVRef> invariant_data_type;
		for(size_t i=2; i<sig.param_types.size(); ++i)
			invariant_data_type.push_back(sig.param_types[i]);

			
		// typecheck elems and function arg
		// iterate(function<State, int, tuple<State, bool>> f, State initial_state) State
		// or
		// iterate(function<State, int, LoopInvariantData, tuple<State, bool>> f, State initial_state, LoopInvariantData invariant_data) State

		// Check func_type
		if(func_type->arg_types.size() != invariant_data_type.size() + 2)
			throw ExceptionWithPosition("function argument to iterate must have 2 + 'num invariant data args' args.", errorContext(call_src_location));
		
		if(*func_type->arg_types[0] != *state_type)
			throw ExceptionWithPosition("First argument type to function argument to iterate must be same as initial_state type.", errorContext(call_src_location));

		if(func_type->arg_types[1]->getType() != Type::IntType)
			throw ExceptionWithPosition("second argument type to function argument to iterate must be int.", errorContext(call_src_location));

		for(size_t i=0; i<invariant_data_type.size(); ++i)
		{
			if(*func_type->arg_types[2 + i] != *invariant_data_type[i])
				throw ExceptionWithPosition("Argument type to function argument to iterate must be same as invariant_data type.", errorContext(call_src_location)); // TODO: improve error msg.
		}

		if(func_type->return_type->getType() != Type::TupleTypeType)
			throw ExceptionWithPosition("function argument to iterate must return tuple<State, bool>", errorContext(call_src_location));

		if(func_type->return_type.downcast<TupleType>()->component_types.size() != 2)
			throw ExceptionWithPosition("function argument to iterate must return tuple<State, bool>", errorContext(call_src_location));

		if(*func_type->return_type.downcast<TupleType>()->component_types[0] != *state_type)
			throw ExceptionWithPosition("function argument to iterate must return tuple<State, bool>", errorContext(call_src_location));

		if(func_type->return_type.downcast<TupleType>()->component_types[1]->getType() != Type::BoolType)
			throw ExceptionWithPosition("function argument to iterate must return tuple<State, bool>", errorContext(call_src_location));

		vector<FunctionDefinition::FunctionArg> args = makeFunctionArgPair("f", func_type, "initial_state", state_type);
		for(size_t i=0; i<invariant_data_type.size(); ++i)
			args.push_back(FunctionDefinition::FunctionArg(invariant_data_type[i], "invariant_data"));

		FunctionDefinitionRef def = new FunctionDefinition(
			SrcLocation::invalidLocation(),
			-1, // order number
			"iterate",
			args,
			ASTNodeRef(), // body expr
			state_type, // return type
			new IterateBuiltInFunc(
				func_type, // func type
				state_type,
				invariant_data_type
			)
		);

		this->sig_to_function_map.insert(std::make_pair(sig, def));
		return def;
	}
	if(sig.name == "fold" && sig.param_types.size() == 3 && sig.param_types[0]->getType() == Type::FunctionType && sig.param_types[1]->getType() == Type::ArrayTypeType)
	{
		const VRef<Function> func_type = sig.param_types[0].downcast<Function>();
		const VRef<ArrayType> array_type = sig.param_types[1].downcast<ArrayType>();
		const TypeVRef state_type = sig.param_types[2];
			
		// fold(function<State, T, State> f, array<T> array, State initial_state) State

		if(func_type->arg_types.size() != 2)
			throw ExceptionWithPosition("function argument to fold must have 2 args.", errorContext(call_src_location));

		if(*func_type->arg_types[0] != *state_type)
			throw ExceptionWithPosition("First argument type to function argument to fold must be same as initial_state type.", errorContext(call_src_location));

		if(*func_type->arg_types[1] != *array_type->elem_type)
			throw ExceptionWithPosition("Second argument type to function argument to fold must be same as array element type.", errorContext(call_src_location));

		if(*func_type->return_type != *state_type)
			throw ExceptionWithPosition("Function argument to fold return type must be same as initial_state type.", errorContext(call_src_location));

		vector<FunctionDefinition::FunctionArg> args;
		args.reserve(3);
		args.push_back(FunctionDefinition::FunctionArg(func_type, "f"));
		args.push_back(FunctionDefinition::FunctionArg(array_type, "array"));
		args.push_back(FunctionDefinition::FunctionArg(state_type, "initial_state"));

		FunctionDefinitionRef def = new FunctionDefinition(
			SrcLocation::invalidLocation(),
			-1, // order number
			"fold",
			args,
			ASTNodeRef(NULL), // body expr
			state_type, // return type
			new ArrayFoldBuiltInFunc(
				func_type, // func type
				array_type,
				state_type
			)
		);

		this->sig_to_function_map.insert(std::make_pair(sig, def));
		return def;
	}

	// def update(CollectionType c, int index, T newval) CollectionType
	// TODO: other types
	if(sig.name == "update" && sig.param_types.size() == 3 && sig.param_types[0]->getType() == Type::ArrayTypeType && sig.param_types[1]->getType() == Type::IntType)
	{
		const TypeVRef collection_type = sig.param_types[0];
		const VRef<Int> index_type = sig.param_types[1].downcast<Int>();
		const TypeVRef value_type = sig.param_types[2];
			
		// NOTE: only supported for arrays currently
		if(collection_type->getType() == Type::ArrayTypeType)
		{
			const ArrayType* array_type = collection_type.downcastToPtr<ArrayType>();
			if(*array_type->elem_type != *value_type)
				throw ExceptionWithPosition("Invalid argument types for update.", errorContext(call_src_location)); // TODO: add error context
		}
		else
			throw ExceptionWithPosition("Invalid first argument to update.", errorContext(call_src_location)); // TODO: add error context

		vector<FunctionDefinition::FunctionArg> args;
		args.reserve(3);
		args.push_back(FunctionDefinition::FunctionArg(collection_type, "c"));
		args.push_back(FunctionDefinition::FunctionArg(index_type, "index"));
		args.push_back(FunctionDefinition::FunctionArg(value_type, "newval"));

		FunctionDefinitionRef def = new FunctionDefinition(
			SrcLocation::invalidLocation(),
			-1, // order number
			"update",
			args,
			ASTNodeRef(NULL), // body expr
			collection_type, // return type
			new UpdateElementBuiltInFunc(
				collection_type
			)
		);

		this->sig_to_function_map.insert(std::make_pair(sig, def));
		return def;
	}

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
				throw ExceptionWithPosition("eN() functions must take one argument.  (While trying to find function " + sig.toString() + ")", errorContext(call_src_location));

			//if(sig.param_types[0]->getType() != Type::VectorTypeType)
			//	throw BaseException("eN() functions must take a vector as their argument.  Call: " + sig.name + ", found arg type: " + sig.param_types[0]->toString());
			if(sig.param_types[0]->getType() == Type::VectorTypeType)
			{

				VRef<VectorType> vec_type(
					(VectorType*)(sig.param_types[0].getPointer()) // NOTE: dirty cast
					);

				if(index >= (int)vec_type->num)
					throw ExceptionWithPosition("eN function has N >= vector size.", errorContext(call_src_location));


				vector<FunctionDefinition::FunctionArg> args(1,
					FunctionDefinition::FunctionArg(
						sig.param_types[0], //TypeRef(new Int()), // type
						"vec" // name
					)
				);
			
				// NOTE: this function def gets created multiple times if eN() is called multiple times with same N?

				FunctionDefinitionRef new_func_def(new FunctionDefinition(
					SrcLocation::invalidLocation(),
					-1, // order number
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

	// Try and find a suitable generic function
	NameToFuncMapType::iterator res = this->name_to_functions_map.find(sig.name); // Get all functions with given name
	if(res != this->name_to_functions_map.end())
	{
		const vector<FunctionDefinitionRef>& funcs = res->second;
		for(size_t z=0; z<funcs.size(); ++z)
		{
			const FunctionDefinition& f = *funcs[z];
			assert(f.sig.name == sig.name);

			if(f.order_num < effective_callsite_order_num) //   !func_def_stack || isTargetDefinedBeforeAllInStack(*func_def_stack, f.order_num))
			{
				if(f.isGenericFunction() && f.sig.param_types.size() == sig.param_types.size())
				{
					bool match = true;
					vector<TypeRef> type_mapping;

					for(unsigned int i=0; match && (i<f.sig.param_types.size()); ++i)
					{
						const bool arg_match = f.sig.param_types[i]->matchTypes(*sig.param_types[i], type_mapping);
						if(!arg_match)
							match = false;
					}


					for(size_t i=0; i<type_mapping.size(); ++i)
						if(type_mapping[i].isNull())
							match = false; // TEMP check this
					
					if(match)
					{
						vector<TypeVRef> type_mapping_vrefs;
						type_mapping_vrefs.reserve(type_mapping.size());
						for(size_t i=0; i<type_mapping.size(); ++i)
							type_mapping_vrefs.push_back(TypeVRef(type_mapping[i]));

						FunctionDefinitionRef new_concrete_func = makeConcreteFunction(funcs[z], type_mapping_vrefs);
						addFunction(new_concrete_func);
						return new_concrete_func;
					}
				}
			}
		}
	}

	return FunctionDefinitionRef();
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
		std::vector<TypeVRef> type_mappings)
{
	//std::cout << "Making concrete function from " << generic_func->sig.toString() << "\n";
	vector<FunctionDefinition::FunctionArg> args;
	args.reserve(generic_func->args.size());
	for(size_t i=0; i<generic_func->args.size(); ++i)
	{
		//args[i] = generic_func->args[i];
		// If generic_func's arg i is generic
		if(generic_func->args[i].type->getType() == Type::GenericTypeType)
		{
			assert(generic_func->args[i].type->getType() == Type::GenericTypeType);
			GenericType* gt = static_cast<GenericType*>(generic_func->args[i].type.getPointer());
			
			// Then replace with the bound concrete type.
			args.push_back(FunctionDefinition::FunctionArg(type_mappings[gt->genericTypeParamIndex()], generic_func->args[i].name));
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

	ASTNodeRef body;
	BuiltInFunctionImpl* built_in_impl = NULL;

	if(generic_func->body.nonNull())
	{
		CloneMapType clone_map;
		body = generic_func->body->clone(clone_map);
	}
	else
	{
		if(generic_func->sig.name == "fold")
		{
			assert(type_mappings.size() == 1);
			//built_in_impl = new ArrayFoldBuiltInFunc(type_mappings[0]);
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
	TypeRef concrete_declared_ret_type;
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


	Reference<FunctionDefinition> def = new FunctionDefinition(
		generic_func->srcLocation(), // Use the generic function's location in src for the location
		generic_func->order_num,
		generic_func->sig.name, // name
		args, // args
		body,
		concrete_declared_ret_type, // return type
		built_in_impl // built in func impl
	);

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
