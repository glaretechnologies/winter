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
#include "utils/stringutils.h"
#include "wnt_Lexer.h"
#include "TokenBase.h"
#include "wnt_LangParser.h"
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
#include "llvm/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/StandardPasses.h"

/*#include "llvm/System/Host.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetMachine.h"*/
#endif


using std::vector;


namespace Winter
{


//static ValueRef powWrapper(const vector<ValueRef>& arg_values)
//{
//	//assert(arg_values.size() == 2);
//	assert(dynamic_cast<const FloatValue*>(arg_values[0].getPointer()));
//	assert(dynamic_cast<const FloatValue*>(arg_values[1].getPointer()));
//
//	// Cast argument 0 to type FloatValue
//	const FloatValue* a = static_cast<const FloatValue*>(arg_values[0].getPointer());
//	const FloatValue* b = static_cast<const FloatValue*>(arg_values[1].getPointer());
//
//	return ValueRef(new FloatValue(std::pow(a->value, b->value)));
//}


static void* allocateRefCountedStructure(uint32 size, void* env)
{
	// TEMP:
	return malloc(size);
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

	// NOTE: ExecutionEngine takes ownership of the module if createJIT is successful.
	std::string error_str;
	this->llvm_exec_engine = llvm::ExecutionEngine::createJIT(
		this->llvm_module, 
		&error_str
	);

	this->llvm_exec_engine->DisableLazyCompilation();
	this->llvm_exec_engine->DisableSymbolSearching();

	this->external_functions = args.external_functions;

	ExternalFunctionRef alloc_ref(new ExternalFunction());
	alloc_ref->interpreted_func = NULL;
	alloc_ref->return_type = TypeRef(new VoidPtrType());
	alloc_ref->sig = FunctionSignature("allocateRefCountedStructure", std::vector<TypeRef>(1, TypeRef(new Int())));
	alloc_ref->func = (void*)(allocateRefCountedStructure);
	this->external_functions.push_back(alloc_ref);


	//TEMP: add some more external functions

	// Add powf
	/*{
		ExternalFunctionRef f(new ExternalFunction());
		f->func = (void*)(float(*)(float, float))std::powf;
		f->interpreted_func = powWrapper;
		f->return_type = TypeRef(new Float());
		f->sig = FunctionSignature("pow", vector<TypeRef>(2, TypeRef(new Float())));
		this->external_functions.push_back(f);
	}*/

	for(unsigned int i=0; i<this->external_functions.size(); ++i)
		addExternalFunction(this->external_functions[i], *this->llvm_context, *this->llvm_module);


	assert(this->llvm_exec_engine);
	assert(error_str.empty());

	// Load source buffers
	//for(unsigned int i=0; i<args.source_buffers.size(); ++i)
		loadSource(args.source_buffers);//[i]);

	this->build();
}


VirtualMachine::~VirtualMachine()
{
	// llvm_exec_engine will delete llvm_module.

	delete this->llvm_exec_engine;

	delete llvm_context;
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


void VirtualMachine::loadSource(const std::vector<SourceBufferRef>& source_buffers)
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


	// Copy func devs to top level frame
	FrameRef top_lvl_frame(new Frame());
	for(size_t i=0; i<func_defs.size(); ++i)
		top_lvl_frame->name_to_functions_map[func_defs[i]->sig.name].push_back(func_defs[i]);


	//BufferRoot* root = dynamic_cast<BufferRoot*>(rootref.getPointer());

	// Do Type Coercion
	{
		std::vector<ASTNode*> stack;
		TraversalPayload payload(TraversalPayload::TypeCoercion, hidden_voidptr_arg, env);
		for(size_t i=0; i<func_defs.size(); ++i)
			func_defs[i]->traverse(payload, stack);
		//root->traverse(payload, stack);
		assert(stack.size() == 0);
	}


	// Link functions
	linker.addExternalFunctions(this->external_functions);
	linker.addFunctions(func_defs);

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


//	rootref->print(0, std::cout);

	
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

	//rootref->print(0, std::cout);
}


void VirtualMachine::build()
{
	//this->rootref->print(0, std::cout); // TEMP

	linker.buildLLVMCode(
		this->llvm_module,
		this->llvm_exec_engine->getTargetData()
	);

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
		llvm::FunctionPassManager fpm(this->llvm_module);

		// Set up the optimizer pipeline.  Start with registering info about how the
		// target lays out data structures.
		fpm.add(new llvm::TargetData(*this->llvm_exec_engine->getTargetData()));


		llvm::createStandardFunctionPasses(
			&fpm,
			2
			);


		llvm::PassManager pm;

		llvm::Pass* inlining_pass = llvm::createFunctionInliningPass();

		llvm::createStandardModulePasses(
			&pm,
			2, // optimisation level
			false, // optimise Size
			true, // unit at a time
			false, // unroll loops
			true, // simplify lib calls
			false, // have exceptions
			inlining_pass // inlining pass
			);



		/*		// Build list of functions with external linkage (entry points)
		std::vector<const char*> export_list;
		std::vector<std::string> export_list_strings;
		for(unsigned int i=0; i<entry_point_function_sigs.size(); ++i)
		{
		//if(compiled_functions.count(entry_point_function_sigs[i]) == 0)
		//	throw VMExcep("entry_point_function_sigs");

		if(compiled_functions.count(entry_point_function_sigs[i]) > 0)
		{
		if(compiled_functions[entry_point_function_sigs[i]]->llvm_func == NULL)
		throw VMInternalExcep("compiled_functions[entry_point_function_sigs[i]]->llvm_func == NULL");

		export_list_strings.push_back(compiled_functions[entry_point_function_sigs[i]]->llvm_func->getName());
		}
		}

		for(unsigned int i=0; i<export_list_strings.size(); ++i)
		export_list.push_back(export_list_strings[i].c_str());

		pm.add(llvm::createInternalizePass(export_list));

		pm.add(llvm::createGlobalDCEPass()); // Delete unreachable internal functions / global vars
		*/
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


	{
		// Dump to stdout
		//this->llvm_module->dump();

		/*std::string errorinfo;
		llvm::raw_fd_ostream f(
		"module.txt",
		errorinfo
		);
		this->llvm_module->print(f, NULL);*/

		//TEMP:
		this->compileToNativeAssembly(this->llvm_module, "module_assembly.txt");
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


//void* VirtualMachine::getJittedFunctionByName(const std::string& name)
//{
//	FunctionDefinitionRef func = linker.findMatchingFunctionByName(name);
//
//	return this->llvm_exec_engine->getPointerToFunction(
//		func->built_llvm_function
//		);
//}


} // end namespace Winter
