/*=====================================================================
PerfTests.cpp
-------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "PerfTests.h"


#include "VirtualMachine.h"
#include "wnt_MathsFuncs.h"
#include <utils/Timer.h>
#include <utils/ConPrint.h>
#include <utils/StringUtils.h>
#include <maths/mathstypes.h>


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
}


} // end namespace Winter
