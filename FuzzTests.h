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
void fuzzTests(const std::string& fuzzer_input_dir, const std::string& fuzzer_output_dir);


void doASTFuzzTests(const std::string& fuzzer_input_dir, const std::string& fuzzer_output_dir);


bool testFuzzProgram(const std::string& src);


} // end namespace Winter


#endif // #if BUILD_TESTS

