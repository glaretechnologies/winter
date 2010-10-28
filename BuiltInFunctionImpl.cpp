#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
#include "wnt_ASTNode.h"
#include <vector>
#include "LLVMTypeUtils.h"
#if USE_LLVM
#include "llvm/Type.h"
#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/CallingConv.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Intrinsics.h>
#endif


using std::vector;


namespace Winter
{


Constructor::Constructor(Reference<StructureType>& struct_type_)
:	struct_type(struct_type_)
{
}


Value* Constructor::invoke(VMState& vmstate)
{
	vector<Value*> field_values(this->struct_type->component_names.size());
	
	for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		field_values[i] = vmstate.argument_stack[vmstate.argument_stack.size() - this->struct_type->component_types.size() + i]->clone();

	return new StructureValue(field_values);
}


llvm::Value* Constructor::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	//TEMP: add type alias for structure type to the module while we're at it.
	params.module->addTypeName(this->struct_type->name, this->struct_type->LLVMType(*params.context));

	if(this->struct_type->passByValue())
	{
		llvm::Value* s = llvm::UndefValue::get(this->struct_type->LLVMType(*params.context));

		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			llvm::Value* arg_value = LLVMTypeUtils::getNthArg(params.currently_building_func, i);

			s = params.builder->CreateInsertValue(
				s,
				arg_value,
				i
			);
		}
		
		return s;
	}
	else
	{


		// Pointer to structure will be in 0th argument.
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		// actual_struct_ptr = &arg_0[0]
		/*llvm::Value* actual_struct_ptr = params.builder->CreateGEP(
			struct_ptr, // ptr
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)) // index
			);*/

		// For each field in the structure
		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			// Get the argument to the constructor

			llvm::Value* arg_value = LLVMTypeUtils::getNthArg(params.currently_building_func, i + 1);

			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)));
			
			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices.begin(),
				indices.end()
			);

			// Store it in the appropriate field in the structure.
			// field_ptr = &actual_struct_ptr.index_i
			/*llvm::Value* field_ptr = params.builder->CreateGEP(
				actual_struct_ptr, // ptr
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)) // index
			);*/

			params.builder->CreateStore(
				arg_value, // value
				field_ptr // ptr
			);
		}

		//assert(0);
		//return struct_ptr;
		//params.builder->
		return NULL;
	}
}


Value* GetField::invoke(VMState& vmstate)
{
	// Top param on arg stack should be a structure
	const StructureValue* s = dynamic_cast<const StructureValue*>(vmstate.argument_stack.back());

	assert(s);
	assert(this->index < s->fields.size());

	return s->fields[this->index]->clone();
}


llvm::Value* GetField::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	if(this->struct_type->passByValue())
	{
		return params.builder->CreateExtractValue(
			LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
			this->index
		);
	}
	else
	{
		// Pointer to structure will be in 0th argument.
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		vector<llvm::Value*> indices;
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
		indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index, true)));

		llvm::Value* field_ptr = params.builder->CreateGEP(
			struct_ptr, // ptr
			indices.begin(),
			indices.end()
			);

		return params.builder->CreateLoad(
			field_ptr
		);
	}
}


Value* GetVectorElement::invoke(VMState& vmstate)
{
	// Top param on arg stack should be a vector
	const VectorValue* vec = dynamic_cast<const VectorValue*>(vmstate.argument_stack.back());

	assert(vec);
	assert(this->index < vec->e.size());

	return vec->e[this->index]->clone();
}


llvm::Value* GetVectorElement::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* vec_value = NULL;
	if(true) // TEMP shouldPassByValue(*this->type()))
	{
		vec_value = LLVMTypeUtils::getNthArg(
			params.currently_building_func, 
			0
		);
	}
	else
	{
		vec_value = params.builder->CreateLoad(
			LLVMTypeUtils::getNthArg(params.currently_building_func, 0),
			false, // true,// TEMP: volatile = true to pick up returned vector);
			"argument" // name
		);
	}

	return params.builder->CreateExtractElement(
		vec_value, // vec
		llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index))
	);
}


Value* ArrayMapBuiltInFunc::invoke(VMState& vmstate)
{
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()]);
	const ArrayValue* from = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1]);

	assert(f);
	assert(from);

	ArrayValue* retval = new ArrayValue();
	retval->e.resize(from->e.size());

	for(unsigned int i=0; i<from->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(from->e[i]); // Push value arg
		
		retval->e[i] = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();
	}

	return retval;
}


llvm::Value* ArrayMapBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}

//------------------------------------------------------------------------------------


Value* ArrayFoldBuiltInFunc::invoke(VMState& vmstate)
{
	// fold(function<T, T, T> func, array<T> array, T initial val) T
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()]);
	const ArrayValue* arr = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1]);
	const Value* initial_val = vmstate.argument_stack[vmstate.func_args_start.back() + 2];

	assert(f && arr && initial_val);

	Value* running_val = initial_val->clone();
	for(unsigned int i=0; i<arr->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(arr->e[i]); // Push value arg
		
		Value* new_running_val = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();

		delete running_val;
		running_val = new_running_val;
	}

	return running_val;
}


llvm::Value* ArrayFoldBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}


//----------------------------------------------------------------------------------------------


Value* IfBuiltInFunc::invoke(VMState& vmstate)
{
	const Value* condition = vmstate.argument_stack[vmstate.argument_stack.size() - 3];
	assert(dynamic_cast<const BoolValue*>(condition));

	if(static_cast<const BoolValue*>(condition)->value) // If condition is true
	{
		return vmstate.argument_stack[vmstate.argument_stack.size() - 2]->clone();
	}
	else
	{
		return vmstate.argument_stack[vmstate.argument_stack.size() - 1]->clone();
	}
}


llvm::Value* IfBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
#if USE_LLVM

	const int arg_offset = this->T->passByValue() ? 0 : 1;

	llvm::Value* condition_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 0 + arg_offset);


	//llvm::Value* child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);
	//llvm::Value* child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);
	llvm::Value* child_a_code = NULL;
	llvm::Value* child_b_code = NULL;
	if(this->T->passByValue())
	{
		child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);
		child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);
	}
	else
	{

		child_a_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 1 + arg_offset);

		/*child_a_code = params.builder->CreateLoad(
			a_ptr
		);*/

		//child_a_code = params.builder->CreateStore(
		//	a_val, // value
		//	return_val_ptr // ptr
		//	);

		child_b_code = LLVMTypeUtils::getNthArg(params.currently_building_func, 2 + arg_offset);

		/*child_b_code = params.builder->CreateLoad(
		b_ptr
		);*/

		//child_b_code = params.builder->CreateStore(
		//	b_val, // value
		//	return_val_ptr // ptr
		//	);
	}






	// Get a pointer to the current function
	llvm::Function* the_function = params.builder->GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the end of the function.
	llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*params.context, "then", the_function);
	llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*params.context, "else");
	llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*params.context, "ifcont");

	params.builder->CreateCondBr(condition_code, ThenBB, ElseBB);

	// Emit then value.
	params.builder->SetInsertPoint(ThenBB);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = params.builder->GetInsertBlock();

	// Emit else block.
	the_function->getBasicBlockList().push_back(ElseBB);
	params.builder->SetInsertPoint(ElseBB);

	params.builder->CreateBr(MergeBB);

	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = params.builder->GetInsertBlock();


	// Emit merge block.
	the_function->getBasicBlockList().push_back(MergeBB);
	params.builder->SetInsertPoint(MergeBB);
	llvm::PHINode *PN = params.builder->CreatePHI(
		this->T->passByValue() ? this->T->LLVMType(*params.context) : LLVMTypeUtils::pointerType(*this->T->LLVMType(*params.context)),
		"iftmp"
	);

	PN->addIncoming(child_a_code, ThenBB);
	PN->addIncoming(child_b_code, ElseBB);

	llvm::Value* phi_result = PN;

	if(this->T->passByValue())
		return phi_result;
	else
	{
		llvm::Value* arg_val = params.builder->CreateLoad(
			phi_result
		);

		llvm::Value* return_val_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		params.builder->CreateStore(
			arg_val, // value
			return_val_ptr // ptr
		);
		return NULL;
	}

#else
	return NULL;
#endif
}


}
