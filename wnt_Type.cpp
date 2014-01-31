#include "wnt_Type.h"


#include "Value.h"
#include "BaseException.h"
#include "LLVMTypeUtils.h"
#include "utils/stringutils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Constants.h"
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


llvm::Value* Type::getInvalidLLVMValue(llvm::LLVMContext& context) const // For array out-of-bounds
{
	return llvm::UndefValue::get(this->LLVMType(context));
}


//==========================================================================


bool Float::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* Float::LLVMType(llvm::LLVMContext& context) const
{ 
	return llvm::Type::getFloatTy(context);
}


Reference<Value> Float::getInvalidValue() const // For array out-of-bounds
{
	return new FloatValue(std::numeric_limits<float>::quiet_NaN());
}


llvm::Value* Float::getInvalidLLVMValue(llvm::LLVMContext& context) const // For array out-of-bounds
{
	return llvm::ConstantFP::get(
		context, 
		llvm::APFloat::getSNaN(llvm::APFloat::IEEEsingle)
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


llvm::Type* GenericType::LLVMType(llvm::LLVMContext& context) const
{
	assert(0);
	return NULL;
}


//==========================================================================


llvm::Type* Int::LLVMType(llvm::LLVMContext& context) const
{ 
	// Note that integer types in LLVM just specify a bit-width, but not if they are signed or not.
	return llvm::Type::getInt32Ty(context);
}


bool Int::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Value* Int::getInvalidLLVMValue(llvm::LLVMContext& context) const // For array out-of-bounds
{
	return llvm::ConstantInt::get(
		context, 
		llvm::APInt(
			32, // num bits
			0, // value
			true // signed
		)
	);
}


Reference<Value> Int::getInvalidValue() const // For array out-of-bounds
{
	return new IntValue(0);
}


//==========================================================================


bool Bool::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* Bool::LLVMType(llvm::LLVMContext& context) const
{ 
	return llvm::Type::getInt1Ty(context);
}


//==========================================================================


bool String::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* String::LLVMType(llvm::LLVMContext& context) const
{
	llvm::Type* field_types[] = {
		llvm::Type::getInt64Ty(context) // Reference count field
	};

	return LLVMTypeUtils::pointerType(*llvm::StructType::get(
		context,
		llvm::makeArrayRef(field_types, 1)
	));

	//return LLVMTypeUtils::voidPtrType(context);
}


//==========================================================================


bool CharType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Type* CharType::LLVMType(llvm::LLVMContext& context) const
{
	return llvm::Type::getInt32Ty(context);
}


//==========================================================================


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
llvm::Type* Function::LLVMType(llvm::LLVMContext& context) const
{
	//TEMP: this need to be in sync with FunctionDefinition::emitLLVMCode()
	const bool simple_func_ptr = true;
	if(simple_func_ptr)
	{
		// Looks like we're not allowed to pass functions directly as args, have to be pointer-to-funcs
		llvm::Type* t = LLVMTypeUtils::pointerType(*LLVMTypeUtils::llvmFunctionType(
			arg_types,
			false, // use captured var struct ptr arg
			return_type,
			context
		));

		//std::cout << "Function::LLVMType: " << std::endl;
		//t->dump();
		//std::cout << std::endl;
		return t;
	}



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
		context
	));

	//vector<const llvm::Type*> field_types;

	// Add pointer to function type
	//field_types.push_back(func_ptr_type);

	//TEMP HACK: no captured vars
	//for(size_t i=0; i<this->captured_vars.size(); ++i)
	//	field_types.push_back(this->captured_vars[i].type->LLVMType(context));

	// Make the vector of fields for the closure type
	vector<llvm::Type*> closure_field_types;
	closure_field_types.push_back(TypeRef(new Int())->LLVMType(context)); // Ref count field
	closure_field_types.push_back(func_ptr_type);
	closure_field_types.push_back(LLVMTypeUtils::getBaseCapturedVarStructType(context)); // cap_var_struct);

	// Return the closure structure type.
	llvm::StructType* closure_struct_type = llvm::StructType::get(
		context,
		closure_field_types
	);

	//std::cout << "closure_struct_type: " << std::endl;
	closure_struct_type->dump();
	//std::cout << std::endl;
	

	// Return pointer to structure type.
	return LLVMTypeUtils::pointerType(*closure_struct_type);
}


//==========================================================================


const std::string ArrayType::toString() const
{ 
	return "array<" + elem_type->toString() + ", " + ::toString(num_elems) + ">";
}


bool ArrayType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is an Array as well.
	const ArrayType* b_ = static_cast<const ArrayType*>(&b);

	return this->elem_type->matchTypes(*b_->elem_type, type_mapping);
}


llvm::Type* ArrayType::LLVMType(llvm::LLVMContext& context) const
{
	// Since Array is pass-by-pointer, return the element value type here.
	// Then Array will be passed as pointer to this type.
	//return this->t->LLVMType(context); //pointerToVoidLLVMType(context);
	return llvm::ArrayType::get(
		this->elem_type->LLVMType(context), // Element type
		this->num_elems // Num elements
	);
}


//==========================================================================


bool Map::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	throw BaseException("Map::matchTypes: unimplemented.");
}


llvm::Type* Map::LLVMType(llvm::LLVMContext& context) const
{
	return LLVMTypeUtils::voidPtrType(context);
}


//==========================================================================


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


llvm::Type* StructureType::LLVMType(llvm::LLVMContext& context) const
{
	vector<llvm::Type*> field_types(this->component_types.size());
	for(size_t i=0; i<this->component_types.size(); ++i)
	{
		field_types[i] = this->component_types[i]->LLVMType(context);
	}

	return llvm::StructType::get(
		context,
		field_types
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


//==========================================================================


const std::string VectorType::toString() const
{
	return "vector<" + this->elem_type->toString() + ", " + ::toString(this->num) + ">";
}


bool VectorType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is a VectorType as well.
	const VectorType* b_ = static_cast<const VectorType*>(&b);

	return this->num == b_->num && this->elem_type->matchTypes(*b_->elem_type, type_mapping);
}


llvm::Type* VectorType::LLVMType(llvm::LLVMContext& context) const
{
	return llvm::VectorType::get(
		this->elem_type->LLVMType(context),
		this->num
	);
}


//===============================================================================


const std::string OpaqueType::toString() const
{
	return "opaque";
}


bool OpaqueType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	//NOTE: This right?
	return this->getType() == b.getType();
}


llvm::Type* OpaqueType::LLVMType(llvm::LLVMContext& context) const
{
	return LLVMTypeUtils::voidPtrType(context);
}


//==========================================================================


const std::string SumType::toString() const
{
	std::vector<std::string> typenames(this->types.size());
	for(size_t i=0; i<this->types.size(); ++i)
		typenames[i] = this->types[i]->toString();

	return StringUtils::join(typenames, " | ");
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


llvm::Type* SumType::LLVMType(llvm::LLVMContext& context) const
{
	std::vector<llvm::Type*> elem_types(1 + this->types.size());

	// Type discriminator field first.
	elem_types[0] = llvm::Type::getInt32Ty(context);

	for(size_t i=0; i<this->types.size(); ++i)
		elem_types[i + 1] = this->types[i]->LLVMType(context);

	return llvm::StructType::get(
		context,
		elem_types
	);
}


//===============================================================================


const std::string ErrorType::toString() const
{
	return "error";
}


bool ErrorType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	//NOTE: This right?
	return this->getType() == b.getType();
}


llvm::Type* ErrorType::LLVMType(llvm::LLVMContext& context) const
{
	return llvm::Type::getVoidTy(context);
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
