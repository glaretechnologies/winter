#include "BuiltInFunctionImpl.h"


#include "VMState.h"
#include "Value.h"
#include "wnt_ASTNode.h"
#include "wnt_FunctionDefinition.h"
#include <vector>
#include "LLVMTypeUtils.h"
#include "utils/platformutils.h"
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


ValueRef Constructor::invoke(VMState& vmstate)
{
	vector<ValueRef> field_values(this->struct_type->component_names.size());

	const size_t func_args_start = vmstate.func_args_start.back();
	
	for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		field_values[i] = vmstate.argument_stack[func_args_start + i];

	return ValueRef(new StructureValue(field_values));
}


llvm::Value* Constructor::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	//TEMP: add type alias for structure type to the module while we're at it.
	//TEMP not accepted as of llvm 3.0 
	//params.module->addTypeName(this->struct_type->name, this->struct_type->LLVMType(*params.context));

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


		// Pointer to structure memory will be in 0th argument.
		llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

		// For each field in the structure
		for(unsigned int i=0; i<this->struct_type->component_types.size(); ++i)
		{
			// Get the argument to the constructor

	
			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)));
			
			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices
			);

			// Store it in the appropriate field in the structure.
			// field_ptr = &actual_struct_ptr.index_i
			/*llvm::Value* field_ptr = params.builder->CreateGEP(
				actual_struct_ptr, // ptr
				llvm::ConstantInt::get(*params.context, llvm::APInt(32, i, true)) // index
			);*/


			llvm::Value* arg_value = LLVMTypeUtils::getNthArg(params.currently_building_func, i + 1);
			if(!this->struct_type->component_types[i]->passByValue())
			{
				// Load the value from memory
				arg_value = params.builder->CreateLoad(
					arg_value // ptr
				);
			}

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


ValueRef GetField::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	// Top param on arg stack should be a structure
	assert(dynamic_cast<const StructureValue*>(vmstate.argument_stack[func_args_start].getPointer()));
	const StructureValue* s = static_cast<const StructureValue*>(vmstate.argument_stack[func_args_start].getPointer());

	assert(s);
	assert(this->index < s->fields.size());

	return s->fields[this->index];
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
		TypeRef field_type = this->struct_type->component_types[this->index];

		if(field_type->passByValue())
		{
			// Pointer to structure will be in 0th argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index, true)));

			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices
				);

			return params.builder->CreateLoad(
				field_ptr
			);
		}
		else
		{
			// Pointer to memory for return value will be 0th argument.
			llvm::Value* return_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);

			// Pointer to structure will be in 1st argument.
			llvm::Value* struct_ptr = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

			vector<llvm::Value*> indices;
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0, true)));
			indices.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, this->index, true)));

			llvm::Value* field_ptr = params.builder->CreateGEP(
				struct_ptr, // ptr
				indices
			);

			llvm::Value* field_val = params.builder->CreateLoad(
				field_ptr
			);

			params.builder->CreateStore(
				field_val, // value
				return_ptr // ptr
			);

			return NULL;
		}
	}
}


ValueRef GetVectorElement::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const VectorValue* vec = static_cast<const VectorValue*>(vmstate.argument_stack[func_args_start].getPointer());

	assert(vec);
	assert(this->index < vec->e.size());

	return vec->e[this->index];
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


ValueRef ArrayMapBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const FunctionValue* f = static_cast<const FunctionValue*>(vmstate.argument_stack[func_args_start].getPointer());
	const ArrayValue* from = static_cast<const ArrayValue*>(vmstate.argument_stack[func_args_start + 1].getPointer());

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

	return ValueRef(retval);
}


llvm::Value* ArrayMapBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	assert(0);
	return NULL;
}

//------------------------------------------------------------------------------------


ValueRef ArrayFoldBuiltInFunc::invoke(VMState& vmstate)
{
	// fold(function<T, T, T> func, array<T> array, T initial val) T
	const FunctionValue* f = dynamic_cast<const FunctionValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
	const ArrayValue* arr = dynamic_cast<const ArrayValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	const ValueRef initial_val = vmstate.argument_stack[vmstate.func_args_start.back() + 2];

	assert(f && arr && initial_val.nonNull());

	ValueRef running_val = initial_val;
	for(unsigned int i=0; i<arr->e.size(); ++i)
	{
		// Set up arg stack
		vmstate.func_args_start.push_back((unsigned int)vmstate.argument_stack.size());
		vmstate.argument_stack.push_back(running_val); // Push value arg
		vmstate.argument_stack.push_back(arr->e[i]); // Push value arg
		
		ValueRef new_running_val = f->func_def->invoke(vmstate);

		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.argument_stack.pop_back(); // Pop Value arg
		vmstate.func_args_start.pop_back();

		//delete running_val;
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


ValueRef IfBuiltInFunc::invoke(VMState& vmstate)
{
	const size_t func_args_start = vmstate.func_args_start.back();

	const Value* condition = vmstate.argument_stack[func_args_start].getPointer();
	assert(dynamic_cast<const BoolValue*>(condition));

	if(static_cast<const BoolValue*>(condition)->value) // If condition is true
	{
		return vmstate.argument_stack[func_args_start + 1];
	}
	else
	{
		return vmstate.argument_stack[func_args_start + 2];
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
		0, // num reserved values
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


//----------------------------------------------------------------------------------------------


ValueRef DotProductBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);

	FloatValue* res = new FloatValue(0.0f);

	for(unsigned int i=0; i<vector_type->num; ++i)
	{
		res->value += static_cast<const FloatValue*>(a->e[i].getPointer())->value + static_cast<const FloatValue*>(b->e[i].getPointer())->value;
	}

	return ValueRef(res);
}


llvm::Value* DotProductBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	// If (have sse4.1)
	if(params.cpu_info->sse4_1)
	{
		// emit dot product intrinsic

		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);
		args.push_back(llvm::ConstantInt::get(*params.context, llvm::APInt(32, 255))); // SSE DPPS control bits

		llvm::Function* dot_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse41_dpps);

		// dot product intrinsic returns a 4-vector.
		llvm::Value* vector_res = params.builder->CreateCall(dot_func, args, "Vector_res");

		return params.builder->CreateExtractElement(
			vector_res, // vec
			llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)) // index
		);
	}
	else
	{
		// emit call to dotproduct fallback: _dotProduct()
		//assert(0);
		//return NULL;

		// x = a[0] * b[0]
		llvm::Value* x = params.builder->CreateBinOp(
			llvm::Instruction::FMul, 
			params.builder->CreateExtractElement(a, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0))),
			params.builder->CreateExtractElement(b, llvm::ConstantInt::get(*params.context, llvm::APInt(32, 0)))
		);
			
		for(unsigned int i=1; i<this->vector_type->num; ++i)
		{
			// y = a[i] * b[i]
			llvm::Value* y = params.builder->CreateBinOp(
				llvm::Instruction::FMul, 
				params.builder->CreateExtractElement(a, llvm::ConstantInt::get(*params.context, llvm::APInt(32, i))),
				params.builder->CreateExtractElement(b, llvm::ConstantInt::get(*params.context, llvm::APInt(32, i)))
			);

			// x = x + y
			x = params.builder->CreateBinOp(
				llvm::Instruction::FAdd, 
				x,
				y
			);
		}

		return x;
	}
}


//----------------------------------------------------------------------------------------------


ValueRef VectorMinBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);

	
	vector<ValueRef> res_values(vector_type->num);

	for(unsigned int i=0; i<vector_type->num; ++i)
	{
		const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
		const float y = static_cast<const FloatValue*>(b->e[i].getPointer())->value;
		res_values[i] = ValueRef(new FloatValue(x < y ? x : y));
	}

	return ValueRef(new VectorValue(res_values));
}


llvm::Value* VectorMinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	if(params.cpu_info->sse1)
	{
		// emit dot product intrinsic

		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);

		llvm::Function* minps_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse_min_ps);

		return params.builder->CreateCall(minps_func, args);
	}
	else
	{
		assert(!"VectorMinBuiltInFunc::emitLLVMCode assumes sse");
		return NULL;
	}
}


//----------------------------------------------------------------------------------------------


ValueRef VectorMaxBuiltInFunc::invoke(VMState& vmstate)
{
	const VectorValue* a = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const VectorValue* b = dynamic_cast<const VectorValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());
	assert(a && b);


	vector<ValueRef> res_values(vector_type->num);

	for(unsigned int i=0; i<vector_type->num; ++i)
	{
		const float x = static_cast<const FloatValue*>(a->e[i].getPointer())->value;
		const float y = static_cast<const FloatValue*>(b->e[i].getPointer())->value;
		res_values[i] = ValueRef(new FloatValue(x > y ? x : y));
	}

	return ValueRef(new VectorValue(res_values));
}


llvm::Value* VectorMaxBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	llvm::Value* a = LLVMTypeUtils::getNthArg(params.currently_building_func, 0);
	llvm::Value* b = LLVMTypeUtils::getNthArg(params.currently_building_func, 1);

	if(params.cpu_info->sse1)
	{
		// emit dot product intrinsic

		vector<llvm::Value*> args;
		args.push_back(a);
		args.push_back(b);

		llvm::Function* maxps_func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::x86_sse_max_ps);

		return params.builder->CreateCall(maxps_func, args);
	}
	else
	{
		assert(!"VectorMaxBuiltInFunc::emitLLVMCode assumes sse");
		return NULL;
	}
}


//----------------------------------------------------------------------------------------------


ValueRef PowBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 0].getPointer());
	const FloatValue* b = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back() + 1].getPointer());

	return ValueRef(new FloatValue(std::pow(a->value, b->value)));
}


llvm::Value* PowBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	vector<llvm::Value*> args;
	args.push_back(LLVMTypeUtils::getNthArg(params.currently_building_func, 0));
	args.push_back(LLVMTypeUtils::getNthArg(params.currently_building_func, 1));

	vector<llvm::Type*> types;
	types.push_back(TypeRef(new Float())->LLVMType(*params.context));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, llvm::Intrinsic::pow, types);

	assert(func);
	assert(func->isIntrinsic());

	return params.builder->CreateCall(
		func,
		args
	);
}


//----------------------------------------------------------------------------------------------


static llvm::Value* emitFloatFloatIntrinsic(EmitLLVMCodeParams& params, llvm::Intrinsic::ID id)
{
	vector<llvm::Value*> args;
	args.push_back(LLVMTypeUtils::getNthArg(params.currently_building_func, 0));

	vector<llvm::Type*> types;
	types.push_back(TypeRef(new Float())->LLVMType(*params.context));

	llvm::Function* func = llvm::Intrinsic::getDeclaration(params.module, id, types);

	return params.builder->CreateCall(
		func,
		args
	);
}


ValueRef SqrtBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::sqrt(a->value)));
}


llvm::Value* SqrtBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitFloatFloatIntrinsic(params, llvm::Intrinsic::sqrt);
}


//----------------------------------------------------------------------------------------------

ValueRef SinBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::sin(a->value)));
}


llvm::Value* SinBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitFloatFloatIntrinsic(params, llvm::Intrinsic::sin);
}


//----------------------------------------------------------------------------------------------


ValueRef CosBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue(std::cos(a->value)));
}


llvm::Value* CosBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return emitFloatFloatIntrinsic(params, llvm::Intrinsic::cos);
}


//----------------------------------------------------------------------------------------------


ValueRef TruncateToIntBuiltInFunc::invoke(VMState& vmstate)
{
	const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new IntValue((int)a->value));
}


llvm::Value* TruncateToIntBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return params.builder->CreateFPToSI(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		llvm::IntegerType::get(*params.context, 32)
	);
}


//----------------------------------------------------------------------------------------------


ValueRef ToFloatBuiltInFunc::invoke(VMState& vmstate)
{
	const IntValue* a = static_cast<const IntValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());

	return ValueRef(new FloatValue((float)a->value));
}


llvm::Value* ToFloatBuiltInFunc::emitLLVMCode(EmitLLVMCodeParams& params) const
{
	return params.builder->CreateSIToFP(
		LLVMTypeUtils::getNthArg(params.currently_building_func, 0), 
		llvm::Type::getFloatTy(*params.context) // dest type
	);
}


//----------------------------------------------------------------------------------------------


//ValueRef AllocateRefCountedStructure::invoke(VMState& vmstate)
//{
//	assert(0);
//	return ValueRef();
//	//const FloatValue* a = static_cast<const FloatValue*>(vmstate.argument_stack[vmstate.func_args_start.back()].getPointer());
//
//	//return ValueRef(new FloatValue(std::cos(a->value)));
//}
//
//AllocateRefCountedStructure::emitLLVMCode(EmitLLVMCodeParams& params) const
//{
//	//return emitFloatFloatIntrinsic(params, llvm::Intrinsic::cos);
//}


}
