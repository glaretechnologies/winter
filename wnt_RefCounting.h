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

// Emit LLVM code for decrStringRefCount(), incrStringRefCount() etc.. to the current LLVM module, store pointers to them in common_functions.
void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions);

void emitDestructorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& refcounted_type);

llvm::Function* getOrInsertDestructorForType(llvm::Module* module, const ConstTypeRef& type);

//void emitCleanupLLVMCode(EmitLLVMCodeParams& params, const TypeRef& type, llvm::Value* val);

// Emit a call to incrStringRefCount(string_val)
//void emitIncrementStringRefCount(EmitLLVMCodeParams& params, llvm::Value* string_val);

// Emit a call to decrStringRefCount(string_val)
//void emitStringCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* string_val);


//void emitIncrementVArrayRefCount(EmitLLVMCodeParams& params, llvm::Value* varray_val);
//void emitVArrayCleanupLLVMCode(EmitLLVMCodeParams& params, llvm::Value* varray_val);


} // end namespace RefCounting


} // end namespace Winter

