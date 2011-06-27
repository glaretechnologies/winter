//Copyright 2009 Nicholas Chapman
#pragma once


#include <vector>
#include <string>
#include <utils/reference.h>
#include <utils/refcounted.h>

namespace llvm { class Type; class Constant; }
#if USE_LLVM
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#endif

namespace Winter
{


class Value;


class Type : public RefCounted
{
public:
	virtual ~Type(){}

	enum TypeType
	{
		GenericTypeType,
		FloatType,
		IntType,
		StringType,
		BoolType,
		MapType,
		ArrayTypeType,
		FunctionType,
		StructureTypeType,
		VectorTypeType,
		VoidPtrTypeType
	};

	virtual TypeType getType() const = 0;
	virtual const std::string toString() const = 0;
	virtual bool lessThan(const Type& b) const = 0;
	virtual bool matchTypes(const Type& b, std::vector<Reference<Type> >& type_mapping) const = 0;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const = 0;
	virtual llvm::Constant* defaultLLVMValue(llvm::LLVMContext& context);
	virtual bool passByValue() const { return true; }
#endif
	//virtual Reference<Value> getDefaultValue();
};


typedef Reference<Type> TypeRef;


class Float : public Type
{
public:
	virtual TypeType getType() const { return FloatType; }
	virtual const std::string toString() const { return "float"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const { return llvm::Type::getFloatTy(context); }
	virtual llvm::Constant* defaultLLVMValue(llvm::LLVMContext& context);
#endif
};


class GenericType : public Type
{
public:
	GenericType(int generic_type_param_index_) : generic_type_param_index(generic_type_param_index_) {}
	virtual TypeType getType() const { return GenericTypeType; }
	virtual const std::string toString() const { return "generic"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
#endif
	const int genericTypeParamIndex() const { return generic_type_param_index; }
private:
	int generic_type_param_index;
};


class Int : public Type
{
public:
	virtual TypeType getType() const { return IntType; }
	virtual const std::string toString() const { return "int"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const { return llvm::Type::getInt32Ty(context); }
#endif
};


class Bool : public Type
{
public:
	virtual TypeType getType() const { return BoolType; }
	virtual const std::string toString() const { return "bool"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const { return llvm::Type::getInt1Ty(context); }
#endif
};


/*class Tuple : public Type
{
	std::vector<TypeRef> types;
};

class TupleN : public Type
{
	TypeRef t;
	int n;
};*/


class String : public Type
{
public:
	virtual TypeType getType() const { return StringType; }
	virtual const std::string toString() const { return "string"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
	virtual bool passByValue() const { return false; }
#endif
};


// This is the type for function values.
// Note that the type really consists of the complete closure
class Function : public Type
{
public:
	Function(const std::vector<TypeRef>& arg_types_, TypeRef return_type_, 
		//const std::vector<TypeRef>& captured_var_types_,
		bool use_captured_vars_) 
		:	arg_types(arg_types_), return_type(return_type_), 
		//captured_var_types(captured_var_types_), 
			use_captured_vars(use_captured_vars_){}

	TypeRef return_type;
	std::vector<TypeRef> arg_types;
	//std::vector<TypeRef> captured_var_types;
	bool use_captured_vars;

	virtual TypeType getType() const { return FunctionType; }
	virtual const std::string toString() const; // { return "function"; }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
#endif
	// Pass by reference, because the actual value passed/returned is a closure structure.
	virtual bool passByValue() const { return true; }
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// Else b is a function as well.

			const Function& b_func = dynamic_cast<const Function&>(b);
			
			if(return_type->lessThan(*b_func.return_type))
				return true;
			else if(b_func.return_type->lessThan(*return_type))
				return false;
			else
			{
				if(arg_types.size() < b_func.arg_types.size())
					return true;
				else if(arg_types.size() > b_func.arg_types.size())
					return false;
				else
				{
					for(unsigned int i=0; i<arg_types.size(); ++i)
					{
						if(arg_types[i]->lessThan(*b_func.arg_types[i]))
							return true;
						else if(b_func.arg_types[i]->lessThan(*arg_types[i]))
							return false;
					}

					return false; // Both types are the same.
				}
			}
		}
	}
};


class Map : public Type
{
public:
	Map(TypeRef a, TypeRef b) : from_type(a), to_type(b) {}

	virtual TypeType getType() const { return MapType; }
	virtual const std::string toString() const { return "map<" + from_type->toString() + ", " + to_type->toString() + ">"; }
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			const Map& bmap = dynamic_cast<const Map&>(b);

			// else b is a map as well
			if(from_type->lessThan(*bmap.from_type))
				return true;
			else if(bmap.from_type->lessThan(*from_type))
				return false;
			else
			{
				// Else from_type == b.from_type
				return to_type < bmap.to_type;
			}
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
	virtual bool passByValue() const { return false; }
#endif

	TypeRef from_type;
	TypeRef to_type;
};


class ArrayType : public Type
{
public:
	ArrayType(const TypeRef& t_) : t(t_) {}
	virtual TypeType getType() const { return ArrayTypeType; }
	virtual const std::string toString() const { return "array<" + t->toString() + ">"; }
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a structure as well
			const ArrayType& b_array = dynamic_cast<const ArrayType&>(b);

			return this->t->lessThan(*b_array.t);
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
	virtual bool passByValue() const { return false; }
#endif


	TypeRef t;
};


class StructureType : public Type
{
public:
	StructureType(const std::string& name_, std::vector<TypeRef> component_types_, std::vector<std::string> component_names_) 
	: name(name_), component_types(component_types_), component_names(component_names_) {}

	virtual TypeType getType() const { return StructureTypeType; }
	virtual const std::string toString() const { return /*"struct " + */name; }
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a structure as well
			const StructureType& b_struct = dynamic_cast<const StructureType&>(b);

			return this->name < b_struct.name;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
	virtual bool passByValue() const { return false; }
#endif

	std::string name;
	std::vector<TypeRef> component_types;
	std::vector<std::string> component_names;
};


class VectorType : public Type
{
public:
	VectorType(const TypeRef t_, unsigned int num_)
	: t(t_), num(num_) {}

	virtual TypeType getType() const { return VectorTypeType; }
	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a VectorType as well
			const VectorType& b_vector = dynamic_cast<const VectorType&>(b);

			if(this->num < b_vector.num)
				return true;
			else if(this->num > b_vector.num)
				return false;
			else
				return this->t->lessThan(*b_vector.t);
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
#endif

	TypeRef t;
	unsigned int num;
};


// Something like void*
class VoidPtrType : public Type
{
public:
	VoidPtrType() {}

	virtual TypeType getType() const { return VoidPtrTypeType; }
	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			return false;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

#if USE_LLVM
	virtual const llvm::Type* LLVMType(llvm::LLVMContext& context) const;
#endif
};


inline bool operator < (const Type& a, const Type& b)
{
	return a.lessThan(b);
}

inline bool operator == (const Type& a, const Type& b)
{
	return !a.lessThan(b) && !b.lessThan(a);
}

inline bool operator != (const Type& a, const Type& b)
{
	return a.lessThan(b) || b.lessThan(a);
}

}

