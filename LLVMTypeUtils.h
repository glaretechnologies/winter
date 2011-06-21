/*=====================================================================
LLVMTypeUtils.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
namespace llvm { class Function; class Type; class LLVMContext; class Twine; }
#if USE_LLVM
#include <llvm/Support/IRBuilder.h>
#endif


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

const llvm::Type* pointerType(const llvm::Type& type);

const llvm::Type* voidPtrType(llvm::LLVMContext& context);

const llvm::Type* getBaseCapturedVarStructType(llvm::LLVMContext& context);
const llvm::Type* getPtrToBaseCapturedVarStructType(llvm::LLVMContext& context);

llvm::FunctionType* llvmFunctionType(const std::vector<TypeRef>& arg_types, 
									 bool captured_var_struct_ptr_arg,
									 TypeRef return_type, 
									 llvm::LLVMContext& context,
									 bool hidden_voidptr_arg
									 );

llvm::Value* createFieldLoad(llvm::Value* structure_ptr, int field_index, llvm::IRBuilder<>* builder, llvm::LLVMContext& context, const llvm::Twine& Name);


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
