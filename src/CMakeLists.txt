# -----------------------------------------------------------------------------
# hollow::hollow - the library
#
# Static by default, shared with HOLLOW_BUILD_SHARED=ON.
# -----------------------------------------------------------------------------

if(HOLLOW_BUILD_SHARED)
    set(HOLLOW_LIBRARY_TYPE SHARED)
else()
    set(HOLLOW_LIBRARY_TYPE STATIC)
endif()

message(STATUS "hollow: building a ${HOLLOW_LIBRARY_TYPE} library")

add_library(hollow ${HOLLOW_LIBRARY_TYPE})
add_library(hollow::hollow ALIAS hollow)

target_sources(
    hollow
    PRIVATE
        hollow.cpp
)

# Public headers: <hollow/...>. Different paths while building and after
# install.
target_include_directories(
    hollow
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# A requirement, not a global flag: consumers get C++20 too.
target_compile_features(hollow PUBLIC cxx_std_20)

# Dependencies:
#   PUBLIC  - used in the public headers; consumers need it too
#   PRIVATE - used only in the .cpp files
target_link_libraries(
    hollow
    PRIVATE
        Threads::Threads
)

# HOLLOW_BUILDING_LIBRARY selects dllexport in <hollow/export.hpp>;
# HOLLOW_SHARED is propagated so consumers select dllimport.
target_compile_definitions(
    hollow
    PRIVATE
        HOLLOW_BUILDING_LIBRARY
)

if(HOLLOW_BUILD_SHARED)
    target_compile_definitions(
        hollow
        PUBLIC
            HOLLOW_SHARED
    )

    # MSVC warns about exported classes that hold standard-library members
    # (C4251) or derive from non-exported bases (C4275). Both are expected
    # here: the library and its consumers share one compiler and runtime.
    target_compile_options(
        hollow
        PUBLIC
            $<$<CXX_COMPILER_ID:MSVC>:/wd4251>
            $<$<CXX_COMPILER_ID:MSVC>:/wd4275>
    )
endif()

target_compile_options(
    hollow
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# Only symbols marked HOLLOW_API are exported from the shared library.
#
# Debugging from other projects:
#
# - DEBUG_POSTFIX lets Debug and Release builds sit side by side in one
#   install prefix (hollowd.* / hollow.*). MSVC cannot mix them: a Debug
#   application needs the Debug library (debug CRT, iterator debug level).
#
# - MSVC_DEBUG_INFORMATION_FORMAT=Embedded (/Z7) stores the debug info inside
#   the object files, so a static hollow.lib carries it with it and the
#   consumer's linker merges it into the consumer's PDB. A shared build still
#   gets its own hollow[d].pdb next to the DLL from the linker.
#   GCC/Clang always embed DWARF (-g) in the .a / .dll / .so.
set_target_properties(
    hollow
    PROPERTIES
        EXPORT_NAME hollow
        DEBUG_POSTFIX d
        MSVC_DEBUG_INFORMATION_FORMAT "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Embedded>"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# - Portable source paths: debug info (and __FILE__) name the library's files
#   /hollow/src/... and /hollow/include/... instead of this PC's folder. The
#   root is the same on every PC and matches the installed package, whose
#   Debug install ships src/ next to include/ (see hollow_install.cmake). A
#   debugger then needs one mapping, /hollow -> the package (or checkout)
#   folder. Compiler messages keep the real paths.
if(MSVC)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" HOLLOW_NATIVE_SOURCE_DIR)
    set(HOLLOW_PATH_MAP /experimental:deterministic "/pathmap:${HOLLOW_NATIVE_SOURCE_DIR}=\\hollow")

    target_compile_options(hollow PRIVATE ${HOLLOW_PATH_MAP})

    # The linker writes the shared library's PDB and must map the same way.
    if(HOLLOW_BUILD_SHARED)
        target_link_options(hollow PRIVATE ${HOLLOW_PATH_MAP})
    endif()
else()
    target_compile_options(hollow PRIVATE "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=/hollow")
endif()
