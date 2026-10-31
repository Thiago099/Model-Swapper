# ---------------------------------------------------- #
#                  CommonLibSSE-NG source              #
# ---------------------------------------------------- #
# Prefer a shared clone only when it includes Skyrim 1.7 support. Older clones
# classify 1.7 as SE and select the wrong address-library IDs.

if(DEFINED ENV{COMMONLIB_SSE_FOLDER} AND NOT "$ENV{COMMONLIB_SSE_FOLDER}" STREQUAL "")
  set(COMMONLIBSSE_DIR "$ENV{COMMONLIB_SSE_FOLDER}")
  set(COMMONLIBSSE_VERSION_HEADER "${COMMONLIBSSE_DIR}/include/SKSE/Version.h")
  set(COMMONLIBSSE_HAS_17_SUPPORT "")
  if(EXISTS "${COMMONLIBSSE_VERSION_HEADER}")
    file(STRINGS "${COMMONLIBSSE_VERSION_HEADER}" COMMONLIBSSE_HAS_17_SUPPORT
      REGEX "RUNTIME_SSE_1_7_104")
  endif()
  if(NOT COMMONLIBSSE_HAS_17_SUPPORT)
    message(STATUS "COMMONLIB_SSE_FOLDER lacks Skyrim 1.7.104 support; using bundled CommonLibSSE-NG")
    set(COMMONLIBSSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
  endif()
else()
  set(COMMONLIBSSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
endif()
if(NOT EXISTS "${COMMONLIBSSE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "CommonLibSSE-NG not found at '${COMMONLIBSSE_DIR}'. Set COMMONLIB_SSE_FOLDER to a Skyrim 1.7.104-capable clone or run: git submodule update --init --recursive")
endif()
file(STRINGS "${COMMONLIBSSE_DIR}/include/SKSE/Version.h" COMMONLIBSSE_HAS_17_SUPPORT
  REGEX "RUNTIME_SSE_1_7_104")
if(NOT COMMONLIBSSE_HAS_17_SUPPORT)
  message(FATAL_ERROR "CommonLibSSE-NG at '${COMMONLIBSSE_DIR}' must be updated for Skyrim 1.7.104 support")
endif()

# This makes norton happy again

set(CMAKE_C_COMPILER ${CCOMPILER_PATH})


# ---------------------------------------------------- #
#               Imports and definitions                #
# ---------------------------------------------------- #

include(cmake/headerlist.cmake)
include(cmake/sourcelist.cmake)
include(cmake/lib/copyOutputs.cmake)
include(cmake/lib/automaticGameFolderOutput.cmake)

cmake_minimum_required(VERSION 3.21)
set(AUTHOR_NAME "SkyrimThiago")
set(PRODUCT_NAME "ModelSwapper")
set(BEAUTIFUL_NAME "Model Swapper")
project(${PRODUCT_NAME} VERSION 0.3.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ---------------------------------------------------- #
#                      Resources                       #
# ---------------------------------------------------- #

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

set(RESOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
set(RESOURCE_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.res)

add_custom_command(
    OUTPUT ${RESOURCE_OUTPUT}
    COMMAND rc /fo ${RESOURCE_OUTPUT} ${RESOURCE_FILE}
    DEPENDS ${RESOURCE_FILE}
)

add_custom_target(Resource ALL DEPENDS ${RESOURCE_OUTPUT})

# ---------------------------------------------------- #
#                Plugin Configuration                  #
# ---------------------------------------------------- #

include(FetchContent)

set(ENABLE_SKYRIM_SE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL " " FORCE)
set(BUILD_TESTS OFF CACHE BOOL " " FORCE)

message(STATUS "CommonLibSSE-NG source: ${COMMONLIBSSE_DIR}")
add_subdirectory("${COMMONLIBSSE_DIR}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)

find_path(CLIB_UTIL_INCLUDE_DIRS "ClibUtil/utils.hpp")

file(
  GLOB_RECURSE
  SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  ${SOURCE_FILES}
)

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${HEADER_FILES} ${SOURCE_FILES}
)

set(BUILD_TESTS OFF)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
  ${PROJECT_NAME}
  AUTHOR AUTHOR_NAME
  SOURCES ${HEADER_FILES} ${SOURCE_FILES}
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) 
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CLIB_UTIL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build the configuration editor even when the plugin target is selected directly.
# Its output stays in the build tree; copyOutputs only deploys the SKSE plugin.
add_subdirectory(editor)
add_dependencies(${PROJECT_NAME} ModelSwapperEditor)
if(MS_EDITOR_BUILD_TESTS)
  enable_testing()
endif()


# ---------------------------------------------------- #
#                        Output                        #
# ---------------------------------------------------- #

set(wildlander_output false)
set(steam_owrt_output false)
set(steam_mods_output true)
automaticGameFolderOutput(wildlander_output, steam_owrt_output, steam_mods_output)
