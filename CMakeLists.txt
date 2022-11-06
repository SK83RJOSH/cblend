cmake_minimum_required(VERSION 3.24)

# Configure CMake
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Configure C/CXX
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add project wide cmake scripts
add_subdirectory(cmake)

# Git version
find_package(Git)

set(GIT_TAG "0.0.1")
set(GIT_SHA "Unknown")

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --first-parent --abbrev=8 --match '*.*.*'
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIPTION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ECHO_ERROR_VARIABLE
        COMMAND_ERROR_IS_FATAL ANY
    )

    string(REPLACE "-" ";" GIT_DESCRIPTION "${GIT_DESCRIPTION}")
    list(LENGTH GIT_DESCRIPTION GIT_DESCRIPTION_LENGTH)

    if(GIT_DESCRIPTION_LENGTH GREATER 1)
        list(GET GIT_DESCRIPTION 0 GIT_TAG)
        list(GET GIT_DESCRIPTION 1 GIT_SHA)
    else()
        list(GET GIT_DESCRIPTION 0 GIT_SHA)
    endif()
endif()

# Configure project
project(
    cblend
    VERSION ${GIT_TAG}
    DESCRIPTION "A modern C/C++ library for reading blend files. Intended to be used for game engine integrations."
    HOMEPAGE_URL "https://github.com/SK83RJOSH/cblend"
    LANGUAGES CXX C
)

include(GenerateExportHeader)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

set(CLANG CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "Clang")

# Configure multi-config
get_property(BUILDING_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(BUILDING_MULTI_CONFIG)
  if(NOT CMAKE_BUILD_TYPE)
    message(TRACE "Setting up multi-config build types")
    set(CMAKE_CONFIGURATION_TYPES
        Debug
        Release
        RelWithDebInfo
        MinSizeRel
        CACHE STRING "Enabled build types" FORCE)
  else()
    message(TRACE "User chose a specific build type, so we are using that")
    set(CMAKE_CONFIGURATION_TYPES
        ${CMAKE_BUILD_TYPE}
        CACHE STRING "Enabled build types" FORCE)
  endif()
endif()

# Configure tests
option(CBLEND_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

# Configure validation
option(CBLEND_ENABLE_VALIDATION "Enable validation (static analysis, coverage, and sanitizers)" OFF)

# Configure static analysis
set(ENABLE_CLANG_TIDY OFF)
set(ENABLE_CPPCHECK OFF)
set(ENABLE_VS_ANALYSIS OFF)

option(CBLEND_ENABLE_STATIC_ANALYSIS "Enable static analysis" ${CBLEND_ENABLE_VALIDATION})

if(CBLEND_ENABLE_STATIC_ANALYSIS)
    set(ENABLE_CLANG_TIDY "ENABLE_CLANG_TIDY")
    set(ENABLE_CPPCHECK "ENABLE_CPPCHECK")
    set(ENABLE_VS_ANALYSIS "ENABLE_VS_ANALYSIS")
endif()

# Configure coverage
set(ENABLE_COVERAGE OFF)

option(CBLEND_ENABLE_COVERAGE "Enable coverage" ${CBLEND_ENABLE_VALIDATION})

if(CBLEND_ENABLE_COVERAGE)
    set(ENABLE_COVERAGE "ENABLE_COVERAGE")
endif()

# Configure doxygen
set(ENABLE_DOXYGEN OFF)
set(DOXYGEN_THEME "awesome-sidebar")

option(CBLEND_ENABLE_DOXYGEN "Enable doxygen" OFF)

if(CBLEND_ENABLE_DOXYGEN)
    set(ENABLE_DOXYGEN "ENABLE_DOXYGEN")
endif()

# Configure sanitizers
set(ENABLE_SANITIZER_ADDRESS OFF)
set(ENABLE_SANITIZER_LEAK OFF)
set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR OFF)

option(CBLEND_ENABLE_SANITIZE "Enable sanitizers" ${CBLEND_ENABLE_VALIDATION})

if(CBLEND_ENABLE_SANITIZE)
    set(ENABLE_SANITIZER_ADDRESS "ENABLE_SANITIZER_ADDRESS")
    # We can't use these sanitizers on windows
    if(NOT CLANG OR NOT WIN32)
        set(ENABLE_SANITIZER_LEAK "ENABLE_SANITIZER_LEAK")
        set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR "ENABLE_SANITIZER_UNDEFINED_BEHAVIOR")
    endif()
endif()

# Configure include-what-you-use
set(ENABLE_INCLUDE_WHAT_YOU_USE OFF)

option(CBLEND_ENABLE_INCLUDE_WHAT_YOU_USE "Enable include-what-you-use" OFF)

if(CBLEND_ENABLE_INCLUDE_WHAT_YOU_USE)
    set(ENABLE_INCLUDE_WHAT_YOU_USE "ENABLE_INCLUDE_WHAT_YOU_USE")
endif()

# Configure PCH
set(ENABLE_PCH ON)

# Can't combine PCH with CLANG_TIDY on non-llvm compilers
if(ENABLE_CLANG_TIDY AND NOT CLANG)
    set(ENABLE_PCH OFF)
endif()

# Initialize project
project_options(
    ENABLE_CACHE
    ${ENABLE_CPPCHECK}
    CPPCHECK_OPTIONS
        --enable=style,performance,warning,portability
        --inline-suppr
        --suppress=cppcheckError
        --suppress=internalAstError
        --suppress=unmatchedSuppression
        --suppress=passedByValue
        --suppress=syntaxError
        --inconclusive
    ${ENABLE_CLANG_TIDY}
    ${ENABLE_VS_ANALYSIS}
    ${ENABLE_COVERAGE}
    ${ENABLE_DOXYGEN}
    WARNINGS_AS_ERRORS
    ${ENABLE_SANITIZER_ADDRESS}
    ${ENABLE_SANITIZER_LEAK}
    ${ENABLE_SANITIZER_UNDEFINED_BEHAVIOR}
    ${ENABLE_PCH}
    PCH_HEADERS
        <algorithm>
        <array>
        <codecvt>
        <concepts>
        <deque>
        <filesystem>
        <fstream>
        <functional>
        <iomanip>
        <istream>
        <locale>
        <map>
        <mutex>
        <ostream>
        <regex>
        <set>
        <span>
        <string_view>
        <string>
        <tuple>
        <unordered_map>
        <unordered_set>
        <variant>
        <vector>
        <tl/expected.hpp>
        <tl/optional.hpp>
    ${ENABLE_INCLUDE_WHAT_YOU_USE}
)

# Add third-party dependencies
add_subdirectory(third_party)

# Configure cblend
set(CBLEND_DIR ${CMAKE_CURRENT_LIST_DIR})
set(CBLEND_TESTS_DIR ${CBLEND_DIR}/tests)
set(CBLEND_INCLUDE_DIR ${CBLEND_DIR}/include)
set(CBLEND_SOURCE_DIR ${CBLEND_DIR}/source)
set(CBLEND_NATVIS_DIR ${CBLEND_DIR}/natvis)
set(CBLEND_EXTERNAL_DIR ${CBLEND_DIR}/external)
set(CBLEND_DEPENDENCIES expected optional range-v3 range-v3-meta range-v3-concepts)
set(CBLEND_EXPORT ${CMAKE_BINARY_DIR}/exports/cblend_export.hpp)

file(GLOB_RECURSE CBLEND_HEADERS CONFIGURE_DEPENDS ${CBLEND_INCLUDE_DIR}/*.hpp)
file(GLOB_RECURSE CBLEND_SOURCES CONFIGURE_DEPENDS ${CBLEND_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE CBLEND_NATVIS CONFIGURE_DEPENDS ${CBLEND_NATVIS_DIR}/*.natvis)

add_library(cblend STATIC ${CBLEND_HEADERS} ${CBLEND_NATVIS} ${CBLEND_SOURCES})
target_include_directories(
    cblend PUBLIC
    $<BUILD_INTERFACE:${CBLEND_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/exports>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cblend>
)
generate_export_header(cblend EXPORT_FILE_NAME ${CBLEND_EXPORT})
target_link_system_libraries(cblend PUBLIC ${CBLEND_DEPENDENCIES})
set_target_properties(cblend PROPERTIES PUBLIC_HEADER "${CBLEND_EXPORT};${CBLEND_HEADERS}")
set_target_properties(cblend PROPERTIES LINKER_LANGUAGE CXX)

# Merge files from source and include in the IDE
function(group_files SOURCES)
    foreach(FILE ${SOURCES})
        # Get the directory of the source file
        get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

        # Remove common directory prefix to make the group
        string(REPLACE "${CBLEND_DIR}" "" GROUP "${PARENT_DIR}")

        # Make sure we are using windows slashes
        string(REPLACE "/" "\\" GROUP "${GROUP}")

        # Strip the root parts for each possible component
        if("${FILE}" MATCHES "include/.*")
            string(SUBSTRING ${GROUP} 8 -1 GROUP)
        elseif("${FILE}" MATCHES "source/.*")
            string(SUBSTRING ${GROUP} 7 -1 GROUP)
        endif()

        # Do the grouping
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endfunction()
group_files("${CBLEND_SOURCES};${CBLEND_HEADERS}")

# Configure tests
if(CBLEND_BUILD_TESTS)
    file(GLOB_RECURSE CBLEND_TEST_SOURCES CONFIGURE_DEPENDS ${CBLEND_TESTS_DIR}/*.cpp)
    add_executable(cblend-tests ${CBLEND_TEST_SOURCES})
    target_link_libraries(cblend-tests PRIVATE project_options project_warnings)
    target_link_system_libraries(cblend-tests PRIVATE Catch2 Catch2WithMain cblend)
    add_test(
        NAME cblend::tests
        COMMAND cblend-tests
        WORKING_DIRECTORY ${CBLEND_TESTS_DIR}
    )
endif()

# Configure install
if(CMAKE_SKIP_INSTALL_RULES)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

package_project(TARGETS cblend project_options project_warnings ${CBLEND_DEPENDENCIES})

# Configure CPack
set(
    CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
