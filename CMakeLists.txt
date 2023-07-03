cmake_minimum_required (VERSION 3.0)


project("winter")

set(CURRENT_TARGET winter)

set(WINTER_BITNESS 64)

set(WINTER_USE_OPENCL TRUE)


SET(GLARE_CORE_TRUNK_DIR "" CACHE PATH "Path to glare-core repo trunk dir.")

if(GLARE_CORE_TRUNK_DIR STREQUAL "")
	MESSAGE(FATAL_ERROR "ERROR: Please set GLARE_CORE_TRUNK_DIR to the path to glare-core repo trunk dir, e.g. -DGLARE_CORE_TRUNK_DIR=path/to/glare/core")
endif()


SET(LIBS_DIR "" CACHE PATH "Path to libraries dir.  llvm builds should be in this dir, for example LIBS_DIR/llvm/llvm_11_0_0_install_vs2022_64")

if(LIBS_DIR STREQUAL "")
	MESSAGE(FATAL_ERROR "ERROR: Please set LIBS_DIR to the path to llvm builds can be found in, e.g. -DLIBS_DIR=c:/libs")
endif()



# Supported versions: 3.4, 3.6, 6.0.0, 8.0.0, 11.0.0, 15.0.7, 16.0.6
set(LLVM_VERSION "15.0.7")

MESSAGE("Target LLVM version: ${LLVM_VERSION}")

# The llvm dirs use the version seperator "_" instead of "."
string(REPLACE "." "_" LLVM_VERSION_PATH ${LLVM_VERSION})


if(WIN32)
	add_definitions(/std:c++17) # Needed for LLVM 16.0.0

	if(MSVC_VERSION EQUAL 1900)
		set(WINTER_VS_VER 2015)
	elseif(MSVC_VERSION EQUAL 1911 OR MSVC_VERSION EQUAL 1912)
		set(WINTER_VS_VER 2017)
	elseif(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
		set(WINTER_VS_VER 2019)
	elseif(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1940)
		set(WINTER_VS_VER 2022)
	else()
		message(FATAL_ERROR "Unhandled MSVC version")
	endif()
	
	set(llvmdir "${LIBS_DIR}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs${WINTER_VS_VER}_${WINTER_BITNESS}")
	
else() # Mac or Linux.
	set(llvmdir "${LIBS_DIR}/llvm/llvm_${LLVM_VERSION_PATH}_install")

#	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "$ENV{INDIGO_OUTPUT}")
#	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_SDKDEBUG "$ENV{INDIGO_OUTPUT}/SDKDebug")
#	
#	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "$ENV{INDIGO_OUTPUT}")
#	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_SDKDEBUG "$ENV{INDIGO_OUTPUT}/SDKDebug")
#	
#	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "$ENV{INDIGO_OUTPUT}/test_builds")
#	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "$ENV{INDIGO_OUTPUT}")
#	set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_SDKDEBUG "$ENV{INDIGO_OUTPUT}/SDKDebug")
	
endif()

MESSAGE("llvmdir: ${llvmdir}")



if(WINTER_USE_OPENCL)
	message("Building with OpenCL support.")
endif()


set(winterdir ".")


set(WINTER_FILES
"${winterdir}/BuiltInFunctionImpl.cpp"
"${winterdir}/LanguageTestUtils.cpp"
"${winterdir}/LanguageTests.cpp"
"${winterdir}/Linker.cpp"
"${winterdir}/LLVMTypeUtils.cpp"
"${winterdir}/TokenBase.cpp"
"${winterdir}/Value.cpp"
"${winterdir}/VirtualMachine.cpp"
"${winterdir}/wnt_ASTNode.cpp"
"${winterdir}/wnt_Diagnostics.cpp"
"${winterdir}/wnt_ExternalFunction.cpp"
"${winterdir}/wnt_FunctionDefinition.cpp"
"${winterdir}/wnt_FunctionExpression.cpp"
"${winterdir}/wnt_FunctionSignature.cpp"
"${winterdir}/wnt_LangParser.cpp"
"${winterdir}/wnt_Lexer.cpp"
"${winterdir}/wnt_SourceBuffer.cpp"
"${winterdir}/wnt_Type.cpp"
"${winterdir}/wnt_RefCounting.cpp"
"${winterdir}/ProofUtils.cpp"
"${winterdir}/wnt_ArrayLiteral.cpp"
"${winterdir}/wnt_TupleLiteral.cpp"
"${winterdir}/wnt_VectorLiteral.cpp"
"${winterdir}/wnt_VArrayLiteral.cpp"
"${winterdir}/wnt_Variable.cpp"
"${winterdir}/FuzzTests.cpp"
"${winterdir}/BaseException.h"
"${winterdir}/BuiltInFunctionImpl.h"
"${winterdir}/LanguageTests.h"
"${winterdir}/LanguageTestUtils.h"
"${winterdir}/Linker.h"
"${winterdir}/LLVMTypeUtils.h"
"${winterdir}/TokenBase.h"
"${winterdir}/Value.h"
"${winterdir}/VirtualMachine.h"
"${winterdir}/VMState.h"
"${winterdir}/wnt_ASTNode.h"
"${winterdir}/wnt_Diagnostics.h"
"${winterdir}/wnt_ExternalFunction.h"
"${winterdir}/wnt_FunctionDefinition.h"
"${winterdir}/wnt_FunctionExpression.h"
"${winterdir}/wnt_FunctionSignature.h"
"${winterdir}/wnt_LangParser.h"
"${winterdir}/wnt_Lexer.h"
"${winterdir}/wnt_SourceBuffer.h"
"${winterdir}/wnt_Type.h"
"${winterdir}/wnt_RefCounting.h"
"${winterdir}/ProofUtils.h"
"${winterdir}/wnt_IfExpression.cpp"
"${winterdir}/wnt_IfExpression.h"
"${winterdir}/wnt_ArrayLiteral.h"
"${winterdir}/wnt_TupleLiteral.h"
"${winterdir}/wnt_VectorLiteral.h"
"${winterdir}/wnt_VArrayLiteral.h"
"${winterdir}/wnt_Variable.h"
"${winterdir}/FuzzTests.h"
"${winterdir}/winter.cpp"
"${winterdir}/notes.txt"
"${winterdir}/todo.txt"
"${winterdir}/wnt_LetASTNode.cpp"
"${winterdir}/wnt_LetASTNode.h"
"${winterdir}/wnt_LetBlock.cpp"
"${winterdir}/wnt_LetBlock.h"
"${winterdir}/wnt_MathsFuncs.cpp"
"${winterdir}/wnt_MathsFuncs.h"
"${winterdir}/LLVMUtils.cpp"
"${winterdir}/LLVMUtils.h"
"${winterdir}/PerfTests.cpp"
"${winterdir}/PerfTests.h"
"${winterdir}/CompiledValue.cpp"
"${winterdir}/CompiledValue.h"
)



########### Utils ################
SET(util_files
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

SET(double_conversion_files
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

SET(xxhash_files
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common/xxhash.c
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common/xxhash.h
)


if(WINTER_USE_OPENCL)
	SET(glare_opencl_files
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


SOURCE_GROUP(winter FILES ${WINTER_FILES})
SOURCE_GROUP(utils FILES ${util_files})
SOURCE_GROUP(double_conversion FILES ${double_conversion_files})
SOURCE_GROUP(opencl FILES ${glare_opencl_files})
SOURCE_GROUP(xxhash FILES ${xxhash_files})


include_directories(
${llvmdir}/include
${GLARE_CORE_TRUNK_DIR}
${GLARE_CORE_TRUNK_DIR}/utils
${GLARE_CORE_TRUNK_DIR}/maths
${GLARE_CORE_TRUNK_DIR}/zstd-1.5.2/lib/common
${GLARE_CORE_TRUNK_DIR}/opencl/khronos
)


add_executable("winter"
${WINTER_FILES}
${util_files}
${maths_files}
${double_conversion_files}
${glare_opencl_files}
${xxhash_files}
)



if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
	add_definitions(/MP) # Build with multiple processes
	
	# Set warning level to 4 - this shows some useful stuff not in /W3 in vs2012.
	add_definitions(/W4)
	
	################################# Ignore some warnings #################################

	# 'conditional expression is constant' - don't want this warning as we tend to turn off code with 'if(false) {}'
	add_definitions(/wd4127)

	# 'unreferenced formal parameter'
	add_definitions(/wd4100)

	########################################################################################
	
	#add_definitions(/GS- /fp:fast)
	
	# Add SSE2 support for 32-bit.  It's implicitly on for 64-bit.
	if(WINTER_BITNESS EQUAL 32)
		add_definitions(/arch:SSE2)
	endif()
	
	add_definitions(-D__SSE__ -D__SSE2__ -D__SSE3__ -D__SSSE3__ -D__SSE4_1__)
	
	# Consider some options.
	if(NO_WHOLE_PROGRAM_OPT)
		SET(GL_OPT)
	else()
		SET(GL_OPT "/GL")
	endif()
		
	# Append optimisation flags.
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			-DBUILD_TESTS=1")	
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	/O2 -D_SECURE_SCL=0 -DNDEBUG -DBUILD_TESTS=1")
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			/O2 -D_SECURE_SCL=0 ${GL_OPT} -DNDEBUG /Zi -DBUILD_TESTS=1")
	
else() # Linux
	#add_definitions(-D__SSSE3__ -D__NO_AVX__)

	SET(LINUX_C_CXX_OPTIONS "-Wall -pthread -mmmx -msse -msse2 -mssse3 -msse4.1")
	
	SET(COMMON_C_CXX_OPTIONS_DEBUG				"${LINUX_C_CXX_OPTIONS} -g")
	SET(COMMON_C_CXX_OPTIONS_RELEASE			"${LINUX_C_CXX_OPTIONS} -O3 -DNDEBUG")
	SET(COMMON_C_CXX_OPTIONS_RELWITHDEBINFO		"${LINUX_C_CXX_OPTIONS} -O2 -DNDEBUG")

	# Append optimisation flags.
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			${COMMON_C_CXX_OPTIONS_DEBUG} -std=c++11 -Wno-reorder -DBUILD_TESTS=1")
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			${COMMON_C_CXX_OPTIONS_RELEASE} -std=c++11 -Wno-reorder")
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	${COMMON_C_CXX_OPTIONS_RELWITHDEBINFO} -std=c++11 -Wno-reorder -DBUILD_TESTS=1")
	
	SET(CMAKE_C_FLAGS_DEBUG				"${CMAKE_C_FLAGS_DEBUG}				${COMMON_C_CXX_OPTIONS_DEBUG}")
	SET(CMAKE_C_FLAGS_RELEASE			"${CMAKE_C_FLAGS_RELEASE}			${COMMON_C_CXX_OPTIONS_RELEASE}")
	SET(CMAKE_C_FLAGS_RELWITHDEBINFO	"${CMAKE_C_FLAGS_RELWITHDEBINFO}	${COMMON_C_CXX_OPTIONS_RELWITHDEBINFO}")
endif()


# Set TARGET_LLVM_VERSION
if(LLVM_VERSION STREQUAL "16.0.6")
	add_definitions("-DTARGET_LLVM_VERSION=160")
elseif(LLVM_VERSION STREQUAL "15.0.7")
	add_definitions("-DTARGET_LLVM_VERSION=150")
elseif(LLVM_VERSION STREQUAL "11.0.0")
	add_definitions("-DTARGET_LLVM_VERSION=110")
elseif(LLVM_VERSION STREQUAL "8.0.0")
	add_definitions("-DTARGET_LLVM_VERSION=80")
elseif(LLVM_VERSION STREQUAL "6.0.0")
	add_definitions("-DTARGET_LLVM_VERSION=60")
else()
	MESSAGE(FATAL_ERROR "Unsupported LLVM version ${LLVM_VERSION}")
endif()


# LLVM include path and compiler settings.
if(WIN32)
	# Append LLVM paths for the configurations
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			/I\"${llvmdir}/include\"")
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	/I\"${llvmdir}/include\"")
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			/I\"${llvmdir}_debug/include\"")
	SET(CMAKE_CXX_FLAGS_SDKDEBUG		"${CMAKE_CXX_FLAGS_SDKDEBUG}		/I\"${llvmdir}_debug/include\"")
else()
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -I${llvmdir}/include -I/usr/local/include")
endif()


# LLVM linker settings.
if(WIN32)
	
	SET(LLVM_LINK_FLAGS_RELEASE			"/LIBPATH:\"${llvmdir}/lib\"")
	SET(LLVM_LINK_FLAGS_RELWITHDEBINFO	"/LIBPATH:\"${llvmdir}/lib\"")
	SET(LLVM_LINK_FLAGS_DEBUG			"/LIBPATH:\"${llvmdir}_debug/lib\"")
	SET(LLVM_LINK_FLAGS_SDKDEBUG		"/LIBPATH:\"${llvmdir}_debug/lib\"")
	
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE			"${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${LLVM_LINK_FLAGS_RELEASE}")
	SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${LLVM_LINK_FLAGS_RELWITHDEBINFO}")
	SET(CMAKE_EXE_LINKER_FLAGS_DEBUG			"${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${LLVM_LINK_FLAGS_DEBUG}")
	SET(CMAKE_EXE_LINKER_FLAGS_SDKDEBUG			"${CMAKE_EXE_LINKER_FLAGS_SDKDEBUG} ${LLVM_LINK_FLAGS_SDKDEBUG}")
	
	SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE			"${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${LLVM_LINK_FLAGS_RELEASE}")
	SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} ${LLVM_LINK_FLAGS_RELWITHDEBINFO}")
	SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG				"${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${LLVM_LINK_FLAGS_DEBUG}")
	SET(CMAKE_MODULE_LINKER_FLAGS_SDKDEBUG			"${CMAKE_MODULE_LINKER_FLAGS_SDKDEBUG} ${LLVM_LINK_FLAGS_SDKDEBUG}")
	
	if(LLVM_VERSION STREQUAL "16.0.6")
		SET(LLVM_LIBS
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
	elseif(LLVM_VERSION STREQUAL "15.0.7")
		SET(LLVM_LIBS
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
	elseif(LLVM_VERSION STREQUAL "11.0.0")
		SET(LLVM_LIBS
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
	elseif(LLVM_VERSION STREQUAL "8.0.0")
		SET(LLVM_LIBS
			LLVMTarget.lib LLVMTextAPI.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMWindowsManifest.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib LLVMXRay.lib LLVMAggressiveInstCombine.lib LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBinaryFormat.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMCoroutines.lib LLVMCoverage.lib LLVMDebugInfoCodeView.lib LLVMDebugInfoDWARF.lib LLVMDebugInfoMSF.lib LLVMDebugInfoPDB.lib LLVMDemangle.lib LLVMDlltoolDriver.lib LLVMExecutionEngine.lib LLVMFuzzMutate.lib LLVMGlobalISel.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipo.lib LLVMIRReader.lib LLVMLibDriver.lib LLVMLineEditor.lib LLVMLinker.lib LLVMLTO.lib LLVMMC.lib LLVMMCA.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMMIRParser.lib LLVMObjCARCOpts.lib LLVMObject.lib LLVMObjectYAML.lib LLVMOption.lib LLVMOptRemarks.lib LLVMOrcJIT.lib LLVMPasses.lib LLVMProfileData.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMSymbolize.lib LLVMTableGen.lib
		)
	elseif(LLVM_VERSION STREQUAL "6.0.0")
		SET(LLVM_LIBS
			LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBinaryFormat.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMCoroutines.lib LLVMCoverage.lib LLVMDebugInfoCodeView.lib LLVMDebugInfoDWARF.lib LLVMDebugInfoMSF.lib LLVMDebugInfoPDB.lib LLVMDemangle.lib LLVMDlltoolDriver.lib LLVMExecutionEngine.lib LLVMFuzzMutate.lib LLVMGlobalISel.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipo.lib LLVMIRReader.lib LLVMLibDriver.lib LLVMLineEditor.lib LLVMLinker.lib LLVMLTO.lib LLVMMC.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMMIRParser.lib LLVMObjCARCOpts.lib LLVMObject.lib LLVMObjectYAML.lib LLVMOption.lib LLVMOrcJIT.lib LLVMPasses.lib LLVMProfileData.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMSymbolize.lib LLVMTableGen.lib LLVMTarget.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMWindowsManifest.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib LLVMXRay.lib
		)
	else()
		MESSAGE("Unsupported LLVM version ${LLVM_VERSION}")
	endif()
		
	#SET(CMAKE_EXE_LINKER_FLAGS		"${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LIBS}")
	#SET(CMAKE_MODULE_LINKER_FLAGS	"${CMAKE_MODULE_LINKER_FLAGS} ${LLVM_LIBS}")
	
	target_link_libraries(${CURRENT_TARGET} 
		${LLVM_LIBS})
else()
	# get the llvm libs
	execute_process(COMMAND "${llvmdir}/bin/llvm-config" "--ldflags" "--libs" "all" OUTPUT_VARIABLE LLVM_LIBS_OUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REPLACE "\n" " " LLVM_LIBS_FINAL ${LLVM_LIBS_OUT})
	
	target_link_libraries(${CURRENT_TARGET} 
		${LLVM_LIBS_FINAL}
		dl
		pthread
		z
	)
	
	# get the llvm flags
	#execute_process(COMMAND "${llvmdir}/bin/llvm-config" "--ldflags" OUTPUT_VARIABLE LLVM_FLAGS_OUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	#string(REPLACE "\n" " " LLVM_FLAGS_FINAL ${LLVM_FLAGS_OUT})
	
	#SET(CMAKE_EXE_LINKER_FLAGS		"${CMAKE_EXE_LINKER_FLAGS} ${LLVM_FLAGS_FINAL}")
	#SET(CMAKE_MODULE_LINKER_FLAGS	"${CMAKE_MODULE_LINKER_FLAGS} ${LLVM_FLAGS_FINAL}")
	
	#set_target_properties(${CURRENT_TARGET} PROPERTIES LINK_FLAGS "-ldl")
endif()
