#include "wnt_Type.h"


#include "Value.h"
#include "BaseException.h"
#include "LLVMTypeUtils.h"
#include "wnt_FunctionDefinition.h"
#include "wnt_RefCounting.h"
#include "utils/StringUtils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <vector>


using namespace std;


namespace Winter
{


//==========================================================================


Reference<Value> Type::getInvalidValue() const // For array out-of-bounds
{
	assert(0);
	return NULL;
}


llvm::Value* Type::getInvalidLLVMValue(llvm::Module& module) const // For array out-of-bounds
{
	return llvm::UndefValue::get(this->LLVMType(module));
}


void Type::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const // Default implementation does nothing.
{
}


void Type::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const // Default implementation does nothing.
{
}


void Type::emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const
{
}


//==========================================================================


bool Float::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* Float::LLVMType(llvm::Module& module) const
{ 
	return llvm::Type::getFloatTy(module.getContext());
}


const std::string Float::OpenCLCType() const
{ 
	if(address_space.empty())
		return "float";
	else
		return address_space + " float";
}


Reference<Value> Float::getInvalidValue() const // For array out-of-bounds
{
	return new FloatValue(std::numeric_limits<float>::quiet_NaN());
}


llvm::Value* Float::getInvalidLLVMValue(llvm::Module& module) const // For array out-of-bounds
{
	return llvm::ConstantFP::get(
		module.getContext(), 
		llvm::APFloat::getSNaN(llvm::APFloat::IEEEsingle)
	);
}


//==========================================================================


bool Double::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* Double::LLVMType(llvm::Module& module) const
{ 
	return llvm::Type::getDoubleTy(module.getContext());
}


const std::string Double::OpenCLCType() const
{ 
	if(address_space.empty())
		return "double";
	else
		return address_space + " double";
}


Reference<Value> Double::getInvalidValue() const // For array out-of-bounds
{
	return new DoubleValue(std::numeric_limits<double>::quiet_NaN());
}


llvm::Value* Double::getInvalidLLVMValue(llvm::Module& module) const // For array out-of-bounds
{
	return llvm::ConstantFP::get(
		module.getContext(), 
		llvm::APFloat::getSNaN(llvm::APFloat::IEEEdouble)
	);
}


//==========================================================================


bool GenericType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->genericTypeParamIndex() < (int)type_mapping.size() && 
		type_mapping[this->genericTypeParamIndex()].nonNull()) // If type mapping for this type already exists
	{
		return *type_mapping[genericTypeParamIndex()] == b;
	}
	else // Else a type mapping for this generic type does not exist yet.
	{
		// Make space for it
		if(this->genericTypeParamIndex() >= (int)type_mapping.size())
			type_mapping.resize(this->genericTypeParamIndex() + 1);
		type_mapping[this->genericTypeParamIndex()] = TypeRef((Type*)&b); // Add the mapping.
		return true;
	}
}


llvm::Type* GenericType::LLVMType(llvm::Module& module) const
{
	assert(0);
	return NULL;
}


//==========================================================================


const std::string Int::toString() const
{
	if(is_signed)
	{
		if(num_bits == 32)
			return "int";
		else
			return "int" + ::toString(num_bits);
	}
	else
	{
		if(num_bits == 32)
			return "uint";
		else
			return "uint" + ::toString(num_bits);
	}
}


llvm::Type* Int::LLVMType(llvm::Module& module) const
{ 
	// Note that integer types in LLVM just specify a bit-width, but not if they are signed or not.
	return llvm::Type::getIntNTy(module.getContext(), num_bits);
}


const std::string Int::OpenCLCType() const
{
	const std::string u_prefix = is_signed ? "" : "u";
	if(num_bits == 16)
		return u_prefix + "short";
	else if(num_bits == 32)
		return u_prefix + "int";
	else if(num_bits == 64)
		return u_prefix + "long";
	else
		throw BaseException("No OpenCL type for int with num bits=" + ::toString(num_bits));
}


bool Int::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is an Int as well.
	const Int& b_ = static_cast<const Int&>(b);
	return num_bits == b_.num_bits && is_signed == b_.is_signed;
}


llvm::Value* Int::getInvalidLLVMValue(llvm::Module& module) const // For array out-of-bounds
{
	return llvm::ConstantInt::get(
		module.getContext(), 
		llvm::APInt(
			num_bits, // num bits
			0, // value
			is_signed // signed
		)
	);
}


Reference<Value> Int::getInvalidValue() const // For array out-of-bounds
{
	return new IntValue(0, true);
}


//==========================================================================


bool Bool::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* Bool::LLVMType(llvm::Module& module) const
{ 
	return llvm::Type::getInt1Ty(module.getContext());
}


//==========================================================================


bool String::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* String::LLVMType(llvm::Module& module) const
{
	// See if there is a struct with this name already:
	const std::string use_name = "string";
	llvm::StructType* existing_struct_type = module.getTypeByName(use_name);
	if(existing_struct_type)
		return LLVMTypeUtils::pointerType(*existing_struct_type);

	// else create the named struct:

	llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()), // Reference count field
		llvm::Type::getInt64Ty(module.getContext()), // length field (num elements)
		llvm::Type::getInt64Ty(module.getContext()), // flags
		llvm::ArrayType::get(  // Variable-size array of element types
			llvm::Type::getIntNTy(module.getContext(), 8),
			0 // Num elements
		)
	};


	llvm::StructType* struct_type = llvm::StructType::create(
		module.getContext(),
		field_types,
		use_name
	);

	return LLVMTypeUtils::pointerType(*struct_type);




/*
	llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()) // Reference count field
	};

	return LLVMTypeUtils::pointerType(*llvm::StructType::get(
		module.getContext(),
		llvm::makeArrayRef(field_types, 1)
	));

	//return LLVMTypeUtils::voidPtrType(context);
	*/

}


void String::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	llvm::CallInst* inst = params.builder->CreateCall(params.common_functions.incrStringRefCountLLVMFunc, ref_counted_value);

	addMetaDataCommentToInstruction(params, inst, comment);
}


void String::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	//llvm::CallInst* inst = params.builder->CreateCall(params.common_functions.decrStringRefCountLLVMFunc, ref_counted_value);

	//addMetaDataCommentToInstruction(params, inst, comment);

	
	/*llvm::FunctionType* destructor_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(*params.context), // return type
		llvm::makeArrayRef(this->LLVMType(*params.module)),
		false // varargs
	);

	llvm::Constant* destructor_func_constant = params.module->getOrInsertFunction(
		"decr_string", // Name
		destructor_type // Type
	);

	// TODO: check cast
	llvm::Function* destructor_func = static_cast<llvm::Function*>(destructor_func_constant);*/

	llvm::Function* destructor_func = RefCounting::getOrInsertDecrementorForType(params.module, this);

	llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, ref_counted_value);

	addMetaDataCommentToInstruction(params, call_inst, comment);

	params.destructors_called_types->insert(this);
}


//==========================================================================


bool CharType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* CharType::LLVMType(llvm::Module& module) const
{
	return llvm::Type::getInt32Ty(module.getContext());
}


//==========================================================================


void Function::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	// Cast to dummy function type.
	const TypeRef dummy_function_type = Function::dummyFunctionType();
	
	llvm::Value* cast_val = params.builder->CreatePointerCast(ref_counted_value, dummy_function_type->LLVMType(*params.module));

	llvm::CallInst* inst = params.builder->CreateCall(params.common_functions.incrClosureRefCountLLVMFunc, cast_val);

	addMetaDataCommentToInstruction(params, inst, comment);
}


void Function::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	llvm::Function* destructor_func = RefCounting::getOrInsertDecrementorForType(params.module, this);
	llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, ref_counted_value);

	addMetaDataCommentToInstruction(params, call_inst, comment);

	// NOTE: this right?
	params.destructors_called_types->insert(this);
	//this->getContainedTypesWithDestructors(*params.destructors_called_types);
}


bool Function::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a Function as well.
	const Function* b_ = static_cast<const Function*>(&b);
	if(this->arg_types.size() != b_->arg_types.size())
		return false;

	for(unsigned int i=0;i<arg_types.size(); ++i)
		if(!this->arg_types[i]->matchTypes(*b_->arg_types[i], type_mapping))
			return false;

//	if(!this->return_type->matchTypes(*b_->return_type, type_mapping))
//		return false;
	return true;
}


const std::string Function::toString() const // { return "function"; 
{
	std::string s = "function<";
	std::vector<std::string> typestrings;
	for(unsigned int i=0;i<arg_types.size(); ++i)
		typestrings.push_back(arg_types[i]->toString());
	
	typestrings.push_back(this->return_type->toString());

	//s += StringUtils::join(typestrings, ", ");

	//s += ", " + this->return_type->toString();
	s += StringUtils::join(typestrings, ", ");
	return s + ">";
}

/*



Let's say we have f(int x, int y, int z) x 
That has captured two vars, and one uncaptured var (z)
so we have
struct CapturedVars
{
	int x;
	int y;
}

void (*FPtr)(int x, int y, int z, CapturedVars* vars);

and finally

struct Closure
{
	FPtr func;
	CapturedVars vars;
}

*/
llvm::Type* Function::LLVMType(llvm::Module& module) const
{
	//TEMP: this need to be in sync with FunctionDefinition::emitLLVMCode()
	const bool simple_func_ptr = false;
	if(simple_func_ptr)
	{
		// Looks like we're not allowed to pass functions directly as args, have to be pointer-to-funcs
		llvm::Type* t = LLVMTypeUtils::pointerType(*LLVMTypeUtils::llvmFunctionType(
			arg_types,
			false, // use captured var struct ptr arg
			return_type,
			module
		));

		//std::cout << "Function::LLVMType: " << std::endl;
		//t->dump();
		//std::cout << std::endl;
		return t;
	}

	const std::string use_name = makeSafeStringForFunctionName(this->toString()) + "closure";
	llvm::StructType* existing_struct_type = module.getTypeByName(use_name);
	if(existing_struct_type)
		return LLVMTypeUtils::pointerType(existing_struct_type);


	// Build Empty LLVM CapturedVars struct
	//vector<const llvm::Type*> cap_var_types;
	
	//for(size_t i=0; i<this->captured_var_types.size(); ++i)
	//	cap_var_types.push_back(this->captured_var_types[i]->LLVMType(context));

	/*const llvm::Type* cap_var_struct = llvm::StructType::get(
		context,
		cap_var_types
	);*/


	// Build vector of function args
	/*vector<const llvm::Type*> llvm_arg_types(this->arg_types.size());
	for(size_t i=0; i<this->arg_types.size(); ++i)
		llvm_arg_types[i] = this->arg_types[i]->LLVMType(context);

	// Add Pointer to captured var struct, if there are any captured vars
	if(use_captured_vars)
		llvm_arg_types.push_back(LLVMTypeUtils::pointerType(*cap_var_struct));

	//TEMP HACK: add hidden void* arg  NOTE: should only do this when hidden_void_arg is true.
	llvm_arg_types.push_back(LLVMTypeUtils::voidPtrType(context));

	// Construct the function pointer type
	const llvm::Type* func_ptr_type = LLVMTypeUtils::pointerType(*llvm::FunctionType::get(
		this->return_type->LLVMType(context), // result type
		llvm_arg_types,
		false // is var arg
	));*/

	llvm::Type* func_ptr_type = LLVMTypeUtils::pointerType(*LLVMTypeUtils::llvmFunctionType(
		arg_types,
		true, // use captured var struct ptr arg
		return_type,
		module
	));

	llvm::Type* destructor_arg_types[1] = { LLVMTypeUtils::getPtrToBaseCapturedVarStructType(module) };

	llvm::FunctionType* destructor_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(module.getContext()), // return type
		destructor_arg_types,
		false // varargs
	);

	//vector<const llvm::Type*> field_types;

	// Add pointer to function type
	//field_types.push_back(func_ptr_type);

	//TEMP HACK: no captured vars
	//for(size_t i=0; i<this->captured_vars.size(); ++i)
	//	field_types.push_back(this->captured_vars[i].type->LLVMType(context));

	// Make the vector of fields for the closure type
	llvm::Type* closure_field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()), // Ref count field
		llvm::Type::getInt64Ty(module.getContext()), // flags field
		func_ptr_type,
		LLVMTypeUtils::pointerType(destructor_type),
		LLVMTypeUtils::getBaseCapturedVarStructType(module) // cap_var_struct
	};

	// Return the closure structure type.
	llvm::StructType* closure_struct_type = llvm::StructType::create(
		module.getContext(),
		llvm::makeArrayRef(closure_field_types),
		use_name
	);

	//std::cout << "closure_struct_type: " << std::endl;
	//closure_struct_type->dump();
	//std::cout << std::endl;
	

	// Return pointer to structure type.
	return LLVMTypeUtils::pointerType(*closure_struct_type);
}


const std::string Function::OpenCLCType() const
{
	throw BaseException("Function (closure) types not supported for OpenCL C emission");
}


void Function::emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const
{
}


void Function::getContainedTypesWithDestructors(std::set<ConstTypeRef, ConstTypeRefLessThan>& types) const
{
}


bool Function::containsType(const Type& other_type) const
{
	return false;
}


//==========================================================================


const std::string ArrayType::toString() const
{ 
	return (address_space.empty() ? "" : address_space + " ") + "array<" + elem_type->toString() + ", " + ::toString(num_elems) + ">";
}


const std::string ArrayType::OpenCLCType() const
{
	// For e.g. an array of floats, use type 'float*'.
	if(this->address_space.empty())
		return "__constant " + elem_type->OpenCLCType() + "*";
	else
		return address_space + " " + elem_type->OpenCLCType() + "*";
}


bool ArrayType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is an Array as well.
	const ArrayType* b_ = static_cast<const ArrayType*>(&b);

	if(this->num_elems != b_->num_elems)
		return false;

	return this->elem_type->matchTypes(*b_->elem_type, type_mapping);
}


llvm::Type* ArrayType::LLVMType(llvm::Module& module) const
{
	return llvm::ArrayType::get(
		this->elem_type->LLVMType(module), // Element type
		this->num_elems // Num elements
	);
}


bool ArrayType::containsType(const Type& other_type) const
{
	return isEqualToOrContains(*this->elem_type, other_type);
}


//==========================================================================


const std::string VArrayType::toString() const
{ 
	return "varray<" + elem_type->toString() + ">";
}


const std::string VArrayType::OpenCLCType() const
{
	// For e.g. an array of floats, use type 'float*'.
	return elem_type->OpenCLCType() + "*";
}


bool VArrayType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is a VArray as well.
	const VArrayType* b_ = static_cast<const VArrayType*>(&b);

	return this->elem_type->matchTypes(*b_->elem_type, type_mapping);
}


llvm::Type* VArrayType::LLVMType(llvm::Module& module) const
{
	// See if there is a struct with this name already:
	/*const std::string use_name = makeSafeStringForFunctionName(this->toString());
	llvm::StructType* existing_struct_type = module.getTypeByName(use_name);
	if(existing_struct_type)
		return LLVMTypeUtils::pointerType(*existing_struct_type);

	// else create the named struct:

	llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()), // Reference count field
		LLVMTypeUtils::pointerType(*this->elem_type->LLVMType(module)) // Pointer to element type
	};

	llvm::StructType* struct_type = llvm::StructType::create(
		module.getContext(),
		llvm::makeArrayRef(field_types, 2)
		// NOTE: is_packed is default = false.
	);

	return LLVMTypeUtils::pointerType(*struct_type);*/

	//return LLVMTypeUtils::pointerType(
	//	*this->elem_type->LLVMType(module) // Element type
	//);
	/*llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()), // Reference count field
		LLVMTypeUtils::pointerType(*this->elem_type->LLVMType(module)) // Pointer to element type
	};

	return LLVMTypeUtils::pointerType(*llvm::StructType::get(
		module.getContext(),
		llvm::makeArrayRef(field_types, 2)
	));*/

	/*return LLVMTypeUtils::pointerType(
		*llvm::Type::getInt64Ty(module.getContext())
	);*/


	// See if there is a struct with this name already:
	const std::string use_name = makeSafeStringForFunctionName(this->toString());
	llvm::StructType* existing_struct_type = module.getTypeByName(use_name);
	if(existing_struct_type)
		return LLVMTypeUtils::pointerType(*existing_struct_type);

	// else create the named struct:

	//vector<llvm::Type*> field_types(this->component_types.size());
	//for(size_t i=0; i<this->component_types.size(); ++i)
	//	field_types[i] = this->component_types[i]->LLVMType(module);

	//return llvm::StructType::create(
	//	module.getContext(),
	//	field_types,
	//	this->name
	//	// NOTE: is_packed is default = false.
	//);


	llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(module.getContext()), // Reference count field
		llvm::Type::getInt64Ty(module.getContext()), // length field (num elements)
		llvm::Type::getInt64Ty(module.getContext()), // flags
		llvm::ArrayType::get(  // Variable-size array of element types
			this->elem_type->LLVMType(module),
			0 // Num elements
		)
	};

	//return LLVMTypeUtils::pointerType(*llvm::StructType::get(
	//	module.getContext(),
	//	llvm::makeArrayRef(field_types)
	//));

	llvm::StructType* struct_type = llvm::StructType::create(
		module.getContext(),
		field_types,
		use_name
	);

	return LLVMTypeUtils::pointerType(*struct_type);
}


void VArrayType::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	//if(*params.currently_building_func_def->returnType() == *this) // Only do ref counting for this value if it is of the enclosing function return type.
	{
		const TypeRef dummy_varray_type = new VArrayType(new Int());

		// Cast to dummy_varray_type
		llvm::Value* cast_val = params.builder->CreatePointerCast(ref_counted_value, dummy_varray_type->LLVMType(*params.module));

		llvm::CallInst* inst = params.builder->CreateCall(params.common_functions.incrVArrayRefCountLLVMFunc, cast_val);

		addMetaDataCommentToInstruction(params, inst, comment);
	}
}


void VArrayType::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	//if(*params.currently_building_func_def->returnType() == *this) // Only do ref counting for this value if it is of the enclosing function return type.
	/*{
		const TypeRef dummy_varray_type = new VArrayType(new Int());

		// Cast to dummy_varray_type
		llvm::Value* cast_val = params.builder->CreatePointerCast(ref_counted_value, dummy_varray_type->LLVMType(*params.module));

		llvm::CallInst* inst = params.builder->CreateCall(params.common_functions.decrVArrayRefCountLLVMFunc, cast_val);

		addMetaDataCommentToInstruction(params, inst, comment);
	}*/

	/*llvm::FunctionType* destructor_type = llvm::FunctionType::get(
		llvm::Type::getVoidTy(*params.context), // return type
		llvm::makeArrayRef(this->LLVMType(*params.module)),
		false // varargs
	);

	llvm::Constant* destructor_func_constant = params.module->getOrInsertFunction(
		"decr_" + this->toString(), // Name
		destructor_type // Type
	);

	// TODO: check cast
	llvm::Function* destructor_func = static_cast<llvm::Function*>(destructor_func_constant);*/

	llvm::Function* destructor_func = RefCounting::getOrInsertDecrementorForType(params.module, this);
	llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, ref_counted_value);

	addMetaDataCommentToInstruction(params, call_inst, comment);

	params.destructors_called_types->insert(this);
	this->getContainedTypesWithDestructors(*params.destructors_called_types);
}


void VArrayType::emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const
{
	llvm::Function* destructor_func = RefCounting::getOrInsertDestructorForType(params.module, this);
	llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, value);

	addMetaDataCommentToInstruction(params, call_inst, comment);

	params.destructors_called_types->insert(this);
	this->getContainedTypesWithDestructors(*params.destructors_called_types);
}


void VArrayType::getContainedTypesWithDestructors(std::set<ConstTypeRef, ConstTypeRefLessThan>& types) const
{
	types.insert(elem_type);
	elem_type->getContainedTypesWithDestructors(types);
}


bool VArrayType::containsType(const Type& other_type) const
{
	return isEqualToOrContains(*this->elem_type, other_type);
}


//==========================================================================


bool Map::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	throw BaseException("Map::matchTypes: unimplemented.");
}


llvm::Type* Map::LLVMType(llvm::Module& module) const
{
	return LLVMTypeUtils::voidPtrType(module.getContext());
}


const std::string Map::OpenCLCType() const
{
	assert(0);
	return "";
}


//==========================================================================


StructureType::StructureType(const std::string& name_, const std::vector<TypeRef>& component_types_, const std::vector<std::string>& component_names_) 
:	Type(StructureTypeType), name(name_), component_types(component_types_), component_names(component_names_)
{
	//if(component_types_.size() != component_names_.size())
	//	throw Winter::BaseException("component_types_.size() != component_names_.size()");
}


const std::string StructureType::toString() const 
{ 
	return (address_space.empty() ? "" : address_space + " ") + name;
}


bool StructureType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a StructureType as well.
	const StructureType* b_ = static_cast<const StructureType*>(&b);

	if(this->name != b_->name)
		return false;

	for(size_t i=0; i<this->component_types.size(); ++i)
	{
		if(!this->component_types[i]->matchTypes(*b_->component_types[i], type_mapping))
			return false;

		// Fields have to have same name as well.
		if(this->component_names[i] != b_->component_names[i])
			return false;
	}

	return true;
}


llvm::Type* StructureType::LLVMType(llvm::Module& module) const
{
	// See if there is a struct with this name already:
	llvm::StructType* existing_struct_type = module.getTypeByName(this->name);
	if(existing_struct_type)
		return existing_struct_type;

	// else create the named struct:

	vector<llvm::Type*> field_types(this->component_types.size());
	for(size_t i=0; i<this->component_types.size(); ++i)
		field_types[i] = this->component_types[i]->LLVMType(module);

	return llvm::StructType::create(
		module.getContext(),
		field_types,
		this->name
		// NOTE: is_packed is default = false.
	);
}



/*const std::string StructureType::toString() const 
{ 
	std::string s = "struct<";
	std::vector<std::string> typestrings;
	for(unsigned int i=0;i<arg_types.size(); ++i)
		typestrings.push_back(arg_types[i]->toString());
	
	typestrings.push_back(this->return_type->toString());

	//s += StringUtils::join(typestrings, ", ");

	//s += ", " + this->return_type->toString();
	s += StringUtils::join(typestrings, ", ");
	return s + ">";
}*/


const std::string StructureType::definitionString() const // Winter definition string, e.g "struct a { float b }"
{
	std::string s = "struct " + name + "\n{\n";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		s += "\t" + component_types[i]->toString() + " " + component_names[i];
		if(i + 1 < component_types.size())
			s += ",";
		s += "\n";
	}

	s += "}\n";
	return s;
}


const std::string StructureType::getOpenCLCDefinition(EmitOpenCLCodeParams& params, bool emit_comments) const // Get full definition string, e.g. struct a { float b; };
{
	const std::string use_name = mapOpenCLCVarName(params.opencl_c_keywords, name);

	std::string s = "typedef struct " + use_name + "\n{\n";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		s += "\t" + component_types[i]->OpenCLCType() + " " + component_names[i] + ";\n";
	}

	s += "} " + use_name + ";\n\n";

/*
	// Make constructor.
	// for struct S { float a, float b }, will look like    
	// S S_float_float(float a, float b) { S s;  s.a = a; s.b = b; return s; }

	// FunctionDefinition::Funct
	FunctionSignature sig(name, component_types);

	s += "// Constructor for " + toString() + "\n";
	s += name + " " + sig.typeMangledName() + "(";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Non pass-by-value types are passed with a const C pointer.
		const std::string use_type = !component_types[i]->OpenCLPassByPointer() ? component_types[i]->OpenCLCType() : ("const " + component_types[i]->OpenCLCType() + "* const ");
		s += use_type + " " + component_names[i];
		if(i + 1 < component_types.size())
			s += ", ";
	}

	s += ") { " + name + " s_; ";

	for(size_t i=0; i<component_types.size(); ++i)
		s += "s_." + component_names[i] + " = " + (!component_types[i]->OpenCLPassByPointer() ? "" : "*") + component_names[i] + "; ";

	s += "return s_; }\n\n";
	*/
	return s;
}


const std::string StructureType::getOpenCLCConstructor(EmitOpenCLCodeParams& params, bool emit_comments) const // Emit constructor for type
{
	std::string s;

	const std::string use_name = mapOpenCLCVarName(params.opencl_c_keywords, name);

	// FunctionDefinition::Funct
	FunctionSignature sig(name, component_types); // Just use raw name here for now.  Will probably not clash with OpenCL C keywords due to type decoration.

	if(emit_comments)
		s += "// Constructor for " + toString() + "\n";
	s += use_name + " " + sig.typeMangledName() + "(";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Non pass-by-value types are passed with a const C pointer.
		const std::string use_type = !component_types[i]->OpenCLPassByPointer() ? component_types[i]->OpenCLCType() : ("const " + component_types[i]->OpenCLCType() + "* const ");
		s += use_type + " " + component_names[i];
		if(i + 1 < component_types.size())
			s += ", ";
	}

	s += ") { " + use_name + " s_; ";

	for(size_t i=0; i<component_types.size(); ++i)
		s += "s_." + component_names[i] + " = " + (!component_types[i]->OpenCLPassByPointer() ? "" : "*") + component_names[i] + "; ";

	s += "return s_; }\n\n";

	return s;
}


std::vector<Reference<TupleType> > StructureType::getElementTupleTypes() const
{
	// TODO: recursive search for tuples.
	std::vector<Reference<TupleType> > res;
	for(size_t i = 0; i<component_types.size(); ++i)
		if(component_types[i]->getType() == Type::TupleTypeType)
			res.push_back(component_types[i].downcast<TupleType>());
	return res;
}


void StructureType::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{

}


void StructureType::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	// Emit call to destructor
	/*if(this->hasDestructor()) // NOTE: might be a bit slow to call this, cache this?
	{
		llvm::Function* destructor_func = RefCounting::getOrInsertDestructorForType(params.module, this);
		llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, ref_counted_value);

		addMetaDataCommentToInstruction(params, call_inst, comment);

		params.destructors_called_types->insert(this);
		this->getContainedTypesWithDestructors(*params.destructors_called_types);
	}*/

#if 0
	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Recursivly call emitCleanupLLVMCode on each refcounted element.
		if(component_types[i]->getType() == Type::StringType ||
			component_types[i]->getType() == Type::VArrayTypeType/* ||
			struct_type.component_types[i]->getType() == Type::StructureTypeType*/) // TODO: handle this
		{

			llvm::Value* ptr = params.builder->CreateStructGEP(ref_counted_value, (unsigned int)i, this->name + "." + component_names[i] + " ptr");
			llvm::LoadInst* val = params.builder->CreateLoad(ptr, this->name + "." + component_names[i] + " ptr");
			//llvm::Value* val = load_instruction;

			//llvm::MDNode* mdnode = llvm::MDNode::get(*params.context, llvm::MDString::get(*params.context, "my md string content"));
			//val->setMetadata("the_id_for_the_metadata", mdnode);
			//const std::string comment =  "StructureType::emitDecrRefCount() for " + this->name + "." + component_names[i];
			//addMetaDataCommentToInstruction(params, val, comment);

			component_types[i]->emitDecrRefCount(params, val, comment);
		}

		/*if(struct_type.component_types[i]->getType() == Type::StringType)
		{ 
			// Emit code to load the string value:
			//structure_val->dump();
			//structure_val->getType()->dump();
			llvm::Value* str_ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i, struct_type.name + ".str ptr");

			// Load the string value
			//str_ptr->dump();
			//str_ptr->getType()->dump();
			llvm::Value* str = params.builder->CreateLoad(str_ptr, struct_type.name + ".str");

			//str->dump();
			//str->getType()->dump();
			emitStringCleanupLLVMCode(params, str);
		}
		else if(struct_type.component_types[i]->getType() == Type::VArrayTypeType)
		{
			// Emit code to load the varray value:
			llvm::Value* ptr = params.builder->CreateStructGEP(structure_val, (unsigned int)i, struct_type.name + ".varray ptr");

			// Load the  value
			llvm::Value* varray_val = params.builder->CreateLoad(ptr, struct_type.name + ".str");

			emitVArrayCleanupLLVMCode(params, varray_val);
		}*/
	}
#endif
}


void StructureType::emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const
{
	// Emit call to destructor
	if(this->hasDestructor()) // NOTE: might be a bit slow to call this, cache this?
	{
		llvm::Function* destructor_func = RefCounting::getOrInsertDestructorForType(params.module, this);
		llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, value);

		addMetaDataCommentToInstruction(params, call_inst, comment);

		params.destructors_called_types->insert(this);
		this->getContainedTypesWithDestructors(*params.destructors_called_types);
	}
}


bool StructureType::hasDestructor() const
{
	// A structure needs a destructor iff any of the contained types need a destructor.
	for(size_t i=0; i<component_types.size(); ++i)
		if(component_types[i]->hasDestructor())
			return true;
	return false;
}


void StructureType::getContainedTypesWithDestructors(std::set<ConstTypeRef, ConstTypeRefLessThan>& types) const
{
	for(size_t i=0; i<component_types.size(); ++i)
	{
		if(component_types[i]->hasDestructor())
			types.insert(component_types[i]);

		component_types[i]->getContainedTypesWithDestructors(types);
	}
}


bool StructureType::containsType(const Type& other_type) const
{
	for(size_t i=0; i<component_types.size(); ++i)
		if(isEqualToOrContains(*this->component_types[i], other_type))
			return true;
	return false;
}


//==========================================================================


TupleType::TupleType(const std::vector<TypeRef>& component_types_) 
:	Type(TupleTypeType), component_types(component_types_)
{
}


bool TupleType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a TupleType as well.
	const TupleType* b_ = static_cast<const TupleType*>(&b);

	if(b_->component_types.size() != component_types.size())
		return false;

	for(size_t i=0; i<this->component_types.size(); ++i)
	{
		if(!this->component_types[i]->matchTypes(*b_->component_types[i], type_mapping))
			return false;
	}

	return true;
}


llvm::Type* TupleType::LLVMType(llvm::Module& module) const
{
	vector<llvm::Type*> field_types(this->component_types.size());
	for(size_t i=0; i<this->component_types.size(); ++i)
		field_types[i] = this->component_types[i]->LLVMType(module);

	return llvm::StructType::get(
		module.getContext(),
		field_types
		// NOTE: is_packed is default = false.
	);
}


const std::string TupleType::toString() const 
{ 
	std::string s = "tuple<";
	std::vector<std::string> typestrings;
	for(unsigned int i=0;i<component_types.size(); ++i)
		typestrings.push_back(component_types[i]->toString());
	s += StringUtils::join(typestrings, ", ");
	return s + ">";
}


// TEMP: copied from FunctionSignature
//static const std::string makeSafeStringForFunctionName(const std::string& s)
//{
//	std::string res = s;
//
//	for(size_t i=0; i<s.size(); ++i)
//		if(!(::isAlphaNumeric(s[i]) || s[i] == '_'))
//			res[i] = '_';
//
//	return res;
//}


/*
 struct { float e0; float e1; }
*/
const std::string TupleType::OpenCLCType() const
{
	return makeSafeStringForFunctionName(this->toString());
	/*std::string s = "struct { ";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		s += component_types[i]->OpenCLCType() + " field_" + ::toString(i) + "; ";
	}

	s += "}";
	return s;*/
}


const std::string TupleType::getOpenCLCDefinition(bool emit_comments) const // Get full definition string, e.g. struct a { float b; };
{
	std::string s;
	if(emit_comments) 
		s += "// Definition of tuple " + toString() + "\n";
	s += "typedef struct\n{\n";

 	const std::string tuple_typename = OpenCLCType(); // makeSafeStringForFunctionName(this->toString());

	for(size_t i=0; i<component_types.size(); ++i)
	{
		s += "\t" + component_types[i]->OpenCLCType() + " field_" + ::toString(i) + ";\n";
	}

	s += "} " + tuple_typename + ";\n\n";


	// Make constructor.
	// for struct tuple_float__float_ { float a, float b }, will look like    
	// tuple_float__float_ tuple_float__float_cnstr(float a, float b) { tuple_float__float_ s;  s.a = a; s.b = b; return s; }
	//
	// for struct tuple_S_ { S s }, will look like    
	// tuple_S_ tuple_S_cnstr(const S* const s) { tuple_S_ res;  res.s = *s; return s; }

	/*
	const std::string constructor_name = makeSafeStringForFunctionName(this->toString()) + "_cnstr";

	s += "// Constructor for " + toString() + "\n";
	s += tuple_typename + " " + constructor_name + "(";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Non pass-by-value types are passed with a const C pointer.
		const std::string use_type = !component_types[i]->OpenCLPassByPointer() ? component_types[i]->OpenCLCType() : ("const " + component_types[i]->OpenCLCType() + "* const ");
		s += use_type + " field_" + ::toString(i);
		if(i + 1 < component_types.size())
			s += ", ";
	}

	s += ") { " + tuple_typename + " s_; ";

	for(size_t i=0; i<component_types.size(); ++i)
		s += "s_.field_" + ::toString(i) + " = " + (!component_types[i]->OpenCLPassByPointer() ? "" : "*") + "field_" + ::toString(i) + "; ";

	s += "return s_; }\n\n";
	*/
	return s;
}


const std::string TupleType::getOpenCLCConstructor(bool emit_comments) const
{
	std::string s;

	const std::string tuple_typename = OpenCLCType(); // makeSafeStringForFunctionName(this->toString());

	// Make constructor.
	// for struct tuple_float__float_ { float a, float b }, will look like    
	// tuple_float__float_ tuple_float__float_cnstr(float a, float b) { tuple_float__float_ s;  s.a = a; s.b = b; return s; }
	//
	// for struct tuple_S_ { S s }, will look like    
	// tuple_S_ tuple_S_cnstr(const S* const s) { tuple_S_ res;  res.s = *s; return s; }


	const std::string constructor_name = makeSafeStringForFunctionName(this->toString()) + "_cnstr";

	if(emit_comments)
		s += "// Constructor for " + toString() + "\n";
	s += tuple_typename + " " + constructor_name + "(";

	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Non pass-by-value types are passed with a const C pointer.
		const std::string use_type = !component_types[i]->OpenCLPassByPointer() ? component_types[i]->OpenCLCType() : ("const " + component_types[i]->OpenCLCType() + "* const ");
		s += use_type + " field_" + ::toString(i);
		if(i + 1 < component_types.size())
			s += ", ";
	}

	s += ") { " + tuple_typename + " s_; ";

	for(size_t i=0; i<component_types.size(); ++i)
		s += "s_.field_" + ::toString(i) + " = " + (!component_types[i]->OpenCLPassByPointer() ? "" : "*") + "field_" + ::toString(i) + "; ";

	s += "return s_; }\n\n";

	return s;
}


void TupleType::emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{

}


void TupleType::emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const
{
	/*llvm::Function* destructor_func = RefCounting::getOrInsertDecrementorForType(params.module, this);
	llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, ref_counted_value);

	addMetaDataCommentToInstruction(params, call_inst, comment);

	params.destructors_called_types->insert(this);
	this->getContainedTypesWithDestructors(*params.destructors_called_types);*/

#if 0
	for(size_t i=0; i<component_types.size(); ++i)
	{
		// Recursivly call emitCleanupLLVMCode on each refcounted element.
		if(component_types[i]->getType() == Type::StringType ||
			component_types[i]->getType() == Type::VArrayTypeType/* ||
			struct_type.component_types[i]->getType() == Type::StructureTypeType*/) // TODO: handle this
		{

			llvm::Value* ptr = params.builder->CreateStructGEP(ref_counted_value, (unsigned int)i, this->toString() + " field " + ::toString(i) + " ptr");
			llvm::LoadInst* val = params.builder->CreateLoad(ptr, this->toString() + " field " + ::toString(i) + " ptr");

			component_types[i]->emitDecrRefCount(params, val, comment);
		}
	}
#endif
}


void TupleType::emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const
{
	// Emit call to destructor
	if(this->hasDestructor()) // NOTE: might be a bit slow to call this, cache this?
	{
		llvm::Function* destructor_func = RefCounting::getOrInsertDestructorForType(params.module, this);
		llvm::CallInst* call_inst = params.builder->CreateCall(destructor_func, value);

		addMetaDataCommentToInstruction(params, call_inst, comment);

		params.destructors_called_types->insert(this);
		this->getContainedTypesWithDestructors(*params.destructors_called_types);
	}
}


void TupleType::getContainedTypesWithDestructors(std::set<ConstTypeRef, ConstTypeRefLessThan>& types) const
{
	for(size_t i=0; i<component_types.size(); ++i)
	{
		if(component_types[i]->hasDestructor())
			types.insert(component_types[i]);

		component_types[i]->getContainedTypesWithDestructors(types);
	}
}


bool TupleType::containsType(const Type& other_type) const
{
	for(size_t i=0; i<component_types.size(); ++i)
		if(isEqualToOrContains(*this->component_types[i], other_type))
			return true;
	return false;
}


//==========================================================================


const std::string VectorType::toString() const
{
	return "vector<" + this->elem_type->toString() + ", " + ::toString(this->num) + ">";
}


const std::string VectorType::OpenCLCType() const
{
	// float4, float8 etc..
	return this->elem_type->OpenCLCType() + ::toString(this->num);
}


bool VectorType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is a VectorType as well.
	const VectorType* b_ = static_cast<const VectorType*>(&b);

	return this->num == b_->num && this->elem_type->matchTypes(*b_->elem_type, type_mapping);
}


llvm::Type* VectorType::LLVMType(llvm::Module& module) const
{
	return llvm::VectorType::get(
		this->elem_type->LLVMType(module),
		this->num
	);
}


//===============================================================================


const std::string OpaqueType::toString() const
{
	return (address_space.empty() ? "" : address_space + " ") + "opaque";
}


bool OpaqueType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	//NOTE: This right?
	return this->getType() == b.getType();
}


llvm::Type* OpaqueType::LLVMType(llvm::Module& module) const
{
	return LLVMTypeUtils::voidPtrType(module.getContext());
}


const std::string OpaqueType::OpenCLCType() const
{ 
	if(address_space.empty())
		return "void*";
	else
		return address_space + " void*";
}


//==========================================================================


const std::string SumType::toString() const
{
	std::vector<std::string> typenames(this->types.size());
	for(size_t i=0; i<this->types.size(); ++i)
		typenames[i] = this->types[i]->toString();

	return StringUtils::join(typenames, " | ");
}


const std::string SumType::OpenCLCType() const
{
	assert(0);
	return "";
}


bool SumType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is a SumType as well.
	// const SumType* b_ = static_cast<const SumType*>(&b);

	assert(0);
	// TODO

	//return this->num == b_->num && this->elem_type->matchTypes(*b_->elem_type, type_mapping);
	return false;
}


llvm::Type* SumType::LLVMType(llvm::Module& module) const
{
	std::vector<llvm::Type*> elem_types(1 + this->types.size());

	// Type discriminator field first.
	elem_types[0] = llvm::Type::getInt32Ty(module.getContext());

	for(size_t i=0; i<this->types.size(); ++i)
		elem_types[i + 1] = this->types[i]->LLVMType(module);

	return llvm::StructType::get(
		module.getContext(),
		elem_types
	);
}


//===============================================================================


const std::string ErrorType::toString() const
{
	return "error";
}


const std::string ErrorType::OpenCLCType() const
{
	assert(0);
	return "";
}


bool ErrorType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	//NOTE: This right?
	return this->getType() == b.getType();
}


llvm::Type* ErrorType::LLVMType(llvm::Module& module) const
{
	return llvm::Type::getVoidTy(module.getContext());
}

//===============================================================================


OpaqueStructureType::OpaqueStructureType(const std::string& name_)
:	Type(StructureTypeType), name(name_)
{}


const std::string OpaqueStructureType::toString() const
{
	return (address_space.empty() ? "" : address_space + " ") + name;
}


const std::string OpaqueStructureType::OpenCLCType() const
{
	return name;
}


bool OpaqueStructureType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a StructureType as well.
	const OpaqueStructureType* b_ = static_cast<const OpaqueStructureType*>(&b);

	return this->name == b_->name;
}


llvm::Type* OpaqueStructureType::LLVMType(llvm::Module& module) const
{
	return llvm::Type::getVoidTy(module.getContext());
}


//===============================================================================


TypeRef errorTypeSum(const TypeRef& t)
{
	vector<TypeRef> elem_types(2);
	elem_types[0] = t;
	elem_types[1] = new ErrorType();
	return new SumType(elem_types);
}


} // end namespace Winter
