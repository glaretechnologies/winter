#include "wnt_Type.h"


#include "utils/stringutils.h"
#include "BaseException.h"
#include "llvm/Constants.h"
#include <vector>


using namespace std;


namespace Winter
{


//==========================================================================


static llvm::Type* pointerToVoidLLVMType(llvm::LLVMContext& context)
{
	return llvm::PointerType::get(llvm::Type::getInt32Ty(context), 0);
}



//==========================================================================


llvm::Constant* Type::defaultLLVMValue(llvm::LLVMContext& context)
{
	assert(0);
	return NULL;
}


//==========================================================================


bool Float::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


llvm::Constant* Float::defaultLLVMValue(llvm::LLVMContext& context)
{
	return llvm::ConstantFP::get(context, llvm::APFloat(0.0f));
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


const llvm::Type* GenericType::LLVMType(llvm::LLVMContext& context) const
{
	assert(0);
	return NULL;
}


//==========================================================================


bool Int::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


//==========================================================================


bool Bool::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


//==========================================================================


bool String::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	return this->getType() == b.getType();
}


const llvm::Type* String::LLVMType(llvm::LLVMContext& context) const
{
	return pointerToVoidLLVMType(context);
}


//==========================================================================


bool Function::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a Function as well.
	const Function* b_ = dynamic_cast<const Function*>(&b);
	if(this->arg_types.size() != b_->arg_types.size())
		return false;

	for(unsigned int i=0;i<arg_types.size(); ++i)
		if(!this->arg_types[i]->matchTypes(*b_->arg_types[i], type_mapping))
			return false;

	if(!this->return_type->matchTypes(*b_->return_type, type_mapping))
		return false;
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


const llvm::Type* Function::LLVMType(llvm::LLVMContext& context) const
{
	return pointerToVoidLLVMType(context);
}


//==========================================================================


bool ArrayType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is an Array as well.
	const ArrayType* b_ = dynamic_cast<const ArrayType*>(&b);

	return this->t->matchTypes(*b_->t, type_mapping);
}


const llvm::Type* ArrayType::LLVMType(llvm::LLVMContext& context) const
{
	return pointerToVoidLLVMType(context);
}


//==========================================================================


bool Map::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	throw BaseException("Map::matchTypes: unimplemented.");
}


const llvm::Type* Map::LLVMType(llvm::LLVMContext& context) const
{
	return pointerToVoidLLVMType(context);
}


//==========================================================================


bool StructureType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;
	// So b is a StructureType as well.
	const StructureType* b_ = dynamic_cast<const StructureType*>(&b);

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


const llvm::Type* StructureType::LLVMType(llvm::LLVMContext& context) const
{
	//return pointerToVoidLLVMType(context);
	vector<const llvm::Type*> field_types(this->component_types.size());
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
	return "vector<" + this->t->toString() + ", " + ::toString(this->num) + ">";
}


bool VectorType::matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const
{
	if(this->getType() != b.getType())
		return false;

	// So b is a VectorType as well.
	const VectorType* b_ = dynamic_cast<const VectorType*>(&b);

	return this->num == b_->num && this->t->matchTypes(*b_->t, type_mapping);
}


const llvm::Type* VectorType::LLVMType(llvm::LLVMContext& context) const
{
	return llvm::VectorType::get(
		this->t->LLVMType(context),
		this->num
	);
}


} // end namespace Winter