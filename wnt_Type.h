/*=====================================================================
wnt_Type.h
----------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/RefCounted.h>
#include <vector>
#include <string>


namespace llvm { class Type; class Constant; class LLVMContext; class Value; class Module; }


namespace Winter
{


class Value;


class Type : public RefCounted
{
public:
	enum TypeType
	{
		GenericTypeType,
		FloatType,
		IntType,
		StringType,
		CharTypeType,
		BoolType,
		MapType,
		ArrayTypeType,
		FunctionType,
		StructureTypeType,
		VectorTypeType,
		OpaqueTypeType,
		SumTypeType,
		ErrorTypeType,
		TupleTypeType
	};

	Type(TypeType t) : type(t) {}
	virtual ~Type(){}

	virtual const std::string toString() const = 0;
	virtual bool lessThan(const Type& b) const = 0;
	virtual bool matchTypes(const Type& b, std::vector<Reference<Type> >& type_mapping) const = 0;
	virtual llvm::Type* LLVMType(llvm::Module& module) const = 0;
	virtual const std::string OpenCLCType() const = 0;
	virtual bool passByValue() const { return true; }
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds

	inline TypeType getType() const { return type; }
private:
	TypeType type;
};


typedef Reference<Type> TypeRef;


inline bool operator < (const Type& a, const Type& b);
inline bool operator > (const Type& a, const Type& b);
inline bool operator == (const Type& a, const Type& b);
inline bool operator != (const Type& a, const Type& b);


class Float : public Type
{
public:
	Float() : Type(FloatType) {}
	virtual const std::string toString() const { return "float"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "float"; }
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
};


class GenericType : public Type
{
public:
	GenericType(int generic_type_param_index_) : Type(GenericTypeType), generic_type_param_index(generic_type_param_index_) {}
	virtual const std::string toString() const { return "generic"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "generic"; }
	const int genericTypeParamIndex() const { return generic_type_param_index; }
private:
	int generic_type_param_index;
};


class Int : public Type
{
public:
	Int(int num_bits_ = 32) : Type(IntType), num_bits(num_bits_) { assert(num_bits == 32 || num_bits == 64); }
	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{ 
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is an Int as well.
			const Int& b_int = static_cast<const Int&>(b);
			return num_bits < b_int.num_bits;
		}
	}

	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds

	int numBits() const { return num_bits; }
private:
	int num_bits; // Should be 32 or 64
};


class Bool : public Type
{
public:
	Bool() : Type(BoolType) {}
	virtual const std::string toString() const { return "bool"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "bool"; }
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
	String() : Type(StringType) {}
	virtual const std::string toString() const { return "string"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "string"; }
	virtual bool passByValue() const { return true; } // Pass the pointer 'by value'
};


class CharType : public Type
{
public:
	CharType() : Type(CharTypeType) {}
	virtual const std::string toString() const { return "char"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "char"; }
	virtual bool passByValue() const { return true; }
};


// This is the type for function values.
// Note that the type really consists of the complete closure
class Function : public Type
{
public:
	Function(const std::vector<TypeRef>& arg_types_, const TypeRef& return_type_, 
		//const std::vector<TypeRef>& captured_var_types_,
		bool use_captured_vars_) 
		:	Type(FunctionType), arg_types(arg_types_), return_type(return_type_), 
		//captured_var_types(captured_var_types_), 
			use_captured_vars(use_captured_vars_){}

	TypeRef return_type;
	std::vector<TypeRef> arg_types;
	//std::vector<TypeRef> captured_var_types;
	bool use_captured_vars;

	virtual const std::string toString() const; // { return "function"; }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
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

			const Function& b_func = static_cast<const Function&>(b);
			
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
	Map(const TypeRef& a, const TypeRef& b) : Type(MapType), from_type(a), to_type(b) {}

	virtual const std::string toString() const { return "map<" + from_type->toString() + ", " + to_type->toString() + ">"; }
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			const Map& bmap = static_cast<const Map&>(b);

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
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
	virtual bool passByValue() const { return false; }

	TypeRef from_type;
	TypeRef to_type;
};


class ArrayType : public Type
{
public:
	ArrayType(const TypeRef& elem_type_, size_t num_elems_) : Type(ArrayTypeType), elem_type(elem_type_), num_elems(num_elems_) { assert(elem_type_.nonNull()); }
	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a structure as well
			const ArrayType& b_array = static_cast<const ArrayType&>(b);

			if(num_elems < b_array.num_elems)
				return true;
			else if(num_elems > b_array.num_elems)
				return false;
			else
				return this->elem_type->lessThan(*b_array.elem_type);
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
	virtual bool passByValue() const { return false; }


	TypeRef elem_type;
	size_t num_elems;
};


class StructureType : public Type
{
public:
	StructureType(const std::string& name_, const std::vector<TypeRef>& component_types_, const std::vector<std::string>& component_names_);

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
			const StructureType& b_struct = static_cast<const StructureType&>(b);

			return this->name < b_struct.name;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return name; }
	virtual bool passByValue() const { return false; }

	const std::string getOpenCLCDefinition(); // Get full definition string, e.g. struct a { float b; };


	std::string name;
	std::vector<TypeRef> component_types;
	std::vector<std::string> component_names;
};

typedef Reference<StructureType> StructureTypeRef;


class TupleType : public Type
{
public:
	TupleType(const std::vector<TypeRef>& component_types_);

	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a structure as well
			const TupleType& b_tuple = static_cast<const TupleType&>(b);

			if(component_types.size() < b_tuple.component_types.size())
				return true;
			else if(component_types.size() > b_tuple.component_types.size())
				return false;

			for(size_t i=0; i<component_types.size(); ++i)
			{
				if(*component_types[i] < *b_tuple.component_types[i])
					return true;
				else if(*component_types[i] > *b_tuple.component_types[i])
					return false;
			}

			return false; // equal
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
	virtual bool passByValue() const { return false; }

	const std::string getOpenCLCDefinition(); // Get full definition string, e.g. struct a { float b; };

	std::vector<TypeRef> component_types;
};


class VectorType : public Type
{
public:
	VectorType(const TypeRef& elem_type_, unsigned int num_)
	: Type(VectorTypeType), elem_type(elem_type_), num(num_) {}

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
			const VectorType& b_vector = static_cast<const VectorType&>(b);

			if(this->num < b_vector.num)
				return true;
			else if(this->num > b_vector.num)
				return false;
			else
				return this->elem_type->lessThan(*b_vector.elem_type);
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;

	TypeRef elem_type;
	unsigned int num;
};


// Something like void*
class OpaqueType : public Type
{
public:
	OpaqueType() : Type(OpaqueTypeType) {}

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
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const { return "void*"; }
};


class SumType : public Type
{
public:
	SumType(const std::vector<TypeRef>& types_)
	:	Type(SumTypeType), types(types_) {}

	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a SumType as well
			const SumType& b_sumtype = static_cast<const SumType&>(b);

			if(this->types.size() < b_sumtype.types.size())
				return true;
			else if(this->types.size() > b_sumtype.types.size())
				return false;
			else
			{
				for(size_t i=0; i<this->types.size(); ++i)
				{
					if((*this->types[i]) < (*b_sumtype.types[i]))
						return true;
					else if((*b_sumtype.types[i]) < (*this->types[i]))
						return false;
				}

				return false;
			}
		}
	}

	virtual bool passByValue() const { return false; }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;

	std::vector<TypeRef> types;
};


class ErrorType : public Type
{
public:
	ErrorType() : Type(ErrorTypeType) {}

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
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType() const;
};


// Some utility methods:
TypeRef errorTypeSum(const TypeRef& t);


inline bool operator < (const Type& a, const Type& b)
{
	return a.lessThan(b);
}

inline bool operator > (const Type& a, const Type& b)
{
	return b.lessThan(a);
}

inline bool operator == (const Type& a, const Type& b)
{
	return !a.lessThan(b) && !b.lessThan(a);
}

inline bool operator != (const Type& a, const Type& b)
{
	return a.lessThan(b) || b.lessThan(a);
}


} // end namespace Winter
