/*=====================================================================
CompiledValue.cpp
-----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "CompiledValue.h"


#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include "utils/StringUtils.h"
#include "maths/mathstypes.h"


using std::vector;


namespace Winter
{


//=====================================================================================


static int64 max_heap_mem_usage = 0;
static int64 cur_heap_mem_usage = 0;


static void* heapAlloc(size_t size)
{
	cur_heap_mem_usage += size;
	max_heap_mem_usage = myMax(max_heap_mem_usage, cur_heap_mem_usage);

	// Allocate an extra 8 bytes to store allocation size (for now), so that we can use it in heapFree().
	uint64* p = (uint64*)::malloc(size + sizeof(uint64));
	*p = size;
	return p + 1;
}


static void heapFree(void* ptr)
{
	uint64* original_ptr = (uint64*)ptr - 1;
	const uint64 allocation_size = *((uint64*)ptr - 1);
	cur_heap_mem_usage -= allocation_size;
	::free(original_ptr);
}


void resetMemUsageStats()
{
	max_heap_mem_usage = 0;
	cur_heap_mem_usage = 0;
}


uint64 getMaxHeapMemUsage()
{
	return max_heap_mem_usage;
}


uint64 getCurrentHeapMemUsage()
{
	return cur_heap_mem_usage;
}


//=====================================================================================


static void* getVoidPtrArg(const vector<ValueRef>& arg_values, int i)
{
	assert(arg_values[i]->valueType() == Value::ValueType_VoidPtr);
	return static_cast<const VoidPtrValue*>(arg_values[i].getPointer())->value;
}


VArrayRep* allocateVArray(uint64 elem_size_B, uint64 num_elems)
{
	// Allocate space for the reference count, length, flags, and data.
	VArrayRep* varray = (VArrayRep*)heapAlloc(sizeof(VArrayRep) + elem_size_B * num_elems);
	return varray;
}


ValueRef allocateVArrayInterpreted(const vector<ValueRef>& args)
{
	assert(0);
	return NULL;
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
int free(VArrayRep* varray)
{
	assert(varray->refcount == 1);
	heapFree(varray);
	return 0;
}


//=====================================================================================


StringRep* allocateStringWithLen(size_t len)
{
	StringRep* string_val = (StringRep*)heapAlloc(sizeof(StringRep) + len);
	string_val->refcount = 0;
	string_val->len = len;
	string_val->flags = 1; // heap allocated
	return string_val;
}


StringRep* allocateString(const char* initial_string_val)
{
	const size_t string_len = strlen(initial_string_val);
	StringRep* string_val = allocateStringWithLen(string_len);
	std::memcpy((char*)string_val + sizeof(StringRep), initial_string_val, string_len);
	return string_val;
}


ValueRef allocateStringInterpreted(const vector<ValueRef>& args)
{
	return new StringValue((const char*)(getVoidPtrArg(args, 0)));
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
int free(StringRep* str)
{
	//assert(str->refcount == 1);
	heapFree(str);
	return 0;
}


//=====================================================================================


} // end namespace Winter
