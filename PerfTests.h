/*=====================================================================
PerfTests.h
-----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


namespace Winter
{


class PerfTests
{
public:
	PerfTests();
	~PerfTests();

	static void doLLVMInit();
	
	static void run();
};


}
