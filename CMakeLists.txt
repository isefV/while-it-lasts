# 3.21 minimum (PROJECT_IS_TOP_LEVEL); policies up to 3.31 are NEW (e.g.
# CMP0141, which lets src/CMakeLists.txt choose the MSVC debug-information
# format).
cmake_minimum_required(VERSION 3.21...3.31)

project(
    hollow
    VERSION 1.1.3
    DESCRIPTION "Hollow, a game built with raylib"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Options
#
# hollow can be the top-level project or part of another one (add_subdirectory(),
# a git submodule, FetchContent). As part of another project it only provides
# the library target by default and leaves the parent's output directories
# alone.
# -----------------------------------------------------------------------------

option(HOLLOW_BUILD_TESTS  "Build the tests and register them with CTest"         ${PROJECT_IS_TOP_LEVEL})
option(HOLLOW_BUILD_APPS   "Build the game in apps/"                              ${PROJECT_IS_TOP_LEVEL})
option(HOLLOW_BUILD_DOCS   "Build the API documentation with Doxygen (target docs)" OFF)
option(HOLLOW_INSTALL      "Generate install rules and the hollow CMake package"    ${PROJECT_IS_TOP_LEVEL})

# -----------------------------------------------------------------------------
# C++ configuration and output layout
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # All runnable artifacts in <build>/bin, for every configuration, so that
    # .vscode/launch.json finds them. As part of another project the parent
    # decides.
    foreach(suffix IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${suffix} "${CMAKE_BINARY_DIR}/bin")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${suffix} "${CMAKE_BINARY_DIR}/lib")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${suffix} "${CMAKE_BINARY_DIR}/lib")
    endforeach()
endif()

# -----------------------------------------------------------------------------
# Dependencies: find_package(<Name>), with one standalone cmake/Find<Name>.cmake
# for each library CMake has no find module for. Such a module uses the library
# in <NAME>_ROOT_DIR when that is set; otherwise FetchContent clones it once
# into extern/<name>. The rules for these files are in README.md.
# -----------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)                             # Threads::Threads
find_package(Raylib REQUIRED)                              # raylib
find_package(NlohmannJson REQUIRED)                        # nlohmann_json::nlohmann_json

# -----------------------------------------------------------------------------
# Library, install rules, tests, apps, docs
# -----------------------------------------------------------------------------

include(GNUInstallDirs)

add_subdirectory(src)

if(HOLLOW_INSTALL)
    include("${PROJECT_SOURCE_DIR}/cmake/hollow_install.cmake")
endif()

if(HOLLOW_BUILD_TESTS)
    find_package(GoogleTest REQUIRED)                      # GTest::gtest_main
    enable_testing()
    add_subdirectory(tests)
endif()

if(HOLLOW_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(HOLLOW_BUILD_DOCS)
    add_subdirectory(docs)
endif()
