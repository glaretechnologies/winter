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
#pragma warning( disable : 4800 ) // 'int' : forcing value to bool 'true' or 'false' (performance warning)
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
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/ADT/Triple.h"
#endif


using std::vector;


namespace Winter
{


static const bool DUMP_MODULE_IR = false;
static const bool PRINT_ASSEMBLY_TO_STDOUT = false;





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


static StringRep* allocateString(const char* initial_string_val, void* env)
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
static int freeString(StringRep* str, void* env)
{
	assert(str->refcount == 1);
	delete str;
	return 0;
}


static int stringLength(StringRep* str, void* env)
{
	return (int)str->string.size();
}


static ValueRef stringLengthInterpreted(const vector<ValueRef>& args)
{
	return new IntValue( getStringArg(args, 0).size() );
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


VirtualMachine::VirtualMachine(const VMConstructionArgs& args)
:	linker(
		true, // hidden_voidptr_arg
		args.env
	),
	env(args.env)
{
	hidden_voidptr_arg = true;

	this->llvm_context = new llvm::LLVMContext();
	
	this->llvm_module = new llvm::Module("WinterModule", *this->llvm_context);

	llvm::InitializeNativeTarget();

	//llvm::TargetOptions to;


	//const char* argv[] = { "dummyprogname", "-vectorizer-min-trip-count=4"};
	//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");

	//const char* argv[] = { "dummyprogname", "-debug"};
	//llvm::cl::ParseCommandLineOptions(2, argv, "my tool");


	//TEMP HACK try and print out assembly
	llvm::EngineBuilder engine_builder(this->llvm_module);
	engine_builder.setEngineKind(llvm::EngineKind::JIT);
	
	llvm::TargetMachine* tm = engine_builder.selectTarget();
	tm->Options.PrintMachineCode = PRINT_ASSEMBLY_TO_STDOUT;
	this->llvm_exec_engine = engine_builder.create(tm);

	this->triple = tm->getTargetTriple();

	

	// NOTE: ExecutionEngine takes ownership of the module if createJIT is successful.
	std::string error_str;
	
	/*this->llvm_exec_engine = llvm::ExecutionEngine::createJIT(
		this->llvm_module, 
		&error_str
	);*/
	

	this->llvm_exec_engine->DisableLazyCompilation();
	//this->llvm_exec_engine->DisableSymbolSearching(); // Symbol searching is required for sin, pow intrinsics etc..

	this->external_functions = args.external_functions;

	ExternalFunctionRef alloc_ref(new ExternalFunction());
	alloc_ref->interpreted_func = NULL;
	alloc_ref->return_type = TypeRef(new VoidPtrType());
	alloc_ref->sig = FunctionSignature("allocateRefCountedStructure", std::vector<TypeRef>(1, TypeRef(new Int())));
	alloc_ref->func = (void*)(allocateRefCountedStructure);
	this->external_functions.push_back(alloc_ref);


	// TEMP: There is a problem with LLVM 3.3 and earlier with the pow intrinsic getting turned into exp2f().
	// So for now just use our own pow() external function.
	external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
		(void*)(float(*)(float, float))std::pow,
		powInterpreted,
		FunctionSignature("pow", vector<TypeRef>(2, new Float())),
		new Float(),
		false // takes hidden voidptr arg
	)));


	// Add allocateString
	external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
		allocateString,
		allocateStringInterpreted, // interpreted func
		FunctionSignature("allocateString", vector<TypeRef>(1, new VoidPtrType())),
		new String(), // return type
		true // takes hidden voidptr arg
	)));

	// Add freeString
	external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
		freeString,
		NULL, // interpreted func TEMP
		FunctionSignature("freeString", vector<TypeRef>(1, new String())),
		new Int(), // return type
		true // takes hidden voidptr arg
	)));
	external_functions.back()->has_side_effects = true;

	// Add stringLength
	external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
		stringLength,
		stringLengthInterpreted, // interpreted func
		FunctionSignature("stringLength", vector<TypeRef>(1, new String())),
		new Int(), // return type
		true // takes hidden voidptr arg
	)));

	// Add concatStrings
	external_functions.push_back(ExternalFunctionRef(new ExternalFunction(
		concatStrings,
		concatStringsInterpreted, // interpreted func
		FunctionSignature("concatStrings", vector<TypeRef>(2, new String())),
		new String(), // return type
		true // takes hidden voidptr arg
	)));

	for(unsigned int i=0; i<this->external_functions.size(); ++i)
		addExternalFunction(this->external_functions[i], *this->llvm_context, *this->llvm_module);


	assert(this->llvm_exec_engine);
	assert(error_str.empty());

	// Load source buffers
	loadSource(args.source_buffers, args.preconstructed_func_defs);

	this->build(args);
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
		context, 
		f->takes_hidden_voidptr_arg //hidden_voidptr_arg
	);

	llvm::Function* llvm_f = static_cast<llvm::Function*>(module.getOrInsertFunction(
		f->sig.toString(), // Name
		llvm_f_type // Type
	));

	this->llvm_exec_engine->addGlobalMapping(llvm_f, f->func);
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


void VirtualMachine::loadSource(const std::vector<SourceBufferRef>& source_buffers, const std::vector<FunctionDefinitionRef>& preconstructed_func_defs)
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
		TraversalPayload payload(TraversalPayload::BindVariables, hidden_voidptr_arg, env);
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
		TraversalPayload payload(TraversalPayload::OperatorOverloadConversion, hidden_voidptr_arg, env);
		
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
		TraversalPayload payload(TraversalPayload::BindVariables, hidden_voidptr_arg, env);
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
			TraversalPayload payload(TraversalPayload::ConstantFolding, hidden_voidptr_arg, env);
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
			TraversalPayload payload(TraversalPayload::TypeCoercion, hidden_voidptr_arg, env);
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
	}



	

	
	while(true)
	{
		// Do in-domain checking (e.g. check elem() calls are in-bounds etc..)
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::CheckInDomain, hidden_voidptr_arg, env);
			for(size_t i=0; i<func_defs.size(); ++i)
				if(!func_defs[i]->is_anon_func)
					func_defs[i]->traverse(payload, stack);
			assert(stack.size() == 0);
		}

		// TEMP: Now that we have domain checked, do some more constant folding

		bool tree_changed = false;

		// Do Constant Folding
		{
			std::vector<ASTNode*> stack;
			TraversalPayload payload(TraversalPayload::ConstantFolding, hidden_voidptr_arg, env);
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
			TraversalPayload payload(TraversalPayload::TypeCoercion, hidden_voidptr_arg, env);
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
	}

		// TypeCheck
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCheck, hidden_voidptr_arg, env);
		for(size_t i=0; i<func_defs.size(); ++i)
			if(!func_defs[i]->is_anon_func)
				func_defs[i]->traverse(payload, stack);
		
		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}

}


void VirtualMachine::build(const VMConstructionArgs& args)
{
	this->llvm_module->setDataLayout(this->llvm_exec_engine->getDataLayout()->getStringRepresentation());

	CommonFunctions common_functions;
	{
		const FunctionSignature allocateStringSig("allocateString", vector<TypeRef>(1, new VoidPtrType()));
		common_functions.allocateStringFunc = findMatchingFunction(allocateStringSig).getPointer();
		assert(common_functions.allocateStringFunc);

		const FunctionSignature freeStringSig("freeString", vector<TypeRef>(1, new String()));
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

	


	const bool optimise = false;
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
		
		//TEMP:
		//this->compileToNativeAssembly(this->llvm_module, "module_assembly.txt");
	}
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


void VirtualMachine::compileToNativeAssembly(llvm::Module* mod, const std::string& filename) 
{
	//std::string err;
	//const llvm::Target* target = llvm::TargetRegistry::getClosestTargetForJIT(err);

	/*std::string FeaturesStr;
	std::auto_ptr<llvm::TargetMachine> target(_arch->CtorFn(*_module,
		FeaturesStr));

	std::ostringstream os;
	llvm::raw_ostream *Out = new llvm::raw_os_ostream(os);
	target->addPassesToEmitFile(*_passManager, *Out,
		llvm::TargetMachine::AssemblyFile, true);
	target->addPassesToEmitFileFinish(*_passManager, 0, true);*/

	/*llvm::Triple triple(mod->getTargetTriple());
	if (triple.getTriple().empty()) {
		triple.setTriple(llvm::sys::getHostTriple());
	}

	const llvm::Target* target = NULL;
	string err;
	target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), err);
	if (!target) {
		llvm::errs() << "Error: unable to pick a target for compiling to assembly"
			<< "\n";
		exit(1);
	}

	llvm::TargetMachine* tm = target->createTargetMachine(triple.getTriple(), "");
	if (!tm) {
		llvm::errs() << "Error! Creation of target machine"
			" failed for triple " << triple.getTriple() << "\n";
		exit(1);
	}

	tm->setAsmVerbosityDefault(true);

	llvm::PassManager passes;
	if (const llvm::TargetData* td = tm->getTargetData()) {
		passes.add(new llvm::TargetData(*td));
	} else {
		passes.add(new llvm::TargetData(mod));
	}

	//std::string Err;
	//const llvm::TargetMachineRegistry::entry* arch = 
	//	llvm::TargetMachineRegistry::getClosestTargetForJIT(Err);

	//llvm::PassManager passes;
	///llvm::TargetData* target_data = new llvm::TargetData(*this->llvm_exec_engine->getTargetData());
	//passes.add();

	//target_data->

	bool disableVerify = true;

	llvm::raw_fd_ostream raw_out(filename.c_str(), err, 0);
	if (!err.empty()) {
		llvm::errs() << "Error when opening file to print assembly to:\n\t"
			<< err << "\n";
		exit(1);
	}

	llvm::formatted_raw_ostream out(raw_out,
		llvm::formatted_raw_ostream::PRESERVE_STREAM);

	

	if (tm->addPassesToEmitFile(passes, 
		out,
		llvm::TargetMachine::CGFT_AssemblyFile,
		llvm::CodeGenOpt::Aggressive,
		disableVerify)) {
			llvm::errs() << "Unable to emit assembly file! " << "\n";
			exit(1);
	}

	passes.run(*mod);*/
}


void* VirtualMachine::getJittedFunctionByName(const std::string& name)
{
	FunctionDefinitionRef func = linker.findMatchingFunctionByName(name);

	return this->llvm_exec_engine->getPointerToFunction(
		func->built_llvm_function
		);
}


} // end namespace Winter
