/*=====================================================================
wnt_Type.h
----------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/VRef.h>
#include <utils/RefCounted.h>
#include <vector>
#include <string>
#include <set>


namespace llvm { class Type; class Constant; class LLVMContext; class Value; class Module; class StructType; class FunctionType; }


namespace Winter
{


class Value;
class EmitLLVMCodeParams;
class EmitOpenCLCodeParams;
class TupleType;
struct ConstTypeRefLessThan;
struct ConstTypeVRefLessThan;


class Type : public RefCounted
{
public:
	enum TypeType
	{
		GenericTypeType,
		FloatType,
		DoubleType,
		IntType,
		StringType,
		CharTypeType,
		BoolType,
		MapType,
		ArrayTypeType,
		VArrayTypeType,
		FunctionType,
		StructureTypeType,
		VectorTypeType,
		OpaqueTypeType,
		SumTypeType,
		ErrorTypeType,
		TupleTypeType,
		//OpaqueStructureTypeType,
		OpenCLImageTypeType
	};

	Type(TypeType t) : type(t) {}
	virtual ~Type(){}

	virtual const std::string toString() const = 0;
	virtual bool lessThan(const Type& b) const = 0;
	virtual bool matchTypes(const Type& b, std::vector<Reference<Type> >& type_mapping) const = 0;
	virtual llvm::Type* LLVMType(llvm::Module& module) const = 0;
	virtual llvm::Type* LLVMStructType(llvm::Module& module) const { return LLVMType(module); }
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const = 0;
	virtual bool OpenCLPassByPointer() const { return false; }

	// If passByValue() is true, arguments of this type are passed directly as arguments.  If false, their address is taken and the pointer is passed instead.
	// Similarly for function return values, non pass-by-value types are returned via the first SRET pointer argument.
	// Structures are not pass-by-value, however some heap-allocated types are, such as the string type.
	virtual bool passByValue() const { return true; }
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const; // Default implementation does nothing.

	// Emit decrementor
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const; // Default implementation does nothing.

	// Emit destructor
	virtual void emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const; // Default implementation does nothing.

	virtual bool hasDestructor() const { return false; }
	virtual void getContainedTypesWithDestructors(std::set<VRef<const Type>, ConstTypeVRefLessThan>& types) const {}
	virtual bool containsType(const Type& other_type) const { return false; }
	virtual bool isHeapAllocated() const { return false; } // same as 'is refcounted'.
	virtual bool requiresCompareEqualFunction() const { return false; } // Do we need to make a __compare_equal function for this type, or can we use some code built into llvm.
	virtual size_t memSize() const = 0; // in bytes

	inline TypeType getType() const { return type; }

	std::string address_space; // For OpenCL code output
private:
	TypeType type;
};


typedef Reference<Type> TypeRef;
typedef VRef<Type> TypeVRef;
typedef Reference<const Type> ConstTypeRef;
typedef VRef<const Type> ConstTypeVRef;


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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
	virtual size_t memSize() const { return 4; }
};


class Double : public Type
{
public:
	Double() : Type(DoubleType) {}
	virtual const std::string toString() const { return "double"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
	virtual size_t memSize() const { return 8; }
};


class GenericType : public Type
{
public:
	GenericType(const std::string& name_, int generic_type_param_index_) : Type(GenericTypeType), name(name_), generic_type_param_index(generic_type_param_index_) {}
	virtual const std::string toString() const { return name; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const { return name; }
	int genericTypeParamIndex() const { return generic_type_param_index; }
	virtual size_t memSize() const { return 0; }
private:
	std::string name;
	int generic_type_param_index;
};


class Int : public Type
{
public:
	Int(int num_bits_ = 32, bool is_signed_ = true) : Type(IntType), num_bits(num_bits_), is_signed(is_signed_) { assert(num_bits == 16 || num_bits == 32 || num_bits == 64); }
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

			if((int)is_signed < (int)b_int.is_signed)
				return true;
			else if((int)b_int.is_signed < (int)is_signed)
				return false;
			else
				return num_bits < b_int.num_bits;
		}
	}

	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual llvm::Value* getInvalidLLVMValue(llvm::Module& module) const; // For array out-of-bounds
	virtual Reference<Value> getInvalidValue() const; // For array out-of-bounds
	virtual size_t memSize() const;

	int numBits() const { return num_bits; }
//private:
	int num_bits; // Should be 16, 32 or 64
	bool is_signed;
};


class Bool : public Type
{
public:
	Bool() : Type(BoolType) {}
	virtual const std::string toString() const { return "bool"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const { return "bool"; }
	virtual size_t memSize() const { return 1; }
};


class String : public Type
{
public:
	String() : Type(StringType) {}
	virtual const std::string toString() const { return "string"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual llvm::Type* LLVMStructType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const { return "string"; }
	virtual bool passByValue() const { return true; } // Pass the pointer 'by value'
	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;

	virtual bool hasDestructor() const { return true; }
	virtual bool isHeapAllocated() const { return true; }
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return 24; } // just returning header size here
};


class CharType : public Type
{
public:
	CharType() : Type(CharTypeType) {}
	virtual const std::string toString() const { return "char"; }
	virtual bool lessThan(const Type& b) const { return getType() < b.getType(); }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const { return "char"; }
	virtual bool passByValue() const { return true; }
	virtual size_t memSize() const { return 1; }
};


// This is the type for function values.
// Note that the type really consists of the complete closure
class Function : public Type
{
public:
	Function(const std::vector<TypeVRef>& arg_types_, const TypeVRef& return_type_, 
		//const std::vector<TypeRef>& captured_var_types_,
		bool use_captured_vars_) 
		:	Type(FunctionType), arg_types(arg_types_), return_type(return_type_), 
		//captured_var_types(captured_var_types_), 
			use_captured_vars(use_captured_vars_){}

	TypeVRef return_type;
	std::vector<TypeVRef> arg_types;
	//std::vector<TypeRef> captured_var_types;
	bool use_captured_vars;


	static int functionPtrIndex() { return 2; } // Index in closure struct of function ptr.
	static int destructorPtrIndex() { return 3; } // Index in closure struct of closure destructor ptr.
	static int capturedVarStructIndex() { return 4; } // Index in closure struct of captured var structure.

	llvm::FunctionType* destructorLLVMType(llvm::Module& module) const;
	llvm::FunctionType* functionLLVMType(llvm::Module& module) const;
	llvm::StructType* closureLLVMStructType(llvm::Module& module) const;

	// Use for passing to ref counting functions etc..
	static VRef<Function> dummyFunctionType() { return new Function(std::vector<TypeVRef>(), new Int(), true); }

	virtual const std::string toString() const; // { return "function"; }
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual llvm::Type* LLVMStructType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	// Pass by reference, because the actual value passed/returned is a closure structure.
	virtual bool passByValue() const { return true; } // Pass the pointer 'by value'

	virtual bool hasDestructor() const { return true; } // Variables in closure may need decrementing/destroying.
	virtual bool isHeapAllocated() const { return true; } // same as 'is refcounted'.

	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;

	virtual void emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const;
	virtual void getContainedTypesWithDestructors(std::set<ConstTypeVRef, ConstTypeVRefLessThan>& types) const;
	virtual bool containsType(const Type& other_type) const;
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return 24; } // TODO: work this out

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
	Map(const TypeVRef& a, const TypeVRef& b) : Type(MapType), from_type(a), to_type(b) {}

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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool passByValue() const { return false; }
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return 24; } // Just header size

	TypeVRef from_type;
	TypeVRef to_type;
};


class ArrayType : public Type
{
public:
	ArrayType(const VRef<Type>& elem_type_, size_t num_elems_) : Type(ArrayTypeType), elem_type(elem_type_), num_elems(num_elems_) {}
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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool passByValue() const { return false; }
	virtual bool containsType(const Type& other_type) const;
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return 24; } // Just header size


	VRef<Type> elem_type;
	size_t num_elems;
};


// Variable-length array
class VArrayType : public Type
{
public:
	VArrayType(const TypeVRef& elem_type_) : Type(VArrayTypeType), elem_type(elem_type_) {}


	llvm::Type* LLVMDataArrayType(llvm::Module& module) const;

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
			const VArrayType& b_array = static_cast<const VArrayType&>(b);
			return this->elem_type->lessThan(*b_array.elem_type);
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual llvm::Type* LLVMStructType(llvm::Module& module) const;
	
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool passByValue() const { return true; } // Pass the pointer 'by value'

	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const;

	virtual bool hasDestructor() const { return true; }
	virtual void getContainedTypesWithDestructors(std::set<ConstTypeVRef, ConstTypeVRefLessThan>& types) const;
	virtual bool containsType(const Type& other_type) const;
	virtual bool isHeapAllocated() const { return true; }
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return 24; } // Just header size

	TypeVRef elem_type;
};


class StructureType : public Type
{
public:
	StructureType(const std::string& name_, const std::vector<TypeVRef>& component_types_, const std::vector<std::string>& component_names_);

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
			const StructureType& b_struct = static_cast<const StructureType&>(b);

			return this->name < b_struct.name;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;

	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool OpenCLPassByPointer() const { return true; }
	virtual bool passByValue() const { return false; }

	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const;

	virtual bool hasDestructor() const;
	virtual void getContainedTypesWithDestructors(std::set<ConstTypeVRef, ConstTypeVRefLessThan>& types) const;
	virtual bool containsType(const Type& other_type) const;
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const;

	const std::string definitionString() const; // Winter definition string, e.g "struct a { float b }"
	const std::string getOpenCLCDefinition(EmitOpenCLCodeParams& params, bool emit_comments) const; // Get full definition string, e.g. "struct a { float b; };"
	const std::string getOpenCLCConstructor(EmitOpenCLCodeParams& params, bool emit_comments) const; // Emit constructor for type

	std::vector<Reference<TupleType> > getElementTupleTypes() const;

	std::string name;
	std::vector<TypeVRef> component_types;
	std::vector<std::string> component_names;
};

typedef Reference<StructureType> StructureTypeRef;
typedef VRef<StructureType> StructureTypeVRef;


class TupleType : public Type
{
public:
	TupleType(const std::vector<TypeVRef>& component_types_);

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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool OpenCLPassByPointer() const { return true; }
	virtual bool passByValue() const { return false; }

	const std::string getOpenCLCDefinition(EmitOpenCLCodeParams& params, bool emit_comments) const; // Get full definition string, e.g. struct a { float b; };
	const std::string getOpenCLCConstructor(EmitOpenCLCodeParams& params, bool emit_comments) const; // Emit constructor for type

	virtual void emitIncrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDecrRefCount(EmitLLVMCodeParams& params, llvm::Value* ref_counted_value, const std::string& comment) const;
	virtual void emitDestructorCall(EmitLLVMCodeParams& params, llvm::Value* value, const std::string& comment) const;
	
	virtual bool hasDestructor() const { return true; }
	virtual void getContainedTypesWithDestructors(std::set<ConstTypeVRef, ConstTypeVRefLessThan>& types) const;
	virtual bool containsType(const Type& other_type) const;
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const;


	std::vector<TypeVRef> component_types;
};

typedef Reference<TupleType> TupleTypeRef;


class VectorType : public Type
{
public:
	VectorType(const TypeVRef& elem_type_, unsigned int num_)
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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool requiresCompareEqualFunction() const { return true; }
	virtual size_t memSize() const { return elem_type->memSize() * num; }

	TypeVRef elem_type;
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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual size_t memSize() const { return 8; }
};


class SumType : public Type
{
public:
	SumType(const std::vector<TypeVRef>& types_)
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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual size_t memSize() const;

	std::vector<TypeVRef> types;
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
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual size_t memSize() const;
};


// An unknown named type.  Useful for parsing an isolated piece of source code where not all types are known.
// NOTE: Instead of using this type, for now we will try the check_structures_exist flag for LangParser::parseBuffer().
// It can just be set to false for parsing isolated bits of code.
/*class OpaqueStructureType : public Type
{
public:
	OpaqueStructureType(const std::string& name_);

	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a OpaqueStructureType as well
			const OpaqueStructureType& b_struct = static_cast<const OpaqueStructureType&>(b);

			return this->name < b_struct.name;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool OpenCLPassByPointer() const { return true; }
	virtual size_t memSize() const { return 0; }

	std::string name;
};*/


class OpenCLImageType : public Type
{
public:
	enum ImageType
	{
		ImageType_Image2D,
		ImageType_Image3D,
		ImageType_Image2DArray,
		ImageType_Image1D,
		ImageType_Image1DBuffer,
		ImageType_Image1DArray
	};
	OpenCLImageType(ImageType image_type);

	virtual const std::string toString() const;
	virtual bool lessThan(const Type& b) const
	{
		if(getType() < b.getType())
			return true;
		else if(b.getType() < getType())
			return false;
		else
		{
			// else b is a OpenCLImageType as well
			const OpenCLImageType& b_struct = static_cast<const OpenCLImageType&>(b);

			return this->image_type < b_struct.image_type;
		}
	}
	virtual bool matchTypes(const Type& b, std::vector<TypeRef>& type_mapping) const;
	virtual llvm::Type* LLVMType(llvm::Module& module) const;
	virtual const std::string OpenCLCType(EmitOpenCLCodeParams& params) const;
	virtual bool OpenCLPassByPointer() const { return false; }
	virtual size_t memSize() const { return 0; }

	ImageType image_type;
};


// Some utility methods:

inline std::vector<TypeVRef> typeSinglet(const TypeVRef& a)
{
	std::vector<TypeVRef> v;
	v.reserve(1);
	v.push_back(a);
	return v;
}


inline std::vector<TypeVRef> typePair(const TypeVRef& a, const TypeVRef& b)
{
	std::vector<TypeVRef> v;
	v.reserve(2);
	v.push_back(a);
	v.push_back(b);
	return v;
}


inline std::vector<TypeVRef> typeTriplet(const TypeVRef& a, const TypeVRef& b, const TypeVRef& c)
{
	std::vector<TypeVRef> v;
	v.reserve(3);
	v.push_back(a);
	v.push_back(b);
	v.push_back(c);
	return v;
}


inline std::vector<TypeVRef> typeQuad(const TypeVRef& a, const TypeVRef& b, const TypeVRef& c, const TypeVRef& d)
{
	std::vector<TypeVRef> v;
	v.reserve(4);
	v.push_back(a);
	v.push_back(b);
	v.push_back(c);
	v.push_back(d);
	return v;
}


inline std::vector<TypeVRef> typePentuplet(const TypeVRef& a, const TypeVRef& b, const TypeVRef& c, const TypeVRef& d, const TypeVRef& e)
{
	std::vector<TypeVRef> v;
	v.reserve(5);
	v.push_back(a);
	v.push_back(b);
	v.push_back(c);
	v.push_back(d);
	v.push_back(e);
	return v;
}


inline std::vector<TypeVRef> typeSextuplet(const TypeVRef& a, const TypeVRef& b, const TypeVRef& c, const TypeVRef& d, const TypeVRef& e, const TypeVRef& f)
{
	std::vector<TypeVRef> v;
	v.reserve(6);
	v.push_back(a);
	v.push_back(b);
	v.push_back(c);
	v.push_back(d);
	v.push_back(e);
	v.push_back(f);
	return v;
}



TypeVRef errorTypeSum(const TypeVRef& t);


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


struct TypeRefLessThan
{
	bool operator() (const TypeRef& a, const TypeRef& b) const { return *a < *b; }
};
struct TypeVRefLessThan
{
	bool operator() (const TypeVRef& a, const TypeVRef& b) const { return *a < *b; }
};


struct ConstTypeRefLessThan
{
	bool operator() (const Reference<const Type>& a, const Reference<const Type>& b) const { return *a < *b; }
};
struct ConstTypeVRefLessThan
{
	bool operator() (const VRef<const Type>& a, const VRef<const Type>& b) const { return *a < *b; }
};

inline bool isEqualToOrContains(const Type& a, const Type& b)
{
	return a == b || a.containsType(b);
}


} // end namespace Winter
