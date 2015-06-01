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


procedure destructor:
---------------------
	for each field/element:
		calls decrementor



procedure decrementor:
----------------------
entry:
	if ref count == 1:
then:
		call destructor
		if heap allocated:
heap_allocated_then_BB:
			call free
else:
	else
		decrement ref count
merge:
	ret void


=====================================================================*/
namespace RefCounting
{

// Emit LLVM code for incrStringRefCount(), incrVArrayRefCount() etc.. to the current LLVM module, store pointers to them in common_functions.
void emitRefCountingFunctions(llvm::Module* module, const llvm::DataLayout* target_data, CommonFunctions& common_functions);

void emitDecrementorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& refcounted_type);
void emitDestructorForType(llvm::Module* module, const llvm::DataLayout* target_data, const CommonFunctions& common_functions, const ConstTypeRef& type);

llvm::Function* getOrInsertDecrementorForType(llvm::Module* module, const ConstTypeRef& type);
llvm::Function* getOrInsertDestructorForType(llvm::Module* module, const ConstTypeRef& type);

} // end namespace RefCounting


} // end namespace Winter

