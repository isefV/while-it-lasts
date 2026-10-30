# -----------------------------------------------------------------------------
# FindNlohmannJson.cmake - nlohmann/json, as the target nlohmann_json::nlohmann_json
#
#   list(APPEND CMAKE_MODULE_PATH "<the folder of this file>")
#   find_package(NlohmannJson REQUIRED)
#   target_link_libraries(my_app PRIVATE nlohmann_json::nlohmann_json)
#
# Where nlohmann/json comes from:
#
#   1. NLOHMANN_JSON_ROOT_DIR, when it is set (-DNLOHMANN_JSON_ROOT_DIR=<dir>, a
#      variable of the calling project, or the environment variable of that
#      name) and holds a nlohmann/json: either an install prefix (the folder
#      with share/cmake/nlohmann_json or lib/cmake/nlohmann_json) or a
#      nlohmann/json source tree.
#   2. Otherwise FetchContent clones NLOHMANN_JSON_GIT_REPOSITORY at
#      NLOHMANN_JSON_GIT_TAG once into NLOHMANN_JSON_SOURCE_DIR and adds it to
#      the calling project. Later configures, in any build folder, use that
#      copy as it is; delete the folder to fetch again (e.g. for another tag).
#
# Variables, all optional, read when find_package() is called:
#
#   NLOHMANN_JSON_ROOT_DIR         a nlohmann/json you have (see 1.)
#   NLOHMANN_JSON_GIT_REPOSITORY   default: https://github.com/nlohmann/json.git
#   NLOHMANN_JSON_GIT_TAG          tag, branch or commit; default: v3.12.0
#   NLOHMANN_JSON_SOURCE_DIR       where the clone goes;
#                                  default: ../extern/nlohmann_json beside this file
#
# Result:
#
#   NlohmannJson_FOUND             TRUE when the target nlohmann_json::nlohmann_json exists
#   NLOHMANN_JSON_DIR              the nlohmann/json in use: its package folder
#                                  (install prefix) or its source tree
#   NLOHMANN_JSON_VERSION          its version
#
# A project that already has the target nlohmann_json::nlohmann_json uses it as
# it is.
# -----------------------------------------------------------------------------

if(TARGET nlohmann_json::nlohmann_json)
    set(NlohmannJson_FOUND TRUE)
    return()
endif()

set(NLOHMANN_JSON_ROOT_DIR "$ENV{NLOHMANN_JSON_ROOT_DIR}"
    CACHE PATH "nlohmann/json to use: an install prefix or a source tree (empty: fetch it with git)")

if(NOT NLOHMANN_JSON_GIT_REPOSITORY)
    set(NLOHMANN_JSON_GIT_REPOSITORY "https://github.com/nlohmann/json.git")
endif()

if(NOT NLOHMANN_JSON_GIT_TAG)
    set(NLOHMANN_JSON_GIT_TAG "v3.12.0")
endif()

if(NOT NLOHMANN_JSON_SOURCE_DIR)
    set(NLOHMANN_JSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../extern/nlohmann_json")
endif()

# Full paths with forward slashes, however they were given (C:\libs\json).
get_filename_component(NLOHMANN_JSON_SOURCE_DIR "${NLOHMANN_JSON_SOURCE_DIR}" ABSOLUTE)
if(NLOHMANN_JSON_ROOT_DIR)
    get_filename_component(NLOHMANN_JSON_ROOT_DIR "${NLOHMANN_JSON_ROOT_DIR}" ABSOLUTE)
endif()

set(NLOHMANN_JSON_DIR "")
set(NLOHMANN_JSON_VERSION "")

# -----------------------------------------------------------------------------
# 1. NLOHMANN_JSON_ROOT_DIR
# -----------------------------------------------------------------------------

set(_nlohmann_json_tree "")     # a source tree to use, instead of the clone

if(NLOHMANN_JSON_ROOT_DIR)

    find_package(nlohmann_json CONFIG QUIET PATHS "${NLOHMANN_JSON_ROOT_DIR}" NO_DEFAULT_PATH)

    if(nlohmann_json_FOUND AND TARGET nlohmann_json::nlohmann_json)
        set(NLOHMANN_JSON_DIR "${nlohmann_json_DIR}")
        set(NLOHMANN_JSON_VERSION "${nlohmann_json_VERSION}")
    elseif(EXISTS "${NLOHMANN_JSON_ROOT_DIR}/single_include/nlohmann/json.hpp")
        set(_nlohmann_json_tree "${NLOHMANN_JSON_ROOT_DIR}")
    else()
        message(WARNING
            "nlohmann/json: NLOHMANN_JSON_ROOT_DIR is ${NLOHMANN_JSON_ROOT_DIR}, which holds "
            "neither an installed nlohmann/json (cmake/nlohmann_json) nor its sources. Using "
            "the fetched nlohmann/json in ${NLOHMANN_JSON_SOURCE_DIR} instead.")
    endif()

endif()

# -----------------------------------------------------------------------------
# 2. FetchContent, from the git repository
# -----------------------------------------------------------------------------

if(NOT NLOHMANN_JSON_DIR)

    include(FetchContent)

    # A source tree that is there already (NLOHMANN_JSON_ROOT_DIR, or fetched
    # before, possibly by another build folder): use it as it is.
    if(_nlohmann_json_tree)
        set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${_nlohmann_json_tree}")
    elseif(EXISTS "${NLOHMANN_JSON_SOURCE_DIR}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${NLOHMANN_JSON_SOURCE_DIR}")
    elseif(NOT NlohmannJson_FIND_QUIETLY)
        message(STATUS "nlohmann/json: fetching ${NLOHMANN_JSON_GIT_TAG} from ${NLOHMANN_JSON_GIT_REPOSITORY} "
                       "into ${NLOHMANN_JSON_SOURCE_DIR} (set NLOHMANN_JSON_ROOT_DIR to use one you have)")
    endif()

    # nlohmann/json's own options: the library only, no tests, no install
    # rules in the calling project.
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)

    # SOURCE_SUBDIR names a folder without a CMakeLists.txt, so FetchContent
    # only provides the sources; add_subdirectory(... EXCLUDE_FROM_ALL) below
    # adds the header-only target.
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY "${NLOHMANN_JSON_GIT_REPOSITORY}"
        GIT_TAG "${NLOHMANN_JSON_GIT_TAG}"
        GIT_SHALLOW TRUE
        SOURCE_DIR "${NLOHMANN_JSON_SOURCE_DIR}"
        SOURCE_SUBDIR "cmake/none"
    )

    FetchContent_MakeAvailable(nlohmann_json)

    unset(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON)

    if(EXISTS "${nlohmann_json_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${nlohmann_json_SOURCE_DIR}" "${nlohmann_json_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()

    if(TARGET nlohmann_json::nlohmann_json)
        set(NLOHMANN_JSON_DIR "${nlohmann_json_SOURCE_DIR}")

        # The version is the one the source tree itself declares.
        file(STRINGS "${nlohmann_json_SOURCE_DIR}/CMakeLists.txt" _nlohmann_json_version
             REGEX "project\\(nlohmann_json VERSION [0-9.]+")
        if(_nlohmann_json_version MATCHES "([0-9]+(\\.[0-9]+)+)")
            set(NLOHMANN_JSON_VERSION "${CMAKE_MATCH_1}")
        endif()
        unset(_nlohmann_json_version)
    endif()

endif()

unset(_nlohmann_json_tree)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    NlohmannJson
    REQUIRED_VARS NLOHMANN_JSON_DIR
    VERSION_VAR NLOHMANN_JSON_VERSION
)
