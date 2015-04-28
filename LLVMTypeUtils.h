/*=====================================================================
LLVMTypeUtils.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#pragma once


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
LLVMTypeUtils
-------------------

=====================================================================*/
namespace LLVMTypeUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n);
llvm::Value* getLastArg(llvm::Function *func);

llvm::Type* pointerType(llvm::Type& type);

llvm::Type* voidPtrType(llvm::LLVMContext& context);

llvm::Type* getBaseCapturedVarStructType(llvm::LLVMContext& context);
llvm::Type* getPtrToBaseCapturedVarStructType(llvm::LLVMContext& context);

llvm::FunctionType* llvmFunctionType(const std::vector<TypeRef>& arg_types, 
									 bool captured_var_struct_ptr_arg,
									 TypeRef return_type, 
									 llvm::Module& module
									 //bool hidden_voidptr_arg
									 );

llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, llvm::IRBuilder<>* builder, const llvm::Twine& name);

void createCollectionCopy(const TypeRef& collection_type, llvm::Value* dest_ptr, llvm::Value* src_ptr, EmitLLVMCodeParams& params);


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
