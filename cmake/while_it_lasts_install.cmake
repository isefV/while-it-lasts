# -----------------------------------------------------------------------------
# while_it_lasts_install.cmake
#
# Install rules and the relocatable CMake package for the library.
#
# Installed layout (<prefix> = CMAKE_INSTALL_PREFIX):
#
#   <prefix>/include/while_it_lasts/...               public headers
#   <prefix>/lib/while_it_lasts[d].lib|.a             static library
#   <prefix>/src/...                         library sources (Debug install)
#   <prefix>/lib/cmake/while_it_lasts/                package: find_package(while_it_lasts)
#
# Install Debug and Release into the same prefix; the package then selects the
# matching one for each configuration of the consuming project:
#
#   cmake --install <build-debug>   --prefix <prefix>
#   cmake --install <build-release> --prefix <prefix>
# -----------------------------------------------------------------------------

include(CMakePackageConfigHelpers)

set(WHILE_IT_LASTS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/while_it_lasts")

install(
    TARGETS while_it_lasts
    EXPORT while_it_lastsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/while_it_lasts"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# A Debug package carries the library's sources beside include/, at the paths
# its debug info names (/while_it_lasts/src/..., see src/CMakeLists.txt), so a copy of
# the package can be debugged into on any PC.
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
    DESTINATION src
    CONFIGURATIONS Debug
    FILES_MATCHING PATTERN "*.cpp" PATTERN "*.hpp"
)

install(
    EXPORT while_it_lastsTargets
    NAMESPACE while_it_lasts::
    DESTINATION "${WHILE_IT_LASTS_INSTALL_CMAKEDIR}"
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/while_it_lastsConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/while_it_lastsConfig.cmake"
    INSTALL_DESTINATION "${WHILE_IT_LASTS_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/while_it_lastsConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
)

# while_it_lastsConfig.cmake finds the dependencies again on the consumer's side: those
# of the PUBLIC dependencies and, the library being static, of the
# libraries it links. Threads has its find module in CMake; a cmake/Find<Name>.cmake such a
# dependency needs is installed here too (FindRaylib.cmake). Test-only ones
# (FindGoogleTest.cmake) and header-only private ones (FindNlohmannJson.cmake)
# aren't.
install(
    FILES
        "${PROJECT_BINARY_DIR}/while_it_lastsConfig.cmake"
        "${PROJECT_BINARY_DIR}/while_it_lastsConfigVersion.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/FindRaylib.cmake"
    DESTINATION "${WHILE_IT_LASTS_INSTALL_CMAKEDIR}"
)
