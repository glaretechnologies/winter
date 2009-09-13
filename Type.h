//Copyright 2009 Nicholas Chapman
#pragma once


#include <vector>
#include <string>
#include "../../indigosvn/trunk/utils/reference.h"
#include "../../indigosvn/trunk/utils/refcounted.h"

namespace llvm { class Type; }
#if USE_LLVM
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#endif

namespace Winter
{


class Type : public RefCounted
{
public:
	virtual ~Type(){}

	enum TypeType
	{
		FloatType,
		IntType,
		StringType,
		BoolType,
		MapType,
		ArrayTypeType,
		FunctionType,
		StructureTypeType,
	};

	virtual TypeType getType() const = 0;
	virtual const std::string toString() const = 0;
	virtual bool lessThan(const Type& b) const = 0;
#if LLVM
	virtual const llvm::Type* LLVMType() const = 0;
#endif
};


typedef Reference<Type> TypeRef;


class Float : public Type
{
public:
	virtual TypeType getType() const { return FloatType; }
	virtual const std::string toString() const { return "float"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
#if LLVM
	virtual const llvm::Type* LLVMType() const { return llvm::Type::FloatTy; }
#endif
};


class Int : public Type
{
public:
	virtual TypeType getType() const { return IntType; }
	virtual const std::string toString() const { return "int"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
#if LLVM
	virtual const llvm::Type* LLVMType() const { return llvm::Type::Int32Ty; }
#endif
};


class Bool : public Type
{
public:
	virtual TypeType getType() const { return BoolType; }
	virtual const std::string toString() const { return "bool"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
#if LLVM
	virtual const llvm::Type* LLVMType() const { return llvm::Type::Int1Ty; }
#endif
};


class Tuple : public Type
{
	std::vector<TypeRef> types;
};

class TupleN : public Type
{
	TypeRef t;
	int n;
};

class String : public Type
{
public:
	virtual TypeType getType() const { return StringType; }
	virtual const std::string toString() const { return "string"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
#if LLVM
	virtual const llvm::Type* LLVMType() const { return NULL; }
#endif
};


class Function : public Type
{
public:
	Function(const std::vector<TypeRef>& arg_types_, TypeRef return_type_) : arg_types(arg_types_), return_type(return_type_) {}

	TypeRef return_type;
	std::vector<TypeRef> arg_types;

	virtual TypeType getType() const { return FunctionType; }
	virtual const std::string toString() const; // { return "function"; }
#if LLVM
	virtual const llvm::Type* LLVMType() const { return NULL; }
#endif
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
#if LLVM
	virtual const llvm::Type* LLVMType() const { return NULL; }
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

	TypeRef t;
};


class StructureType : public Type
{
public:
	StructureType(const std::string& name_, std::vector<TypeRef> component_types_, std::vector<std::string> component_names_) 
	: name(name_), component_types(component_types_), component_names(component_names_) {}

	virtual TypeType getType() const { return StructureTypeType; }
	virtual const std::string toString() const { return "struct " + name; }
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

#if LLVM
	virtual const llvm::Type* LLVMType() const { return NULL; }
#endif

	std::string name;
	std::vector<TypeRef> component_types;
	std::vector<std::string> component_names;
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

