/*=====================================================================
LLVMTypeUtils.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Oct 20 15:22:37 +1300 2010
=====================================================================*/
#include "LLVMTypeUtils.h"


#if USE_LLVM
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
#endif


namespace LLVMTypeUtils
{


llvm::Value* getNthArg(llvm::Function *func, int n)
{
	llvm::Function::arg_iterator args = func->arg_begin();
	for(int i=0; i<n; ++i)
		args++;
	return args;
}


const llvm::Type* pointerType(const llvm::Type& type)
{
	return llvm::PointerType::get(
		&type, 
		0 // AddressSpace
	);
}


}; // end namespace LLVMTypeUtils
