/*=====================================================================
LLVMTypeUtils.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#pragma once


#include "wnt_Type.h"
namespace llvm { class Function; class Type; class LLVMContext; }


/*=====================================================================
LLVMTypeUtils
-------------------

=====================================================================*/
namespace LLVMTypeUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n);

const llvm::Type* pointerType(const llvm::Type& type);


}; // end namespace LLVMTypeUtils
