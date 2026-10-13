# -----------------------------------------------------------------------------
# FindGoogleTest.cmake - GoogleTest, as the targets GTest::gtest,
# GTest::gtest_main, GTest::gmock and GTest::gmock_main
#
#   list(APPEND CMAKE_MODULE_PATH "<the folder of this file>")
#   find_package(GoogleTest REQUIRED)
#   target_link_libraries(my_tests PRIVATE GTest::gtest_main)
#
# Where GoogleTest comes from:
#
#   1. GOOGLETEST_ROOT_DIR, when it is set (-DGOOGLETEST_ROOT_DIR=<dir>, a
#      variable of the calling project, or the environment variable of that
#      name) and holds a GoogleTest: either an install prefix (the folder with
#      lib/cmake/GTest) or a GoogleTest source tree.
#   2. Otherwise FetchContent clones GOOGLETEST_GIT_REPOSITORY at
#      GOOGLETEST_GIT_TAG once into GOOGLETEST_SOURCE_DIR and builds it as part
#      of the calling project. Later configures, in any build folder, use that
#      copy as it is; delete the folder to fetch again (e.g. for another tag).
#
# Variables, all optional, read when find_package() is called:
#
#   GOOGLETEST_ROOT_DIR         a GoogleTest you have (see 1.)
#   GOOGLETEST_GIT_REPOSITORY   default: https://github.com/google/googletest.git
#   GOOGLETEST_GIT_TAG          tag, branch or commit; default: v1.17.0
#   GOOGLETEST_SOURCE_DIR       where the clone goes;
#                               default: ../extern/googletest beside this file
#
# Result:
#
#   GoogleTest_FOUND            TRUE when the GTest:: targets exist
#   GOOGLETEST_DIR              the GoogleTest in use: its package folder
#                               (install prefix) or its source tree
#   GOOGLETEST_VERSION          its version
#
# A project that already has GTest::gtest_main is used as it is.
# -----------------------------------------------------------------------------

if(TARGET GTest::gtest_main)
    set(GoogleTest_FOUND TRUE)
    return()
endif()

set(GOOGLETEST_ROOT_DIR "$ENV{GOOGLETEST_ROOT_DIR}"
    CACHE PATH "GoogleTest to use: an install prefix or a source tree (empty: fetch it with git)")

if(NOT GOOGLETEST_GIT_REPOSITORY)
    set(GOOGLETEST_GIT_REPOSITORY "https://github.com/google/googletest.git")
endif()

if(NOT GOOGLETEST_GIT_TAG)
    set(GOOGLETEST_GIT_TAG "v1.17.0")
endif()

if(NOT GOOGLETEST_SOURCE_DIR)
    set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../extern/googletest")
endif()

# Full paths with forward slashes, however they were given (C:\libs\googletest).
get_filename_component(GOOGLETEST_SOURCE_DIR "${GOOGLETEST_SOURCE_DIR}" ABSOLUTE)
if(GOOGLETEST_ROOT_DIR)
    get_filename_component(GOOGLETEST_ROOT_DIR "${GOOGLETEST_ROOT_DIR}" ABSOLUTE)
endif()

set(GOOGLETEST_DIR "")
set(GOOGLETEST_VERSION "")

# -----------------------------------------------------------------------------
# 1. GOOGLETEST_ROOT_DIR
# -----------------------------------------------------------------------------

set(_googletest_tree "")        # a source tree to build, instead of the clone

if(GOOGLETEST_ROOT_DIR)

    find_package(GTest CONFIG QUIET PATHS "${GOOGLETEST_ROOT_DIR}" NO_DEFAULT_PATH)

    if(GTest_FOUND)
        set(GOOGLETEST_DIR "${GTest_DIR}")
        set(GOOGLETEST_VERSION "${GTest_VERSION}")
    elseif(EXISTS "${GOOGLETEST_ROOT_DIR}/googletest/CMakeLists.txt")
        set(_googletest_tree "${GOOGLETEST_ROOT_DIR}")
    else()
        message(WARNING
            "GoogleTest: GOOGLETEST_ROOT_DIR is ${GOOGLETEST_ROOT_DIR}, which holds neither an "
            "installed GoogleTest (lib/cmake/GTest) nor its sources. Using the fetched "
            "GoogleTest in ${GOOGLETEST_SOURCE_DIR} instead.")
    endif()

endif()

# -----------------------------------------------------------------------------
# 2. FetchContent, from the git repository
# -----------------------------------------------------------------------------

if(NOT GOOGLETEST_DIR)

    include(FetchContent)

    # A source tree that is there already (GOOGLETEST_ROOT_DIR, or fetched
    # before, possibly by another build folder): use it as it is.
    if(_googletest_tree)
        set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${_googletest_tree}")
    elseif(EXISTS "${GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${GOOGLETEST_SOURCE_DIR}")
    elseif(NOT GoogleTest_FIND_QUIETLY)
        message(STATUS "GoogleTest: fetching ${GOOGLETEST_GIT_TAG} from ${GOOGLETEST_GIT_REPOSITORY} "
                       "into ${GOOGLETEST_SOURCE_DIR} (set GOOGLETEST_ROOT_DIR to use one you have)")
    endif()

    # GoogleTest's own options: the same C++ runtime as the code it is linked
    # into (MSVC), and no install rules for it in the calling project.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY "${GOOGLETEST_GIT_REPOSITORY}"
        GIT_TAG "${GOOGLETEST_GIT_TAG}"
        GIT_SHALLOW TRUE
        SOURCE_DIR "${GOOGLETEST_SOURCE_DIR}"
    )

    FetchContent_MakeAvailable(googletest)

    unset(FETCHCONTENT_SOURCE_DIR_GOOGLETEST)

    if(TARGET GTest::gtest_main)
        set(GOOGLETEST_DIR "${googletest_SOURCE_DIR}")

        # The version is the one the source tree itself declares.
        file(STRINGS "${googletest_SOURCE_DIR}/CMakeLists.txt" _googletest_version
             REGEX "set\\(GOOGLETEST_VERSION [0-9.]+\\)")
        if(_googletest_version MATCHES "([0-9]+(\\.[0-9]+)+)")
            set(GOOGLETEST_VERSION "${CMAKE_MATCH_1}")
        endif()
        unset(_googletest_version)
    endif()

endif()

unset(_googletest_tree)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    GoogleTest
    REQUIRED_VARS GOOGLETEST_DIR
    VERSION_VAR GOOGLETEST_VERSION
)
