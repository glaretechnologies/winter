/*=====================================================================
FuzzTests.h
-----------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include <string>


#if BUILD_TESTS


namespace Winter
{


/*=====================================================================
FuzzTests
-------------------

=====================================================================*/
void fuzzTests();


void doASTFuzzTests();


bool testFuzzProgram(const std::string& src);


} // end namespace Winter


#endif // #if BUILD_TESTS

