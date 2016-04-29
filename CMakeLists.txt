cmake_minimum_required (VERSION 2.8.7)

# set a dummy project name, so we can detect the bitness
project (dummy)

set(PROJECT_NAME "winter")


set(WINTER_USE_OPENCL TRUE)


# detect bitness
if(WIN32)
	if(CMAKE_CL_64)
		set(WINTER_64_BIT TRUE)
		set(PROJECT_NAME "winter64")
	else()
		set(WINTER_64_BIT FALSE)
		set(PROJECT_NAME "winter32")
	endif()
else()
	#linux stuff goes here
	set(WINTER_64_BIT FALSE)
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
MESSAGE("INDIGO_TRUNK_DIR: ${INDIGO_TRUNK_DIR}")




set(LLVM_VERSION "3.4")
# The llvm dirs use the version seperator "_" instead of "."
string(REPLACE "." "_" LLVM_VERSION_PATH ${LLVM_VERSION})


set(LIBS_ENV "D:/programming")

if(WIN32)
	# VS subdir
	if(MSVC_VERSION EQUAL 1500) # VS2008
		set(VS_SUB_DIR "vs2008")
	elseif(MSVC_VERSION EQUAL 1600) # VS2010
		set(VS_SUB_DIR "vs2010")
	elseif(MSVC_VERSION EQUAL 1700) # VS2012
		set(VS_SUB_DIR "vs2012")
	endif()
		
	if(WINTER_64_BIT)
		if(MSVC_VERSION EQUAL 1500) # VS2008
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2008_64")
		elseif(MSVC_VERSION EQUAL 1600) # VS2010
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2010_64")
		elseif(MSVC_VERSION EQUAL 1700) # VS2012
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2012_64")
		elseif(MSVC_VERSION EQUAL 1800) # VS2013
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2013_64")
		else()
			message(FATAL_ERROR "unhandled MSVC version")
		endif()
		
		#message("llvmdir: ${llvmdir}")
	
		
		# Set the output directory.  Cmake will append the configuration string to this.
#		set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x64")
#		set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x64")
#		set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x64")
	else()
		if(MSVC_VERSION EQUAL 1500) # VS2008
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2008_32")
		elseif(MSVC_VERSION EQUAL 1600) # VS2010
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2010_32")
		elseif(MSVC_VERSION EQUAL 1700) # VS2012
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2012_32")
		elseif(MSVC_VERSION EQUAL 1800) # VS2013
			set(llvmdir "${LIBS_ENV}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs2013_32")
		else()
			message(FATAL_ERROR "unhandled MSVC version")
		endif()
		
		# Set the output directory.  Cmake will append the configuration string to this.
#		set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x86")
#		set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x86")
#		set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "$ENV{INDIGO_OUTPUT}/${VS_SUB_DIR}/indigo_x86")	
	endif()
	
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

# Add OpenCL paths
if(WINTER_USE_OPENCL)
	message("Building with OpenCL support.")
	
	if(WIN32)
		if(DEFINED ENV{AMDAPPSDKROOT})
			message("OpenCL include path: Using AMDAPPSDKROOT")
			SET(WINTER_INCLUDE_DIRS "${WINTER_INCLUDE_DIRS} $ENV{AMDAPPSDKROOT}include")
		else()
			message("OpenCL include path: Using CUDA_INC_PATH")
			SET(WINTER_INCLUDE_DIRS "${WINTER_INCLUDE_DIRS} $ENV{CUDA_INC_PATH}")
		endif()
	elseif(APPLE)
	else() # linux
		# Just use the OpenCL headers from the CUDA SDK for now.
		addIncludeDirectory("/usr/local/cuda/include")
	endif()
	
	add_definitions("-DUSE_OPENCL=1")
endif()


set(winterdir ".")


set(WINTER_FILES
"${winterdir}/BuiltInFunctionImpl.cpp"
"${winterdir}/LanguageTests.cpp"
"${winterdir}/Linker.cpp"
"${winterdir}/LLVMTypeUtils.cpp"
"${winterdir}/TokenBase.cpp"
"${winterdir}/Value.cpp"
"${winterdir}/VirtualMachine.cpp"
"${winterdir}/wnt_ASTNode.cpp"
"${winterdir}/wnt_Diagnostics.cpp"
"${winterdir}/wnt_ExternalFunction.cpp"
"${winterdir}/wnt_Frame.cpp"
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
"${winterdir}/BuiltInFunctionImp.h"
"${winterdir}/BuiltInFunctionImpl.h"
"${winterdir}/GeneratedTokens.h"
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
"${winterdir}/wnt_Frame.h"
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
"${winterdir}/wnt_LLVMVersion.h"
"${winterdir}/wnt_ArrayLiteral.h"
"${winterdir}/wnt_TupleLiteral.h"
"${winterdir}/wnt_VectorLiteral.h"
"${winterdir}/wnt_VArrayLiteral.h"
"${winterdir}/wnt_Variable.h"
"${winterdir}/FuzzTests.h"
"${winterdir}/TestUtils.cpp"
"${winterdir}/TestUtils.h"
"${winterdir}/winter.cpp"
"${winterdir}/notes.txt"
"${winterdir}/todo.txt"
"${winterdir}/wnt_LetASTNode.cpp"
"${winterdir}/wnt_LetASTNode.h"
"${winterdir}/wnt_LetBlock.cpp"
"${winterdir}/wnt_LetBlock.h"
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
${INDIGO_TRUNK_DIR}/utils/UTF8Utils.cpp
${INDIGO_TRUNK_DIR}/utils/UTF8Utils.h
)

SET(maths_files
${INDIGO_TRUNK_DIR}/maths/SSE.cpp
${INDIGO_TRUNK_DIR}/maths/SSE.h
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
${INDIGO_TRUNK_DIR}/xxHash-r39/xxhash.c
${INDIGO_TRUNK_DIR}/xxHash-r39/xxhash.h
)


if(WINTER_USE_OPENCL)
	SET(glare_opencl_files
	${INDIGO_TRUNK_DIR}/opencl/OpenCL.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCL.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLKernel.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLKernel.h
	${INDIGO_TRUNK_DIR}/opencl/OpenCLBuffer.cpp
	${INDIGO_TRUNK_DIR}/opencl/OpenCLBuffer.h
	)
endif()


SOURCE_GROUP(winter FILES ${WINTER_FILES})
SOURCE_GROUP(utils FILES ${util_files})
SOURCE_GROUP(double_conversion FILES ${double_conversion_files})
SOURCE_GROUP(opencl FILES ${glare_opencl_files})
SOURCE_GROUP(xxhash FILES ${xxhash_files})


include_directories(
${llvmdir}/include
${INDIGO_TRUNK_DIR}
${INDIGO_TRUNK_DIR}/utils
${INDIGO_TRUNK_DIR}/maths
${INDIGO_TRUNK_DIR}/xxHash-r39
${WINTER_INCLUDE_DIRS}
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
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			${WIN_RUNTIME_OPT} -D_SECURE_SCL=0 /Ox ${GL_OPT} -DNDEBUG /Zi")
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
	
	SET(LLVM_LIBS
		LLVMAnalysis.lib LLVMAsmParser.lib LLVMAsmPrinter.lib LLVMBitReader.lib LLVMBitWriter.lib LLVMCodeGen.lib LLVMCore.lib LLVMExecutionEngine.lib LLVMInstCombine.lib LLVMInstrumentation.lib LLVMInterpreter.lib LLVMipa.lib LLVMipo.lib 
		LLVMJIT.lib 
		LLVMLinker.lib LLVMMC.lib LLVMMCDisassembler.lib LLVMMCJIT.lib LLVMMCParser.lib LLVMObject.lib LLVMRuntimeDyld.lib LLVMScalarOpts.lib LLVMSelectionDAG.lib LLVMSupport.lib LLVMTarget.lib LLVMTransformUtils.lib LLVMVectorize.lib LLVMX86AsmParser.lib LLVMX86AsmPrinter.lib LLVMX86CodeGen.lib LLVMX86Desc.lib LLVMX86Disassembler.lib LLVMX86Info.lib LLVMX86Utils.lib
	)
		
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
