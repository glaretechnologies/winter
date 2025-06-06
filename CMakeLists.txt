cmake_minimum_required (VERSION 3.5)


project("winter")

set(CURRENT_TARGET winter)

set(WINTER_USE_OPENCL OFF CACHE BOOL "Enable OpenCL support, currently just used for test execution.")
set(WINTER_INCLUDE_TESTS TRUE)

SET(GLARE_CORE_TRUNK_DIR "../glare-core" CACHE PATH "Path to glare-core repo trunk dir.")

if(GLARE_CORE_TRUNK_DIR STREQUAL "")
	MESSAGE(FATAL_ERROR "ERROR: Please set GLARE_CORE_TRUNK_DIR to the path to glare-core repo trunk dir, e.g. -DGLARE_CORE_TRUNK_DIR=path/to/glare/core")
endif()


SET(LIBS_DIR "" CACHE PATH "Path to libraries dir.  llvm builds should be in this dir, for example LIBS_DIR/llvm/llvm_11_0_0_install_vs2022_64")

if(LIBS_DIR STREQUAL "")
	MESSAGE(FATAL_ERROR "ERROR: Please set LIBS_DIR to the path to llvm builds can be found in, e.g. -DLIBS_DIR=c:/libs")
endif()



SET(WINTER_LLVM_VERSION "15.0.7" CACHE STRING "LLVM version to use")


MESSAGE("Building with OpenCL support: ${WINTER_USE_OPENCL} (-DWINTER_USE_OPENCL=OFF/ON))")
MESSAGE("Libraries dir: ${LIBS_DIR} (-DLIBS_DIR)")
MESSAGE("Glare-core trunk dir: ${GLARE_CORE_TRUNK_DIR} (-DGLARE_CORE_TRUNK_DIR)")
MESSAGE("Winter LLVM version: ${WINTER_LLVM_VERSION} (-DWINTER_LLVM_VERSION)")



# Work out WINTER_LLVM_DIR
# The llvm dirs use the version seperator "_" instead of "."
string(REPLACE "." "_" LLVM_VERSION_PATH ${WINTER_LLVM_VERSION})
if(WIN32)
	if(MSVC_VERSION EQUAL 1900)
		set(WINTER_VS_VER 2015)
	elseif(MSVC_VERSION EQUAL 1911 OR MSVC_VERSION EQUAL 1912)
		set(WINTER_VS_VER 2017)
	elseif(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
		set(WINTER_VS_VER 2019)
	elseif(MSVC_VERSION GREATER_EQUAL 1930)
		set(WINTER_VS_VER 2022)
	else()
		message(FATAL_ERROR "Unhandled MSVC version")
	endif()
	
	set(WINTER_LLVM_DIR "${LIBS_DIR}/llvm/llvm_${LLVM_VERSION_PATH}_install_vs${WINTER_VS_VER}_64")
elseif(APPLE)
	set(WINTER_LLVM_DIR "${LIBS_DIR}/llvm/llvm_${LLVM_VERSION_PATH}_install")
else() # else Linux.
	set(WINTER_LLVM_DIR "${LIBS_DIR}/llvm/llvm_${LLVM_VERSION_PATH}_dylib_install")
endif()

MESSAGE("LLVM dir: ${WINTER_LLVM_DIR}")

SET(WINTER_DIR ".")

# defines WINTER_FILES, WINTER_LLVM_LIBS
include(embed_winter.cmake)


SET(WINTER_FILES 
winter.cpp
${WINTER_FILES}
)

SOURCE_GROUP(winter FILES ${WINTER_FILES})
SOURCE_GROUP(utils FILES ${WINTER_UTIL_FILES})
SOURCE_GROUP(double_conversion FILES ${WINTER_DOUBLE_CONVERSION_FILES})
SOURCE_GROUP(opencl FILES ${WINTER_OPENCL_FILES})
SOURCE_GROUP(xxhash FILES ${WINTER_XXHASH_FILES})


include_directories(${WINTER_INCLUDE_DIRECTORIES})

add_executable("winter"
${WINTER_FILES}
${WINTER_OTHER_FILES}
)

set_target_properties(${CURRENT_TARGET} PROPERTIES
	CXX_STANDARD 17 # Needed for LLVM 16.0.0
)

add_definitions("-DBUILD_TESTS=1")

if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
	add_definitions(/MP) # Build with multiple processes
	
	add_definitions(/W4) # Set warning level to 4
	
	# Ignore some warnings
	add_definitions(/wd4127) # 'conditional expression is constant' - don't want this warning as we tend to turn off code with 'if(false) {}'
	add_definitions(/wd4100) # 'unreferenced formal parameter'
	
	#add_definitions(/GS- /fp:fast)
	
	add_definitions(-D__SSE__ -D__SSE2__ -D__SSE3__ -D__SSSE3__ -D__SSE4_1__)
	
	# Append optimisation flags.
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			")	
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	/O2 -D_SECURE_SCL=0 -DNDEBUG")
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			/O2 -D_SECURE_SCL=0 -DNDEBUG /Zi") # /Zi to produce a PDB file
	
elseif(APPLE)
	


else() # Linux	
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -mssse3 -msse4.1 -Wno-reorder")
	SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -mssse3 -msse4.1")
endif()


add_definitions("${WINTER_PREPROCESSOR_DEFINITIONS}")


# LLVM include path and compiler settings.
if(WIN32)
	# Append LLVM paths for the configurations
	SET(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE}			/I\"${WINTER_LLVM_DIR}/include\"")
	SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO}	/I\"${WINTER_LLVM_DIR}/include\"")
	SET(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG}			/I\"${WINTER_LLVM_DIR}_debug/include\"")
else()
	SET(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -I${WINTER_LLVM_DIR}/include -I/usr/local/include")
endif()


# LLVM linker settings.
if(WIN32)

	SET(LLVM_LINK_FLAGS_RELEASE			"/LIBPATH:\"${WINTER_LLVM_DIR}/lib\"")
	SET(LLVM_LINK_FLAGS_RELWITHDEBINFO	"/LIBPATH:\"${WINTER_LLVM_DIR}/lib\"")
	SET(LLVM_LINK_FLAGS_DEBUG			"/LIBPATH:\"${WINTER_LLVM_DIR}_debug/lib\"")
	 
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE			"${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${LLVM_LINK_FLAGS_RELEASE}")
	SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${LLVM_LINK_FLAGS_RELWITHDEBINFO}")
	SET(CMAKE_EXE_LINKER_FLAGS_DEBUG			"${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${LLVM_LINK_FLAGS_DEBUG}")
	
	SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE			"${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${LLVM_LINK_FLAGS_RELEASE}")
	SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO	"${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} ${LLVM_LINK_FLAGS_RELWITHDEBINFO}")
	SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG				"${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${LLVM_LINK_FLAGS_DEBUG}")
	
	
	target_link_libraries(${CURRENT_TARGET} 
		${WINTER_LLVM_LIBS}
	)

elseif(APPLE)

	target_link_libraries(${CURRENT_TARGET} 
		${WINTER_LLVM_LIBS}
		dl
		pthread
		curses # used by LLVM
		z # Used by llvm 
		"-framework Foundation"
		"-framework Carbon"
	)

else()
	
	target_link_libraries(${CURRENT_TARGET} 
		${WINTER_LLVM_LIBS}
		dl
		pthread
		z # Used by llvm 
	)
endif()
