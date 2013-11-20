/*=====================================================================
RefCounting.h
-------------------
Copyright Nicholas Chapman
Generated at 2011-04-25 19:15:39 +0100
=====================================================================*/
#pragma once


#include "wnt_ASTNode.h"


namespace Winter
{


/*=====================================================================
RefCounting
-------------------

=====================================================================*/
namespace RefCounting
{


void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions);

void emitCleanupLLVMCode(EmitLLVMCodeParams& params, const TypeRef& type, llvm::Value* val);

void emitIncrementStringRefCount(EmitLLVMCodeParams& params, llvm::Value* string_val);
void emitStringCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val);


} // end namespace RefCounting


} // end namespace Winter

