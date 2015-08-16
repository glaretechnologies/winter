/*=====================================================================
VirtualMachine.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#include "VirtualMachine.h"


#include <iostream>
#include <cassert>
#include <fstream>
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/UTF8Utils.h"
#include "utils/ContainerUtils.h"
#include "utils/TaskManager.h"
#include "utils/Exception.h"
#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_RefCounting.h"
#include "wnt_ASTNode.h"
#include "wnt_Frame.h"
#include "wnt_LLVMVersion.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "LanguageTests.h"
#include "VirtualMachine.h"
#include "LLVMTypeUtils.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include "llvm/IR/Module.h"
#if TARGET_LLVM_VERSION >= 36
#include "llvm/IR/Verifier.h"
#else
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/ObjectImage.h"
#endif
#include "llvm/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Analysis/Lint.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <iostream>


using std::vector;
using std::string;


namespace Winter
{
	

static const bool DUMP_MODULE_IR = false; // Dumps to "unoptimised_module_IR.txt", "optimised_module_IR.txt" in current working dir.
static const bool DUMP_ASSEMBLY = false; // Dumpts to "module_assembly.txt" in current working dir.


static const bool USE_MCJIT = true;


//=====================================================================================


static void* getVoidPtrArg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const VoidPtrValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const VoidPtrValue*>(arg_values[i].getPointer())->value;
}


static const std::string& getStringArg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const StringValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const StringValue*>(arg_values[i].getPointer())->value;
}


static int64 getInt64Arg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const IntValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const IntValue*>(arg_values[i].getPointer())->value;
}


static const std::string& getCharArg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const CharValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const CharValue*>(arg_values[i].getPointer())->value;
}


static int64 string_count = 0;//TEMP
static int64 varray_count = 0;//TEMP
static int64 closure_count = 0;//TEMP



void debugIncrStringCount()
{
	string_count++;
}


void debugDecrStringCount()
{
	string_count--;
}


static StringRep* allocateString(const char* initial_string_val/*, void* env*/)
{
	string_count++;//TEMP

	const size_t string_len = strlen(initial_string_val);
	StringRep* string_val = (StringRep*)malloc(sizeof(StringRep) + string_len);
	std::memcpy((char*)string_val + sizeof(StringRep), initial_string_val, string_len);
	return string_val;
}


static ValueRef allocateStringInterpreted(const vector<ValueRef>& args)
{
	return new StringValue((const char*)(getVoidPtrArg(args, 0)));
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
static int freeString(StringRep* str/*, void* env*/)
{
	string_count--;//TEMP

	assert(str->refcount == 1);
	free(str);
	return 0;
}


// Returns number of unicode chars in the string.
static int stringLength(StringRep* str)
{
	const uint64 num_bytes = str->len;
	const char* const data = (const char*)str + sizeof(StringRep);
	return (int)UTF8Utils::numCodePointsInString(data, num_bytes);
}


static ValueRef stringLengthInterpreted(const vector<ValueRef>& args)
{
	return new IntValue( (int)UTF8Utils::numCodePointsInString(getStringArg(args, 0)) );
}


static StringRep* concatStrings(StringRep* a, StringRep* b, void* env)
{
	string_count++;//TEMP

	const uint64 new_len = a->len + b->len;
	StringRep* new_s = (StringRep*)malloc(sizeof(StringRep) + new_len);
	new_s->refcount = 1;
	new_s->len = new_len;
	new_s->flags = 1; // heap allocated
	std::memcpy((uint8*)new_s + sizeof(StringRep), (uint8*)a + sizeof(StringRep), a->len);
	std::memcpy((uint8*)new_s + sizeof(StringRep) + a->len, (uint8*)b + sizeof(StringRep), b->len);
	return new_s;
}


static ValueRef concatStringsInterpreted(const vector<ValueRef>& args)
{
	return new StringValue( getStringArg(args, 0) + getStringArg(args, 1) );
}


static StringRep* charToString(uint32 c, void* env)
{
	string_count++;//TEMP

	// Work out number of bytes used
	const int num_bytes = (int)UTF8Utils::numBytesForChar((uint8)(c & 0xFF));

	StringRep* new_s = (StringRep*)malloc(sizeof(StringRep) + num_bytes);
	new_s->refcount = 1;
	new_s->len = num_bytes;
	new_s->flags = 1; // heap allocated
	std::memcpy((uint8*)new_s + sizeof(StringRep), &c, num_bytes); // Copy data
	return new_s;
}


static ValueRef charToStringInterpreted(const vector<ValueRef>& args)
{
	return new StringValue(getCharArg(args, 0));
}


// codePoint(char c) int
static int codePoint(uint32 c, void* env)
{
	return (int)UTF8Utils::codePointForUTF8Char(c);
}


static ValueRef codePointInterpreted(const vector<ValueRef>& args)
{
	return new IntValue(UTF8Utils::codePointForUTF8CharString(getCharArg(args, 0)));
}


static int stringElem(StringRep* s, uint64 index)
{
	try
	{
		const uint8* data = (const uint8*)s + sizeof(StringRep);
		return (int)UTF8Utils::charAt(data, s->len, index);
	}
	catch(Indigo::Exception&)
	{
		assert(0); // Out of bounds read
		return 0;
	}
}


static ValueRef stringElemInterpreted(const vector<ValueRef>& args)
{
	try
	{
		const std::string& s = getStringArg(args, 0);
		const int64 index = getInt64Arg(args, 1);
		const uint32 c = UTF8Utils::charAt((const uint8*)s.c_str(), s.size(), index);
		return new CharValue(UTF8Utils::charString(c));
	}
	catch(Indigo::Exception& e)
	{
		// Out of bounds read.
		throw BaseException(e.what());
	}
}


//=====================================================================================


// In-memory string representation for a Winter VArray
class VArrayRep
{
public:
	uint64 refcount;
	uint64 len;
	uint64 flags;
	// Data follows..
};


static VArrayRep* allocateVArray(uint64 elem_size_B, uint64 num_elems)
{
	varray_count++;//TEMP

	// Allocate space for the reference count, length, flags, and data.
	VArrayRep* varray = (VArrayRep*)malloc(sizeof(VArrayRep) + elem_size_B * num_elems);
	return varray;
}


static ValueRef allocateVArrayInterpreted(const vector<ValueRef>& args)
{
	assert(0);
	return NULL;
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
static int freeVArray(VArrayRep* varray)
{
	assert(varray_count >= 1);
	varray_count--;//TEMP

	assert(varray->refcount == 1);
	free(varray);
	return 0;
}


//=====================================================================================


// In-memory string representation for a Winter VArray
class ClosureRep
{
public:
	uint64 refcount;
	uint64 len; // not used currently.
	uint64 flags;
	// Data follows..
};


static ClosureRep* allocateClosure(uint64 size_B)
{
	closure_count++;//TEMP

	// Allocate space for the reference count, length, flags, and data.
	ClosureRep* closure = (ClosureRep*)malloc(size_B);
	closure->refcount = 666;
	closure->len = 666;
	closure->flags = 666;
	//closure->flags = 1; // heap allocated
	return closure;
}


static ValueRef allocateClosureInterpreted(const vector<ValueRef>& args)
{
	assert(0);
	return NULL;
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
static int freeClosure(ClosureRep* closure)
{
	closure_count--;//TEMP

	assert(closure->refcount == 1);
	free(closure);
	return 0;
}


//=====================================================================================
class ExecArrayMapTask;

static Indigo::TaskManager* winter_global_task_manager = NULL;
static std::vector<Reference<ExecArrayMapTask> > exec_array_map_tasks;

typedef void (WINTER_JIT_CALLING_CONV * VARRAY_WORK_FUNCTION) (uint64* output, uint64* input, size_t begin, size_t end); // Winter code

class ExecMapTask : public Indigo::Task
{
public:
	virtual void run(size_t thread_index)
	{
		// Call back into Winter JITed code to compute the map on this slice.
		work_function(output, input, begin, end);
	}

	uint64* output;
	uint64* input;
	size_t begin;
	size_t end;
	VARRAY_WORK_FUNCTION work_function;
};


void execVArrayMap(uint64* output, uint64* input, VARRAY_WORK_FUNCTION work_function)
{
	const size_t input_len = input[1];
	assert(output[1] == input_len);
	const size_t num_threads = winter_global_task_manager->getNumThreads();
	size_t slice_size = input_len / num_threads;
	if(slice_size * num_threads < input_len) // If slice_size was rounded down:
		slice_size++;

	for(size_t i=0; i<num_threads; ++i)
	{
		Reference<ExecMapTask> t = new ExecMapTask();
		t->begin = i*slice_size;
		t->end = myMin(input_len, (i+1)*slice_size);
		t->work_function = work_function;
		winter_global_task_manager->addTask(t);
	}

	winter_global_task_manager->waitForTasksToComplete();
}


typedef void (WINTER_JIT_CALLING_CONV * ARRAY_WORK_FUNCTION) (void* output, void* input, void* map_function, size_t begin, size_t end); // Winter code

class ExecArrayMapTask : public Indigo::Task
{
public:
	virtual void run(size_t thread_index)
	{
		// Call back into Winter JITed code to compute the map on this slice.
		//work_function(output, input, map_function, begin, end);
		work_function( (float*)output + begin, (float*)input + begin, map_function, 0, end - begin);
	}

	void* output;
	void* input;
	void* map_function;
	size_t begin;
	size_t end;
	ARRAY_WORK_FUNCTION work_function;
};


void execArrayMap(void* output, void* input, size_t array_size, void* map_function, ARRAY_WORK_FUNCTION work_function)
{
	const size_t input_len = array_size;
	const size_t num_threads = winter_global_task_manager->getNumThreads();
	size_t slice_size = input_len / num_threads;
	if(slice_size * num_threads < input_len) // If slice_size was rounded down:
		slice_size++;

	for(size_t i=0; i<num_threads; ++i)
	{
		Reference<ExecArrayMapTask> t = exec_array_map_tasks[i]; // new ExecArrayMapTask();
		t->output = output;
		t->input = input;
		t->map_function = map_function;
		t->begin = i*slice_size;
		t->end = myMin(input_len, (i+1)*slice_size);
		t->work_function = work_function;
		winter_global_task_manager->addTask(t);
	}

	winter_global_task_manager->waitForTasksToComplete();
}


//=====================================================================================


static void tracePrintFloat(const char* var_name, float x)
{
	std::cout << std::string(var_name) << " = " << toString(x) << std::endl;
}


#if TARGET_LLVM_VERSION >= 34
#else
static float getFloatArg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const FloatValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const FloatValue*>(arg_values[i].getPointer())->value;
}


static ValueRef powInterpreted(const vector<ValueRef>& args)
{
	return new FloatValue(std::pow(getFloatArg(args, 0), getFloatArg(args, 1)));
}
#endif


// We need to define our own memory manager so that we can supply our own getPointerToNamedFunction() method, that will return pointers to external functions.
// This seems to be necessary when using MCJIT.
class WinterMemoryManager : public llvm::SectionMemoryManager
{
public:
	virtual ~WinterMemoryManager() {}


#if TARGET_LLVM_VERSION >= 34
	virtual uint64_t getSymbolAddress(const std::string& name)
	{
		std::map<std::string, void*>::iterator res = func_map->find(name);
		if(res != func_map->end())
			return (uint64_t)res->second;

		if(name == "execArrayMap")
			return (uint64_t)execArrayMap;
		if(name == "tracePrintFloat")
			return (uint64_t)tracePrintFloat;

		// For some reason, DynamicLibrary::SearchForAddressOfSymbol() doesn't seem to work on Windows 32-bit.  So just manually resolve these symbols.
		if(name == "sinf")
			return (uint64_t)sinf;
		else if(name == "cosf")
			return (uint64_t)cosf;
		else if(name == "powf")
			return (uint64_t)powf;
		else if(name == "expf")
			return (uint64_t)expf;
		else if(name == "logf")
			return (uint64_t)logf;
		else if(name == "memcpy")
			return (uint64_t)memcpy;

		// For OS X:
		if(name == "_sinf")
			return (uint64_t)sinf;
		else if(name == "_cosf")
			return (uint64_t)cosf;
		else if(name == "_powf")
			return (uint64_t)powf;
		else if(name == "_expf")
			return (uint64_t)expf;
		else if(name == "_logf")
			return (uint64_t)logf;
		else if(name == "_memcpy")
			return (uint64_t)memcpy;
#if defined(OSX)
		else if(name == "_memset_pattern16")
			return (uint64_t)memset_pattern16;
		else if(name == "_exp2f")
			return (uint64_t)exp2f;
#endif

		// If function was not in func_map (i.e. was not an 'external' function), then use normal symbol resolution, for functions like sinf, cosf etc..

		void* f = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);

		assert(f);
		if(!f)
			throw BaseException("Internal error: failed to find symbol '" + name + "'");
		return (uint64_t)f;
	}
#else
	virtual void *getPointerToNamedFunction(const std::string& name, bool AbortOnFailure = true)
	{
		std::map<std::string, void*>::iterator res = func_map->find(name);
		if(res != func_map->end())
			return res->second;

		// If function was not in func_map (i.e. was not an 'external' function), then use normal symbol resolution, for functions like sinf, cosf etc..

		// For some reason, DynamicLibrary::SearchForAddressOfSymbol() doesn't seem to work on Windows 32-bit.  So just manually resolve these symbols.
		if(name == "sinf")
			return (void*)sinf;
		else if(name == "cosf")
			return (void*)cosf;
		else if(name == "powf")
			return (void*)powf;
		else if(name == "expf")
			return (void*)expf;
		else if(name == "logf")
			return (void*)logf;

		// For OS X:
		if(name == "_sinf")
			return (void*)sinf;
		else if(name == "_cosf")
			return (void*)cosf;
		else if(name == "_powf")
			return (void*)powf;
		else if(name == "_expf")
			return (void*)expf;
		else if(name == "_logf")
			return (void*)logf;

		void* f = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);

		assert(f);
		if(!f)
			throw BaseException("Internal error: failed to find symbol '" + name + "'");
		return f;
	}
#endif
	
	std::map<std::string, void*>* func_map;
};



VirtualMachine::VirtualMachine(const VMConstructionArgs& args)
:	linker(
		true, // hidden_voidptr_arg
		args.env
	),
	env(args.env),
	llvm_context(NULL),
	llvm_module(NULL),
	llvm_exec_engine(NULL)
{
	assert(string_count == 0 && varray_count == 0 && closure_count == 0);

	stats.num_heap_allocation_calls = 0;
	stats.num_closure_allocations = 0;
	
	/*if(!winter_global_task_manager)
	{
		const size_t num_threads = 8;
		winter_global_task_manager = new Indigo::TaskManager(num_threads);
		exec_array_map_tasks.resize(num_threads);
		for(size_t i=0; i<num_threads; ++i)
			exec_array_map_tasks[i] = new ExecArrayMapTask();
	}*/

	try
	{
		hidden_voidptr_arg = true;

		this->external_functions = args.external_functions;


		// Add allocateString
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)allocateString,
				allocateStringInterpreted, // interpreted func
				FunctionSignature("allocateString", vector<TypeRef>(1, new OpaqueType())),
				new String() // return type
			)));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeString
		{
			vector<TypeRef> arg_types(1, new String());
			//arg_types.push_back(new VoidPtrType());
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)freeString,
				NULL, // interpreted func TEMP
				FunctionSignature("freeString", arg_types),
				new Int() // return type
			)));
			external_functions.back()->has_side_effects = true;
		}

		// Add length (stringLength)
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)stringLength,
			stringLengthInterpreted, // interpreted func
			FunctionSignature("length", vector<TypeRef>(1, new String())),
			new Int() // return type
		)));

		// Add concatStrings
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)concatStrings,
			concatStringsInterpreted, // interpreted func
			FunctionSignature("concatStrings", vector<TypeRef>(2, new String())),
			new String() // return type
		)));

		// Add toString(char)
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)charToString,
			charToStringInterpreted, // interpreted func
			FunctionSignature("toString", vector<TypeRef>(1, new CharType())),
			new String() // return type
		)));

		// Add codePoint(char c) int
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)codePoint,
			codePointInterpreted, // interpreted func
			FunctionSignature("codePoint", vector<TypeRef>(1, new CharType())),
			new Int() // return type
		)));

		// Add elem(string s, uint64 index) char
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)stringElem,
			stringElemInterpreted, // interpreted func
			FunctionSignature("elem", typePair(new String(), new Int(64))),
			new CharType() // return type
		)));

		
		// Add allocateVArray
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)allocateVArray,
				allocateVArrayInterpreted, // interpreted func
				FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int(64))),
				new VArrayType(new Int()) //new OpaqueType() // return type.  Just make this a void*, will cast return value to correct type
			)));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeVArray
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)freeVArray,
				NULL, // interpreted func TEMP
				FunctionSignature("freeVArray", vector<TypeRef>(1, new VArrayType(new Int()))), // new OpaqueType())),
				new Int() // return type
			)));
			external_functions.back()->has_side_effects = true;
		}

		const TypeRef dummy_func_type = Function::dummyFunctionType();

		// Add allocateClosure
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)allocateClosure,
				allocateClosureInterpreted, // interpreted func
				FunctionSignature("allocateClosure", vector<TypeRef>(1, new Int(64))),
				dummy_func_type // return type
			)));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeClosure
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)freeClosure,
				NULL, // interpreted func TEMP
				FunctionSignature("freeClosure", vector<TypeRef>(1, dummy_func_type)),
				new Int() // return type
			)));
			external_functions.back()->has_side_effects = true;
		}


		// Load source buffers
		loadSource(args, args.source_buffers, args.preconstructed_func_defs);

		if(args.build_llvm_code)
		{

			this->llvm_context = new llvm::LLVMContext();
			this->llvm_module = new llvm::Module("WinterModule", *this->llvm_context);

			//llvm::TargetOptions to;
			//const char* argv[] = { "dummyprogname", "-vectorizer-min-trip-count=4"};
			//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");
			//const char* argv[] = { "dummyprogname", "-debug"};
			//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");

#if TARGET_LLVM_VERSION >= 36
			llvm::EngineBuilder engine_builder(std::unique_ptr<llvm::Module>(this->llvm_module));
#else
			llvm::EngineBuilder engine_builder(this->llvm_module);
#endif
			engine_builder.setEngineKind(llvm::EngineKind::JIT);
#if TARGET_LLVM_VERSION <= 34
			if(USE_MCJIT) engine_builder.setUseMCJIT(true);
#endif


			if(USE_MCJIT)
			{
				WinterMemoryManager* mem_manager = new WinterMemoryManager();
				mem_manager->func_map = &func_map;
#if TARGET_LLVM_VERSION >= 36
				engine_builder.setMCJITMemoryManager(std::unique_ptr<llvm::SectionMemoryManager>(mem_manager));
#elif TARGET_LLVM_VERSION >= 34
				engine_builder.setMCJITMemoryManager(mem_manager);
#else
				engine_builder.setJITMemoryManager(mem_manager);
#endif
			}

			this->triple = llvm::sys::getProcessTriple();
			if(USE_MCJIT) this->triple.append("-elf"); // MCJIT requires the -elf suffix currently, see https://groups.google.com/forum/#!topic/llvm-dev/DOmHEXhNNWw

			// Select the host computer architecture as the target.
			this->target_machine = engine_builder.selectTarget(
				llvm::Triple(this->triple), // target triple
				"",  // march
				llvm::sys::getHostCPUName(), // mcpu - e.g. "corei7", "core-avx2"
				llvm::SmallVector<std::string, 4>());

			// Enable floating point op fusion, to allow for FMA codegen.
			this->target_machine->Options.AllowFPOpFusion = llvm::FPOpFusion::Fast;

			this->llvm_exec_engine = engine_builder.create(target_machine);


			this->llvm_exec_engine->DisableLazyCompilation();
			//this->llvm_exec_engine->DisableSymbolSearching(); // Symbol searching is required for sin, pow intrinsics etc..


		
			/*ExternalFunctionRef alloc_ref(new ExternalFunction());
			alloc_ref->interpreted_func = NULL;
			alloc_ref->return_type = TypeRef(new OpaqueType());
			alloc_ref->sig = FunctionSignature("allocateRefCountedStructure", std::vector<TypeRef>(1, TypeRef(new Int())));
			alloc_ref->func = (void*)(allocateRefCountedStructure);
			this->external_functions.push_back(alloc_ref);*/

		
		

			// There is a problem with LLVM 3.3 and earlier with the pow intrinsic getting turned into exp2f().
			// So for now just use our own pow() external function.
#if TARGET_LLVM_VERSION < 34
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)(float(*)(float, float))std::pow,
				powInterpreted,
				FunctionSignature("pow", vector<TypeRef>(2, new Float())),
				new Float()
			)));
#endif

			for(unsigned int i=0; i<this->external_functions.size(); ++i)
				addExternalFunction(this->external_functions[i], *this->llvm_context, *this->llvm_module);

			this->build(args);
		}
	}
	catch(BaseException& e)
	{
		// Since we threw an exception in the constructor, the destructor will not be run.
		// So we need to delete these objects now.
		delete this->llvm_exec_engine;

		delete llvm_context;

		throw e; // Re-throw exception
	}
}


VirtualMachine::~VirtualMachine()
{
	// llvm_exec_engine will delete llvm_module.
	assert(string_count == 0 && varray_count == 0 && closure_count == 0);

	delete this->llvm_exec_engine;

	delete llvm_context;
}


void VirtualMachine::init()
{
	// Since we will be calling LLVM functions from multiple threads, we need to call this.
#if TARGET_LLVM_VERSION < 36
	llvm::llvm_start_multithreaded();
#endif

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
}


void VirtualMachine::shutdown() // Calls llvm_shutdown()
{
	llvm::llvm_shutdown(); // This calls llvm::llvm_stop_multithreaded() as well (on <= LLVM 3.4 at least).
}


void VirtualMachine::addExternalFunction(const ExternalFunctionRef& f, llvm::LLVMContext& context, llvm::Module& module)
{
	llvm::FunctionType* llvm_f_type = LLVMTypeUtils::llvmFunctionType(
		f->sig.param_types,
		false, // captured vars struct ptr
		f->return_type, 
		module
	);

	llvm::Function* llvm_f = static_cast<llvm::Function*>(module.getOrInsertFunction(
		makeSafeStringForFunctionName(f->sig.toString()), // Name
		llvm_f_type // Type
	));

	if(USE_MCJIT)
	{
		func_map[makeSafeStringForFunctionName(f->sig.toString())] = f->func;

		// On OS X, the requested symbol names seem to have an underscore prepended, so add an entry with a leading underscore.
		func_map["_" + makeSafeStringForFunctionName(f->sig.toString())] = f->func;
	}
	else
	{
		// This doesn't seem to work with MCJIT:
		this->llvm_exec_engine->addGlobalMapping(llvm_f, f->func);
	}
}


static void optimiseFunctions(llvm::FunctionPassManager& fpm, llvm::Module* module, bool verbose)
{
	fpm.doInitialization();

	for(llvm::Module::iterator i = module->begin(); i != module->end(); ++i)
	{
		if(!i->isIntrinsic())
		{
			fpm.run(*i);
		}
	}

	fpm.doFinalization();
}


// Returns if tree changed.
bool VirtualMachine::doInliningPass()
{
	// Do a CountFunctionCalls pass.  This will compute payload.calls_to_func_count.
	std::vector<ASTNode*> stack;
	TraversalPayload payload(TraversalPayload::CountFunctionCalls);
	payload.linker = &linker;
	{
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);
	}

	for(size_t i=0; i<linker.top_level_defs.size(); ++i)
	{
		std::unordered_set<std::string> used_names;
		// Do a pass over this function to get the set of names used, so that we can avoid them when generating new names for inlined let vars.
		payload.operation = TraversalPayload::GetAllNamesInScope;
		payload.used_names = &used_names;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);

		payload.operation = TraversalPayload::InlineFunctionCalls;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);
	}

	const bool tree_changed = payload.tree_changed;

	// Rebind, some function expressions may be statically bound now.
	if(tree_changed)
	{
		TraversalPayload payload(TraversalPayload::BindVariables);
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);
	}

	return tree_changed;
}


bool VirtualMachine::doDeadCodeEliminationPass()
{
	std::vector<ASTNode*> stack;
	TraversalPayload payload(TraversalPayload::DeadCodeElimination_ComputeAlive);
	for(size_t i=0; i<linker.top_level_defs.size(); ++i)
	{
		//std::cout << "\n=====DCE pass before:======\n";
		//linker.top_level_defs[i]->print(0, std::cout);

		// Do a pass to get the set of live LetASTNodes for this function
		payload.operation = TraversalPayload::DeadCodeElimination_ComputeAlive;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);

		// The payload will now contain the alive/reachable set.

		// Remove unused let vars.
		payload.operation = TraversalPayload::DeadCodeElimination_RemoveDead;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);

		//std::cout << "\n=====DCE pass results:======\n";
		//linker.top_level_defs[i]->print(0, std::cout);
	}

	return payload.tree_changed;
}


void VirtualMachine::doDeadFunctionEliminationPass(const VMConstructionArgs& args)
{
	// do dead-function elimination pass.  This removes all function definitions that are not reachable (through direct or indirect function calls) from the set of entry functions.

	// std::cout << "Doing dead-function elimination pass..." << std::endl;

	std::vector<ASTNode*> stack;
	TraversalPayload payload(TraversalPayload::DeadFunctionElimination);
	payload.linker = &linker;
		
	// Add entry functions to reachable set and defs_to_process set.
	for(size_t i=0; i<args.entry_point_sigs.size(); ++i)
	{
		FunctionDefinitionRef f = linker.findMatchingFunctionSimple(args.entry_point_sigs[i]);
		//if(f.isNull())
		//	throw BaseException("Failed to find entry point function " + args.entry_point_sigs[i].toString());
		if(f.nonNull())
		{
			payload.reachable_nodes.insert(f.getPointer()); // Mark as reachable
			payload.nodes_to_process.push_back(f.getPointer()); // Add to to-process set.
		}
	}

	while(!payload.nodes_to_process.empty())
	{
		// Pop a node off the stack
		ASTNode* def_to_process = payload.nodes_to_process.back();
		payload.nodes_to_process.pop_back();

		payload.processed_nodes.insert(def_to_process); // Mark node as processed

		def_to_process->traverse(payload, stack); // Process it
		assert(stack.size() == 0);
	}


	// TEMP HACK: add some special functions to reachable set.
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new OpaqueType()));
		payload.reachable_nodes.insert(findMatchingFunction(allocateStringSig).getPointer());

		vector<TypeRef> argtypes(1, new String());
		const FunctionSignature freeStringSig("freeString", argtypes);
		payload.reachable_nodes.insert(findMatchingFunction(freeStringSig).getPointer());

		payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int(64)))).getPointer());

		payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeVArray", vector<TypeRef>(1, new VArrayType(new Int())))).getPointer());

		payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateClosure", vector<TypeRef>(1, new Int(64)))).getPointer());

		const TypeRef dummy_func_type = Function::dummyFunctionType();
		payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeClosure", vector<TypeRef>(1, dummy_func_type))).getPointer());
	}

	// Now remove any non-reachable functions and named constants from linker.top_level_defs
	std::vector<ASTNodeRef> new_top_level_defs;
	std::vector<ASTNodeRef> unreachable_defs;
	for(size_t i=0; i<linker.top_level_defs.size(); ++i)
		if(payload.reachable_nodes.find(linker.top_level_defs[i].getPointer()) != payload.reachable_nodes.end()) // if in reachable set:
			new_top_level_defs.push_back(linker.top_level_defs[i]);
		else
			unreachable_defs.push_back(linker.top_level_defs[i]);

	linker.top_level_defs = new_top_level_defs;

	// Rebuild linker.sig_to_function_map
	Linker::SigToFuncMapType new_map;
	for(auto i = linker.sig_to_function_map.begin(); i != linker.sig_to_function_map.end(); ++i)
	{
		FunctionDefinitionRef def = i->second;
		if(payload.reachable_nodes.find(def.getPointer()) != payload.reachable_nodes.end()) // if in reachable set:
			new_map.insert(std::make_pair(def->sig, def));
	}
	linker.sig_to_function_map = new_map;
		
		
	// Print out reachable function sigs
	/*std::cout << "Reachable defs:" << std::endl;
	for(auto i = payload.reachable_nodes.begin(); i != payload.reachable_nodes.end(); ++i)
	{
		if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
		{
			FunctionDefinition* def = (FunctionDefinition*)*i;
			std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
		}
	}

	// Print out unreachable functions:
	std::cout << "Unreachable defs:" << std::endl;
	for(auto i = unreachable_defs.begin(); i != unreachable_defs.end(); ++i)
	{
		if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
		{
			FunctionDefinition* def = (FunctionDefinition*)(*i).getPointer();
			std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
		}
	}

	// Print out reachable function sigs
	std::cout << "new_top_level_defs:" << std::endl;
	for(auto i = new_top_level_defs.begin(); i != new_top_level_defs.end(); ++i)
	{
		if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
		{
			FunctionDefinition* def = (FunctionDefinition*)i->getPointer();
			std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
		}
	}*/
}


void VirtualMachine::loadSource(const VMConstructionArgs& args, const std::vector<SourceBufferRef>& source_buffers, const std::vector<FunctionDefinitionRef>& preconstructed_func_defs)
{
	std::map<std::string, TypeRef> named_types;
	int function_order_num = 0;

	for(size_t i=0; i<preconstructed_func_defs.size(); ++i)
		linker.top_level_defs.push_back(preconstructed_func_defs[i]);

	linker.addExternalFunctions(this->external_functions);

	LangParser parser;
	for(size_t i=0; i<source_buffers.size(); ++i)
	{
		std::vector<Reference<TokenBase> > tokens;
		Lexer::process(source_buffers[i], tokens);

		Reference<BufferRoot> buffer_root = parser.parseBuffer(tokens, source_buffers[i], named_types, named_types_ordered, function_order_num);

		// Run any function rewriters passed in by the user on the parsed function definitions.
		for(size_t z=0; z<args.function_rewriters.size(); ++z)
		{
			// TODO: just rewrite one function at a time?
			vector<FunctionDefinitionRef> func_defs;
			for(size_t q=0; q<buffer_root->top_level_defs.size(); ++q)
				if(buffer_root->top_level_defs[q]->nodeType() == ASTNode::FunctionDefinitionType)
					func_defs.push_back(buffer_root->top_level_defs[q].downcast<FunctionDefinition>());
			
			args.function_rewriters[z]->rewrite(func_defs, source_buffers[i]);
		}

		// Add functions and named constants parsed from this source buffer to linker.
		linker.addTopLevelDefs(buffer_root->top_level_defs);
	}

	// TEMP: print out functions
	//for(size_t i=0; i<func_defs.size(); ++i)
	//	func_defs[i]->print(0, std::cout);

	// Bind variables and function names
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		payload.linker = &linker;
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);

		assert(stack.size() == 0);
	}

	/*if(!linker.concrete_funcs.empty())
	{
		append(func_defs, linker.concrete_funcs);
		linker.concrete_funcs.resize(0);
	}*/
	
	// Do Operator overloading conversion.
	// NOTE: This is done during binding stage now.
	/*bool op_overloading_changed_tree = false;
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::OperatorOverloadConversion);
		
		// Add linker info, so we can bind new functions such as op_add immediately.
		//payload.top_lvl_frame = top_lvl_frame;
		payload.linker = &linker;

		for(size_t i=0; i<linker.func_defs.size(); ++i)
			if(!linker.func_defs[i]->is_anon_func)
				linker.func_defs[i]->traverse(payload, stack);

		for(size_t i=0; i<named_constants.size(); ++i)
			named_constants[i]->traverse(payload, stack);

		//root->traverse(payload, stack);
		assert(stack.size() == 0);

		op_overloading_changed_tree = payload.tree_changed;
	}*/
	
	// Link functions again if tree has changed due to operator overloading conversion,
	// because we will now have unbound references to functions like 'op_add'.
	//if(op_overloading_changed_tree)
	//{
	//	std::vector<ASTNode*> stack;
	//	TraversalPayload payload(TraversalPayload::LinkFunctions, hidden_voidptr_arg, env);
	//	payload.linker = &linker;
	//	root->traverse(payload, stack);
	//	assert(stack.size() == 0);
	//}
	// Bind variables
	/*if(op_overloading_changed_tree)
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		//payload.top_lvl_frame = top_lvl_frame;
		payload.linker = &linker;
		for(size_t i=0; i<linker.func_defs.size(); ++i)
			if(!linker.func_defs[i]->is_anon_func)
				linker.func_defs[i]->traverse(payload, stack);

		for(size_t i=0; i<named_constants.size(); ++i)
			named_constants[i]->traverse(payload, stack);

		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}*/

	while(true)
	{
		bool tree_changed = false;
		
		// Do Constant Folding
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::ComputeCanConstantFold);
			for(size_t i=0; i<linker.top_level_defs.size(); ++i)
				linker.top_level_defs[i]->traverse(payload, stack);
			
			assert(stack.size() == 0);

			tree_changed = tree_changed || payload.tree_changed;
		}

		
		/*{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::ConstantFolding);
			for(size_t i=0; i<linker.func_defs.size(); ++i)
				if(!linker.func_defs[i]->is_anon_func)
					linker.func_defs[i]->traverse(payload, stack);

			for(size_t i=0; i<named_constants.size(); ++i)
				named_constants[i]->traverse(payload, stack);

			//root->traverse(payload, stack);
			assert(stack.size() == 0);

			tree_changed = tree_changed || payload.tree_changed;
		}*/

		

		// Do another pass of type coercion, as constant folding may have made new literals that can be coerced.
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::TypeCoercion);
			for(size_t i=0; i<linker.top_level_defs.size(); ++i)
				linker.top_level_defs[i]->traverse(payload, stack);

			assert(stack.size() == 0);

			tree_changed = tree_changed || payload.tree_changed;
		}

		// Do another bind pass as type coercion may have allowed some previously unbound functions to be bound now due to e.g. int->float type coercion.
		/*{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::BindVariables);
			//payload.top_lvl_frame = top_lvl_frame;
			payload.linker = &linker;
			for(size_t i=0; i<linker.func_defs.size(); ++i)
				if(!linker.func_defs[i]->is_anon_func)
					linker.func_defs[i]->traverse(payload, stack);

			for(size_t i=0; i<named_constants.size(); ++i)
				named_constants[i]->traverse(payload, stack);

			//root->traverse(payload, stack);
			assert(stack.size() == 0);
		}*/

//		rootref->print(0, std::cout);

		// NOTE: do we even need to loop here in any circumstance?
		if(!tree_changed)
			break;
	}


	// Do a final bind variables pass, with check_bindings true
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		payload.check_bindings = true;
		payload.linker = &linker;
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);

		assert(stack.size() == 0);
	}

	
	/*while(true)
	{

		// TEMP: Now that we have domain checked, do some more constant folding

		bool tree_changed = false;

		// Do Constant Folding
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::ConstantFolding);
			for(size_t i=0; i<func_defs.size(); ++i)
				if(!func_defs[i]->is_anon_func)
					func_defs[i]->traverse(payload, stack);
			//root->traverse(payload, stack);
			assert(stack.size() == 0);

			tree_changed = tree_changed || payload.tree_changed;
		}

		// Do another pass of type coercion, as constant folding may have made new literals that can be coerced.
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::TypeCoercion);
			for(size_t i=0; i<func_defs.size(); ++i)
				if(!func_defs[i]->is_anon_func)
					func_defs[i]->traverse(payload, stack);
			//root->traverse(payload, stack);
			assert(stack.size() == 0);

			tree_changed = tree_changed || payload.tree_changed;
		}

//		rootref->print(0, std::cout);

		if(!tree_changed)
			break;
	}*/

	

	// TypeCheck.  Will throw a BaseException if anything is mistyped.
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCheck);

		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);
		
		assert(stack.size() == 0);
	}

	// Do in-domain checking (e.g. check elem() calls are in-bounds etc..)
	// Do this after type-checking, so type-checking can check that all types are non-null (e.g. all funcs are bound).
	// This is because domain checking needs to use the types.
	if(!args.allow_unsafe_operations)
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::CheckInDomain);
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);

		assert(stack.size() == 0);
	}



	while(true)
	{
		bool inline_change = doInliningPass();

		bool dce_change = doDeadCodeEliminationPass();

		bool changed = inline_change || dce_change;
		if(!changed)
			break;
	}




	// do dead-function elimination pass.  This removes all function definitions that are not reachable (through direct or indirect function calls) from the set of entry functions.
	if(true)
	{
		// std::cout << "Doing dead-function elimination pass..." << std::endl;

		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::DeadFunctionElimination);
		payload.linker = &linker;
		
		// Add entry functions to reachable set and defs_to_process set.
		for(size_t i=0; i<args.entry_point_sigs.size(); ++i)
		{
			FunctionDefinitionRef f = linker.findMatchingFunctionSimple(args.entry_point_sigs[i]);
			//if(f.isNull())
			//	throw BaseException("Failed to find entry point function " + args.entry_point_sigs[i].toString());
			if(f.nonNull())
			{
				payload.reachable_nodes.insert(f.getPointer()); // Mark as reachable
				payload.nodes_to_process.push_back(f.getPointer()); // Add to to-process set.
			}
		}

		while(!payload.nodes_to_process.empty())
		{
			// Pop a node off the stack
			ASTNode* def_to_process = payload.nodes_to_process.back();
			payload.nodes_to_process.pop_back();

			payload.processed_nodes.insert(def_to_process); // Mark node as processed

			def_to_process->traverse(payload, stack); // Process it
			assert(stack.size() == 0);
		}


		// TEMP HACK: add some special functions to reachable set.
		{
			const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new OpaqueType()));
			payload.reachable_nodes.insert(findMatchingFunction(allocateStringSig).getPointer());

			vector<TypeRef> argtypes(1, new String());
			const FunctionSignature freeStringSig("freeString", argtypes);
			payload.reachable_nodes.insert(findMatchingFunction(freeStringSig).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int(64)))).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeVArray", vector<TypeRef>(1, new VArrayType(new Int())))).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateClosure", vector<TypeRef>(1, new Int(64)))).getPointer());

			const TypeRef dummy_func_type = Function::dummyFunctionType();
			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeClosure", vector<TypeRef>(1, dummy_func_type))).getPointer());
		}

		// Now remove any non-reachable functions and named constants from linker.top_level_defs
		std::vector<ASTNodeRef> new_top_level_defs;
		std::vector<ASTNodeRef> unreachable_defs;
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			if(payload.reachable_nodes.find(linker.top_level_defs[i].getPointer()) != payload.reachable_nodes.end()) // if in reachable set:
				new_top_level_defs.push_back(linker.top_level_defs[i]);
			else
				unreachable_defs.push_back(linker.top_level_defs[i]);

		linker.top_level_defs = new_top_level_defs;

		// Rebuild linker.sig_to_function_map
		Linker::SigToFuncMapType new_map;
		for(auto i = linker.sig_to_function_map.begin(); i != linker.sig_to_function_map.end(); ++i)
		{
			FunctionDefinitionRef def = i->second;
			if(payload.reachable_nodes.find(def.getPointer()) != payload.reachable_nodes.end()) // if in reachable set:
				new_map.insert(std::make_pair(def->sig, def));
		}
		linker.sig_to_function_map = new_map;
		
		
		// Print out reachable function sigs
		/*std::cout << "Reachable defs:" << std::endl;
		for(auto i = payload.reachable_nodes.begin(); i != payload.reachable_nodes.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)*i;
				std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
			}
		}

		// Print out unreachable functions:
		std::cout << "Unreachable defs:" << std::endl;
		for(auto i = unreachable_defs.begin(); i != unreachable_defs.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)(*i).getPointer();
				std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
			}
		}

		// Print out reachable function sigs
		std::cout << "new_top_level_defs:" << std::endl;
		for(auto i = new_top_level_defs.begin(); i != new_top_level_defs.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)i->getPointer();
				std::cout << "\t" << def->sig.toString() << " (" + toHexString((uint64)def) + ")\n";
			}
		}*/
	}

	// Do a pass to get pointers to anon functions
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::AddAnonFuncsToLinker);
		payload.linker = &linker;
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);
	}
}


void VirtualMachine::build(const VMConstructionArgs& args)
{
	this->llvm_module->setDataLayout(this->llvm_exec_engine->getDataLayout()->getStringRepresentation());

	
	CommonFunctions common_functions;
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new OpaqueType()));
		common_functions.allocateStringFunc = findMatchingFunction(allocateStringSig).getPointer();
		assert(common_functions.allocateStringFunc);

		vector<TypeRef> argtypes(1, new String());
		const FunctionSignature freeStringSig("freeString", argtypes);
		common_functions.freeStringFunc = findMatchingFunction(freeStringSig).getPointer();
		assert(common_functions.freeStringFunc);

		common_functions.allocateVArrayFunc = findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int(64)))).getPointer();
		assert(common_functions.allocateVArrayFunc);

		common_functions.freeVArrayFunc = findMatchingFunction(FunctionSignature("freeVArray", vector<TypeRef>(1, new VArrayType(new Int()))))./*new OpaqueType*/getPointer();
		assert(common_functions.freeVArrayFunc);

		
		common_functions.allocateClosureFunc = findMatchingFunction(FunctionSignature("allocateClosure", vector<TypeRef>(1, new Int(64)))).getPointer();
		assert(common_functions.allocateClosureFunc);

		const TypeRef dummy_func_type = Function::dummyFunctionType();
		common_functions.freeClosureFunc = findMatchingFunction(FunctionSignature("freeClosure", vector<TypeRef>(1, dummy_func_type))).getPointer();
		assert(common_functions.freeClosureFunc);
	}

	RefCounting::emitRefCountingFunctions(this->llvm_module, this->llvm_exec_engine->getDataLayout(), common_functions);
	


	/*for(size_t i = 0; i != named_types_ordered.size(); ++i)
	{
		if(named_types_ordered[i]->getType() == Type::StructureTypeType)
		{
			StructureType* struct_type = named_types_ordered[i].downcastToPtr<StructureType>();

			const llvm::StructLayout* struct_layout = this->llvm_exec_engine->getDataLayout()->getStructLayout(static_cast<llvm::StructType*>(named_types_ordered[i]->LLVMType(*this->llvm_context)));

			std::cout << "------------" << struct_type->name << " layout -----------" << std::endl;
			std::cout << "total size: " << struct_layout->getSizeInBytes() << " B" << std::endl;
		}
	}*/

	linker.buildLLVMCode(
		this->llvm_module,
		this->llvm_exec_engine->getDataLayout(),
		common_functions,
		this->stats,
		args.emit_trace_code
	);

	
	// rename / obfuscate pass (with interalize + global DCE to remove unused code)
	/*{
		llvm::PassManager pm;
		//pm.add(llvm::createMetaRenamerPass());

		// Do internalize pass.  This pass has to be added before the other optimisation passes or it won't do anything.
		{
			// Build list of functions with external linkage (entry points)
			
			std::vector<std::string> export_list_strings;
			for(unsigned int i=0; i<args.entry_point_sigs.size(); ++i)
			{
				FunctionDefinitionRef func = linker.findMatchingFunctionSimple(args.entry_point_sigs[i]);

				if(func.nonNull() && func->built_llvm_function)
					export_list_strings.push_back(func->built_llvm_function->getName()); // NOTE: is LLVM func built yet?
			}

			std::vector<const char*> export_list;
			for(unsigned int i=0; i<export_list_strings.size(); ++i)
				export_list.push_back(export_list_strings[i].c_str());


			export_list.push_back("pluto323");

			pm.add(llvm::createInternalizePass(export_list));
		}

		pm.add(llvm::createGlobalDCEPass());
		pm.run(*this->llvm_module);
	}*/
	
	
	// Dump unoptimised module bitcode to 'unoptimised_module.txt'
	if(DUMP_MODULE_IR)
	{
#if TARGET_LLVM_VERSION >= 36
		std::error_code errorinfo;
		llvm::raw_fd_ostream f(
			"unoptimised_module_IR.txt",
			errorinfo,
			llvm::sys::fs::F_Text
		);
#else
		std::string errorinfo;
		llvm::raw_fd_ostream f(
			"unoptimised_module_IR.txt",
			errorinfo
		);
#endif
		this->llvm_module->print(f, NULL);
	}

	// Verify module
	{
#if TARGET_LLVM_VERSION >= 36
		std::string error_string_stream_str;
		llvm::raw_string_ostream error_string_stream(error_string_stream_str);

		const bool ver_errors = llvm::verifyModule(
			*this->llvm_module,
			&error_string_stream
		);

		if(ver_errors)
		{
			std::cout << "Module verification errors." << std::endl;
			//this->llvm_module->dump();
			std::cout << "------errors:------" << std::endl;
			std::cout << error_string_stream_str << std::endl;

			//TEMP: write to disk
			/*{
				std::ofstream f("verification_errors.txt");
				f << error_string_stream_str;
			}*/

			throw BaseException("Module verification errors.");
		}
#else
		string error_str;
		const bool ver_errors = llvm::verifyModule(
			*this->llvm_module, 
			llvm::ReturnStatusAction, // Action to take
			&error_str
			);
		if(ver_errors)
		{
			std::cout << "Module verification errors: " << error_str << std::endl;
			{
				std::ofstream f("verification_errors.txt");
				f << error_str;
			}
			//this->llvm_module->dump();
			throw BaseException("Module verification errors: " + error_str);
		}
#endif
		assert(!ver_errors);
	}

	


	const bool optimise = true;
	const bool verbose = false;

	// Do Linting
	/*llvm::lintModule(*this->llvm_module);

	for(llvm::Module::iterator i = this->llvm_module->begin(); i != this->llvm_module->end(); ++i)
		if(!i->isIntrinsic() && !i->isDeclaration())
			llvm::lintFunction(*i);
	*/



	// Do LLVM optimisations
	if(optimise)
	{
		llvm::FunctionPassManager fpm(this->llvm_module);

		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
#if TARGET_LLVM_VERSION <= 34
		fpm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#else
		fpm.add(new llvm::DataLayoutPass());
#endif
		//std:: cout << ("Setting triple to " + this->triple) << std::endl;
		fpm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		llvm::PassManager pm;
#if TARGET_LLVM_VERSION <= 34
		pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#else
		pm.add(new llvm::DataLayoutPass());
#endif

		pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		// Required for loop vectorisation to work properly.
		target_machine->addAnalysisPasses(fpm);
		target_machine->addAnalysisPasses(pm);

		llvm::PassManagerBuilder builder;

		// Turn on vectorisation!
		builder.BBVectorize = false; // Disabled due to being buggy: https://llvm.org/bugs/show_bug.cgi?id=23845
		builder.SLPVectorize = true;
		builder.LoopVectorize = true;

		builder.Inliner = llvm::createFunctionInliningPass();

		builder.OptLevel = 3;
		
		// Do internalize pass.  This pass has to be added before the other optimisation passes or it won't do anything.
		{
			// Build list of functions with external linkage (entry points)
			
			std::vector<std::string> export_list_strings;
			for(unsigned int i=0; i<args.entry_point_sigs.size(); ++i)
			{
				FunctionDefinitionRef func = linker.findMatchingFunctionSimple(args.entry_point_sigs[i]);

				if(func.nonNull() && func->built_llvm_function)
					export_list_strings.push_back(func->built_llvm_function->getName()); // NOTE: is LLVM func built yet?
			}

			std::vector<const char*> export_list;
			for(unsigned int i=0; i<export_list_strings.size(); ++i)
				export_list.push_back(export_list_strings[i].c_str());

			pm.add(llvm::createInternalizePass(export_list));
		}
		
		builder.populateFunctionPassManager(fpm);
		builder.populateModulePassManager(pm);

		optimiseFunctions(fpm, this->llvm_module, verbose);

		// Run module optimisation.  This may remove some functions, so we have to be careful accessing llvm functions from now on.
		{
			if(verbose)
				std::cout << "Optimising module... " << std::endl;
			const bool changed = pm.run(*this->llvm_module);
			if(verbose)
				std::cout << "Done. (changed = " + toString(changed) + ")" << std::endl;
		}

		optimiseFunctions(fpm, this->llvm_module, verbose);
	}



	// Verify module again.
	{
#if TARGET_LLVM_VERSION >= 36
		const bool ver_errors = llvm::verifyModule(
			*this->llvm_module
			// TODO: pass std out
		);
		if(ver_errors)
		{
			std::cout << "Module verification errors." << std::endl;
			this->llvm_module->dump();
			throw BaseException("Module verification errors.");
		}
#else
		string error_str;
		const bool ver_errors = llvm::verifyModule(
			*this->llvm_module, 
			llvm::ReturnStatusAction, // Action to take
			&error_str
			);
		if(ver_errors)
		{
			std::cout << "Module verification errors: " << error_str << std::endl;
			this->llvm_module->dump();
			throw BaseException("Module verification errors: " + error_str);
		}
#endif
		assert(!ver_errors);
	}

	// Dump module bitcode to 'module.txt'
	if(DUMP_MODULE_IR)
	{
#if TARGET_LLVM_VERSION >= 36
		std::error_code errorinfo;
		llvm::raw_fd_ostream f(
			"optimised_module_IR.txt",
			errorinfo,
			llvm::sys::fs::F_Text
		);
#else
		std::string errorinfo;
		llvm::raw_fd_ostream f(
			"optimised_module_IR.txt",
			errorinfo
		);
#endif
		this->llvm_module->print(f, NULL);
	}

	if(DUMP_ASSEMBLY)
		this->compileToNativeAssembly(this->llvm_module, "module_assembly.txt");

	this->llvm_exec_engine->finalizeObject();
}


VirtualMachine::OpenCLCCode VirtualMachine::buildOpenCLCode(const BuildOpenCLCodeArgs& args) const
{
	OpenCLCCode res;

	EmitOpenCLCodeParams params;
	params.uid = 0;

	std::string top_level_def_src;
	for(size_t i=0; i<linker.top_level_defs.size(); ++i)
	{
		if(linker.top_level_defs[i]->nodeType() == ASTNode::FunctionDefinitionType)
		{
			const FunctionDefinitionRef f = linker.top_level_defs[i].downcast<FunctionDefinition>();
			if(!f->isGenericFunction() && !f->isExternalFunction() && f->built_in_func_impl.isNull())
				top_level_def_src += f->emitOpenCLC(params) + "\n";

			// Collect any tuple types used in external functions
			if(f->isExternalFunction())
			{
				for(unsigned int i=0; i<f->args.size(); ++i)
					if(f->args[i].type->getType() == Type::TupleTypeType)
						params.tuple_types_used.insert(f->args[i].type.downcast<TupleType>());
				
				if(f->returnType()->getType() == Type::TupleTypeType)
					params.tuple_types_used.insert(f->returnType().downcast<TupleType>());
			}
		}
		else if(linker.top_level_defs[i]->nodeType() == ASTNode::NamedConstantType)
		{
			top_level_def_src += linker.top_level_defs[i]->emitOpenCLC(params) + "\n";
		}
	}

	

	// TODO: Will need to handle dependecies between tuples here as well.

	std::set<TupleTypeRef, TypeRefLessThan> emitted_tuples;
	std::string struct_def_code = "// OpenCL structure definitions for Winter structs and tuples, from VirtualMachine::buildOpenCLCode()\n";
	std::string constructor_code;

	// Spit out structure definitions and constructors
	for(auto i = 0; i != named_types_ordered.size(); ++i)
		if(named_types_ordered[i]->getType() == Type::StructureTypeType)
		{
			StructureType* struct_type = static_cast<StructureType*>(named_types_ordered[i].getPointer());

			// Emit definitions of any tuples used in this structure, that haven't been defined already.
			std::vector<TupleTypeRef> used_tuples = struct_type->getElementTupleTypes();
			for(size_t z=0; z<used_tuples.size(); ++z)
			{
				if(!ContainerUtils::contains(emitted_tuples, used_tuples[z])) // If not emitted yet:
				{
					struct_def_code += used_tuples[z]->getOpenCLCDefinition();
					emitted_tuples.insert(used_tuples[z]); // Add to set of emitted tuples
				}
			}

			struct_def_code += struct_type->getOpenCLCDefinition(); // Emit structure definition
			constructor_code += struct_type->getOpenCLCConstructor();
		}

	// Spit out any remaining tuple definitions
	for(auto i = params.tuple_types_used.begin(); i != params.tuple_types_used.end(); ++i)
		if(!ContainerUtils::contains(emitted_tuples, *i)) // If not emitted yet:
		{
			struct_def_code += (*i)->getOpenCLCDefinition();
			constructor_code += (*i)->getOpenCLCConstructor();
			emitted_tuples.insert(*i); // Add to set of emitted tuples
		}

	for(auto i = args.tuple_types_used.begin(); i != args.tuple_types_used.end(); ++i)
		if(!ContainerUtils::contains(emitted_tuples, *i)) // If not emitted yet:
		{
			struct_def_code += (*i)->getOpenCLCDefinition();
			constructor_code += (*i)->getOpenCLCConstructor();
			emitted_tuples.insert(*i); // Add to set of emitted tuples
		}

		// Add some Winter built-in functions.  TODO: move this stuff some place better?
	const std::string built_in_func_code = 
"// Winter built-in functions \n\
float toFloat_int_(int x) { return (float)x; } \n\
int truncateToInt_float_(float x) { return (int)x; } \n\
int8 truncateToInt_vector_float__8__(float8 v) { return convert_int8(v); }  \n\
long toInt_opaque_(void* p) { return (long)p; }  \n\
float print_float_(float x) { printf((__constant char *)\"%f\\n\", x); return x; }    \n\
int toInt32_int64_(long x) { return (int)x; }		\n\
long toInt64_int_(int x) { return (long)x; }		\n\
\n";

	res.struct_def_code = struct_def_code;
	res.function_code = constructor_code + "\n\n" + built_in_func_code + "\n\n" + params.file_scope_code + "\n\n" + top_level_def_src;
	return res;
}


std::string VirtualMachine::buildOpenCLCodeCombined(const BuildOpenCLCodeArgs& args) const
{
	OpenCLCCode opencl_code = buildOpenCLCode(args);
	return opencl_code.struct_def_code + opencl_code.function_code;
}


Reference<FunctionDefinition> VirtualMachine::findMatchingFunction(
	const FunctionSignature& sig)
{
	return linker.findMatchingFunctionSimple(sig);
}


void* VirtualMachine::getJittedFunction(const FunctionSignature& sig)
{
	FunctionDefinitionRef func = linker.findMatchingFunctionSimple(sig);

	if(func.isNull())
		throw Winter::BaseException("Failed to find function " + sig.toString());
	
	return this->llvm_exec_engine->getPointerToFunction(
		func->built_llvm_function
	);
}


// Writes the assembly for the module to disk at filename.  Throws Winter::BaseException on failure.
void VirtualMachine::compileToNativeAssembly(llvm::Module* mod, const std::string& filename) 
{
	target_machine->setAsmVerbosityDefault(true);

	llvm::PassManager pm;
#if TARGET_LLVM_VERSION < 36
	pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#endif
	pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

	std::string err;
#if TARGET_LLVM_VERSION >= 36
	std::error_code errcode;
	llvm::raw_fd_ostream raw_out(filename.c_str(), errcode, llvm::sys::fs::F_None);
	if(errcode)
		throw Winter::BaseException("Error when opening file to print assembly to: " + errcode.message());
#elif TARGET_LLVM_VERSION == 34
	llvm::raw_fd_ostream raw_out(filename.c_str(), err, llvm::sys::fs::F_None);
#else
	llvm::raw_fd_ostream raw_out(filename.c_str(), err, 0);
#endif
	if(!err.empty())
		throw Winter::BaseException("Error when opening file to print assembly to: " + err);


	llvm::formatted_raw_ostream out(raw_out, llvm::formatted_raw_ostream::PRESERVE_STREAM);

	if (this->target_machine->addPassesToEmitFile(
		pm, 
		out,
		llvm::TargetMachine::CGFT_AssemblyFile
	))
		throw Winter::BaseException("Unable to emit assembly file!");

	pm.run(*mod);
}


void* VirtualMachine::getJittedFunctionByName(const std::string& name)
{
	FunctionDefinitionRef func = linker.findMatchingFunctionByName(name);

	return this->llvm_exec_engine->getPointerToFunction(
		func->built_llvm_function
		);
}


bool VirtualMachine::isFunctionCalled(const std::string& name)
{
	FunctionDefinitionRef f = linker.findMatchingFunctionByName(name);
	if(f.isNull())
		return false;

	return f->getNumUses() > 0;
}


} // end namespace Winter
