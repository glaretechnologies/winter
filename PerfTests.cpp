/*=====================================================================
PerfTests.cpp
-------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "PerfTests.h"


#include "LanguageTestUtils.h"
#include "VirtualMachine.h"
#include "wnt_MathsFuncs.h"
#include "utils/TestUtils.h"
#include "utils/Timer.h"
#include "utils/ConPrint.h"
#include "utils/StringUtils.h"
#include "maths/mathstypes.h"


namespace Winter
{


PerfTests::PerfTests()
{}


PerfTests::~PerfTests()
{}


static float refFloatTrigFunc(float x)
{
	return std::sin(x) + std::cos(x + 0.4f) * std::tan(x * 0.4f) + std::sin(x * 1.4f);
}


void PerfTests::run()
{
#if BUILD_TESTS
	const std::string float_trig_winter_src = "def main(float x) float : sin(x) + cos(x + 0.4f) * tan(x * 0.4f) + sin(x * 1.4f)";

	const int num_trials = 100;
	const int num_iters = 200000;

	//------------ Measure refFloatTrigFunc time -------------
	{
		double min_elapsed = std::numeric_limits<double>::max();
		float sum = 0;
		for(int t=0; t<num_trials; ++t)
		{
			Timer timer;
			for(int i=0; i<num_iters; ++i)
			{
				sum += refFloatTrigFunc((float)i * 0.0001f);
			}
			min_elapsed = myMin(min_elapsed, timer.elapsed());
		}
		conPrint("refFloatTrigFunc:                    " + toString(min_elapsed * 1.0e9 / num_iters) + " ns");
		printVar(sum);
	}

	//------------ Measure winter float trig time -------------
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", float_trig_winter_src));
		MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));
		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull()) throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::FloatType) throw BaseException("main did not return float.");

		float(WINTER_JIT_CALLING_CONV* jitted_func)(float) = (float(WINTER_JIT_CALLING_CONV*)(float))vm.getJittedFunction(mainsig);

		double min_elapsed = std::numeric_limits<double>::max();
		float sum = 0;
		for(int t=0; t<num_trials; ++t)
		{
			Timer timer;
			for(int i=0; i<num_iters; ++i)
			{
				sum += jitted_func((float)i * 0.0001f);
			}
			min_elapsed = myMin(min_elapsed, timer.elapsed());
		}
		conPrint("refFloatTrigFunc:                    " + toString(min_elapsed * 1.0e9 / num_iters) + " ns");
		printVar(sum);
	}

	//------------ Measure winter float trig time with small code model -------------
	{
		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", float_trig_winter_src));
		MathsFuncs::appendExternalMathsFuncs(vm_args.external_functions);

		vm_args.small_code_model = true;

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(1, new Float()));
		vm_args.entry_point_sigs.push_back(mainsig);

		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);
		if(maindef.isNull()) throw BaseException("Failed to find function " + mainsig.toString());
		if(maindef->returnType()->getType() != Type::FloatType) throw BaseException("main did not return float.");

		float(WINTER_JIT_CALLING_CONV* jitted_func)(float) = (float(WINTER_JIT_CALLING_CONV*)(float))vm.getJittedFunction(mainsig);

		double min_elapsed = std::numeric_limits<double>::max();
		float sum = 0;
		for(int t=0; t<num_trials; ++t)
		{
			Timer timer;
			for(int i=0; i<num_iters; ++i)
			{
				sum += jitted_func((float)i * 0.0001f);
			}
			min_elapsed = myMin(min_elapsed, timer.elapsed());
		}
		conPrint("refFloatTrigFunc (small code model): " + toString(min_elapsed * 1.0e9 / num_iters) + " ns");
		printVar(sum);
	}


	//------------ Measure the performance of some AVX code, to make sure the transitions are correct (with vzeroupper etc..) -----------
	try
	{
		const std::string src = "struct Float8Struct { vector<float, 8> v } \n\
			def min(Float8Struct a, Float8Struct b) Float8Struct : Float8Struct(min(a.v, b.v)) \n\
			def main(Float8Struct a, Float8Struct b) Float8Struct : \n\
				min(a, b)";

		VMConstructionArgs vm_args;
		vm_args.source_buffers.push_back(new SourceBuffer("buffer", src));

		const std::vector<std::string> field_names(1, "v");
		const std::vector<TypeVRef> field_types(1, new VectorType(new Float(), 8));
		TypeVRef float_8_struct_type = new StructureType("Float8Struct", field_types, field_names);

		const FunctionSignature mainsig("main", std::vector<TypeVRef>(2, float_8_struct_type));
		vm_args.entry_point_sigs.push_back(mainsig);
		VirtualMachine vm(vm_args);

		Reference<FunctionDefinition> maindef = vm.findMatchingFunction(mainsig);

		void (WINTER_JIT_CALLING_CONV *f)(Float8Struct*, const Float8Struct*, const Float8Struct*) = 
			(void (WINTER_JIT_CALLING_CONV *)(Float8Struct*, const Float8Struct*, const Float8Struct*))vm.getJittedFunction(mainsig);

		Float8Struct a;
		a.v.e[0] = 10;
		a.v.e[1] = 2;
		a.v.e[2] = 30;
		a.v.e[3] = 4;
		a.v.e[4] = 4;
		a.v.e[5] = 5;
		a.v.e[6] = 6;
		a.v.e[7] = 7;

		Float8Struct b;
		b.v.e[0] = 1;
		b.v.e[1] = 20;
		b.v.e[2] = 3;
		b.v.e[3] = 40;
		b.v.e[4] = 4;
		b.v.e[5] = 5;
		b.v.e[6] = 6;
		b.v.e[7] = 7;

		Float8Struct target_result;
		target_result.v.e[0] = 1;
		target_result.v.e[1] = 2;
		target_result.v.e[2] = 3;
		target_result.v.e[3] = 4;
		target_result.v.e[4] = 4;
		target_result.v.e[5] = 5;
		target_result.v.e[6] = 6;
		target_result.v.e[7] = 7;

		// Call the JIT'd function
		Float8Struct jitted_result;

		double min_elapsed = std::numeric_limits<double>::max();
		//float sum = 0;
		for(int t=0; t<num_trials; ++t)
		{
			Timer timer;
			for(int i=0; i<num_iters; ++i)
			{
				f(&jitted_result, &a, &b);
			}
			min_elapsed = myMin(min_elapsed, timer.elapsed());
		}

		testAssert(epsEqual(jitted_result, target_result)); // Check JIT'd result.

		conPrint("AVX test: " + toString(min_elapsed * 1.0e9 / num_iters) + " ns");
	}
	catch(Winter::BaseException& e)
	{
		failTest(e.what());
	}
#endif
}


} // end namespace Winter
