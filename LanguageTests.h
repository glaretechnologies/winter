/*=====================================================================
LanguageTests.h
---------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


namespace Winter
{


class LanguageTests
{
public:
	LanguageTests();
	~LanguageTests();

#if BUILD_TESTS
	static void doLLVMInit();
	

	static void run();
#endif

private:
};


}


