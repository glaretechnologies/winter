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
#include "utils/ContainerUtils.h"
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
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;
using std::string;


namespace Winter
{
	

static const bool DUMP_MODULE_IR = false; // Dumps to "unoptimised_module_IR.txt", "optimised_module_IR.txt" in current working dir.
static const bool DUMP_ASSEMBLY = false; // Dumpts to "module_assembly.txt" in current working dir.


static const bool USE_MCJIT = true;


//=====================================================================================


class StringRep
{
public:
	uint64 refcount;
	std::string string; // UTF-8 encoding
};


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


static int64 string_count = 0;//TEMP
static int64 varray_count = 0;//TEMP


static StringRep* allocateString(const char* initial_string_val/*, void* env*/)
{
	string_count++;//TEMP

	StringRep* r = new StringRep();
	//r->refcount = 1;
	r->string = std::string(initial_string_val);
	return r;
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
	delete str;
	return 0;
}


static int stringLength(StringRep* str)
{
	return (int)str->string.size();
}


static ValueRef stringLengthInterpreted(const vector<ValueRef>& args)
{
	return new IntValue( (int)getStringArg(args, 0).size() );
}



static StringRep* concatStrings(StringRep* a, StringRep* b, void* env)
{
	string_count++;//TEMP

	StringRep* s = new StringRep();
	s->refcount = 1;
	s->string = a->string + b->string;
	return s;
}


static ValueRef concatStringsInterpreted(const vector<ValueRef>& args)
{
	return new StringValue( getStringArg(args, 0) + getStringArg(args, 1) );
}


//=====================================================================================


//class VArrayRep
//{
//public:
//	uint64 refcount;
//	//uint8* data;
//};


static uint8* allocateVArray(const int elem_size_B, const int num_elems)
{
	varray_count++;//TEMP

	/*VArrayRep* r = new VArrayRep();
	//r->refcount = 1;
	r->data = new uint8[sizeof(uint64) + elem_size_B * num_elems];
	return r;*/

	// Allocate space for the reference count and data.
	uint8* varray = new uint8[sizeof(uint64) + elem_size_B * num_elems];
	return varray;
}


static ValueRef allocateVArrayInterpreted(const vector<ValueRef>& args)
{
	assert(0);
	return NULL;
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
static int freeVArray(uint8* varray)
{
	varray_count--;//TEMP

	/*assert(varray->refcount == 1);
	delete[] varray->data;
	delete varray;
	return 0;*/
	assert(*((uint64*)varray) == 1);
	delete[] varray;
	return 0;
}


//=====================================================================================



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
	assert(string_count == 0 && varray_count == 0);

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

		// Add stringLength
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)stringLength,
			stringLengthInterpreted, // interpreted func
			FunctionSignature("stringLength", vector<TypeRef>(1, new String())),
			new Int() // return type
		)));

		// Add concatStrings
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)concatStrings,
			concatStringsInterpreted, // interpreted func
			FunctionSignature("concatStrings", vector<TypeRef>(2, new String())),
			new String() // return type
		)));


		
		// Add allocateVArray
		{
			external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
				(void*)allocateVArray,
				allocateVArrayInterpreted, // interpreted func
				FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int())),
				new VArrayType(new Int()) //new OpaqueType() // return type.  Just make this a void*, will cast return value to correct type
			)));
			external_functions.back()->has_side_effects = true;
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


		// Load source buffers
		loadSource(args, args.source_buffers, args.preconstructed_func_defs);


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
	assert(string_count == 0 && varray_count == 0);

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

		// Do Function inlining
		/*{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::InlineFunctionCalls);
			for(size_t i=0; i<func_defs.size(); ++i)
				if(!func_defs[i]->is_anon_func)
					func_defs[i]->traverse(payload, stack);
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

		common_functions.allocateVArrayFunc = findMatchingFunction(FunctionSignature("allocateVArray", vector<TypeRef>(2, new Int()))).getPointer();
		assert(common_functions.allocateVArrayFunc);

		common_functions.freeVArrayFunc = findMatchingFunction(FunctionSignature("freeVArray", vector<TypeRef>(1, new VArrayType(new Int()))))./*new OpaqueType*/getPointer();
		assert(common_functions.freeVArrayFunc);
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
		common_functions
	);
	
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

	// Do LLVM optimisations
	if(optimise)
	{
		//this->llvm_module->setDataLayout(

		llvm::FunctionPassManager fpm(this->llvm_module);

		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
#if TARGET_LLVM_VERSION <= 34
		fpm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#endif
		//std:: cout << ("Setting triple to " + this->triple) << std::endl;
		fpm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		llvm::PassManager pm;
#if TARGET_LLVM_VERSION <= 34
		pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
#endif
		pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		llvm::PassManagerBuilder builder;

		// Turn on vectorisation!
		builder.BBVectorize = true;
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


const std::string VirtualMachine::buildOpenCLCode()
{
	std::string s;

	// Add some Winter built-in functions
	s +=
"float toFloat_int_(int x) { return (float)x; } \n\
int truncateToInt_float_(float x) { return (int)x; } \n\
int8 truncateToInt_vector_float__8__(float8 v) { return convert_int8(v); }  \n\
long toInt_opaque_(void* p) { return (long)p; }  \n\
float print_float_(float x) { printf(\"%f\", x); return x; }    \n\
\n";




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
		}
		else if(linker.top_level_defs[i]->nodeType() == ASTNode::NamedConstantType)
		{
			top_level_def_src += linker.top_level_defs[i]->emitOpenCLC(params) + "\n";
		}
	}

	// Emit tuple struct definitions and constructors
	//for(std::set<TupleTypeRef>::iterator i = params.tuple_types_used.begin(); i != params.tuple_types_used.end(); ++i)
	//	s += (*i)->getOpenCLCDefinition();


	// TODO: Will need to handle dependecies between tuples here as well.

	std::set<TupleTypeRef, TypeRefLessThan> emitted_tuples;

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
					s += used_tuples[z]->getOpenCLCDefinition();
					emitted_tuples.insert(used_tuples[z]); // Add to set of emitted tuples
				}
			}

			s += struct_type->getOpenCLCDefinition(); // Emit structure definition
		}

	// Spit out any remaining tuple definitions
	for(auto i = params.tuple_types_used.begin(); i != params.tuple_types_used.end(); ++i)
		if(!ContainerUtils::contains(emitted_tuples, *i)) // If not emitted yet:
			s += (*i)->getOpenCLCDefinition();


	// Spit out function definitions
	//s += linker.buildOpenCLCode();
	return s + "\n\n" + params.file_scope_code + "\n\n" + top_level_def_src;
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
