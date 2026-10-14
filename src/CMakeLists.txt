# -----------------------------------------------------------------------------
# hollow::hollow - the library holding the game's code
#
# Always a static library: it is linked into the game (apps/app) and the tests.
# -----------------------------------------------------------------------------

add_library(hollow STATIC)
add_library(hollow::hollow ALIAS hollow)

target_sources(
    hollow
    PRIVATE
        hollow.cpp
        data/game_data.cpp
        effects/effects.cpp
        objects/actor.cpp
        objects/sprite.cpp
        system/assets.cpp
        system/audio.cpp
        system/canvas.cpp
        system/dialogue_runner.cpp
        system/input.cpp
        system/script.cpp
        system/story_state.cpp
        ui/bark.cpp
        ui/dialogue_box.cpp
        ui/inventory_panel.cpp
        ui/text_renderer.cpp
        ui/widgets.cpp
        utility/color.cpp
        utility/json_util.cpp
        utility/random.cpp
        utility/text.cpp
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
#
# raylib is in the public headers. Inside BUILD_INTERFACE it stays out of the
# exported targets (raylib built here is in no export set); the installed
# package links it again after find_dependency(Raylib), see
# cmake/hollowConfig.cmake.in. nlohmann/json is header-only and used in the
# .cpp files only, so the installed static library needs nothing of it.
target_link_libraries(
    hollow
    PUBLIC
        $<BUILD_INTERFACE:raylib>
    PRIVATE
        Threads::Threads
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

target_compile_definitions(
    hollow
    PRIVATE
        HOLLOW_VERSION="${PROJECT_VERSION}"
)

target_compile_options(
    hollow
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# Debugging from other projects:
#
# - DEBUG_POSTFIX lets Debug and Release builds sit side by side in one
#   install prefix (hollowd.* / hollow.*). MSVC cannot mix them: a Debug
#   application needs the Debug library (debug CRT, iterator debug level).
#
# - MSVC_DEBUG_INFORMATION_FORMAT=Embedded (/Z7) stores the debug info inside
#   the object files, so hollow.lib carries it with it and the consumer's
#   linker merges it into the consumer's PDB.
#   GCC/Clang always embed DWARF (-g) in the .a.
set_target_properties(
    hollow
    PROPERTIES
        EXPORT_NAME hollow
        DEBUG_POSTFIX d
        MSVC_DEBUG_INFORMATION_FORMAT "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Embedded>"
)

# - Portable source paths: debug info (and __FILE__) name the library's files
#   /hollow/src/... and /hollow/include/... instead of this PC's folder. The
#   root is the same on every PC and matches the installed package, whose
#   Debug install ships src/ next to include/ (see hollow_install.cmake). A
#   debugger then needs one mapping, /hollow -> the package (or checkout)
#   folder. Compiler messages keep the real paths.
if(MSVC)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" HOLLOW_NATIVE_SOURCE_DIR)

    target_compile_options(
        hollow
        PRIVATE
            /experimental:deterministic
            "/pathmap:${HOLLOW_NATIVE_SOURCE_DIR}=\\hollow"
    )
else()
    target_compile_options(hollow PRIVATE "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=/hollow")
endif()
