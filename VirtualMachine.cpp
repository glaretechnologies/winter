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
#include "utils/fileutils.h"
#include "utils/stringutils.h"
#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
#include "wnt_RefCounting.h"
#include "wnt_ASTNode.h"
#include "wnt_Frame.h"
#include "VMState.h"
#include "Linker.h"
#include "Value.h"
#include "LanguageTests.h"
#include "VirtualMachine.h"
#include "LLVMTypeUtils.h"
#if USE_LLVM
#pragma warning(push, 0) // Disable warnings
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/ObjectImage.h"
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
#pragma warning(pop) // Re-enable warnings
#endif


using std::vector;
//#define USE_LLVM_3_4 1


namespace Winter
{
	

static const bool DUMP_MODULE_IR = false;
static const bool DUMP_ASSEMBLY = false;


static const bool USE_MCJIT = true;


static void* allocateRefCountedStructure(uint32 size, void* env)
{
	// TEMP:
	return malloc(size);
}


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


static StringRep* allocateString(const char* initial_string_val/*, void* env*/)
{
	StringRep* r = new StringRep();
	//r->refcount = 1;
	r->string = std::string(initial_string_val);
	return r;
}


static ValueRef allocateStringInterpreted(const vector<ValueRef>& args)
{
	//StringRep* r = new StringRep();
	//r->refcount = 1;
	//r->string = std::string((const char*)(getVoidPtrArg(args, 0)));
	return new StringValue((const char*)(getVoidPtrArg(args, 0)));
}


// NOTE: just return an int here as all external funcs need to return something (non-void).
static int freeString(StringRep* str/*, void* env*/)
{
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
	StringRep* s = new StringRep();
	s->refcount = 1;
	s->string = a->string + b->string;
	return s;
}


static ValueRef concatStringsInterpreted(const vector<ValueRef>& args)
{
	return new StringValue( getStringArg(args, 0) + getStringArg(args, 1) );
}


static float getFloatArg(const vector<ValueRef>& arg_values, int i)
{
	assert(dynamic_cast<const FloatValue*>(arg_values[i].getPointer()) != NULL);
	return static_cast<const FloatValue*>(arg_values[i].getPointer())->value;
}


static ValueRef powInterpreted(const vector<ValueRef>& args)
{
	return new FloatValue(std::pow(getFloatArg(args, 0), getFloatArg(args, 1)));
}


// We need to define our own memory manager so that we can supply our own getPointerToNamedFunction() method, that will return pointers to external functions.
// This seems to be necessary when using MCJIT.
class WinterMemoryManager : public llvm::SectionMemoryManager
{
public:
	virtual ~WinterMemoryManager() {}


#if USE_LLVM_3_4
	virtual uint64_t getSymbolAddress(const std::string& name)
	{
		std::map<std::string, void*>::iterator res = func_map->find(name);
		if(res != func_map->end())
			return (uint64_t)res->second;

		// If function was not in func_map (i.e. was not an 'external' function), then use normal symbol resolution, for functions like sinf, cosf etc..

		void* f = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);

		assert(f);
		return (uint64_t)f;
	}
#else
	virtual void *getPointerToNamedFunction(const std::string& name, bool AbortOnFailure = true)
	{
		std::map<std::string, void*>::iterator res = func_map->find(name);
		if(res != func_map->end())
			return res->second;

		// If function was not in func_map (i.e. was not an 'external' function), then use normal symbol resolution, for functions like sinf, cosf etc..

		void* f = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);

		assert(f);
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
	try
	{
		hidden_voidptr_arg = true;

		this->llvm_context = new llvm::LLVMContext();
	
		this->llvm_module = new llvm::Module("WinterModule", *this->llvm_context);

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();

		//llvm::TargetOptions to;

		//const char* argv[] = { "dummyprogname", "-vectorizer-min-trip-count=4"};
		//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");

		//const char* argv[] = { "dummyprogname", "-debug"};
		//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");


		llvm::EngineBuilder engine_builder(this->llvm_module);
		engine_builder.setEngineKind(llvm::EngineKind::JIT);
		if(USE_MCJIT) engine_builder.setUseMCJIT(true);


		if(USE_MCJIT)
		{
			WinterMemoryManager* mem_manager = new WinterMemoryManager();
			mem_manager->func_map = &func_map;
#if USE_LLVM_3_4
			engine_builder.setMCJITMemoryManager(mem_manager);
#else
			engine_builder.setJITMemoryManager(mem_manager);
#endif
		}

		this->triple = llvm::sys::getProcessTriple();
		if(USE_MCJIT) this->triple.append("-elf"); // MCJIT requires the -elf suffix currently, see https://groups.google.com/forum/#!topic/llvm-dev/DOmHEXhNNWw

		
		this->target_machine = engine_builder.selectTarget(
			llvm::Triple(this->triple), // target triple
			"",  // march
			"", // "core-avx2",  // mcpu
			llvm::SmallVector<std::string, 4>());

		// Enable floating point op fusion, to allow for FMA codegen.
		this->target_machine->Options.AllowFPOpFusion = llvm::FPOpFusion::Fast;

		this->llvm_exec_engine = engine_builder.create(target_machine);


		this->llvm_exec_engine->DisableLazyCompilation();
		//this->llvm_exec_engine->DisableSymbolSearching(); // Symbol searching is required for sin, pow intrinsics etc..

		this->external_functions = args.external_functions;

		ExternalFunctionRef alloc_ref(new ExternalFunction());
		alloc_ref->interpreted_func = NULL;
		alloc_ref->return_type = TypeRef(new OpaqueType());
		alloc_ref->sig = FunctionSignature("allocateRefCountedStructure", std::vector<TypeRef>(1, TypeRef(new Int())));
		alloc_ref->func = (void*)(allocateRefCountedStructure);
		this->external_functions.push_back(alloc_ref);


		// TEMP: There is a problem with LLVM 3.3 and earlier with the pow intrinsic getting turned into exp2f().
		// So for now just use our own pow() external function.
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)(float(*)(float, float))std::pow,
			powInterpreted,
			FunctionSignature("pow", vector<TypeRef>(2, new Float())),
			new Float()
		)));


		// Add allocateString
		external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
			(void*)allocateString,
			allocateStringInterpreted, // interpreted func
			FunctionSignature("allocateString", vector<TypeRef>(1, new OpaqueType())),
			new String() // return type
		)));

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

		for(unsigned int i=0; i<this->external_functions.size(); ++i)
			addExternalFunction(this->external_functions[i], *this->llvm_context, *this->llvm_module);


		// Load source buffers
		loadSource(args, args.source_buffers, args.preconstructed_func_defs);

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

	delete this->llvm_exec_engine;

	delete llvm_context;
}


void VirtualMachine::shutdown() // Calls llvm_shutdown()
{
	llvm::llvm_shutdown();
}


void VirtualMachine::addExternalFunction(const ExternalFunctionRef& f, llvm::LLVMContext& context, llvm::Module& module)
{
	llvm::FunctionType* llvm_f_type = LLVMTypeUtils::llvmFunctionType(
		f->sig.param_types,
		false, // captured vars struct ptr
		f->return_type, 
		context
	);

	llvm::Function* llvm_f = static_cast<llvm::Function*>(module.getOrInsertFunction(
		makeSafeStringForFunctionName(f->sig.toString()), // Name
		llvm_f_type // Type
	));

	if(USE_MCJIT)
	{
		func_map[makeSafeStringForFunctionName(f->sig.toString())] = f->func;
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
	vector<FunctionDefinitionRef> func_defs;
	std::map<std::string, TypeRef> named_types;

	LangParser parser;
	for(size_t i=0; i<source_buffers.size(); ++i)
	{
		std::vector<Reference<TokenBase> > tokens;
		Lexer::process(source_buffers[i], tokens);

		vector<FunctionDefinitionRef> buffer_func_defs;
		parser.parseBuffer(tokens, source_buffers[i], buffer_func_defs, named_types);

		// Do AddOpaqueEnvArg pass.
		// This is a backwards-compatibility hack for old Indigo shaders that don't have an explicit 'opaque env' arg.
		if(args.add_opaque_env_arg)
		{
			// Look for a method called eval.  If last arg does not have type opaque, then we need to do the add_opaque_env_arg stuff
			bool need_to_do_add_opaque_env_arg = false;
			for(size_t z=0; z<buffer_func_defs.size(); ++z)
			{
				if(buffer_func_defs[z]->sig.name == "eval")
				{
					if(buffer_func_defs[z]->sig.param_types.empty() || buffer_func_defs[z]->sig.param_types.back()->getType() != Type::OpaqueTypeType) // if zero args or last arg is not opaque env:
						need_to_do_add_opaque_env_arg = true;
				}
			}

			// If we are loading from a shader string (and not ISL_stdlib.txt), and there is no function that takes an arg with type opaque, this is probably old code.
			bool any_func_has_opaque_arg = false;
			if(!hasSuffix(source_buffers[i]->name, "ISL_stdlib.txt"))
				for(size_t z=0; z<buffer_func_defs.size(); ++z)
					for(size_t m=0; m<buffer_func_defs[z]->sig.param_types.size(); ++m)
						if(buffer_func_defs[z]->sig.param_types[m]->getType() == Type::OpaqueTypeType)
							any_func_has_opaque_arg = true;

			need_to_do_add_opaque_env_arg = need_to_do_add_opaque_env_arg || !any_func_has_opaque_arg;


			if(need_to_do_add_opaque_env_arg)
			{
				std::vector<ASTNode*> stack;
				TraversalPayload payload(TraversalPayload::AddOpaqueEnvArg);
				for(size_t i=0; i<buffer_func_defs.size(); ++i)
					if(!buffer_func_defs[i]->is_anon_func)
						buffer_func_defs[i]->traverse(payload, stack);
				assert(stack.size() == 0);
			}
		}

		func_defs.insert(func_defs.end(), buffer_func_defs.begin(), buffer_func_defs.end());
	}

	func_defs.insert(func_defs.end(), preconstructed_func_defs.begin(), preconstructed_func_defs.end());


	// Copy func devs to top level frame
	FrameRef top_lvl_frame(new Frame());
	for(size_t i=0; i<func_defs.size(); ++i)
		top_lvl_frame->name_to_functions_map[func_defs[i]->sig.name].push_back(func_defs[i]);


	//BufferRoot* root = dynamic_cast<BufferRoot*>(rootref.getPointer());

	// Do Type Coercion

	// TEMP HACK IMPORTANT NO TYPE COERCION
	/*{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCoercion, hidden_voidptr_arg, env);
		for(size_t i=0; i<func_defs.size(); ++i)
			func_defs[i]->traverse(payload, stack);
		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}*/


	// Link functions
	linker.addExternalFunctions(this->external_functions);
	linker.addFunctions(func_defs);

	// TEMP: print out functions
	//for(size_t i=0; i<func_defs.size(); ++i)
	//	func_defs[i]->print(0, std::cout);

	// Bind variables
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		payload.top_lvl_frame = top_lvl_frame;
		payload.linker = &linker;
		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
				func_defs[i]->traverse(payload, stack);
		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}




	//{
	//	std::vector<ASTNode*> stack;
	//	TraversalPayload payload(TraversalPayload::LinkFunctions, hidden_voidptr_arg, env);
	//	payload.linker = &linker;
	//	root->traverse(payload, stack);
	//	assert(stack.size() == 0);
	//}

	// Do Operator overloading conversion
	bool op_overloading_changed_tree = false;
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::OperatorOverloadConversion);
		
		// Add linker info, so we can bind new functions such as op_add immediately.
		payload.top_lvl_frame = top_lvl_frame;
		payload.linker = &linker;

		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
				func_defs[i]->traverse(payload, stack);
		//root->traverse(payload, stack);
		assert(stack.size() == 0);

		op_overloading_changed_tree = payload.tree_changed;
	}
	
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
	if(op_overloading_changed_tree)
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::BindVariables);
		payload.top_lvl_frame = top_lvl_frame;
		payload.linker = &linker;
		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
				func_defs[i]->traverse(payload, stack);
		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}


	//for(size_t i=0; i<func_defs.size(); ++i)
	//	func_defs[i]->print(0, std::cout);

	
	while(true)
	{
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

		// Do another bind pass as type coercion may have allowed some previously unbound functions to be bound now due to e.g. int->float type coercion.
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::BindVariables);
			payload.top_lvl_frame = top_lvl_frame;
			payload.linker = &linker;
			for(size_t i=0; i<func_defs.size(); ++i)
				if(!func_defs[i]->is_anon_func)
					func_defs[i]->traverse(payload, stack);
			//root->traverse(payload, stack);
			assert(stack.size() == 0);
		}

//		rootref->print(0, std::cout);

		if(!tree_changed)
			break;
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

	// Do in-domain checking (e.g. check elem() calls are in-bounds etc..)
	if(!args.allow_unsafe_operations)
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::CheckInDomain);
		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
				func_defs[i]->traverse(payload, stack);
		assert(stack.size() == 0);
	}

		// TypeCheck
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCheck);
		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
			{
				func_defs[i]->traverse(payload, stack);
				assert(stack.size() == 0);
			}
		
		//root->traverse(payload, stack);
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
		//argtypes.push_back(new VoidPtrType());
		const FunctionSignature freeStringSig("freeString", argtypes);
		common_functions.freeStringFunc = findMatchingFunction(freeStringSig).getPointer();
		assert(common_functions.freeStringFunc);
	}

	RefCounting::emitRefCountingFunctions(this->llvm_module, this->llvm_exec_engine->getDataLayout(), common_functions);


	linker.buildLLVMCode(
		this->llvm_module,
		this->llvm_exec_engine->getDataLayout(),
		common_functions
	);
	
	// Dump unoptimised module bitcode to 'unoptimised_module.txt'
	if(DUMP_MODULE_IR)
	{
		std::string errorinfo;
		llvm::raw_fd_ostream f(
			"unoptimised_module.txt",
			errorinfo
		);
		this->llvm_module->print(f, NULL);
	}

	// Verify module
	{
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
		fpm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
		//std:: cout << ("Setting triple to " + this->triple) << std::endl;
		fpm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

		llvm::PassManager pm;
		pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
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
				FunctionDefinitionRef func = linker.findMatchingFunction(args.entry_point_sigs[i]);

				if(func.nonNull())
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
		string error_str;
		const bool ver_errors = llvm::verifyModule(
			*this->llvm_module, 
			llvm::ReturnStatusAction, // Action to take
			&error_str
			);
		if(ver_errors)
		{
			std::cout << "Module verification errors: " << error_str << std::endl;
		}
		assert(!ver_errors);
	}

	// Dump module bitcode to 'module.txt'
	if(DUMP_MODULE_IR)
	{
		std::string errorinfo;
		llvm::raw_fd_ostream f(
			"module.txt",
			errorinfo
		);
		this->llvm_module->print(f, NULL);
	}

	if(DUMP_ASSEMBLY)
		this->compileToNativeAssembly(this->llvm_module, "module_assembly.txt");

	this->llvm_exec_engine->finalizeObject();
}


Reference<FunctionDefinition> VirtualMachine::findMatchingFunction(
	const FunctionSignature& sig)
{
	return linker.findMatchingFunction(sig);
}


void* VirtualMachine::getJittedFunction(const FunctionSignature& sig)
{
	FunctionDefinitionRef func = linker.findMatchingFunction(sig);

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
	pm.add(new llvm::DataLayout(*this->llvm_exec_engine->getDataLayout()));
	pm.add(new llvm::TargetLibraryInfo(llvm::Triple(this->triple)));

	std::string err;
#if USE_LLVM_3_4
	llvm::raw_fd_ostream raw_out(filename.c_str(), err, llvm::sys::fs::F_None);
#else
	llvm::raw_fd_ostream raw_out(filename.c_str(), err, 0);
#endif
	if (!err.empty())
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


} // end namespace Winter
