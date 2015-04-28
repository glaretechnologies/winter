#include "Value.h"


#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "utils/StringUtils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Constants.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;


namespace Winter
{


//------------------------------------------------------------------------------------------


llvm::Constant* IntValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	return llvm::ConstantInt::get(
		*params.context,
		llvm::APInt(32, this->value, 
			true // signed
		)
	);
}


//------------------------------------------------------------------------------------------


const std::string FloatValue::toString() const
{
	return ::toString(this->value);
}


llvm::Constant* FloatValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	return llvm::ConstantFP::get(
		*params.context, 
		llvm::APFloat(this->value)
	);
}


//------------------------------------------------------------------------------------------


llvm::Constant* BoolValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	return llvm::ConstantInt::get(
		*params.context, 
		llvm::APInt(
			1, // num bits
			this->value ? 1 : 0, // value
			false // signed
		)
	);
}


//------------------------------------------------------------------------------------------


llvm::Constant* StringValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(0);
	return NULL;
}


//------------------------------------------------------------------------------------------


llvm::Constant* CharValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(0);
	return NULL;
}


//------------------------------------------------------------------------------------------


llvm::Constant* MapValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(0);
	return NULL;
}


//------------------------------------------------------------------------------------------


StructureValue::~StructureValue()
{
	//for(unsigned int i=0; i<this->fields.size(); ++i)
	//	delete fields[i];
}


Value* StructureValue::clone() const
{
	vector<ValueRef> field_clones(this->fields.size());

	for(unsigned int i=0; i<this->fields.size(); ++i)
		field_clones[i] = this->fields[i]->clone();

	return new StructureValue(field_clones);
}


llvm::Constant* StructureValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(type->getType() == Type::StructureTypeType);

	vector<llvm::Constant*> llvm_fields(this->fields.size());
	for(unsigned int i=0; i<this->fields.size(); ++i)
		llvm_fields[i] = this->fields[i]->getConstantLLVMValue(params, type.downcast<StructureType>()->component_types[i]);


	return llvm::ConstantStruct::get(
		(llvm::StructType*)type->LLVMType(*params.module),
		llvm_fields
	);
}


//------------------------------------------------------------------------------------------


llvm::Constant* FunctionValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(0);
	return NULL;
}


//------------------------------------------------------------------------------------------


ArrayValue::~ArrayValue()
{
}


Value* ArrayValue::clone() const
{
	ArrayValue* ret = new ArrayValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = this->e[i]->clone();

	return ret;
}


const std::string ArrayValue::toString() const
{
	std::string s = "array[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


llvm::Constant* ArrayValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(type->getType() == Type::ArrayTypeType);

	vector<llvm::Constant*> llvm_elems(this->e.size());
	for(unsigned int i=0; i<this->e.size(); ++i)
		llvm_elems[i] = this->e[i]->getConstantLLVMValue(params, type.downcast<ArrayType>()->elem_type);


	assert(type->LLVMType(*params.module)->isArrayTy());

	return llvm::ConstantArray::get(
		(llvm::ArrayType*)type->LLVMType(*params.module),
		llvm_elems
	);
}


//------------------------------------------------------------------------------------------


VectorValue::~VectorValue()
{
	//for(unsigned int i=0; i<this->e.size(); ++i)
	//	delete e[i];
}


Value* VectorValue::clone() const
{
	VectorValue* ret = new VectorValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = this->e[i]->clone();

	return ret;
}


const std::string VectorValue::toString() const
{
	std::string s = "vector[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


llvm::Constant* VectorValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(type->getType() == Type::VectorTypeType);

	vector<llvm::Constant*> llvm_elems(this->e.size());
	for(unsigned int i=0; i<this->e.size(); ++i)
		llvm_elems[i] = this->e[i]->getConstantLLVMValue(params, type.downcast<StructureType>()->component_types[i]);


	return llvm::ConstantVector::get(
		llvm_elems
	);
}


//------------------------------------------------------------------------------------------


TupleValue::~TupleValue()
{
}


Value* TupleValue::clone() const
{
	TupleValue* ret = new TupleValue();
	ret->e.resize(this->e.size());

	for(unsigned int i=0; i<this->e.size(); ++i)
		ret->e[i] = this->e[i]->clone();

	return ret;
}


const std::string TupleValue::toString() const
{
	std::string s = "tuple[";
	for(unsigned int i=0; i<this->e.size(); ++i)
	{
		s += this->e[i]->toString() + (i + 1 < e.size() ? ", " : "");
	}
	return s + "]";
}


llvm::Constant* TupleValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(type->getType() == Type::TupleTypeType);

	vector<llvm::Constant*> llvm_fields(this->e.size());
	for(unsigned int i=0; i<this->e.size(); ++i)
		llvm_fields[i] = this->e[i]->getConstantLLVMValue(params, type.downcast<TupleType>()->component_types[i]);


	return llvm::ConstantStruct::get(
		(llvm::StructType*)type->LLVMType(*params.module),
		llvm_fields
	);
}


//------------------------------------------------------------------------------------------


const std::string VoidPtrValue::toString() const
{
	return "void* " + ::toString((uint64)this->value);
}


llvm::Constant* VoidPtrValue::getConstantLLVMValue(EmitLLVMCodeParams& params, const Reference<Type>& type) const
{
	assert(0);
	return NULL;
}


}
