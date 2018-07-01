/*=====================================================================
VirtualMachine.cpp
------------------
Copyright Glare Technologies Limited 2018 -
Generated at Mon Sep 13 22:23:44 +1200 2010
=====================================================================*/
#include "VirtualMachine.h"


#include "Linker.h"
#include "Value.h"
#include "CompiledValue.h"
#include "TokenBase.h"
#include "wnt_Lexer.h"
#include "wnt_LangParser.h"
#include "wnt_RefCounting.h"
#include "wnt_ASTNode.h"
#include "LLVMTypeUtils.h"
#include "utils/FileUtils.h"
#include "utils/StringUtils.h"
#include "utils/UTF8Utils.h"
#include "utils/ContainerUtils.h"
#include "utils/TaskManager.h"
#include "utils/Exception.h"
#include "utils/Timer.h"
#include "utils/ConPrint.h"
#include "utils/PlatformUtils.h"
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
#include "llvm/IR/Module.h"
#if TARGET_LLVM_VERSION >= 60
#include "llvm/IR/LegacyPassManager.h"
#else
#include "llvm/PassManager.h"
#endif
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
#if TARGET_LLVM_VERSION >= 60
#include "llvm/Analysis/TargetLibraryInfo.h"
#else
#include "llvm/Target/TargetLibraryInfo.h"
#endif
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Analysis/Lint.h"
#if TARGET_LLVM_VERSION >= 60
#include "llvm/IR/DiagnosticHandler.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/DiagnosticInfo.h"
#endif
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include <cassert>
#include <fstream>
#include <unordered_map>


using std::vector;
using std::string;


#if !defined(TARGET_LLVM_VERSION)
#error Target LLVM Version must be defined via TARGET_LLVM_VERSION
#endif


namespace Winter
{


// Dumps to "unoptimised_module_IR.txt", "optimised_module_IR.txt" in current working dir.
// Also dumps to "module_assembly.txt" in current working dir.
static const bool DUMP_MODULE_IR_AND_ASSEMBLY = false;


//=====================================================================================


static const std::string& getStringArg(const vector<ValueRef>& arg_values, int i)
{
	assert(arg_values[i]->valueType() == Value::ValueType_String);
	return static_cast<const StringValue*>(arg_values[i].getPointer())->value;
}


static int64 getInt64Arg(const vector<ValueRef>& arg_values, int i)
{
	assert(arg_values[i]->valueType() == Value::ValueType_Int);
	return static_cast<const IntValue*>(arg_values[i].getPointer())->value;
}


static const std::string& getCharArg(const vector<ValueRef>& arg_values, int i)
{
	assert(arg_values[i]->valueType() == Value::ValueType_Char);
	return static_cast<const CharValue*>(arg_values[i].getPointer())->value;
}



static int64 closure_count = 0;//TEMP


// Returns number of unicode chars in the string.
static int stringLength(StringRep* str)
{
	const uint64 num_bytes = str->len;
	const char* const data = (const char*)str + sizeof(StringRep);
	return (int)UTF8Utils::numCodePointsInString(data, num_bytes);
}


static ValueRef stringLengthInterpreted(const vector<ValueRef>& args)
{
	return new IntValue( (int)UTF8Utils::numCodePointsInString(getStringArg(args, 0)), /*is_signed=*/true );
}


static StringRep* concatStrings(StringRep* a, StringRep* b, void* env)
{
	const uint64 new_len = a->len + b->len;
	StringRep* new_s = allocateStringWithLen(new_len);
	new_s->refcount = 1;
	std::memcpy((uint8*)new_s + sizeof(StringRep), (uint8*)a + sizeof(StringRep), a->len);
	std::memcpy((uint8*)new_s + sizeof(StringRep) + a->len, (uint8*)b + sizeof(StringRep), b->len);
	return new_s;
}


static ValueRef concatStringsInterpreted(const vector<ValueRef>& args)
{
	return new StringValue( getStringArg(args, 0) + getStringArg(args, 1) );
}


static bool compareEqualString(StringRep* a, StringRep* b, void* env)
{
	if(a->len != b->len)
		return false;
	return std::memcmp((uint8*)a + sizeof(StringRep), (uint8*)b + sizeof(StringRep), a->len) == 0;
}


static ValueRef compareEqualStringInterpreted(const vector<ValueRef>& args)
{
	return new BoolValue(getStringArg(args, 0) == getStringArg(args, 1));
}


static bool compareNotEqualString(StringRep* a, StringRep* b, void* env)
{
	if(a->len != b->len)
		return true;
	return std::memcmp((uint8*)a + sizeof(StringRep), (uint8*)b + sizeof(StringRep), a->len) != 0;
}


static ValueRef compareNotEqualStringInterpreted(const vector<ValueRef>& args)
{
	return new BoolValue(getStringArg(args, 0) != getStringArg(args, 1));
}


static StringRep* charToString(uint32 c, void* env)
{
	// Work out number of bytes used
	const int num_bytes = (int)UTF8Utils::numBytesForChar((uint8)(c & 0xFF));

	StringRep* new_s = allocateStringWithLen(num_bytes);
	new_s->refcount = 1;
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
	return new IntValue(UTF8Utils::codePointForUTF8CharString(getCharArg(args, 0)), /*is_signed=*/true);
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


// In-memory string representation for a Winter Closure
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
	::free(closure);
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
	conPrint(std::string(var_name) + " = " + toString(x));
}


// We need to define our own memory manager so that we can supply our own getPointerToNamedFunction() method, that will return pointers to external functions.
// This seems to be necessary when using MCJIT.
class WinterMemoryManager : public llvm::SectionMemoryManager
{
public:
	virtual ~WinterMemoryManager() {}

	bool use_small_code_model;
	std::vector<VirtualMachine::AllocationBlock> blocks;
	

	uint8_t* allocateSection(void* suggested_addr, uintptr_t Size, unsigned Alignment)
	{
#ifdef _WIN32
		uint8_t* p = (uint8_t*)::VirtualAlloc(
			suggested_addr,
			Size, // size
			MEM_RESERVE | MEM_COMMIT, // allocation type
			PAGE_READWRITE // memory protection flags
		);

		if(p)
		{
			blocks.push_back(VirtualMachine::AllocationBlock());
			blocks.back().alloced_mem = p;
			blocks.back().size = Size;
		}
		else
			throw Winter::BaseException("allocateSection(): Allocation failed.");

		return p;
#else
		throw Winter::BaseException("allocateSection(): not implemented.");
#endif
	}


	virtual uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment,
		unsigned SectionID,
		llvm::StringRef SectionName)
	{
		if(use_small_code_model)
		{
			const size_t desired_addr = 0x80000000 + blocks.size() * 0x100000;
			uint8* p = allocateSection(
				(void*)desired_addr, // desired starting address
				Size, Alignment);
			return p;
		}
		else
			return llvm::SectionMemoryManager::allocateCodeSection(Size, Alignment, SectionID, SectionName);
	}


	virtual uint8_t* allocateDataSection(uintptr_t Size, unsigned Alignment,
		unsigned SectionID,
		llvm::StringRef SectionName,
		bool isReadOnly)
	{
		if(use_small_code_model)
		{
			const size_t desired_addr = 0x81000000 + blocks.size() * 0x100000;
			uint8* p = allocateSection(
				(void*)desired_addr, // desired starting address
				Size, Alignment);
			return p;
		}
		else
			return llvm::SectionMemoryManager::allocateDataSection(Size, Alignment, SectionID, SectionName, isReadOnly);
	}


	virtual bool finalizeMemory(std::string *ErrMsg = 0)
	{
		if(use_small_code_model)
		{
#ifdef _WIN32
			for(size_t i=0; i<blocks.size(); ++i)
			{
				DWORD OldFlags;
				if(!VirtualProtect(blocks[i].alloced_mem, blocks[i].size, PAGE_EXECUTE_READ, &OldFlags))
					throw Winter::BaseException("VirtualProtect failed.");

				FlushInstructionCache(GetCurrentProcess(), blocks[i].alloced_mem, blocks[i].size);
			}
#endif
			return false;
		}
		else
			return llvm::SectionMemoryManager::finalizeMemory(ErrMsg);
	}

	virtual uint64_t getSymbolAddress(const std::string& name)
	{
		std::unordered_map<std::string, void*>::iterator res = func_map->find(name);
		if(res != func_map->end())
			return (uint64_t)res->second;

		// We generally want to explictly find and return the function pointer here, instead of falling back to using
		// llvm::sys::DynamicLibrary::SearchForAddressOfSymbol().  This is because SearchForAddressOfSymbol() just does
		// a search through loaded DLLs, which may be slow, and may not give the symbol that we want.
		// For example "sin" looked up with SearchForAddressOfSymbol() was returning a sin function from ucrtbase(d).dll,
		// which is slower than the sin function explicitly returned below.

		if(name == "execArrayMap")
			return (uint64_t)execArrayMap;
		if(name == "tracePrintFloat")
			return (uint64_t)tracePrintFloat;

		if(name == "sinf")
			return (uint64_t)sinf;
		else if(name == "cosf")
			return (uint64_t)cosf;
		else if(name == "powf")
			return (uint64_t)powf;
		else if(name == "expf")
			return (uint64_t)expf;
#if !defined(_MSC_VER) || (_MSC_VER >= 1900) // Visual Studio versions before 2015 don't define exp2f.
		else if(name == "exp2f")
			return (uint64_t)exp2f;
#endif
		else if(name == "logf")
			return (uint64_t)logf;
		
		else if(name == "sin")
			return (uint64_t)static_cast<double(*)(double)>(sin); // Use static_cast to pick the correct overload.
		else if(name == "cos")
			return (uint64_t)static_cast<double(*)(double)>(cos);
		else if(name == "pow")
			return (uint64_t)static_cast<double(*)(double, double)>(pow);
		else if(name == "exp")
			return (uint64_t)static_cast<double(*)(double)>(exp);
#if !defined(_MSC_VER) || (_MSC_VER >= 1900) // Visual Studio versions before 2015 don't define exp2.
		else if(name == "exp2")
			return (uint64_t)static_cast<double(*)(double)>(exp2);
#endif
		else if(name == "log")
			return (uint64_t)static_cast<double(*)(double)>(log);

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
#if !defined(_MSC_VER) || (_MSC_VER >= 1900) // Visual Studio versions before 2015 don't define exp2f.
		else if(name == "_exp2f")
			return (uint64_t)exp2f;
#endif
		else if(name == "_logf")
			return (uint64_t)logf;
		else if(name == "_memcpy")
			return (uint64_t)memcpy;
		// OS X double precision functions:
		else if(name == "_sin")
			return (uint64_t)static_cast<double(*)(double)>(sin); // Use static_cast to pick the correct overload.
		else if(name == "_cos")
			return (uint64_t)static_cast<double(*)(double)>(cos);
		else if(name == "_pow")
			return (uint64_t)static_cast<double(*)(double, double)>(pow);
		else if(name == "_exp")
			return (uint64_t)static_cast<double(*)(double)>(exp);
#if !defined(_MSC_VER) || (_MSC_VER >= 1900) // Visual Studio versions before 2015 don't define exp2.
		else if(name == "_exp2")
			return (uint64_t)static_cast<double(*)(double)>(exp2);
#endif
		else if(name == "_log")
			return (uint64_t)static_cast<double(*)(double)>(log);
#if defined(OSX)
		else if(name == "_memset_pattern16")
			return (uint64_t)memset_pattern16;
#endif

		// If function was not in func_map (i.e. was not an 'external' function), then use normal symbol resolution, for functions like sinf, cosf etc..

		// conPrint("Warning, falling back to DynamicLibrary::SearchForAddressOfSymbol() for name '" + name + "'...");

		void* f = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);

		//assert(f);
		if(!f)
			throw BaseException("Internal error: failed to find symbol '" + name + "'");
		return (uint64_t)f;
	}
	
	std::unordered_map<std::string, void*>* func_map;
};



VirtualMachine::VirtualMachine(const VMConstructionArgs& args)
:	linker(
		args.try_coerce_int_to_double_first,
		args.emit_in_bound_asserts,
		args.real_is_double
	),
	llvm_context(NULL),
	llvm_module(NULL),
	llvm_exec_engine(NULL),
	vm_args(args)
{
	assert(closure_count == 0);

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
		this->external_functions = args.external_functions;


		// Add allocateString
		{
			external_functions.push_back(new ExternalFunction(
				(void*)allocateString,
				allocateStringInterpreted, // interpreted func
				FunctionSignature("allocateString", vector<TypeVRef>(1, new OpaqueType())),
				new String() // return type
			));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeString
		{
			vector<TypeVRef> arg_types(1, new String());
			//arg_types.push_back(new VoidPtrType());
			external_functions.push_back(new ExternalFunction(
				(void*)(int (*)(StringRep*))free,
				NULL, // interpreted func TEMP
				FunctionSignature("freeString", arg_types),
				new Int() // return type
			));
			external_functions.back()->has_side_effects = true;
		}

		// Add length (stringLength)
		external_functions.push_back(new ExternalFunction(
			(void*)stringLength,
			stringLengthInterpreted, // interpreted func
			FunctionSignature("length", vector<TypeVRef>(1, new String())),
			new Int(), // return type
			ExternalFunction::unknownTimeBound(), // time bound
			1024, // stack space bound
			0 // heap space bound
		));

		// Add concatStrings
		external_functions.push_back(new ExternalFunction(
			(void*)concatStrings,
			concatStringsInterpreted, // interpreted func
			FunctionSignature("concatStrings", vector<TypeVRef>(2, new String())),
			new String() // return type
		));

		// Add toString(char)
		external_functions.push_back(new ExternalFunction(
			(void*)charToString,
			charToStringInterpreted, // interpreted func
			FunctionSignature("toString", vector<TypeVRef>(1, new CharType())),
			new String(), // return type
			1000, // time bound
			1024, // stack size bound
			sizeof(StringRep) + 4 // heap size bound - a single char may take up to 4 bytes to store.
		));

		// Add codePoint(char c) int
		external_functions.push_back(new ExternalFunction(
			(void*)codePoint,
			codePointInterpreted, // interpreted func
			FunctionSignature("codePoint", vector<TypeVRef>(1, new CharType())),
			new Int(), // return type
			16, // time bound
			1024, // stack space bound
			0 // heap space bound
		));

		// Add elem(string s, uint64 index) char
		external_functions.push_back(new ExternalFunction(
			(void*)stringElem,
			stringElemInterpreted, // interpreted func
			FunctionSignature("elem", typePair(new String(), new Int(64))),
			new CharType(), // return type
			ExternalFunction::unknownTimeBound(), // time bound
			1024, // stack space bound
			0 // heap space bound
		));

		// Add compareEqualString
		external_functions.push_back(new ExternalFunction(
			(void*)compareEqualString,
			compareEqualStringInterpreted, // interpreted func
			FunctionSignature("compareEqualString", std::vector<TypeVRef>(2, new String())),
			new Bool(), // return type
			1024, // stack space bound
			0 // heap space bound
		));

		// Add compareNotEqualString
		external_functions.push_back(new ExternalFunction(
			(void*)compareNotEqualString,
			compareNotEqualStringInterpreted, // interpreted func
			FunctionSignature("compareNotEqualString", std::vector<TypeVRef>(2, new String())),
			new Bool(), // return type
			1024, // stack space bound
			0 // heap space bound
		));

		// Add allocateVArray
		{
			external_functions.push_back(new ExternalFunction(
				(void*)allocateVArray,
				allocateVArrayInterpreted, // interpreted func
				FunctionSignature("allocateVArray", vector<TypeVRef>(2, new Int(64))),
				new VArrayType(new Int()) //new OpaqueType() // return type.  Just make this a void*, will cast return value to correct type
			));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeVArray
		{
			external_functions.push_back(new ExternalFunction(
				(void*)(int (*)(VArrayRep*))free,
				NULL, // interpreted func TEMP
				FunctionSignature("freeVArray", vector<TypeVRef>(1, new VArrayType(new Int()))), // new OpaqueType())),
				new Int() // return type
			));
			external_functions.back()->has_side_effects = true;
		}

		const TypeVRef dummy_func_type = Function::dummyFunctionType();

		// Add allocateClosure
		{
			external_functions.push_back(new ExternalFunction(
				(void*)allocateClosure,
				allocateClosureInterpreted, // interpreted func
				FunctionSignature("allocateClosure", vector<TypeVRef>(1, new Int(64))),
				dummy_func_type // return type
			));
			external_functions.back()->has_side_effects = true;
			external_functions.back()->is_allocation_function = true;
		}

		// Add freeClosure
		{
			external_functions.push_back(new ExternalFunction(
				(void*)freeClosure,
				NULL, // interpreted func TEMP
				FunctionSignature("freeClosure", vector<TypeVRef>(1, dummy_func_type)),
				new Int() // return type
			));
			external_functions.back()->has_side_effects = true;
		}


		// Load source buffers
		loadSource(args, args.source_buffers, args.preconstructed_func_defs);

		if(args.build_llvm_code)
		{

			this->llvm_context = new llvm::LLVMContext();
			this->llvm_module = new llvm::Module("WinterModule", *this->llvm_context);

#if TARGET_LLVM_VERSION >= 36
			llvm::EngineBuilder engine_builder(std::unique_ptr<llvm::Module>(this->llvm_module));
#else
			llvm::EngineBuilder engine_builder(this->llvm_module);
#endif
			engine_builder.setEngineKind(llvm::EngineKind::JIT);
#if TARGET_LLVM_VERSION <= 34
			engine_builder.setUseMCJIT(true);
#endif
			if(args.small_code_model)
				engine_builder.setCodeModel(llvm::CodeModel::Small);


			WinterMemoryManager* mem_manager = new WinterMemoryManager();
			mem_manager->use_small_code_model = args.small_code_model;
			mem_manager->func_map = &func_map;
#if TARGET_LLVM_VERSION >= 36
			engine_builder.setMCJITMemoryManager(std::unique_ptr<llvm::SectionMemoryManager>(mem_manager));
#else
			engine_builder.setMCJITMemoryManager(mem_manager);
#endif

			// OSX_DEPLOYMENT_TARGET should correspond to the version number in the -mmacosx-version-min
			// flag passed to the compiler.
#if defined(OSX_DEPLOYMENT_TARGET)
			this->triple = "x86_64-apple-macosx" + std::string(OSX_DEPLOYMENT_TARGET);
#else
			this->triple = llvm::sys::getProcessTriple();
#endif
			this->triple.append("-elf"); // MCJIT requires the -elf suffix currently, see https://groups.google.com/forum/#!topic/llvm-dev/DOmHEXhNNWw


			PlatformUtils::CPUInfo cpu_info;
			PlatformUtils::getCPUInfo(cpu_info);

			// There is an issue with LLVM, that if it encounters a newer CPU model than it knows about, then it just returns 
			// "x86-64", which has less feature support than we actually have.  So in this case just use "corei7" which should give us the features we need.
			// This issue seems to have been fixed in LLVM 6, which tries to autodetect a suitable cpu if it doesn't have an exact match.
			std::string cpu_name;
#if TARGET_LLVM_VERSION >= 60
			cpu_name = llvm::sys::getHostCPUName();
#else
			if(cpu_info.family == 6 && cpu_info.model > 70)
				cpu_name = "corei7";
			else
				cpu_name = llvm::sys::getHostCPUName();
#endif

			// Select the host computer architecture as the target.
			this->target_machine = engine_builder.selectTarget(
				llvm::Triple(this->triple), // target triple
				"",  // march
				cpu_name, // mcpu - e.g. "corei7", "core-avx2"
				llvm::SmallVector<std::string, 4>());

			// Enable floating point op fusion, to allow for FMA codegen.
			this->target_machine->Options.AllowFPOpFusion = llvm::FPOpFusion::Fast;

			this->llvm_exec_engine = engine_builder.create(target_machine);

			this->llvm_exec_engine->DisableLazyCompilation();

		
			for(unsigned int i=0; i<this->external_functions.size(); ++i)
				addExternalFunction(this->external_functions[i], *this->llvm_context, *this->llvm_module);

			this->build(args);

			jit_mem_blocks.insert(jit_mem_blocks.end(), mem_manager->blocks.begin(), mem_manager->blocks.end());
		}
	}
	catch(BaseException& e)
	{
		// Since we threw an exception in the constructor, VirtualMachine destructor will not be run.
		// So we need to delete these objects now.
		delete this->llvm_exec_engine;

		delete this->llvm_context;

		throw e; // Re-throw exception
	}
}


VirtualMachine::~VirtualMachine()
{
	// llvm_exec_engine will delete llvm_module.
	assert(closure_count == 0);

	delete this->llvm_exec_engine;

	delete this->llvm_context;

	for(size_t i=0; i<jit_mem_blocks.size(); ++i)
	{
#ifdef _WIN32
		if(::VirtualFree(jit_mem_blocks[i].alloced_mem, 0, MEM_RELEASE) == 0)
			conPrint("Error: VirtualFree failed: " + PlatformUtils::getLastErrorString());
#endif
	}
}


void VirtualMachine::init()
{
	// Since we will be calling LLVM functions from multiple threads, we need to call this.
#if TARGET_LLVM_VERSION < 36
	llvm::llvm_start_multithreaded();
#endif

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();

#if TARGET_LLVM_VERSION >= 60
	// Enable -warn-stack-size= option.
	// We use this to get the stack size for compiled functions.
	// The option boolean is a global static, so set it once here.
	const char* argv[] = { "dummyprogname", "-warn-stack-size=0" };
	llvm::cl::ParseCommandLineOptions(2, argv);
#endif
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

	module.getOrInsertFunction(
		makeSafeStringForFunctionName(f->sig.toString()), // Name
		llvm_f_type // Type
	);

	func_map[makeSafeStringForFunctionName(f->sig.toString())] = f->func;

	// On OS X, the requested symbol names seem to have an underscore prepended, so add an entry with a leading underscore.
	func_map["_" + makeSafeStringForFunctionName(f->sig.toString())] = f->func;
}


#if TARGET_LLVM_VERSION >= 60
#define PASS_MANANGER_NAMESPACE llvm::legacy
#else
#define PASS_MANANGER_NAMESPACE llvm
#endif


static void optimiseFunctions(PASS_MANANGER_NAMESPACE::FunctionPassManager& fpm, llvm::Module* module, bool verbose)
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

	// Do a CountArgumentRefs pass.  This will count the number of references to each function argument in the body of each function.
	{
		payload.operation = TraversalPayload::CountArgumentRefs;
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
		TraversalPayload payload2(TraversalPayload::BindVariables);
		payload2.linker = &linker;
		for(size_t i=0; i<linker.top_level_defs.size(); ++i)
			linker.top_level_defs[i]->traverse(payload2, stack);
	}

	return tree_changed;
}


// Dead code elimination pass.
// Removes let variables that are not referenced.
// This is an intra-function pass.
// It works by traversing the AST, except we won't traverse to let variables, unless they are referenced by a variable in the let block expression.
bool VirtualMachine::doDeadCodeEliminationPass()
{
	// Timer timer;

	std::vector<ASTNode*> stack;
	TraversalPayload payload(TraversalPayload::DeadCodeElimination_ComputeAlive);
	for(size_t i=0; i<linker.top_level_defs.size(); ++i)
	{
		//std::cout << "\n=====DCE pass before:======\n";
		//linker.top_level_defs[i]->print(0, std::cout);
		//std::cout << "doing DCE on func " << i << std::endl;

		// Do a pass to get the set of live LetASTNodes for this function
		payload.operation = TraversalPayload::DeadCodeElimination_ComputeAlive;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);

		// The payload will now contain the alive/reachable set of let vars.

		// Remove unused let vars.
		payload.operation = TraversalPayload::DeadCodeElimination_RemoveDead;
		linker.top_level_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);

		//std::cout << "\n=====DCE pass results:======\n";
		//linker.top_level_defs[i]->print(0, std::cout);
	}

	// std::cout << "VirtualMachine::doDeadCodeEliminationPass() took " + timer.elapsedString() << std::endl;
	return payload.tree_changed;
}


void VirtualMachine::loadSource(const VMConstructionArgs& args, const std::vector<SourceBufferRef>& source_buffers, const std::vector<FunctionDefinitionRef>& preconstructed_func_defs)
{
	std::map<std::string, TypeVRef> named_types;
	int function_order_num = 0;

	for(size_t i=0; i<preconstructed_func_defs.size(); ++i)
		linker.addFunction(preconstructed_func_defs[i]);

	linker.addExternalFunctions(this->external_functions);

	LangParser parser(args.floating_point_literals_default_to_double, args.real_is_double);
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
		/*{
			std::ofstream file("pre_inlining_source.win");
			for(size_t i=0; i<linker.top_level_defs.size(); ++i)
				file << linker.top_level_defs[i]->sourceString() << "\n\n";
		}*/

		bool inline_change = doInliningPass();

		bool dce_change = doDeadCodeEliminationPass();

		// Do SimplifyIfExpression pass
		bool simplify_if_change = false;
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::SimplifyIfExpression);
			for(size_t i=0; i<linker.top_level_defs.size(); ++i)
				linker.top_level_defs[i]->traverse(payload, stack);
			simplify_if_change = payload.tree_changed;
		}

		/*{
			std::ofstream file("post_inlining_source.win");
			for(size_t i=0; i<linker.top_level_defs.size(); ++i)
				file << linker.top_level_defs[i]->sourceString() << "\n\n";
		}*/

		bool changed = inline_change || dce_change || simplify_if_change;
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

			if(payload.processed_nodes.find(def_to_process) == payload.processed_nodes.end()) // If not already processed:
			{
				payload.processed_nodes.insert(def_to_process); // Mark node as processed

				def_to_process->traverse(payload, stack); // Process it
				assert(stack.size() == 0);
			}
		}


		// TEMP HACK: add some special functions to reachable set.
		{
			const FunctionSignature allocateStringSig("allocateString", vector<TypeVRef>(1, new OpaqueType()));
			payload.reachable_nodes.insert(findMatchingFunction(allocateStringSig).getPointer());

			vector<TypeVRef> argtypes(1, new String());
			const FunctionSignature freeStringSig("freeString", argtypes);
			payload.reachable_nodes.insert(findMatchingFunction(freeStringSig).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeVRef>(2, new Int(64)))).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeVArray", vector<TypeVRef>(1, new VArrayType(new Int())))).getPointer());

			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("allocateClosure", vector<TypeVRef>(1, new Int(64)))).getPointer());

			const TypeVRef dummy_func_type = Function::dummyFunctionType();
			payload.reachable_nodes.insert(findMatchingFunction(FunctionSignature("freeClosure", vector<TypeVRef>(1, dummy_func_type))).getPointer());
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
		/*conPrint("Reachable defs:");
		for(auto i = payload.reachable_nodes.begin(); i != payload.reachable_nodes.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)*i;
				conPrint("\t" + def->sig.toString() + " (" + toHexString((uint64)def) + ")\n");
			}
		}

		// Print out unreachable functions:
		conPrint("Unreachable defs:");
		for(auto i = unreachable_defs.begin(); i != unreachable_defs.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)(*i).getPointer();
				conPrint("\t" + def->sig.toString() + " (" + toHexString((uint64)def) + ")\n");
			}
		}

		// Print out reachable function sigs
		conPrint("new_top_level_defs:");
		for(auto i = new_top_level_defs.begin(); i != new_top_level_defs.end(); ++i)
		{
			if((*i)->nodeType() == ASTNode::FunctionDefinitionType)
			{
				FunctionDefinition* def = (FunctionDefinition*)i->getPointer();
				conPrint("\t" + def->sig.toString() + " (" + toHexString((uint64)def) + ")\n");
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


#if TARGET_LLVM_VERSION >= 60
struct GlobalValueInternaliser
{
	bool operator() (const llvm::GlobalValue& global_val)
	{
		if(llvm::isa<llvm::Function>(&global_val))
			return entry_point_funcs.count(llvm::cast<llvm::Function>(&global_val)) != 0;
		else
			return false;
	}

	std::set<const llvm::Function*> entry_point_funcs;
};
#endif


static const llvm::DataLayout* getDataLayout(llvm::ExecutionEngine* llvm_exec_engine)
{
#if TARGET_LLVM_VERSION >= 60
	return &llvm_exec_engine->getDataLayout();
#else
	return  llvm_exec_engine->getDataLayout();
#endif
}


// DiagnosticHandler does not seem to be present in earlier LLVMS (e.g. 3.4).
#if TARGET_LLVM_VERSION >= 60
struct WinterDiagHandler : public llvm::DiagnosticHandler
{
	virtual bool handleDiagnostics(const llvm::DiagnosticInfo& info)
	{
		// We'll use this diagnostic handler to get warnings about stack size, in order to get the stack sizes
		// for compiled functions.
		if(info.getKind() == llvm::DK_StackSize)
		{
			const llvm::DiagnosticInfoStackSize& stack_info = llvm::cast<llvm::DiagnosticInfoStackSize>(info);
			const llvm::Function& func = stack_info.getFunction();
			const uint64 stack_size = stack_info.getResourceSize();
			stack_sizes->insert(std::make_pair(&func, stack_size));
		}
		else
		{
			if(false)
			{
				std::string msg;
				llvm::raw_string_ostream stream(msg);
				llvm::DiagnosticPrinterRawOStream printer(stream);
				info.print(printer);
				stream.flush();
				conPrint(msg);
			}
		}
		return true;
	}

	std::unordered_map<const llvm::Function*, uint64>* stack_sizes;
};
#endif


void VirtualMachine::build(const VMConstructionArgs& args)
{
#if TARGET_LLVM_VERSION >= 60
	WinterDiagHandler* handler = new WinterDiagHandler();
	handler->stack_sizes = &this->stack_sizes;
	this->llvm_context->setDiagnosticHandler(std::unique_ptr<WinterDiagHandler>(handler));
#endif

	this->llvm_module->setDataLayout(getDataLayout(llvm_exec_engine)->getStringRepresentation());

	
	CommonFunctions common_functions;
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeVRef>(1, new OpaqueType()));
		common_functions.allocateStringFunc = findMatchingFunction(allocateStringSig).getPointer();
		assert(common_functions.allocateStringFunc);

		vector<TypeVRef> argtypes(1, new String());
		const FunctionSignature freeStringSig("freeString", argtypes);
		common_functions.freeStringFunc = findMatchingFunction(freeStringSig).getPointer();
		assert(common_functions.freeStringFunc);

		common_functions.allocateVArrayFunc = findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeVRef>(2, new Int(64)))).getPointer();
		assert(common_functions.allocateVArrayFunc);

		common_functions.freeVArrayFunc = findMatchingFunction(FunctionSignature("freeVArray", vector<TypeVRef>(1, new VArrayType(new Int()))))./*new OpaqueType*/getPointer();
		assert(common_functions.freeVArrayFunc);

		
		common_functions.allocateClosureFunc = findMatchingFunction(FunctionSignature("allocateClosure", vector<TypeVRef>(1, new Int(64)))).getPointer();
		assert(common_functions.allocateClosureFunc);

		const TypeVRef dummy_func_type = Function::dummyFunctionType();
		common_functions.freeClosureFunc = findMatchingFunction(FunctionSignature("freeClosure", vector<TypeVRef>(1, dummy_func_type))).getPointer();
		assert(common_functions.freeClosureFunc);
	}

	RefCounting::emitRefCountingFunctions(this->llvm_module, getDataLayout(llvm_exec_engine), common_functions);
	


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
		getDataLayout(llvm_exec_engine),
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
	if(DUMP_MODULE_IR_AND_ASSEMBLY)
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

		conPrint("Dumped unoptimised module IR to 'unoptimised_module_IR.txt'.");
	}

	verifyModule(this->llvm_module);

	const bool optimise = true;
	const bool verbose = false;

#if BUILD_TESTS
	// Add a 'lint' pass that detects common mistakes.
	llvm::lintModule(*this->llvm_module);

	for(llvm::Module::iterator i = this->llvm_module->begin(); i != this->llvm_module->end(); ++i)
		if(!i->isIntrinsic() && !i->isDeclaration())
			llvm::lintFunction(*i);
#endif


	// Do LLVM optimisations
	if(optimise)
	{
		PASS_MANANGER_NAMESPACE::FunctionPassManager fpm(this->llvm_module);

		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
#if TARGET_LLVM_VERSION <= 34
		fpm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#elif TARGET_LLVM_VERSION <= 36
		fpm.add(new llvm::DataLayoutPass());
#else
		this->llvm_module->setDataLayout(this->target_machine->createDataLayout());
#endif
		
#if TARGET_LLVM_VERSION <= 36
		//std:: cout << ("Setting triple to " + this->triple) << std::endl;
		fpm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));
#endif

		PASS_MANANGER_NAMESPACE::PassManager pm;
#if TARGET_LLVM_VERSION <= 34
		pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#elif TARGET_LLVM_VERSION <= 36
		pm.add(new llvm::DataLayoutPass());
#endif

#if TARGET_LLVM_VERSION <= 36
		pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		// Required for loop vectorisation to work properly.
		target_machine->addAnalysisPasses(fpm);
		target_machine->addAnalysisPasses(pm);
#endif

		llvm::PassManagerBuilder builder;

		// Turn on vectorisation!
		builder.SLPVectorize = true;
		builder.LoopVectorize = true;

		builder.Inliner = llvm::createFunctionInliningPass();

		builder.OptLevel = 3;
		
		// Do internalize pass.  This pass has to be added before the other optimisation passes or it won't do anything.
		{
			// Build list of functions with external linkage (entry points)

			std::set<const llvm::Function*> entry_point_funcs;
			
			std::vector<std::string> export_list_strings;
			for(unsigned int i=0; i<args.entry_point_sigs.size(); ++i)
			{
				FunctionDefinitionRef func = linker.findMatchingFunctionSimple(args.entry_point_sigs[i]);

				if(func.nonNull() && func->built_llvm_function)
				{
					entry_point_funcs.insert(func->built_llvm_function);
					export_list_strings.push_back(func->built_llvm_function->getName()); // NOTE: is LLVM func built yet?
				}
			}

#if TARGET_LLVM_VERSION >= 60
			GlobalValueInternaliser internaliser;
			internaliser.entry_point_funcs = entry_point_funcs;
			pm.add(llvm::createInternalizePass(internaliser));
#else
			std::vector<const char*> export_list;
			for(unsigned int i=0; i<export_list_strings.size(); ++i)
				export_list.push_back(export_list_strings[i].c_str());

			pm.add(llvm::createInternalizePass(export_list));
#endif
		}
		
		builder.populateFunctionPassManager(fpm);
		builder.populateModulePassManager(pm);

		optimiseFunctions(fpm, this->llvm_module, verbose);

		// Run module optimisation.  This may remove some functions, so we have to be careful accessing llvm functions from now on.
		{
			if(verbose)
				conPrint("Optimising module... ");
			const bool changed = pm.run(*this->llvm_module);
			if(verbose)
				conPrint("Done. (changed = " + toString(changed) + ")");
		}

		optimiseFunctions(fpm, this->llvm_module, verbose);
	}


	// Verify module again now that we have done optimisations.
	verifyModule(this->llvm_module);


	// Dump module bitcode to 'module.txt'
	if(DUMP_MODULE_IR_AND_ASSEMBLY)
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

		conPrint("Dumped optimised module IR to 'optimised_module_IR.txt'.");
	}

	if(DUMP_MODULE_IR_AND_ASSEMBLY)
		this->compileToNativeAssembly(this->llvm_module, "module_assembly.txt");

	this->llvm_exec_engine->finalizeObject();


	// Assign reported stack sizes to our functions
	for(auto i = linker.sig_to_function_map.begin(); i != linker.sig_to_function_map.end(); ++i)
	{
		if(i->second->built_llvm_function != NULL)
		{
			auto res = stack_sizes.find(i->second->built_llvm_function);
			if(res != stack_sizes.end())
				i->second->llvm_reported_stack_size = (int64)res->second;
		}
	}
}


VirtualMachine::OpenCLCCode VirtualMachine::buildOpenCLCode(const BuildOpenCLCodeArgs& args) const
{
	OpenCLCCode res;

	EmitOpenCLCodeParams params;
	params.uid = 0;
	params.emit_comments = vm_args.comments_in_opencl_output;
	params.emit_in_bound_asserts = vm_args.emit_in_bound_asserts;

	// Build set of OpenCL C keywords.  This is needed to detect and rename any variables etc.. in winter programs 
	// that are OpenCL C keywords.
	// See Section '6.1.9 Keywords' of OpenCL 2.0 spec
	const char* opencl_c_keywords[] = {
		// Preprocessor keywords
		"__OPENCL_VERSION__",
		"__FILE__",
		"__LINE__",

		// C99 keywords  (from http://stackoverflow.com/a/8140120)
		"auto",
		"enum",       
		"break",
		"extern",
		"case",
		"float",
		"char",
		"for",
		"const",
		"goto",
		"continue",
		"if",
		"default",
		"inline",    
		"do",
		"int",
		"double",
		"long",
		"else",
		"register",
		"restrict",
		"return",
		"short",
		"signed",
		"sizeof",
		"static",
		"struct",
		"switch",
		"typedef",
		"union",
		"unsigned",
		"void",
		"volatile",
		"while",
		"_Bool",
		"_Complex",
		"_Imaginary",
				
		"half", // not actually described as a keyword in the spec, but seems to be treated as one.

		// Table 6.3
		"image2d_t",
		"image3d_t",
		"image2d_array_t",
		"image1d_t",
		"image1d_buffer_t",
		"image1d_array_t",
		"image2d_depth_t",
		"image2d_array_depth_t",
		"sampler_t",
		"queue_t",
		"ndrange_t",
		"clk_event_t",
		"reserve_id_t",
		"event_t",
		"cl_mem_fence_flags",

		// Address space qualifiers
		"__global",
		"global",
		"__local",
		"local",
		"__constant",
		"constant",
		"__private",
		"private",

		// Function qualifiers
		"__kernel",
		"kernel",

		// Access qualifiers
		"__read_only",
		"read_only",
		"__write_only",
		"write_only",
		"__read_write",
		"read_write",

		// misc
		"uniform",
		"pipe",
	};

	for(size_t i=0; i != staticArrayNumElems(opencl_c_keywords); ++i)
		params.opencl_c_keywords.insert(opencl_c_keywords[i]);

	const char* vector_base_types[] = {
		"char",
		"uchar",
		"short",
		"ushort",
		"int",
		"uint",
		"long",
		"ulong",
		"float",
		"double"
	};
	for(int i=0; i != staticArrayNumElems(vector_base_types); ++i)
	{
		params.opencl_c_keywords.insert(std::string(vector_base_types[i]) + "2");
		params.opencl_c_keywords.insert(std::string(vector_base_types[i]) + "3");
		params.opencl_c_keywords.insert(std::string(vector_base_types[i]) + "4");
		params.opencl_c_keywords.insert(std::string(vector_base_types[i]) + "8");
		params.opencl_c_keywords.insert(std::string(vector_base_types[i]) + "16");
	}

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
				for(unsigned int z=0; z<f->args.size(); ++z)
					if(f->args[z].type->getType() == Type::TupleTypeType)
						params.tuple_types_used.insert(f->args[z].type.downcast<TupleType>());
				
				if(f->returnType()->getType() == Type::TupleTypeType)
					params.tuple_types_used.insert(f->returnType().downcast<TupleType>());
			}
		}
		else if(linker.top_level_defs[i]->nodeType() == ASTNode::NamedConstantType)
		{
			top_level_def_src += linker.top_level_defs[i]->emitOpenCLC(params) + "\n";
		}
	}

	

	// TODO: Will need to handle dependencies between tuples here as well.

	std::set<TupleTypeRef, TypeRefLessThan> emitted_tuples;
	std::string struct_def_code = this->vm_args.comments_in_opencl_output ? "// OpenCL structure definitions for Winter structs and tuples, from VirtualMachine::buildOpenCLCode()\n" : "";
	std::string constructor_code = "// Constructor OpenCL code, from VirtualMachine::buildOpenCLCode()\n";

	// Spit out structure definitions and constructors
	for(size_t i = 0; i != named_types_ordered.size(); ++i)
		if(named_types_ordered[i]->getType() == Type::StructureTypeType)
		{
			StructureType* struct_type = static_cast<StructureType*>(named_types_ordered[i].getPointer());

			// Emit definitions of any tuples used in this structure, that haven't been defined already.
			std::vector<TupleTypeRef> used_tuples = struct_type->getElementTupleTypes();
			for(size_t z=0; z<used_tuples.size(); ++z)
			{
				if(!ContainerUtils::contains(emitted_tuples, used_tuples[z])) // If not emitted yet:
				{
					struct_def_code += used_tuples[z]->getOpenCLCDefinition(vm_args.comments_in_opencl_output);
					emitted_tuples.insert(used_tuples[z]); // Add to set of emitted tuples
				}
			}

			struct_def_code += struct_type->getOpenCLCDefinition(params, vm_args.comments_in_opencl_output); // Emit structure definition
			constructor_code += struct_type->getOpenCLCConstructor(params, vm_args.comments_in_opencl_output);
		}

	// Spit out any remaining tuple definitions
	for(auto i = params.tuple_types_used.begin(); i != params.tuple_types_used.end(); ++i)
		if(!ContainerUtils::contains(emitted_tuples, *i)) // If not emitted yet:
		{
			struct_def_code += (*i)->getOpenCLCDefinition(vm_args.comments_in_opencl_output);
			constructor_code += (*i)->getOpenCLCConstructor(vm_args.comments_in_opencl_output);
			emitted_tuples.insert(*i); // Add to set of emitted tuples
		}

	for(auto i = args.tuple_types_used.begin(); i != args.tuple_types_used.end(); ++i)
		if(!ContainerUtils::contains(emitted_tuples, *i)) // If not emitted yet:
		{
			struct_def_code += (*i)->getOpenCLCDefinition(vm_args.comments_in_opencl_output);
			constructor_code += (*i)->getOpenCLCConstructor(vm_args.comments_in_opencl_output);
			emitted_tuples.insert(*i); // Add to set of emitted tuples
		}

	// Add some Winter built-in functions.  TODO: move this stuff some place better?
	std::string built_in_func_code = 
		"// Winter built-in functions \n"
		"float toFloat_int_(int x) { return (float)x; } \n"
		"float toFloat_uint_(uint x) { return (float)x; } \n"
		"int truncateToInt_float_(float x) { return (int)x; } \n"
		"int8 truncateToInt_vector_float__8__(float8 v) { return convert_int8(v); }  \n"
		"long toInt_opaque_(void* p) { return (long)p; }  \n"
		"int toInt32_int64_(long x) { return (int)x; }		\n"
		"long toInt64_int_(int x) { return (long)x; }		\n"
		"bool isFinite_float_(float x) { return isfinite(x); }		\n"
		"bool isNAN_float_(float x) { return isnan(x); }		\n"
		"float mod_float__float_(float x, float y) { if(x < 0) { const float z = y - fmod(-x, y); return z == y ? 0.f : z; } else return fmod(x, y); }     \n"
		"int mod_int__int_(int x, int y) { return (unsigned int)x % y; }        \n"
		"float dot1_vector_float__2___vector_float__2__(float2 a, float2 b) { return a.x * b.x; } \n"
		"float dot2_vector_float__2___vector_float__2__(float2 a, float2 b) { return a.x * b.x + a.y * b.y; } \n"
		"float dot1_vector_float__4___vector_float__4__(float4 a, float4 b) { return a.x * b.x; } \n"
		"float dot2_vector_float__4___vector_float__4__(float4 a, float4 b) { return a.x * b.x + a.y * b.y; } \n"
		"float dot3_vector_float__4___vector_float__4__(float4 a, float4 b) { return a.x * b.x + a.y * b.y + a.z * b.z; } \n"
		"float dot4_vector_float__4___vector_float__4__(float4 a, float4 b) { return dot(a, b); } \n"
		"bool __compare_equal_vector_float__2___vector_float__2__(float2 a, float2 b) { return all(a == b); } \n"
		"bool __compare_equal_vector_float__4___vector_float__4__(float4 a, float4 b) { return all(a == b); } \n"
		"bool __compare_equal_vector_float__8___vector_float__8__(float8 a, float8 b) { return all(a == b); } \n"
		"bool __compare_not_equal_vector_float__2___vector_float__2__(float2 a, float2 b) { return any(a != b); } \n"
		"bool __compare_not_equal_vector_float__4___vector_float__4__(float4 a, float4 b) { return any(a != b); } \n"
		"bool __compare_not_equal_vector_float__8___vector_float__8__(float8 a, float8 b) { return any(a != b); } \n"
		;

	if(vm_args.emit_opencl_printf_calls)
	{
		built_in_func_code += 
			"float print_bool_(bool x) { if(x) { printf((__constant char *)\"true\\n\"); } else { printf((__constant char *)\"false\\n\"); } return x; }    \n"
			"float print_float_(float x) { printf((__constant char *)\"%1.7f\\n\", x); return x; }    \n"
			"int print_int_(int x) { printf((__constant char *)\"%i\\n\", x); return x; }    \n";
	}
	else
	{
		built_in_func_code += 
			"// NOTE: printf calls have been disabled as emit_opencl_printf_calls vm arg is false.	\n"
			"float print_bool_(bool x) { return x; }    \n"
			"float print_float_(float x) { return x; }    \n"
			"int print_int_(int x) { return x; }    \n";
	}


	if(vm_args.opencl_double_support)
	{
		built_in_func_code +=
			"double toDouble_int_(int x) { return (double)x; } \n"
			"int truncateToInt_double_(double x) { return (int)x; } \n"
			"int8 truncateToInt_vector_double__8__(float8 v) { return convert_int8(v); }  \n"
			"double mod_double__double_(double x, double y) { if(x < 0) { const double z = y - fmod(-x, y); return z == y ? 0.0 : z; } else return fmod(x, y); }     \n"
			"bool isFinite_double_(double x) { return isfinite(x); }		\n"
			"bool isNAN_double_(double x) { return isnan(x); }		\n"
			"double dot1_vector_double__2___vector_double__2__(double2 a, double2 b) { return a.x * b.x; } \n"
			"double dot2_vector_double__2___vector_double__2__(double2 a, double2 b) { return a.x * b.x + a.y * b.y; } \n"
			"double dot1_vector_double__4___vector_double__4__(double4 a, double4 b) { return a.x * b.x; } \n"
			"double dot2_vector_double__4___vector_double__4__(double4 a, double4 b) { return a.x * b.x + a.y * b.y; } \n"
			"double dot3_vector_double__4___vector_double__4__(double4 a, double4 b) { return a.x * b.x + a.y * b.y + a.z * b.z; } \n"
			"double dot4_vector_double__4___vector_double__4__(double4 a, double4 b) { return dot(a, b); } \n"
			"bool __compare_equal_vector_double__2___vector_double__2__(double2 a, double2 b) { return all(a == b); } \n"
			"bool __compare_equal_vector_double__4___vector_double__4__(double4 a, double4 b) { return all(a == b); } \n"
			"bool __compare_equal_vector_double__8___vector_double__8__(double8 a, double8 b) { return all(a == b); } \n"
			"bool __compare_not_equal_vector_double__2___vector_double__2__(double2 a, double2 b) { return any(a != b); } \n"
			"bool __compare_not_equal_vector_double__4___vector_double__4__(double4 a, double4 b) { return any(a != b); } \n"
			"bool __compare_not_equal_vector_double__8___vector_double__8__(double8 a, double8 b) { return any(a != b); } \n"
			;

		if(vm_args.emit_opencl_printf_calls)
			built_in_func_code += "double print_double_(double x) { printf((__constant char *)\"%f\\n\", x); return x; }    \n";
		else
			built_in_func_code += 
				"// NOTE: printf calls have been disabled as emit_opencl_printf_calls vm arg is false.  \n"
				"double print_double_(double x) { return x; }    \n";
	}

	if(vm_args.emit_in_bound_asserts)
	{
		if(vm_args.emit_opencl_printf_calls)
		{
			built_in_func_code += 
				"#define winterAssert(expr) doWinterAssert(expr, #expr, __FILE__, __LINE__)		\n"
				"\n"
				"inline void doWinterAssert(bool val, const __constant char* message, const __constant char* file, unsigned int line)	\n"
				"{\n"
				"	if(!val)	\n"
				"		printf(\"!!! winter assert failed: %s, file %s, line %i\\n\", message, file, line);	\n"
				"}\n";
		}
		else
		{
			// TODO: issue some kind of warning here, since asserts are useless without printf?
			assert(0);
			built_in_func_code += 
				"#define winterAssert(expr) doWinterAssert(expr, #expr, __FILE__, __LINE__)		\n"
				"\n"
				"// NOTE: printf calls have been disabled as emit_opencl_printf_calls vm arg is false.  \n"
				"inline void doWinterAssert(bool val, const __constant char* message, const __constant char* file, unsigned int line)	\n"
				"{}\n";
		}
	}

	if(vm_args.real_is_double)
	{
		if(vm_args.opencl_double_support)
			built_in_func_code += "double toReal_int_(int x) { return (double)x; } \n";
	}
	else
		built_in_func_code += "float toReal_int_(int x) { return (float)x; } \n";

	built_in_func_code += "// End Winter built-in functions\n";

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


void VirtualMachine::verifyModule(llvm::Module* the_llvm_module)
{
	string error_str;

#if TARGET_LLVM_VERSION >= 36
	llvm::raw_string_ostream error_string_stream(error_str);

	const bool ver_errors = llvm::verifyModule(
		*the_llvm_module,
		&error_string_stream
	);
#else
	const bool ver_errors = llvm::verifyModule(
		*the_llvm_module, 
		llvm::ReturnStatusAction, // Action to take
		&error_str
	);
#endif

	if(ver_errors)
	{
		conPrint("Module verification errors: " + error_str);

		// Write to disk
		/*{
			std::ofstream f("verification_errors.txt");
			f << error_str;
		}*/

		//the_llvm_module->dump();
		throw BaseException("Module verification errors: " + error_str);
	}
}


// Writes the assembly for the module to disk at filename.  Throws Winter::BaseException on failure.
void VirtualMachine::compileToNativeAssembly(llvm::Module* mod, const std::string& filename) 
{
#if TARGET_LLVM_VERSION <= 36
	target_machine->setAsmVerbosityDefault(true);
#endif

	std::string err;
#if TARGET_LLVM_VERSION >= 36
	std::error_code errcode;
	llvm::raw_fd_ostream raw_out(filename.c_str(), errcode, llvm::sys::fs::F_None);
	if(errcode)
		throw Winter::BaseException("Error when opening file to print assembly to: " + errcode.message());
#else
	llvm::raw_fd_ostream raw_out(filename.c_str(), err, llvm::sys::fs::F_None);
#endif
	if(!err.empty())
		throw Winter::BaseException("Error when opening file to print assembly to: " + err);


	PASS_MANANGER_NAMESPACE::PassManager pm;
#if TARGET_LLVM_VERSION < 36
	pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#endif
	
#if TARGET_LLVM_VERSION >= 60
	if (this->target_machine->addPassesToEmitFile(
		pm, 
		raw_out,
		llvm::TargetMachine::CGFT_AssemblyFile
	))
		throw Winter::BaseException("Unable to emit assembly file!");
#else
	pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

	llvm::formatted_raw_ostream out(raw_out, llvm::formatted_raw_ostream::PRESERVE_STREAM);

	if (this->target_machine->addPassesToEmitFile(
		pm, 
		out,
		llvm::TargetMachine::CGFT_AssemblyFile
	))
		throw Winter::BaseException("Unable to emit assembly file!");
#endif

	pm.run(*mod);

	conPrint("Dumped module assembly to '" + filename + "'.");
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
