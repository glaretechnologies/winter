/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#include "LLVMTypeUtils.h"


#include "wnt_LLVMVersion.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
//#include "llvm/IR/Module.h"
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


namespace Winter
{


namespace LLVMUtils
{


#if TARGET_LLVM_VERSION >= 60
llvm::Value* createStructGEP(llvm::IRBuilder</*true, */llvm::ConstantFolder, llvm::IRBuilderDefaultInserter/*<true>*/ >* builder,
		llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name)
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
		llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name)
#endif
{
#if TARGET_LLVM_VERSION >= 60 // not sure the actual version the type arg was introduced.
	return builder->CreateStructGEP(NULL, struct_ptr, field_index);
#else
	return builder->CreateStructGEP(      struct_ptr, field_index);
#endif
}


}; // end namespace LLVMUtils


}; // end namespace Winter
