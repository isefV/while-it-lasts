# While It Lasts: building

The short version is in the [README](../README.md). This file has the rest.

## Requirements

| | |
|---|---|
| C++ | C++20 |
| Compiler | MSVC (Visual Studio 2022/2026), GCC or Clang with C++20 support |
| CMake | 3.21 or newer |
| OS | Windows and Linux |
| Dependencies | Threads (system); raylib and nlohmann/json for the game, GoogleTest for the tests, cloned with git into `extern/` unless `RAYLIB_ROOT_DIR` / `NLOHMANN_JSON_ROOT_DIR` / `GOOGLETEST_ROOT_DIR` name ones you have |

## VS Code tasks

Every step is a task: *Terminal › Run Task...* (Ctrl+Shift+B = `Build`). Each
task asks for the compiler (**msvc | gcc | clang**), then **Debug | Release**,
and runs plain `cmake` commands. On Windows the tasks load the Visual Studio
developer environment themselves; gcc and clang must be on `PATH`.

| Task | Does |
|---|---|
| `Configure` | Configure `build/<toolchain>-<config>` from scratch (new CMake cache) |
| `Build` | Configure, then build the library |
| `Build game` | Configure, then build the game, `bin/While It Lasts.exe` (library and assets with it) |
| `Rebuild` | Configure, then clean and build the library |
| `Clean` | Delete the build outputs; the configuration stays |
| `Install` | Build the library and install it to `install/<toolchain>` |
| `Ctest` | Build everything (library, tests, game) and run the tests |
| `Debug build: msvc`, `Debug build: gcc` | Configure and build everything in Debug, in `build/<toolchain>-Debug` |

With the CMake Tools extension the compiler can also be picked with *CMake:
Select a Kit*: `.vscode/cmake-kits.json` lists `msvc`, `gcc` and `clang`, and
the extension then builds in the same `build/<kit>-<config>` folders as the
tasks. The `msvc` kit names a Visual Studio by its instance id; on another PC
replace it with the `instanceId` that `vswhere` prints.

The launch configurations in `.vscode/launch.json` start the game in a window
and the tests under the debugger.

## A release build

A Release build has no console window on Windows. To build a program that also
needs no Visual C++ runtime on the player's PC, link the runtime statically:

```bash
cmake -S . -B build/msvc-Release -G Ninja -C .vscode/cmake/msvc.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build/msvc-Release
ctest --test-dir build/msvc-Release
```

Then ship `While It Lasts.exe` and the `assets` folder from `bin/`, together.

## Tests

`ctest` runs the unit tests, the story branches on the real data, a check of
every data file (`while_it_lasts.app`) and two whole playthroughs
(`while_it_lasts.walkthrough.*`). The playthroughs open a hidden window, so skip
them on a machine without a desktop: `ctest -LE window`.

## Build options

| CMake option | Default | Meaning |
|---|---|---|
| `WHILE_IT_LASTS_BUILD_TESTS` | `ON` when top-level | Tests |
| `WHILE_IT_LASTS_BUILD_APPS` | `ON` when top-level | The game in `apps/` |
| `WHILE_IT_LASTS_BUILD_DOCS` | `OFF` | Target `docs` (Doxygen) |
| `WHILE_IT_LASTS_INSTALL` | `ON` when top-level | Install rules and the CMake package of the library |

## Dependencies

Every dependency is found with `find_package(<Name>)` in the top-level
`CMakeLists.txt`. A library CMake has no find module for gets one file,
`cmake/Find<Name>.cmake`; [cmake/FindGoogleTest.cmake](../cmake/FindGoogleTest.cmake)
is the example to copy.

| Variable of `FindGoogleTest.cmake` | Default | Meaning |
|---|---|---|
| `GOOGLETEST_ROOT_DIR` | the environment variable of that name | A GoogleTest you have: an install prefix or a source tree |
| `GOOGLETEST_GIT_REPOSITORY` | `https://github.com/google/googletest.git` | Repository to clone when there is none |
| `GOOGLETEST_GIT_TAG` | `v1.17.0` | Tag, branch or commit to clone |
| `GOOGLETEST_SOURCE_DIR` | `extern/googletest` | Where the clone goes |

[cmake/FindRaylib.cmake](../cmake/FindRaylib.cmake) follows the same rules and
provides the target `raylib`:

| Variable of `FindRaylib.cmake` | Default | Meaning |
|---|---|---|
| `RAYLIB_ROOT_DIR` | the environment variable of that name | A raylib you have: an install prefix or a source tree |
| `RAYLIB_GIT_REPOSITORY` | `https://github.com/raysan5/raylib.git` | Repository to clone when there is none |
| `RAYLIB_GIT_TAG` | `6.0` | Tag, branch or commit to clone |
| `RAYLIB_SOURCE_DIR` | `extern/raylib` | Where the clone goes |

[cmake/FindNlohmannJson.cmake](../cmake/FindNlohmannJson.cmake) provides the
target `nlohmann_json::nlohmann_json` the same way (`NLOHMANN_JSON_ROOT_DIR`,
`NLOHMANN_JSON_GIT_REPOSITORY`, `NLOHMANN_JSON_GIT_TAG` = `v3.12.0`,
`NLOHMANN_JSON_SOURCE_DIR` = `extern/nlohmann_json`).

raylib is written in C, so the build also needs a C compiler (the files in
`.vscode/cmake/` set it next to the C++ one).

Rules for a `cmake/Find<Name>.cmake`:

1. **A find module per library.** The file is named `Find<Name>.cmake` and is
   used through `find_package(<Name>)`, never through `include()`. It ends
   with `find_package_handle_standard_args(<Name> ...)`, so `REQUIRED`,
   `QUIET` and a requested version work as for any other package.
2. **The library's own prefix.** Its variables start with the library's name
   in upper case: `GOOGLETEST_ROOT_DIR`, `GOOGLETEST_GIT_TAG`. Never
   `WHILE_IT_LASTS_*`.
3. **Standalone.** The file uses no variable or option of this project (no
   `WHILE_IT_LASTS_*`, no `PROJECT_SOURCE_DIR`); paths are relative to the file
   itself (`CMAKE_CURRENT_LIST_DIR`). Copied into another project's `cmake/`
   folder it works unchanged.
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

A dependency of the installed library (any library it links, since it is
static) is also found again by `cmake/while_it_lastsConfig.cmake.in` with
`find_dependency(<Name>)`; its `Find<Name>.cmake`, if it has one, is installed
next to the package (`cmake/while_it_lasts_install.cmake`): raylib, with
`FindRaylib.cmake`. GoogleTest is used by the tests only, and nlohmann/json is
header-only and private to the library's `.cpp` files, so they need neither.

## Assets

Every build copies `assets/` to `<build>/bin/assets`, next to the program, which
loads its files from there.
