cmake_minimum_required (VERSION 2.8.7)

# set a dummy project name, so we can detect the bitness
project (dummy)

set(PROJECT_NAME "winter")


set(WINTER_USE_OPENCL TRUE)


# detect bitness
if(WIN32)
	if(CMAKE_CL_64)
		set(WINTER_64_BIT TRUE)
		set(WINTER_BITNESS 64)
		set(PROJECT_NAME "winter64")
	else()
		set(WINTER_64_BIT FALSE)
		set(WINTER_BITNESS 32)
		set(PROJECT_NAME "winter32")
	endif()
else()
	#linux stuff goes here
	set(WINTER_64_BIT FALSE)
	set(WINTER_BITNESS 64)
	set(PROJECT_NAME "winter64")
endif()

# Set the real project name
project(${PROJECT_NAME})


set(CURRENT_TARGET winter)


# mark_as_advanced(CMAKE_C_FLAGS_SDKDEBUG CMAKE_CXX_FLAGS_SDKDEBUG CMAKE_EXE_LINKER_FLAGS_SDKDEBUG CMAKE_SHARED_LINKER_FLAGS_SDKDEBUG CMAKE_MODULE_LINKER_FLAGS_SDKDEBUG)

# suppress regen after build. this gets rid of the ZERO_CHECK project, which triggers the regen
set(CMAKE_SUPPRESS_REGENERATION TRUE)


# Function for checking an environment variable exists
function(checkAndGetEnvVar envvar result)
	if(DEFINED ENV{${envvar}})
		# To prevent issues with \ characters in macros, replace possible \ INDIGO_LIBS env vars and store it in INDIGO_LIBS_ENV.
		string(REPLACE "\\" "/" var $ENV{${envvar}})
		set (${result} ${var} PARENT_SCOPE)
	else()
		MESSAGE(FATAL_ERROR "ERROR: Environment variable ${envvar} not defined.")
	endif()
endfunction()

checkAndGetEnvVar(INDIGO_TRUNK_DIR INDIGO_TRUNK_DIR)
#set(INDIGO_TRUNK_DIR $ENV{INDIGO_TRUNK_DIR})
MESSAGE("Indigo trunk dir: ${INDIGO_TRUNK_DIR}")



# Supported versions: 3.4, 3.6, 6.0.0.
set(LLVM_VERSION "6.0.0")

MESSAGE("Target LLVM version: ${LLVM_VERSION}")

# The llvm dirs use the version seperator "_" instead of "."
string(REPLACE "." "_" LLVM_VERSION_PATH ${LLVM_VERSION})


set(LIBS_ENV "D:/programming")

if(WIN32)
	if(MSVC_VERSION EQUAL 1600) # VS2012
		set(INDIGO_VS_VER 2010)
	elseif(MSVC_VERSION EQUAL 1700) # VS2012
		set(INDIGO_VS_VER 2012)
	elseif(MSVC_VERSION EQUAL 1800) # VS2013
		set(INDIGO_VS_VER 2013)
	elseif(MSVC_VERSION EQUAL 1900) # VS2015
		set(INDIGO_VS_VER 2015)
	elseif(MSVC_VERSION EQUAL 1911 OR MSVC_VERSION EQUAL 1912) # VS2017
		set(INDIGO_VS_VER 2017)
	else()
		message(FATAL_ERROR "Unhandled MSVC version")
	endif()
	
	set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs${INDIGO_VS_VER}_${WINTER_BITNESS}")
	
else() # Mac or Linux.
	set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install")

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


set(WINTER_INCLUDE_DIRS "")


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
${INDIGO_TRUNK_DIR}/utils/Clock.cpp
${INDIGO_TRUNK_DIR}/utils/Clock.h
${INDIGO_TRUNK_DIR}/utils/Condition.cpp
${INDIGO_TRUNK_DIR}/utils/Condition.h
${INDIGO_TRUNK_DIR}/utils/ConPrint.cpp
${INDIGO_TRUNK_DIR}/utils/ConPrint.h
${INDIGO_TRUNK_DIR}/utils/ContainerUtils.h
${INDIGO_TRUNK_DIR}/utils/CycleTimer.cpp
${INDIGO_TRUNK_DIR}/utils/CycleTimer.h
${INDIGO_TRUNK_DIR}/utils/DynamicLib.cpp
${INDIGO_TRUNK_DIR}/utils/DynamicLib.h
${INDIGO_TRUNK_DIR}/utils/FileUtils.cpp
${INDIGO_TRUNK_DIR}/utils/FileUtils.h
${INDIGO_TRUNK_DIR}/utils/Lock.cpp
${INDIGO_TRUNK_DIR}/utils/Lock.h
${INDIGO_TRUNK_DIR}/utils/MemMappedFile.cpp
${INDIGO_TRUNK_DIR}/utils/MemMappedFile.h
${INDIGO_TRUNK_DIR}/utils/MTwister.cpp
${INDIGO_TRUNK_DIR}/utils/MTwister.h
${INDIGO_TRUNK_DIR}/utils/Mutex.cpp
${INDIGO_TRUNK_DIR}/utils/Mutex.h
${INDIGO_TRUNK_DIR}/utils/MyThread.cpp
${INDIGO_TRUNK_DIR}/utils/MyThread.h
${INDIGO_TRUNK_DIR}/utils/Parser.cpp
${INDIGO_TRUNK_DIR}/utils/Parser.h
${INDIGO_TRUNK_DIR}/utils/Platform.h
${INDIGO_TRUNK_DIR}/utils/PlatformUtils.cpp
${INDIGO_TRUNK_DIR}/utils/PlatformUtils.h
${INDIGO_TRUNK_DIR}/utils/PrintOutput.cpp
${INDIGO_TRUNK_DIR}/utils/PrintOutput.h
${INDIGO_TRUNK_DIR}/utils/StandardPrintOutput.cpp
${INDIGO_TRUNK_DIR}/utils/StandardPrintOutput.h
${INDIGO_TRUNK_DIR}/utils/Reference.cpp
${INDIGO_TRUNK_DIR}/utils/Reference.h
${INDIGO_TRUNK_DIR}/utils/ReferenceTest.cpp
${INDIGO_TRUNK_DIR}/utils/ReferenceTest.h
${INDIGO_TRUNK_DIR}/utils/StringUtils.cpp
${INDIGO_TRUNK_DIR}/utils/StringUtils.h
${INDIGO_TRUNK_DIR}/utils/Task.cpp
${INDIGO_TRUNK_DIR}/utils/Task.h
${INDIGO_TRUNK_DIR}/utils/TaskManager.cpp
${INDIGO_TRUNK_DIR}/utils/TaskManager.h
${INDIGO_TRUNK_DIR}/utils/TaskRunnerThread.cpp
${INDIGO_TRUNK_DIR}/utils/TaskRunnerThread.h
${INDIGO_TRUNK_DIR}/utils/Timer.cpp
${INDIGO_TRUNK_DIR}/utils/Timer.h
${INDIGO_TRUNK_DIR}/utils/VRef.h
${INDIGO_TRUNK_DIR}/utils/UTF8Utils.cpp
${INDIGO_TRUNK_DIR}/utils/UTF8Utils.h
)

SET(maths_files
${INDIGO_TRUNK_DIR}/maths/SSE.cpp
${INDIGO_TRUNK_DIR}/maths/SSE.h
)

SET(testutils_files
${INDIGO_TRUNK_DIR}/indigo/TestUtils.cpp
${INDIGO_TRUNK_DIR}/indigo/TestUtils.h
)


SET(double_conversion_files
${INDIGO_TRUNK_DIR}/double-conversion/bignum-dtoa.cc
${INDIGO_TRUNK_DIR}/double-conversion/bignum-dtoa.h
${INDIGO_TRUNK_DIR}/double-conversion/bignum.cc
${INDIGO_TRUNK_DIR}/double-conversion/bignum.h
${INDIGO_TRUNK_DIR}/double-conversion/cached-powers.cc
${INDIGO_TRUNK_DIR}/double-conversion/cached-powers.h
${INDIGO_TRUNK_DIR}/double-conversion/diy-fp.cc
${INDIGO_TRUNK_DIR}/double-conversion/diy-fp.h
${INDIGO_TRUNK_DIR}/double-conversion/double-conversion.cc
${INDIGO_TRUNK_DIR}/double-conversion/double-conversion.h
${INDIGO_TRUNK_DIR}/double-conversion/fast-dtoa.cc
${INDIGO_TRUNK_DIR}/double-conversion/fast-dtoa.h
${INDIGO_TRUNK_DIR}/double-conversion/fixed-dtoa.cc
${INDIGO_TRUNK_DIR}/double-conversion/fixed-dtoa.h
${INDIGO_TRUNK_DIR}/double-conversion/ieee.h
${INDIGO_TRUNK_DIR}/double-conversion/strtod.cc
${INDIGO_TRUNK_DIR}/double-conversion/strtod.h
${INDIGO_TRUNK_DIR}/double-conversion/utils.h
)

SET(xxhash_files
${INDIGO_TRUNK_DIR}/zstd-1.0.0/lib/common/xxhash.c
${INDIGO_TRUNK_DIR}/zstd-1.0.0/lib/common/xxhash.h
)


if(WINTER_USE_OPENCL)
	SET(glare_opencl_files
	${INDIGO_TRUNK_DIR}/opencl/OpenCL.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCL.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLKernel.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLKernel.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLBuffer.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLBuffer.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLContext.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLContext.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLDevice.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLDevice.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLPlatform.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLPlatform.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLProgram.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLProgram.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLCommandQueue.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLCommandQueue.h
	)
endif()


SOURCE_GROUP(winter FILES ${WINTER_FILES})
SOURCE_GROUP(utils FILES ${util_files})
SOURCE_GROUP(maths FILES ${maths_files})
SOURCE_GROUP(double_conversion FILES ${double_conversion_files})
SOURCE_GROUP(opencl FILES ${glare_opencl_files})
SOURCE_GROUP(xxhash FILES ${xxhash_files})
SOURCE_GROUP(testutils FILES ${testutils_files})


include_directories(
${llvmdir}/include
${INDIGO_TRUNK_DIR}
${INDIGO_TRUNK_DIR}/utils
${INDIGO_TRUNK_DIR}/maths
${INDIGO_TRUNK_DIR}/zstd-1.0.0/lib/common
${INDIGO_TRUNK_DIR}/opencl/khronos
${WINTER_INCLUDE_DIRS}
)


add_executable("winter"
${WINTER_FILES}
${util_files}
${maths_files}
${double_conversion_files}
${glare_opencl_files}
${xxhash_files}
${testutils_files}
)





if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
	add_definitions(/MP)
	
	# Set warning level to 4 - this shows some useful stuff not in /W3 in vs2012.
	add_definitions(/W4)
	
	##### Ignore some warnings #####	
	# ''function': was declared deprecated'
	add_definitions(/wd4996)
	
	# ''this' : used in base member initializer list' (get it in LLVM a lot)
	add_definitions(/wd4355)
	
	# 'conditional expression is constant' - don't want this warning as we tend to turn off code with 'if(false) {}'
	add_definitions(/wd4127)

	# 'unreferenced formal parameter'
	add_definitions(/wd4100)
	
	# 'assignment operator could not be generated'
	add_definitions(/wd4512)	

	# 'nonstandard extension used : nameless struct/union'
	add_definitions(/wd4201)
	################################
	
	#add_definitions(/GS- /fp:fast)
	
	# Add SSE2 support for 32-bit.  It's implicitly on for 64-bit.
	if(WINTER_64_BIT)
	else()
		add_definitions(/arch:SSE2)
	endif()
	
	add_definitions(-D__SSE__ -D__SSE2__ -D__SSE3__ -D__SSSE3__ -D__SSE4_1__)
	
	# Consider some options.
	if(NO_WHOLE_PROGRAM_OPT)
		SET(GL_OPT)
	else()
		SET(GL_OPT "/GL")
	endif()
	
	if(WIN_RUNTIME_STATIC)
		SET(WIN_RUNTIME_OPT "/MT")
		SET(WIN_RUNTIME_OPT_DEBUG "/MTd")
	else()
		SET(WIN_RUNTIME_OPT)
		SET(WIN_RUNTIME_OPT_DEBUG)
	endif()
	
	# Append optimisation flags.
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			${WIN_RUNTIME_OPT_DEBUG} -DBUILD_TESTS=1")
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			${WIN_RUNTIME_OPT} -D_SECURE_SCL=0 /Ox ${GL_OPT} -DNDEBUG /Zi -DBUILD_TESTS=1")
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	${WIN_RUNTIME_OPT} /O2 -D_SECURE_SCL=0 -DNDEBUG -DBUILD_TESTS=1")
	
else() # Linux
	#add_definitions(-D__SSSE3__ -D__NO_AVX__)

	SET(LINUX_C_CXX_OPTIONS "-Wall -pthread -mmmx -msse -msse2 -mssse3")
	
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
if(LLVM_VERSION STREQUAL "6.0.0")
	add_definitions("-DTARGET_LLVM_VERSION=60")
elseif(LLVM_VERSION STREQUAL "3.6")
	add_definitions("-DTARGET_LLVM_VERSION=36")
elseif(LLVM_VERSION STREQUAL "3.4")
	add_definitions("-DTARGET_LLVM_VERSION=34")
else()
	MESSAGE("Unsupported LLVM version ${LLVM_VERSION}")
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
	
	if(LLVM_VERSION STREQUAL "6.0.0")
		SET(LLVM_LIBS
			LLVMAArch64AsmParser.lib
			LLVMAArch64AsmPrinter.lib
			LLVMAArch64CodeGen.lib
			LLVMAArch64Desc.lib
			LLVMAArch64Disassembler.lib
			LLVMAArch64Info.lib
			LLVMAArch64Utils.lib
			LLVMAMDGPUAsmParser.lib
			LLVMAMDGPUAsmPrinter.lib
			LLVMAMDGPUCodeGen.lib
			LLVMAMDGPUDesc.lib
			LLVMAMDGPUDisassembler.lib
			LLVMAMDGPUInfo.lib
			LLVMAMDGPUUtils.lib
			LLVMAnalysis.lib
			LLVMARMAsmParser.lib
			LLVMARMAsmPrinter.lib
			LLVMARMCodeGen.lib
			LLVMARMDesc.lib
			LLVMARMDisassembler.lib
			LLVMARMInfo.lib
			LLVMARMUtils.lib
			LLVMAsmParser.lib
			LLVMAsmPrinter.lib
			LLVMBinaryFormat.lib
			LLVMBitReader.lib
			LLVMBitWriter.lib
			LLVMBPFAsmParser.lib
			LLVMBPFAsmPrinter.lib
			LLVMBPFCodeGen.lib
			LLVMBPFDesc.lib
			LLVMBPFDisassembler.lib
			LLVMBPFInfo.lib
			LLVMCodeGen.lib
			LLVMCore.lib
			LLVMCoroutines.lib
			LLVMCoverage.lib
			LLVMDebugInfoCodeView.lib
			LLVMDebugInfoDWARF.lib
			LLVMDebugInfoMSF.lib
			LLVMDebugInfoPDB.lib
			LLVMDemangle.lib
			LLVMDlltoolDriver.lib
			LLVMExecutionEngine.lib
			LLVMFuzzMutate.lib
			LLVMGlobalISel.lib
			LLVMHexagonAsmParser.lib
			LLVMHexagonCodeGen.lib
			LLVMHexagonDesc.lib
			LLVMHexagonDisassembler.lib
			LLVMHexagonInfo.lib
			LLVMInstCombine.lib
			LLVMInstrumentation.lib
			LLVMInterpreter.lib
			LLVMipo.lib
			LLVMIRReader.lib
			LLVMLanaiAsmParser.lib
			LLVMLanaiAsmPrinter.lib
			LLVMLanaiCodeGen.lib
			LLVMLanaiDesc.lib
			LLVMLanaiDisassembler.lib
			LLVMLanaiInfo.lib
			LLVMLibDriver.lib
			LLVMLineEditor.lib
			LLVMLinker.lib
			LLVMLTO.lib
			LLVMMC.lib
			LLVMMCDisassembler.lib
			LLVMMCJIT.lib
			LLVMMCParser.lib
			LLVMMipsAsmParser.lib
			LLVMMipsAsmPrinter.lib
			LLVMMipsCodeGen.lib
			LLVMMipsDesc.lib
			LLVMMipsDisassembler.lib
			LLVMMipsInfo.lib
			LLVMMIRParser.lib
			LLVMMSP430AsmPrinter.lib
			LLVMMSP430CodeGen.lib
			LLVMMSP430Desc.lib
			LLVMMSP430Info.lib
			LLVMNVPTXAsmPrinter.lib
			LLVMNVPTXCodeGen.lib
			LLVMNVPTXDesc.lib
			LLVMNVPTXInfo.lib
			LLVMObjCARCOpts.lib
			LLVMObject.lib
			LLVMObjectYAML.lib
			LLVMOption.lib
			LLVMOrcJIT.lib
			LLVMPasses.lib
			LLVMPowerPCAsmParser.lib
			LLVMPowerPCAsmPrinter.lib
			LLVMPowerPCCodeGen.lib
			LLVMPowerPCDesc.lib
			LLVMPowerPCDisassembler.lib
			LLVMPowerPCInfo.lib
			LLVMProfileData.lib
			LLVMRuntimeDyld.lib
			LLVMScalarOpts.lib
			LLVMSelectionDAG.lib
			LLVMSparcAsmParser.lib
			LLVMSparcAsmPrinter.lib
			LLVMSparcCodeGen.lib
			LLVMSparcDesc.lib
			LLVMSparcDisassembler.lib
			LLVMSparcInfo.lib
			LLVMSupport.lib
			LLVMSymbolize.lib
			LLVMSystemZAsmParser.lib
			LLVMSystemZAsmPrinter.lib
			LLVMSystemZCodeGen.lib
			LLVMSystemZDesc.lib
			LLVMSystemZDisassembler.lib
			LLVMSystemZInfo.lib
			LLVMTableGen.lib
			LLVMTarget.lib
			LLVMTransformUtils.lib
			LLVMVectorize.lib
			LLVMWindowsManifest.lib
			LLVMX86AsmParser.lib
			LLVMX86AsmPrinter.lib
			LLVMX86CodeGen.lib
			LLVMX86Desc.lib
			LLVMX86Disassembler.lib
			LLVMX86Info.lib
			LLVMX86Utils.lib
			LLVMXCoreAsmPrinter.lib
			LLVMXCoreCodeGen.lib
			LLVMXCoreDesc.lib
			LLVMXCoreDisassembler.lib
			LLVMXCoreInfo.lib
			LLVMXRay.lib
		)
	elseif(LLVM_VERSION STREQUAL "3.6")
		SET(LLVM_LIBS
			LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMExecutionEngine.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipa.lib LLVMipo.lib 			
			LLVMLinker.lib LLVMMC.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMObject.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMTarget.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib
		)
	elseif(LLVM_VERSION STREQUAL "3.4")
		SET(LLVM_LIBS
			LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMExecutionEngine.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipa.lib LLVMipo.lib 
			LLVMJIT.lib 
			LLVMLinker.lib LLVMMC.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMObject.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMTarget.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib
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
		${LLVM_LIBS_FINAL})
	
	# get the llvm flags
	#execute_process(COMMAND "${llvmdir}/bin/llvm-config" "--ldflags" OUTPUT_VARIABLE LLVM_FLAGS_OUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	#string(REPLACE "\n" " " LLVM_FLAGS_FINAL ${LLVM_FLAGS_OUT})
	
	#SET(CMAKE_EXE_LINKER_FLAGS		"${CMAKE_EXE_LINKER_FLAGS} ${LLVM_FLAGS_FINAL}")
	#SET(CMAKE_MODULE_LINKER_FLAGS	"${CMAKE_MODULE_LINKER_FLAGS} ${LLVM_FLAGS_FINAL}")
endif()
