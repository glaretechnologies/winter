/*=====================================================================
FuzzTests.h
-----------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


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
