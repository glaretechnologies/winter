/*=====================================================================
CompiledValue.h
---------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "Value.h"
#include <string>
#include <vector>


namespace Winter
{


// In-memory string representation for a Winter VArray
class VArrayRep
{
public:
	uint64 refcount;
	uint64 len;
	uint64 flags;
	// Data follows..
};


// In-memory string representation for a Winter string
class StringRep
{
public:
	uint64 refcount;
	uint64 len;
	uint64 flags;
	// Data follows..

	uint64 getRefCount() const { return refcount; }
	const std::string toStdString() { return std::string(((const char*)this) + sizeof(StringRep), ((const char*)this) + sizeof(StringRep) + len); }
};


VArrayRep* allocateVArray(uint64 elem_size_B, uint64 num_elems);
int free(VArrayRep* varray_rep);
ValueRef allocateVArrayInterpreted(const std::vector<ValueRef>& args);


StringRep* allocateStringWithLen(size_t len);
StringRep* allocateString(const char* initial_string_val);
int free(StringRep* str);
ValueRef allocateStringInterpreted(const std::vector<ValueRef>& args);


void resetMemUsageStats();
uint64 getMaxHeapMemUsage();
uint64 getCurrentHeapMemUsage();


/*=====================================================================
CompiledValRef
--------------
=====================================================================*/
template <class T>
class CompiledValRef
{
public:
	CompiledValRef()
	:	ob(0)
	{}

	CompiledValRef(T* ob_)
	{
		ob = ob_;

		if(ob)
			ob->refcount++;
	}

	template<class T2>
	CompiledValRef(const CompiledValRef<T2>& other)
	{
		ob = other.getPointer();

		if(ob)
			ob->refcount++;
	}

	CompiledValRef(const CompiledValRef<T>& other)
	{
		ob = other.ob;

		if(ob)
			ob->refcount++;
	}

	~CompiledValRef()
	{
		if(ob)
		{
			int64 new_ref_count = --ob->refcount;
			if(new_ref_count == 0)
				free(ob);
		}
	}

	
	CompiledValRef& operator = (const CompiledValRef& other)
	{
		T* old_ob = ob;

		ob = other.ob;
		// NOTE: if a reference is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ob)
			ob->refcount++;

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ob)
		{
			int64 ref_count = --ob->refcount;
			if(ref_count == 0)
				free(old_ob);
		}

		return *this;
	}


	// An assignment operator from a raw pointer.
	// We have this method so that the code 
	//
	// Reference<T> ref;
	// ref = new T();
	//
	// will work without a temporary reference object being created and then assigned to ref.
	CompiledValRef& operator = (T* new_ob)
	{
		T* old_ob = ob;

		ob = new_ob;
		// NOTE: if a reference is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ob)
			ob->refcount++;

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ob)
		{
			int64 ref_count = --ob->refcount;
			if(ref_count == 0)
				free(old_ob);
		}

		return *this;
	}


	// Compares the pointer values.
	bool operator < (const CompiledValRef& other) const
	{
		return ob < other.ob;
	}


	// Compares the pointer values.
	bool operator == (const CompiledValRef& other) const
	{
		return ob == other.ob;
	}


	inline T& operator * ()
	{
		assert(ob);
		return *ob;
	}

	
	inline const T& operator * () const
	{
		assert(ob);
		return *ob;
	}


	inline T* operator -> () const
	{
		return ob;
	}

	inline T* getPointer() const
	{
		return ob;
	}

	// Alias for getPointer()
	inline T* ptr() const
	{
		return ob;
	}

	inline bool isNull() const
	{
		return ob == 0;
	}
	inline bool nonNull() const
	{
		return ob != 0;
	}

	inline T* getPointer()
	{
		return ob;
	}

	// Alias for getPointer()
	inline T* ptr()
	{
		return ob;
	}

	template <class T2>
	inline bool isType() const
	{
		return dynamic_cast<const T2*>(ob) != 0;
	}
	
private:
	T* ob;
};



} // end namespace Winter

