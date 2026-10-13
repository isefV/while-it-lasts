# -----------------------------------------------------------------------------
# hollow_install.cmake
#
# Install rules and the relocatable CMake package for hollow.
#
# Installed layout (<prefix> = CMAKE_INSTALL_PREFIX):
#
#   <prefix>/include/hollow/...               public headers
#   <prefix>/lib/hollow[d].lib|.a             static library or import library
#   <prefix>/lib/libhollow[d].so*             shared library (Linux)
#   <prefix>/bin/hollow[d].dll                shared library (Windows)
#   <prefix>/bin/hollow[d].pdb                shared library debug symbols (MSVC)
#   <prefix>/src/...                         library sources (Debug install)
#   <prefix>/lib/cmake/hollow/                package: find_package(hollow)
#
# Install Debug and Release into the same prefix; the package then selects the
# matching one for each configuration of the consuming project:
#
#   cmake --install <build-debug>   --prefix <prefix>
#   cmake --install <build-release> --prefix <prefix>
#
# Install static and shared builds into different prefixes.
# -----------------------------------------------------------------------------

include(CMakePackageConfigHelpers)

set(HOLLOW_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/hollow")

install(
    TARGETS hollow
    EXPORT hollowTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/hollow"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# A Debug package carries the library's sources beside include/, at the paths
# its debug info names (/hollow/src/..., see src/CMakeLists.txt), so a copy of
# the package can be debugged into on any PC.
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
    DESTINATION src
    CONFIGURATIONS Debug
    FILES_MATCHING PATTERN "*.cpp" PATTERN "*.hpp"
)

# The linker PDB of the shared library, so debuggers find it next to the DLL.
# (A static library needs none: its debug info is embedded, see
# src/CMakeLists.txt.) OPTIONAL: Release builds produce no PDB.
if(MSVC AND HOLLOW_BUILD_SHARED)
    install(
        FILES "$<TARGET_PDB_FILE:hollow>"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        OPTIONAL
    )
endif()

install(
    EXPORT hollowTargets
    NAMESPACE hollow::
    DESTINATION "${HOLLOW_INSTALL_CMAKEDIR}"
)

# Values baked into hollowConfig.cmake.
set(HOLLOW_PACKAGE_SHARED "${HOLLOW_BUILD_SHARED}")

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/hollowConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/hollowConfig.cmake"
    INSTALL_DESTINATION "${HOLLOW_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/hollowConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
)

# hollowConfig.cmake finds the dependencies again on the consumer's side: those
# of the PUBLIC dependencies (and, for a static library, of the libraries it
# links). Threads has its find module in CMake; a cmake/Find<Name>.cmake such a
# dependency needs is installed here too. Test-only ones (FindGoogleTest.cmake)
# aren't.
install(
    FILES
        "${PROJECT_BINARY_DIR}/hollowConfig.cmake"
        "${PROJECT_BINARY_DIR}/hollowConfigVersion.cmake"
    DESTINATION "${HOLLOW_INSTALL_CMAKEDIR}"
)
