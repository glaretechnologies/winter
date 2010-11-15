/*=====================================================================
LLVMTypeUtils.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
namespace llvm { class Function; class Type; class LLVMContext; }


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

llvm::FunctionType* llvmFunctionType(const std::vector<TypeRef>& arg_types, TypeRef return_type, llvm::LLVMContext& context,
									 bool hidden_voidptr_arg);


}; // end namespace LLVMTypeUtils


}; // end namespace Winter
