# -----------------------------------------------------------------------------
# FindRaylib.cmake - raylib, as the target raylib
#
#   list(APPEND CMAKE_MODULE_PATH "<the folder of this file>")
#   find_package(Raylib REQUIRED)
#   target_link_libraries(my_game PRIVATE raylib)
#
# Where raylib comes from:
#
#   1. RAYLIB_ROOT_DIR, when it is set (-DRAYLIB_ROOT_DIR=<dir>, a variable of
#      the calling project, or the environment variable of that name) and holds
#      a raylib: either an install prefix (the folder with lib/cmake/raylib) or
#      a raylib source tree.
#   2. Otherwise FetchContent clones RAYLIB_GIT_REPOSITORY at RAYLIB_GIT_TAG
#      once into RAYLIB_SOURCE_DIR and builds it as part of the calling
#      project. Later configures, in any build folder, use that copy as it is;
#      delete the folder to fetch again (e.g. for another tag).
#
# Variables, all optional, read when find_package() is called:
#
#   RAYLIB_ROOT_DIR         a raylib you have (see 1.)
#   RAYLIB_GIT_REPOSITORY   default: https://github.com/raysan5/raylib.git
#   RAYLIB_GIT_TAG          tag, branch or commit; default: 6.0
#   RAYLIB_SOURCE_DIR       where the clone goes;
#                           default: ../extern/raylib beside this file
#
# Result:
#
#   Raylib_FOUND            TRUE when the target raylib exists
#   RAYLIB_DIR              the raylib in use: its package folder (install
#                           prefix) or its source tree
#   RAYLIB_VERSION          its version
#
# A project that already has the target raylib is used as it is.
# -----------------------------------------------------------------------------

if(TARGET raylib)
    set(Raylib_FOUND TRUE)
    return()
endif()

set(RAYLIB_ROOT_DIR "$ENV{RAYLIB_ROOT_DIR}"
    CACHE PATH "raylib to use: an install prefix or a source tree (empty: fetch it with git)")

if(NOT RAYLIB_GIT_REPOSITORY)
    set(RAYLIB_GIT_REPOSITORY "https://github.com/raysan5/raylib.git")
endif()

if(NOT RAYLIB_GIT_TAG)
    set(RAYLIB_GIT_TAG "6.0")
endif()

if(NOT RAYLIB_SOURCE_DIR)
    set(RAYLIB_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../extern/raylib")
endif()

# Full paths with forward slashes, however they were given (C:\libs\raylib).
get_filename_component(RAYLIB_SOURCE_DIR "${RAYLIB_SOURCE_DIR}" ABSOLUTE)
if(RAYLIB_ROOT_DIR)
    get_filename_component(RAYLIB_ROOT_DIR "${RAYLIB_ROOT_DIR}" ABSOLUTE)
endif()

set(RAYLIB_DIR "")
set(RAYLIB_VERSION "")

# -----------------------------------------------------------------------------
# 1. RAYLIB_ROOT_DIR
# -----------------------------------------------------------------------------

set(_raylib_tree "")            # a source tree to build, instead of the clone

if(RAYLIB_ROOT_DIR)

    find_package(raylib CONFIG QUIET PATHS "${RAYLIB_ROOT_DIR}" NO_DEFAULT_PATH)

    if(raylib_FOUND AND TARGET raylib)
        set(RAYLIB_DIR "${raylib_DIR}")
        set(RAYLIB_VERSION "${raylib_VERSION}")
    elseif(EXISTS "${RAYLIB_ROOT_DIR}/src/raylib.h")
        set(_raylib_tree "${RAYLIB_ROOT_DIR}")
    else()
        message(WARNING
            "raylib: RAYLIB_ROOT_DIR is ${RAYLIB_ROOT_DIR}, which holds neither an "
            "installed raylib (lib/cmake/raylib) nor its sources. Using the fetched "
            "raylib in ${RAYLIB_SOURCE_DIR} instead.")
    endif()

endif()

# -----------------------------------------------------------------------------
# 2. FetchContent, from the git repository
# -----------------------------------------------------------------------------

if(NOT RAYLIB_DIR)

    include(FetchContent)

    # A source tree that is there already (RAYLIB_ROOT_DIR, or fetched before,
    # possibly by another build folder): use it as it is.
    if(_raylib_tree)
        set(FETCHCONTENT_SOURCE_DIR_RAYLIB "${_raylib_tree}")
    elseif(EXISTS "${RAYLIB_SOURCE_DIR}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_RAYLIB "${RAYLIB_SOURCE_DIR}")
    elseif(NOT Raylib_FIND_QUIETLY)
        message(STATUS "raylib: fetching ${RAYLIB_GIT_TAG} from ${RAYLIB_GIT_REPOSITORY} "
                       "into ${RAYLIB_SOURCE_DIR} (set RAYLIB_ROOT_DIR to use one you have)")
    endif()

    # raylib's own options: the library only, without its examples.
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    # SOURCE_SUBDIR names a folder without a CMakeLists.txt, so FetchContent
    # only provides the sources; add_subdirectory(... EXCLUDE_FROM_ALL) below
    # then keeps raylib's install rules out of the calling project.
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY "${RAYLIB_GIT_REPOSITORY}"
        GIT_TAG "${RAYLIB_GIT_TAG}"
        GIT_SHALLOW TRUE
        SOURCE_DIR "${RAYLIB_SOURCE_DIR}"
        SOURCE_SUBDIR "cmake/none"
    )

    FetchContent_MakeAvailable(raylib)

    unset(FETCHCONTENT_SOURCE_DIR_RAYLIB)

    if(EXISTS "${raylib_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${raylib_SOURCE_DIR}" "${raylib_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()

    if(TARGET raylib)
        set(RAYLIB_DIR "${raylib_SOURCE_DIR}")

        # The version is the one the source tree itself declares.
        file(STRINGS "${raylib_SOURCE_DIR}/src/raylib.h" _raylib_version
             REGEX "#define RAYLIB_VERSION +\"[0-9.]+")
        if(_raylib_version MATCHES "([0-9]+(\\.[0-9]+)+)")
            set(RAYLIB_VERSION "${CMAKE_MATCH_1}")
        endif()
        unset(_raylib_version)
    endif()

endif()

unset(_raylib_tree)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Raylib
    REQUIRED_VARS RAYLIB_DIR
    VERSION_VAR RAYLIB_VERSION
)
