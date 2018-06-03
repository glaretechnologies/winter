/*=====================================================================
LLVMTypeUtils.h
-------------------
Copyright Glare Technologies Limited 2018 -
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

llvm::Type* pointerType(llvm::Type& type);
llvm::Type* pointerType(llvm::Type* type);

llvm::Type* voidPtrType(llvm::LLVMContext& context);

llvm::Type* getBaseCapturedVarStructType(llvm::Module& module);
llvm::Type* getPtrToBaseCapturedVarStructType(llvm::Module& module);

llvm::FunctionType* llvmFunctionType(const std::vector<TypeVRef>& arg_types, 
									 bool captured_var_struct_ptr_arg,
									 TypeVRef return_type, 
									 llvm::Module& module
									 );

}; // end namespace LLVMTypeUtils


}; // end namespace Winter
