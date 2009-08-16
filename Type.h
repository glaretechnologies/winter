//Copyright 2009 Nicholas Chapman
#pragma once


#include <vector>
#include <string>
#include "../../indigosvn/trunk/utils/reference.h"
#include "../../indigosvn/trunk/utils/refcounted.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"


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
		FunctionType
	};

	virtual TypeType getType() const = 0;
	virtual const std::string toString() const = 0;
	virtual bool lessThan(const Type& b) const = 0;
	virtual const llvm::Type* LLVMType() const = 0;
};


typedef Reference<Type> TypeRef;


class Float : public Type
{
public:
	virtual TypeType getType() const { return FloatType; }
	virtual const std::string toString() const { return "float"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual const llvm::Type* LLVMType() const { return llvm::Type::FloatTy; }
};


class Int : public Type
{
public:
	virtual TypeType getType() const { return IntType; }
	virtual const std::string toString() const { return "int"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual const llvm::Type* LLVMType() const { return llvm::Type::Int32Ty; }
};


class Bool : public Type
{
public:
	virtual TypeType getType() const { return BoolType; }
	virtual const std::string toString() const { return "bool"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual const llvm::Type* LLVMType() const { return llvm::Type::Int1Ty; }
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
	virtual const llvm::Type* LLVMType() const { return NULL; }
};


class Function : public Type
{
public:
	Function(const std::vector<TypeRef>& arg_types_, TypeRef return_type_) : arg_types(arg_types_), return_type(return_type_) {}

	TypeRef return_type;
	std::vector<TypeRef> arg_types;

	virtual TypeType getType() const { return FunctionType; }
	virtual const std::string toString() const { return "function"; }
	virtual const llvm::Type* LLVMType() const { return NULL; }
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
	virtual const llvm::Type* LLVMType() const { return NULL; }

	TypeRef from_type;
	TypeRef to_type;
};


inline bool operator < (const Type& a, const Type& b)
{
	return a.lessThan(b);
}

inline bool operator == (const Type& a, const Type& b)
{
	return !a.lessThan(b) && !b.lessThan(a);
}

}

