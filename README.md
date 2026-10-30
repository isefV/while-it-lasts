# While It Lasts

**While It Lasts** is a C++20 game built with [raylib](https://www.raylib.com).

The game code lives in the `hollow` library (static or shared, on Windows and
Linux); the program in `apps/app` links it with raylib, opens the window and
loads its files from `assets/`. The library can also be used from another
CMake project through `find_package` or as part of it (`add_subdirectory`, git
submodule, `FetchContent`), including debugging into its sources.

- Building, internals and contributing: [docs/README-Guideline.md](docs/README-Guideline.md)
- History of changes: [docs/README-Catalog.md](docs/README-Catalog.md)

```
hollow/
├── CMakeLists.txt       options, dependencies, library, install, tests, apps, docs
├── cmake/               one Find<Name>.cmake per dependency (FindRaylib.cmake, FindGoogleTest.cmake), install rules
├── include/hollow/      public headers
├── src/                 the library's sources
├── apps/                apps/app: the game (raylib window), and apps/consumer: a separate project using the library
├── assets/              the game's files (textures, sounds, fonts, ...), copied to <build>/bin/assets
├── tests/               GoogleTest tests (CTest)
├── docs/                README-Guideline.md, README-Catalog.md, Doxygen (HOLLOW_BUILD_DOCS)
├── extern/              dependencies cloned by FetchContent (not in git)
├── README.md  LICENSE.md
└── .vscode/             tasks (configure, build, rebuild, clean, install, ctest), launch configurations, settings
```

---

## 1. Requirements

| | |
|---|---|
| C++ | C++20 |
| Compiler | MSVC (Visual Studio 2022/2026), GCC or Clang with C++20 support |
| CMake | 3.21 or newer |
| OS | Windows and Linux |
| Dependencies | Threads (system); raylib for the game and GoogleTest for the tests, cloned with git into `extern/raylib` and `extern/googletest` unless `RAYLIB_ROOT_DIR` / `GOOGLETEST_ROOT_DIR` name ones you have (see [5. Dependencies](#5-dependencies)) |

## 2. Build, test, install

In VS Code every step is a task: *Terminal › Run Task › CMake: ...*
(Ctrl+Shift+B = `CMake: build`). Each task asks **Debug | Release › static
library | shared library** and runs plain `cmake` commands. On Windows the
tasks load the Visual Studio developer environment themselves; choose the
compiler with `"hollow.toolchain"` (`msvc`, `gcc`, `clang`) in
`.vscode/settings.json`.

| Task | Does |
|---|---|
| `CMake: configure` | configure `build/<toolchain>-<config>-<linkage>` from scratch (new CMake cache) |
| `CMake: build` | configure, then build the library |
| `CMake: rebuild` | configure, then clean and build the library |
| `CMake: clean` | delete the build outputs; the configuration stays |
| `CMake: install` | build the library and install it to `install/<toolchain>/<linkage>` |
| `CMake: ctest` | build everything (library, tests, apps) and run the tests |

The same in a terminal (Windows: a *Developer PowerShell for VS*):

```bash
cmake -S . -B build/msvc-Debug-static -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=cl -DHOLLOW_BUILD_SHARED=OFF
cmake --build build/msvc-Debug-static                                   # library, tests, apps
ctest --test-dir build/msvc-Debug-static                                # tests
cmake --install build/msvc-Debug-static --prefix install/msvc/static   # package
```

Install Debug **and** Release into the same prefix; a consuming project then
picks the matching one. Static and shared go to different prefixes.

```
install/msvc/static/
├── include/hollow/...          public headers
├── lib/hollow.lib, hollowd.lib  libraries (shared: import libraries, plus bin/*.dll and bin/hollowd.pdb)
├── src/...                    library sources, for debugging into it (Debug install)
└── lib/cmake/hollow/           the CMake package: find_package(hollow)
```

## 3. Usage as a library

### A. As an installed package (`find_package`)

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_app LANGUAGES CXX)

find_package(hollow CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE hollow::hollow)

# Shared library on Windows: copy the DLLs next to the executable. (On Linux
# the program finds libhollow.so through the RPATH that CMake sets.)
if(WIN32 AND hollow_SHARED)
    add_custom_command(TARGET my_app POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${hollow_RUNTIME_DIR}" "$<TARGET_FILE_DIR:my_app>")
endif()
```

Configure with `-DCMAKE_PREFIX_PATH=<package folder>`. Debug builds link
`hollowd`, Release builds `hollow`.

**Debugging into the library.** The library's debug info names its sources
`\hollow\src\...` (GCC: `/hollow/src/...`) on every PC, and a Debug package
ships them in `src/`. Map that root to the package folder once:

```json
"sourceFileMap": { "\\hollow": "C:/libs/hollow" }
```

(GDB: `"/hollow"` as the key, or `set substitute-path /hollow C:/libs/hollow`.)
Then F11 on a hollow call opens the package's copy of the source.

### B. From the sources

```cmake
add_subdirectory(third_party/hollow)                    # a copy or git submodule
# or:
include(FetchContent)
FetchContent_Declare(hollow GIT_REPOSITORY <url> GIT_TAG <tag>)   # or SOURCE_DIR <folder>
FetchContent_MakeAvailable(hollow)

target_link_libraries(my_app PRIVATE hollow::hollow)
```

As part of another project only the library is built: tests, apps and
install rules are off (turn them on with `HOLLOW_BUILD_TESTS`,
`HOLLOW_BUILD_APPS`, `HOLLOW_INSTALL`). [apps/consumer](apps/consumer) is a
complete consumer that builds both ways.

## 4. Build options

| CMake option | Default | Meaning |
|---|---|---|
| `HOLLOW_BUILD_SHARED` | `OFF` (follows `BUILD_SHARED_LIBS`) | Shared instead of static library |
| `HOLLOW_BUILD_TESTS` | `ON` when top-level | Tests |
| `HOLLOW_BUILD_APPS` | `ON` when top-level | The game in `apps/` (needs raylib) |
| `HOLLOW_BUILD_DOCS` | `OFF` | Target `docs` (Doxygen) |
| `HOLLOW_INSTALL` | `ON` when top-level | Install rules and the CMake package |

The variables of the dependencies (`GOOGLETEST_ROOT_DIR`, ...) belong to their
find modules, not to hollow: see the next section.

## 5. Dependencies

Every dependency is found with `find_package(<Name>)` in the top-level
`CMakeLists.txt`. A library CMake has no find module for gets one file,
`cmake/Find<Name>.cmake`; [cmake/FindGoogleTest.cmake](cmake/FindGoogleTest.cmake)
is the example to copy.

| Variable of `FindGoogleTest.cmake` | Default | Meaning |
|---|---|---|
| `GOOGLETEST_ROOT_DIR` | the environment variable of that name | A GoogleTest you have: an install prefix or a source tree |
| `GOOGLETEST_GIT_REPOSITORY` | `https://github.com/google/googletest.git` | Repository to clone when there is none |
| `GOOGLETEST_GIT_TAG` | `v1.17.0` | Tag, branch or commit to clone |
| `GOOGLETEST_SOURCE_DIR` | `extern/googletest` | Where the clone goes |

[cmake/FindRaylib.cmake](cmake/FindRaylib.cmake) follows the same rules and
provides the target `raylib`:

| Variable of `FindRaylib.cmake` | Default | Meaning |
|---|---|---|
| `RAYLIB_ROOT_DIR` | the environment variable of that name | A raylib you have: an install prefix or a source tree |
| `RAYLIB_GIT_REPOSITORY` | `https://github.com/raysan5/raylib.git` | Repository to clone when there is none |
| `RAYLIB_GIT_TAG` | `6.0` | Tag, branch or commit to clone |
| `RAYLIB_SOURCE_DIR` | `extern/raylib` | Where the clone goes |

raylib is written in C, so the build also needs a C compiler (the files in
`.vscode/cmake/` set it next to the C++ one).

Rules for a `cmake/Find<Name>.cmake`:

1. **A find module per library.** The file is named `Find<Name>.cmake` and is
   used through `find_package(<Name>)`, never through `include()`. It ends
   with `find_package_handle_standard_args(<Name> ...)`, so `REQUIRED`,
   `QUIET` and a requested version work as for any other package.
2. **The library's own prefix.** Its variables start with the library's name
   in upper case: `GOOGLETEST_ROOT_DIR`, `GOOGLETEST_GIT_TAG`. Never `HOLLOW_*`.
3. **Standalone.** The file uses no variable or option of this project (no
   `HOLLOW_*`, no `PROJECT_SOURCE_DIR`); paths are relative to the file itself
   (`CMAKE_CURRENT_LIST_DIR`). Copied into another project's `cmake/` folder
   it works unchanged.
4. **`<NAME>_ROOT_DIR` first.** When that variable (cache, calling project or
   environment) names a folder holding the library, it is used from there.
   Otherwise the library is fetched; a `<NAME>_ROOT_DIR` that holds no such
   library gives a warning and is fetched as well.
5. **Fetched from its git repository into `extern/`.** `FetchContent` with
   `GIT_REPOSITORY` and `GIT_TAG` clones the library once into
   `extern/<name>`; later configures, in any build folder, use that copy.
   Delete `extern/<name>` to fetch again, for example after changing the tag.
6. **Standalone messages.** Messages start with the library's name
   (`GoogleTest: ...`) and name only the library's own variables, nothing of
   this project.

A dependency of the installed library (a `PUBLIC` one, or any library a static
hollow links) is also found again by `cmake/hollowConfig.cmake.in` with
`find_dependency(<Name>)`; its `Find<Name>.cmake`, if it has one, is installed
next to the package (`cmake/hollow_install.cmake`). GoogleTest is used by the
tests only and raylib by the game in `apps/app` only, so they need neither.

## 6. Assets

Put the game's files (textures, sounds, fonts, ...) in `assets/`. Every build
of `hollow_app` copies the folder to `<build>/bin/assets`, next to the
program, which loads from there:

```cpp
const std::string assets = std::string(GetApplicationDirectory()) + "assets/";
Texture2D player = LoadTexture((assets + "player.png").c_str());
```
