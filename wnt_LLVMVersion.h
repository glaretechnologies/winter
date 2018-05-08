/*=====================================================================
wnt_LLVMVersion.h
-----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


// LLVM version 3.4 should be '34'.
// LLVM version 3.6 should be '36'.
// LLVM Version 6.0.0 should be '60'.
#define TARGET_LLVM_VERSION 34

// LLVM 3.6 is the last LLVM version that builds with vs2012.
// NOTE: llvm 3.6 crashes on Mac.
