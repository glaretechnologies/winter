cmake_minimum_required (VERSION 3.0)


# Cmake variables that should be set before including this file:
#
# WINTER_DIR				- Path to the directory this file is in.
# WINTER_LLVM_VERSION		- LLVM version to use.  Should be one of 6.0.0, 8.0.0, 11.0.0, 15.0.7, 16.0.6.
# GLARE_CORE_TRUNK_DIR		- optional, used as a path prefix for utils files etc. from glare-core repo, whose paths are returned in WINTER_UTIL_FILES etc.
# WINTER_USE_OPENCL			- set to TRUE or FALSE, depending on if you want runtime OpenCL execution support.
# WINTER_INCLUDE_TESTS		- set to TRUE or FALSE, depending if you want to include unit testing code.

# This file sets the following variables: 
#
# WINTER_PREPROCESSOR_DEFINITIONS	- basically just "-DTARGET_LLVM_VERSION=xxx"
#
# WINTER_INCLUDE_DIRECTORIES		- list of needed include directories, from glare-core, for compiling required code (apart from LLVM include dir)
#
# WINTER_FILES						- list of winter .cpp and .h filenames needed for Winter.
#
# WINTER_LLVM_LIBS					- list of LLVM .lib or .a filenames needed for linking.
#
# WINTER_UTIL_FILES					- list of .cpp and .h paths from glare-core utils dir needed by Winter
# WINTER_DOUBLE_CONVERSION_FILES	- list of .cpp and .h paths from glare-core double_conversion dir needed by Winter
# WINTER_XXHASH_FILES				- list of .cpp and .h paths from glare-core xxhash dir needed by Winter
# WINTER_OPENCL_FILES				- list of .cpp and .h paths from glare-core openl dir needed by Winter
# 
# WINTER_OTHER_FILES				- a combined variable containing the contents of WINTER_LLVM_LIBS, WINTER_UTIL_FILES, WINTER_DOUBLE_CONVERSION_FILES, WINTER_XXHASH_FILES, WINTER_OPENCL_FILES


if(NOT DEFINED WINTER_DIR)
	MESSAGE(FATAL_ERROR "WINTER_DIR CMake variable must be defined before including embed_winter.cmake.")
endif()

if(NOT DEFINED WINTER_LLVM_VERSION)
	MESSAGE(FATAL_ERROR "WINTER_LLVM_VERSION CMake variable must be defined before including embed_winter.cmake.")
endif()

if(NOT DEFINED WINTER_USE_OPENCL)
	MESSAGE(FATAL_ERROR "WINTER_USE_OPENCL CMake variable must be defined (to TRUE or FALSE) before including embed_winter.cmake.")
endif()

# Set WINTER_PREPROCESSOR_DEFINITIONS
if(WINTER_LLVM_VERSION STREQUAL "16.0.6")
	SET(WINTER_PREPROCESSOR_DEFINITIONS "-DTARGET_LLVM_VERSION=160")
elseif(WINTER_LLVM_VERSION STREQUAL "15.0.7")
	SET(WINTER_PREPROCESSOR_DEFINITIONS "-DTARGET_LLVM_VERSION=150")
elseif(WINTER_LLVM_VERSION STREQUAL "11.0.0")
	SET(WINTER_PREPROCESSOR_DEFINITIONS "-DTARGET_LLVM_VERSION=110")
elseif(WINTER_LLVM_VERSION STREQUAL "8.0.0")
	SET(WINTER_PREPROCESSOR_DEFINITIONS "-DTARGET_LLVM_VERSION=80")
elseif(WINTER_LLVM_VERSION STREQUAL "6.0.0")
	SET(WINTER_PREPROCESSOR_DEFINITIONS "-DTARGET_LLVM_VERSION=60")
else()
	MESSAGE(FATAL_ERROR "Unsupported LLVM version '${WINTER_LLVM_VERSION}'")
endif()

if(WINTER_USE_OPENCL)
	SET(WINTER_PREPROCESSOR_DEFINITIONS
	${WINTER_PREPROCESSOR_DEFINITIONS}
	-DWINTER_OPENCL_SUPPORT=1
	)
endif()


set(WINTER_FILES
"${WINTER_DIR}/BuiltInFunctionImpl.cpp"
"${WINTER_DIR}/Linker.cpp"
"${WINTER_DIR}/LLVMTypeUtils.cpp"
"${WINTER_DIR}/TokenBase.cpp"
"${WINTER_DIR}/Value.cpp"
"${WINTER_DIR}/VirtualMachine.cpp"
"${WINTER_DIR}/wnt_ASTNode.cpp"
"${WINTER_DIR}/wnt_Diagnostics.cpp"
"${WINTER_DIR}/wnt_ExternalFunction.cpp"
"${WINTER_DIR}/wnt_FunctionDefinition.cpp"
"${WINTER_DIR}/wnt_FunctionExpression.cpp"
"${WINTER_DIR}/wnt_FunctionSignature.cpp"
"${WINTER_DIR}/wnt_LangParser.cpp"
"${WINTER_DIR}/wnt_Lexer.cpp"
"${WINTER_DIR}/wnt_SourceBuffer.cpp"
"${WINTER_DIR}/wnt_Type.cpp"
"${WINTER_DIR}/wnt_RefCounting.cpp"
"${WINTER_DIR}/ProofUtils.cpp"
"${WINTER_DIR}/wnt_ArrayLiteral.cpp"
"${WINTER_DIR}/wnt_TupleLiteral.cpp"
"${WINTER_DIR}/wnt_VectorLiteral.cpp"
"${WINTER_DIR}/wnt_VArrayLiteral.cpp"
"${WINTER_DIR}/wnt_Variable.cpp"
"${WINTER_DIR}/BaseException.h"
"${WINTER_DIR}/BuiltInFunctionImpl.h"
"${WINTER_DIR}/LanguageTests.h"
"${WINTER_DIR}/LanguageTestUtils.h"
"${WINTER_DIR}/Linker.h"
"${WINTER_DIR}/LLVMTypeUtils.h"
"${WINTER_DIR}/TokenBase.h"
"${WINTER_DIR}/Value.h"
"${WINTER_DIR}/VirtualMachine.h"
"${WINTER_DIR}/VMState.h"
"${WINTER_DIR}/wnt_ASTNode.h"
"${WINTER_DIR}/wnt_Diagnostics.h"
"${WINTER_DIR}/wnt_ExternalFunction.h"
"${WINTER_DIR}/wnt_FunctionDefinition.h"
"${WINTER_DIR}/wnt_FunctionExpression.h"
"${WINTER_DIR}/wnt_FunctionSignature.h"
"${WINTER_DIR}/wnt_LangParser.h"
"${WINTER_DIR}/wnt_Lexer.h"
"${WINTER_DIR}/wnt_SourceBuffer.h"
"${WINTER_DIR}/wnt_Type.h"
"${WINTER_DIR}/wnt_RefCounting.h"
"${WINTER_DIR}/ProofUtils.h"
"${WINTER_DIR}/wnt_IfExpression.cpp"
"${WINTER_DIR}/wnt_IfExpression.h"
"${WINTER_DIR}/wnt_ArrayLiteral.h"
"${WINTER_DIR}/wnt_TupleLiteral.h"
"${WINTER_DIR}/wnt_VectorLiteral.h"
"${WINTER_DIR}/wnt_VArrayLiteral.h"
"${WINTER_DIR}/wnt_Variable.h"
"${WINTER_DIR}/FuzzTests.h"
"${WINTER_DIR}/wnt_LetASTNode.cpp"
"${WINTER_DIR}/wnt_LetASTNode.h"
"${WINTER_DIR}/wnt_LetBlock.cpp"
"${WINTER_DIR}/wnt_LetBlock.h"
"${WINTER_DIR}/wnt_MathsFuncs.cpp"
"${WINTER_DIR}/wnt_MathsFuncs.h"
"${WINTER_DIR}/LLVMUtils.cpp"
"${WINTER_DIR}/LLVMUtils.h"
"${WINTER_DIR}/CompiledValue.cpp"
"${WINTER_DIR}/CompiledValue.h"
)

if(WINTER_INCLUDE_TESTS)
	set(WINTER_FILES
		${WINTER_FILES}
		"${WINTER_DIR}/LanguageTestUtils.cpp"
		"${WINTER_DIR}/LanguageTests.cpp"
		"${WINTER_DIR}/FuzzTests.cpp"
		"${WINTER_DIR}/PerfTests.cpp"
		"${WINTER_DIR}/PerfTests.h"
	)
endif()


########### Utils ################
SET(WINTER_UTIL_FILES
${GLARE_CORE_TRUNK_DIR}/utils/ArgumentParser.cpp
${GLARE_CORE_TRUNK_DIR}/utils/ArgumentParser.h
${GLARE_CORE_TRUNK_DIR}/utils/Clock.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Clock.h
${GLARE_CORE_TRUNK_DIR}/utils/Condition.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Condition.h
${GLARE_CORE_TRUNK_DIR}/utils/ConPrint.cpp
${GLARE_CORE_TRUNK_DIR}/utils/ConPrint.h
${GLARE_CORE_TRUNK_DIR}/utils/ContainerUtils.h
${GLARE_CORE_TRUNK_DIR}/utils/DynamicLib.cpp
${GLARE_CORE_TRUNK_DIR}/utils/DynamicLib.h
${GLARE_CORE_TRUNK_DIR}/utils/FileUtils.cpp
${GLARE_CORE_TRUNK_DIR}/utils/FileUtils.h
${GLARE_CORE_TRUNK_DIR}/utils/Lock.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Lock.h
${GLARE_CORE_TRUNK_DIR}/utils/MemAlloc.cpp
${GLARE_CORE_TRUNK_DIR}/utils/MemAlloc.h
${GLARE_CORE_TRUNK_DIR}/utils/MemMappedFile.cpp
${GLARE_CORE_TRUNK_DIR}/utils/MemMappedFile.h
${GLARE_CORE_TRUNK_DIR}/utils/Mutex.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Mutex.h
${GLARE_CORE_TRUNK_DIR}/utils/MyThread.cpp
${GLARE_CORE_TRUNK_DIR}/utils/MyThread.h
${GLARE_CORE_TRUNK_DIR}/utils/Parser.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Parser.h
${GLARE_CORE_TRUNK_DIR}/utils/Platform.h
${GLARE_CORE_TRUNK_DIR}/utils/PlatformUtils.cpp
${GLARE_CORE_TRUNK_DIR}/utils/PlatformUtils.h
${GLARE_CORE_TRUNK_DIR}/utils/PrintOutput.h
${GLARE_CORE_TRUNK_DIR}/utils/StandardPrintOutput.cpp
${GLARE_CORE_TRUNK_DIR}/utils/StandardPrintOutput.h
${GLARE_CORE_TRUNK_DIR}/utils/Reference.h
${GLARE_CORE_TRUNK_DIR}/utils/StringUtils.cpp
${GLARE_CORE_TRUNK_DIR}/utils/StringUtils.h
${GLARE_CORE_TRUNK_DIR}/utils/Task.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Task.h
${GLARE_CORE_TRUNK_DIR}/utils/TaskManager.cpp
${GLARE_CORE_TRUNK_DIR}/utils/TaskManager.h
${GLARE_CORE_TRUNK_DIR}/utils/TaskRunnerThread.cpp
${GLARE_CORE_TRUNK_DIR}/utils/TaskRunnerThread.h
${GLARE_CORE_TRUNK_DIR}/utils/TestUtils.cpp
${GLARE_CORE_TRUNK_DIR}/utils/TestUtils.h
${GLARE_CORE_TRUNK_DIR}/utils/Timer.cpp
${GLARE_CORE_TRUNK_DIR}/utils/Timer.h
${GLARE_CORE_TRUNK_DIR}/utils/VRef.h
${GLARE_CORE_TRUNK_DIR}/utils/UTF8Utils.cpp
${GLARE_CORE_TRUNK_DIR}/utils/UTF8Utils.h
)

SET(WINTER_DOUBLE_CONVERSION_FILES
${GLARE_CORE_TRUNK_DIR}/double-conversion/bignum.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/bignum.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/bignum-dtoa.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/bignum-dtoa.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/cached-powers.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/cached-powers.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/diy-fp.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/double-conversion.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/double-to-string.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/double-to-string.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/fast-dtoa.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/fast-dtoa.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/fixed-dtoa.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/fixed-dtoa.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/ieee.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/string-to-double.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/string-to-double.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/strtod.cc
${GLARE_CORE_TRUNK_DIR}/double-conversion/strtod.h
${GLARE_CORE_TRUNK_DIR}/double-conversion/utils.h
)

SET(WINTER_XXHASH_FILES
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common/xxhash.c
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common/xxhash.h
)


if(WINTER_USE_OPENCL)
	SET(WINTER_OPENCL_FILES
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCL.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCL.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLKernel.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLKernel.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLBuffer.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLBuffer.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLContext.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLContext.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLDevice.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLDevice.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLPlatform.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLPlatform.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLProgram.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLProgram.h
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLCommandQueue.cpp
	${GLARE_CORE_TRUNK_DIR}/opencl/OpenCLCommandQueue.h
	)
endif()

SET(WINTER_OTHER_FILES 
	${WINTER_UTIL_FILES} ${WINTER_DOUBLE_CONVERSION_FILES} ${WINTER_XXHASH_FILES} ${WINTER_OPENCL_FILES})


set(WINTER_INCLUDE_DIRECTORIES
#${WINTER_LLVM_DIR}/include
${GLARE_CORE_TRUNK_DIR}
${GLARE_CORE_TRUNK_DIR}/utils
${GLARE_CORE_TRUNK_DIR}/maths
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common
${GLARE_CORE_TRUNK_DIR}/opencl/khronos
)


#SET(LLVM_LIBS_DIR "${WINTER_LLVM_DIR}/lib")

# LLVM linker settings.
if(WIN32)
	if(WINTER_LLVM_VERSION STREQUAL "16.0.6")
		SET(WINTER_LLVM_LIBS
			LLVMAggressiveInstCombine.lib            
			LLVMAnalysis.lib                         
			LLVMAsmParser.lib                        
			LLVMAsmPrinter.lib                       
			LLVMBinaryFormat.lib                     
			LLVMBitReader.lib                        
			LLVMBitstreamReader.lib                  
			LLVMBitWriter.lib                        
			LLVMCFGuard.lib                          
			LLVMCodeGen.lib                          
			LLVMCore.lib                             
			LLVMCoroutines.lib                       
			LLVMCoverage.lib                         
			LLVMDebugInfoCodeView.lib                
			LLVMDebugInfoDWARF.lib                   
			LLVMDebugInfoGSYM.lib                    
			LLVMDebugInfoMSF.lib                     
			LLVMDebugInfoPDB.lib                     
			LLVMDemangle.lib                         
			LLVMDlltoolDriver.lib                    
			LLVMDWARFLinker.lib                      
			LLVMExecutionEngine.lib                  
			LLVMExtensions.lib                       
			LLVMFrontendOpenACC.lib                  
			LLVMFrontendOpenMP.lib                   
			LLVMFuzzMutate.lib                       
			LLVMGlobalISel.lib                       
			LLVMInstCombine.lib                      
			LLVMInstrumentation.lib                  
			LLVMInterpreter.lib                      
			LLVMipo.lib                              
			LLVMIRReader.lib                         
			LLVMJITLink.lib                          
			LLVMLibDriver.lib                        
			LLVMLineEditor.lib                       
			LLVMLinker.lib                           
			LLVMLTO.lib                              
			LLVMMC.lib                               
			LLVMMCA.lib                              
			LLVMMCDisassembler.lib                   
			LLVMMCJIT.lib                            
			LLVMMCParser.lib                         
			LLVMMIRParser.lib                        
			LLVMObjCARCOpts.lib                      
			LLVMObject.lib                           
			LLVMObjectYAML.lib                       
			LLVMOption.lib                           
			LLVMOrcError.lib                         
			LLVMOrcJIT.lib                           
			LLVMPasses.lib                           
			LLVMProfileData.lib                      
			LLVMRemarks.lib                          
			LLVMRuntimeDyld.lib                      
			LLVMScalarOpts.lib                       
			LLVMSelectionDAG.lib                     
			LLVMSupport.lib                          
			LLVMSymbolize.lib                        
			LLVMTableGen.lib                         
			LLVMTarget.lib                           
			LLVMTextAPI.lib                          
			LLVMTransformUtils.lib                   
			LLVMVectorize.lib                        
			LLVMWindowsManifest.lib                  
			LLVMX86AsmParser.lib                     
			LLVMX86CodeGen.lib                       
			LLVMX86Desc.lib                          
			LLVMX86Disassembler.lib                  
			LLVMX86Info.lib                          
			LLVMXRay.lib
		)
	elseif(WINTER_LLVM_VERSION STREQUAL "15.0.7")
		SET(WINTER_LLVM_LIBS
			LLVMAggressiveInstCombine.lib
			LLVMAnalysis.lib
			LLVMAsmParser.lib
			LLVMAsmPrinter.lib
			LLVMBinaryFormat.lib
			LLVMBitReader.lib
			LLVMBitstreamReader.lib
			LLVMBitWriter.lib
			LLVMCFGuard.lib
			LLVMCodeGen.lib
			LLVMCore.lib
			LLVMCoroutines.lib
			LLVMCoverage.lib
			LLVMDebugInfoCodeView.lib
			LLVMDebuginfod.lib
			LLVMDebugInfoDWARF.lib
			LLVMDebugInfoGSYM.lib
			LLVMDebugInfoMSF.lib
			LLVMDebugInfoPDB.lib
			LLVMDemangle.lib
			LLVMDlltoolDriver.lib
			LLVMDWARFLinker.lib
			LLVMDWP.lib
			LLVMExecutionEngine.lib
			LLVMExtensions.lib
			LLVMFileCheck.lib
			LLVMFrontendOpenACC.lib
			LLVMFrontendOpenMP.lib
			LLVMFuzzerCLI.lib
			LLVMFuzzMutate.lib
			LLVMGlobalISel.lib
			LLVMInstCombine.lib
			LLVMInstrumentation.lib
			LLVMInterfaceStub.lib
			LLVMInterpreter.lib
			LLVMipo.lib
			LLVMIRReader.lib
			LLVMJITLink.lib
			LLVMLibDriver.lib
			LLVMLineEditor.lib
			LLVMLinker.lib
			LLVMLTO.lib
			LLVMMC.lib
			LLVMMCA.lib
			LLVMMCDisassembler.lib
			LLVMMCJIT.lib
			LLVMMCParser.lib
			LLVMMIRParser.lib
			LLVMObjCARCOpts.lib
			LLVMObjCopy.lib
			LLVMObject.lib
			LLVMObjectYAML.lib
			LLVMOption.lib
			LLVMOrcJIT.lib
			LLVMOrcShared.lib
			LLVMOrcTargetProcess.lib
			LLVMPasses.lib
			LLVMProfileData.lib
			LLVMRemarks.lib
			LLVMRuntimeDyld.lib
			LLVMScalarOpts.lib
			LLVMSelectionDAG.lib
			LLVMSupport.lib
			LLVMSymbolize.lib
			LLVMTableGen.lib
			LLVMTableGenGlobalISel.lib
			LLVMTarget.lib
			LLVMTextAPI.lib
			LLVMTransformUtils.lib
			LLVMVectorize.lib
			LLVMWindowsDriver.lib
			LLVMWindowsManifest.lib
			LLVMX86AsmParser.lib
			LLVMX86CodeGen.lib
			LLVMX86Desc.lib
			LLVMX86Disassembler.lib
			LLVMX86Info.lib
			LLVMX86TargetMCA.lib
			LLVMXRay.lib
		)
	elseif(WINTER_LLVM_VERSION STREQUAL "11.0.0")
		SET(WINTER_LLVM_LIBS
			LLVMAggressiveInstCombine.lib            
			LLVMAnalysis.lib                         
			LLVMAsmParser.lib                        
			LLVMAsmPrinter.lib                       
			LLVMBinaryFormat.lib                     
			LLVMBitReader.lib                        
			LLVMBitstreamReader.lib                  
			LLVMBitWriter.lib                        
			LLVMCFGuard.lib                          
			LLVMCodeGen.lib                          
			LLVMCore.lib                             
			LLVMCoroutines.lib                       
			LLVMCoverage.lib                         
			LLVMDebugInfoCodeView.lib                
			LLVMDebugInfoDWARF.lib                   
			LLVMDebugInfoGSYM.lib                    
			LLVMDebugInfoMSF.lib                     
			LLVMDebugInfoPDB.lib                     
			LLVMDemangle.lib                         
			LLVMDlltoolDriver.lib                    
			LLVMDWARFLinker.lib                      
			LLVMExecutionEngine.lib                  
			LLVMExtensions.lib                       
			LLVMFrontendOpenACC.lib                  
			LLVMFrontendOpenMP.lib                   
			LLVMFuzzMutate.lib                       
			LLVMGlobalISel.lib                       
			LLVMInstCombine.lib                      
			LLVMInstrumentation.lib                  
			LLVMInterpreter.lib                      
			LLVMipo.lib                              
			LLVMIRReader.lib                         
			LLVMJITLink.lib                          
			LLVMLibDriver.lib                        
			LLVMLineEditor.lib                       
			LLVMLinker.lib                           
			LLVMLTO.lib                              
			LLVMMC.lib                               
			LLVMMCA.lib                              
			LLVMMCDisassembler.lib                   
			LLVMMCJIT.lib                            
			LLVMMCParser.lib                         
			LLVMMIRParser.lib                        
			LLVMObjCARCOpts.lib                      
			LLVMObject.lib                           
			LLVMObjectYAML.lib                       
			LLVMOption.lib                           
			LLVMOrcError.lib                         
			LLVMOrcJIT.lib                           
			LLVMPasses.lib                           
			LLVMProfileData.lib                      
			LLVMRemarks.lib                          
			LLVMRuntimeDyld.lib                      
			LLVMScalarOpts.lib                       
			LLVMSelectionDAG.lib                     
			LLVMSupport.lib                          
			LLVMSymbolize.lib                        
			LLVMTableGen.lib                         
			LLVMTarget.lib                           
			LLVMTextAPI.lib                          
			LLVMTransformUtils.lib                   
			LLVMVectorize.lib                        
			LLVMWindowsManifest.lib                  
			LLVMX86AsmParser.lib                     
			LLVMX86CodeGen.lib                       
			LLVMX86Desc.lib                          
			LLVMX86Disassembler.lib                  
			LLVMX86Info.lib                          
			LLVMXRay.lib
		)
	elseif(WINTER_LLVM_VERSION STREQUAL "8.0.0")
		SET(WINTER_LLVM_LIBS
			LLVMTarget.lib LLVMTextAPI.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMWindowsManifest.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib LLVMXRay.lib LLVMAggressiveInstCombine.lib LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBinaryFormat.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMCoroutines.lib LLVMCoverage.lib LLVMDebugInfoCodeView.lib LLVMDebugInfoDWARF.lib LLVMDebugInfoMSF.lib LLVMDebugInfoPDB.lib LLVMDemangle.lib LLVMDlltoolDriver.lib LLVMExecutionEngine.lib LLVMFuzzMutate.lib LLVMGlobalISel.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipo.lib LLVMIRReader.lib LLVMLibDriver.lib LLVMLineEditor.lib LLVMLinker.lib LLVMLTO.lib LLVMMC.lib LLVMMCA.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMMIRParser.lib LLVMObjCARCOpts.lib LLVMObject.lib LLVMObjectYAML.lib LLVMOption.lib LLVMOptRemarks.lib LLVMOrcJIT.lib LLVMPasses.lib LLVMProfileData.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMSymbolize.lib LLVMTableGen.lib
		)
	elseif(WINTER_LLVM_VERSION STREQUAL "6.0.0")
		SET(WINTER_LLVM_LIBS
			LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBinaryFormat.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMCoroutines.lib LLVMCoverage.lib LLVMDebugInfoCodeView.lib LLVMDebugInfoDWARF.lib LLVMDebugInfoMSF.lib LLVMDebugInfoPDB.lib LLVMDemangle.lib LLVMDlltoolDriver.lib LLVMExecutionEngine.lib LLVMFuzzMutate.lib LLVMGlobalISel.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipo.lib LLVMIRReader.lib LLVMLibDriver.lib LLVMLineEditor.lib LLVMLinker.lib LLVMLTO.lib LLVMMC.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMMIRParser.lib LLVMObjCARCOpts.lib LLVMObject.lib LLVMObjectYAML.lib LLVMOption.lib LLVMOrcJIT.lib LLVMPasses.lib LLVMProfileData.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMSymbolize.lib LLVMTableGen.lib LLVMTarget.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMWindowsManifest.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib LLVMXRay.lib
		)
	else()
		MESSAGE("Unsupported LLVM version ${WINTER_LLVM_VERSION}")
	endif()
		
else()
	# get the llvm libs
	execute_process(COMMAND "${LLVM_DIR}/bin/llvm-config" "--ldflags" "--libs" "all" OUTPUT_VARIABLE LLVM_LIBS_OUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE "\n" " " WINTER_LLVM_LIBS ${LLVM_LIBS_OUT})
endif()
