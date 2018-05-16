/*=====================================================================
LLVMUtils.h
-----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"
#ifdef _MSC_VER // If compiling with Visual C++
#pragma warning(push, 0) // Disable warnings
#endif
#include <llvm/IR/IRBuilder.h>
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
#include "wnt_Type.h"


namespace llvm { class Function; class FunctionType; class Type; class LLVMContext; class Twine; class Module; }
class EmitLLVMCodeParams;


namespace Winter
{


/*=====================================================================
LLVMUtils
---------

=====================================================================*/
namespace LLVMUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n);
llvm::Value* getLastArg(llvm::Function *func);


#if TARGET_LLVM_VERSION >= 60
llvm::Value* createStructGEP(llvm::IRBuilder</*true, */llvm::ConstantFolder, llvm::IRBuilderDefaultInserter/*<true>*/ >* builder,
	llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name = "");
#else
llvm::Value* createStructGEP(llvm::IRBuilder<true, llvm::ConstantFolder, llvm::IRBuilderDefaultInserter<true> >* builder,
	llvm::Value* struct_ptr, unsigned int field_index, const llvm::Twine& name = "");
#endif


//llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, llvm::IRBuilder<>* builder, const llvm::Twine& name);


void createCollectionCopy(const TypeVRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params);


}; // end namespace LLVMUtils


}; // end namespace Winter
