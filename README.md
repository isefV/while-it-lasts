# hollow

**Hollow** is a C++20 game built with [raylib](https://www.raylib.com).

The game code lives in the static `hollow` library (Windows and Linux); the
program in `apps/app` links it with raylib, opens the window and
loads its files from `assets/`. The library can also be used from another
CMake project through `find_package` or as part of it (`add_subdirectory`, git
submodule, `FetchContent`), including debugging into its sources.

- Building, internals and contributing: [docs/README-Guideline.md](docs/README-Guideline.md)
- History of changes: [docs/README-Catalog.md](docs/README-Catalog.md)

## The game: The Hollow Dawn

A short narrative game: Rob half-carries his friend Jace up the mountain where they
grew up. Five stages (campfire, factory, perimeter, mountain steps, summit),
told through dialogue, a few simple actions and choices that come back later:
the night starts at midnight and ends at sunrise on the summit.

| Key | |
|---|---|
| A / D, ← / → | walk |
| E, standing next to it | use the thing marked with the E prompt, talk to X |
| W / S, ↑ / ↓ | pick another thing when several are within reach |
| G | take hold of X when he can't walk, and let go of him |
| F, R, Q | in a fight: swing the bat, shoot, dodge back |
| E, Space, Enter | next line (the first press shows the whole line) |
| ↑ / ↓, 1–4, then E or Enter | choose an option |
| Tab, I | inventory (arrows to look through it, E to use a bandage, the antibiotics, a bottle, …) |
| Esc | pause |
| F11, Alt+Enter | fullscreen / window |

The story is data, not code: dialogue, rooms, items and the story variables
are JSON files in `assets/data/`, described in
[docs/DATA-FORMAT.md](docs/DATA-FORMAT.md). The decisions behind the
prototype are in [docs/DECISIONS.md](docs/DECISIONS.md). Every object is a
coloured rectangle until its image exists in `assets/`, so art never blocks a
playable build (file names: visual-asset-config.txt).

```
"The Hollow Dawn"                  fullscreen; --windowed for a 1280 x 720 window
"The Hollow Dawn" --check          read and check every data file, no window
"The Hollow Dawn" --room summit --set cig_count=0      start anywhere, with any state
"The Hollow Dawn" --walkthrough tests/walkthrough/full.txt   play the game by itself
```

Where the code is:

| Folder | |
|---|---|
| `include/hollow/`, `src/` | the library: `system/` (canvas, input, assets, audio, story state, conditions and effects, dialogue runner), `data/` (loading and checking the JSON), `objects/` (rooms, items, characters, sprite sheets, actors), `effects/` (fades, shake, firelight, apparitions, particles), `ui/` (dialogue box, inventory, prompts, barks, menus), `utility/` |
| `apps/app/` | the game: `game.cpp` (screens, rooms, interaction, X following and being dragged), `fight.cpp` (the real-time fights and waves), `revive.cpp` (the near-death scene), `reaper.cpp` (the Reaper following them), `scripts/` (the campsite timer, the creature, the checkpoint stealth, the sunrise, the ending), `walkthrough.cpp` |
| `assets/data/` | the story |
| `tests/` | unit tests, story branches on the real data, `walkthrough/` playthroughs |

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
└── .vscode/             tasks (configure, build, rebuild, clean, install, ctest), CMake Tools kits, launch configurations, settings
```

---

## 1. Requirements

| | |
|---|---|
| C++ | C++20 |
| Compiler | MSVC (Visual Studio 2022/2026), GCC or Clang with C++20 support |
| CMake | 3.21 or newer |
| OS | Windows and Linux |
| Dependencies | Threads (system); raylib and nlohmann/json for the library, GoogleTest for the tests, cloned with git into `extern/raylib`, `extern/nlohmann_json` and `extern/googletest` unless `RAYLIB_ROOT_DIR` / `NLOHMANN_JSON_ROOT_DIR` / `GOOGLETEST_ROOT_DIR` name ones you have (see [5. Dependencies](#5-dependencies)) |

## 2. Build, test, install

In VS Code every step is a task: *Terminal › Run Task › CMake: ...*
(Ctrl+Shift+B = `CMake: build`). Each task asks for the compiler
(**msvc | gcc | clang**), then **Debug | Release**, and runs plain `cmake`
commands. On Windows the tasks load the Visual Studio developer environment
themselves; gcc and clang must be on `PATH`.

| Task | Does |
|---|---|
| `CMake: configure` | configure `build/<toolchain>-<config>` from scratch (new CMake cache) |
| `CMake: build` | configure, then build the library |
| `Build game` | configure, then build the game, `bin/The Hollow Dawn.exe` (library and assets with it) |
| `CMake: rebuild` | configure, then clean and build the library |
| `CMake: clean` | delete the build outputs; the configuration stays |
| `CMake: install` | build the library and install it to `install/<toolchain>` |
| `CMake: ctest` | build everything (library, tests, apps) and run the tests |

The same in a terminal (Windows: a *Developer PowerShell for VS*):

```bash
cmake -S . -B build/msvc-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build/msvc-Debug                            # library, tests, apps
ctest --test-dir build/msvc-Debug                         # tests
cmake --install build/msvc-Debug --prefix install/msvc    # package
```

With the CMake Tools extension the compiler can also be picked with *CMake:
Select a Kit*: `.vscode/cmake-kits.json` lists `msvc`, `gcc` and `clang`, and
the extension then builds in the same `build/<kit>-<config>` folders as the
tasks. The `msvc` kit names a Visual Studio by its instance id; on another PC
replace it with the `instanceId` that `vswhere` prints.

Install Debug **and** Release into the same prefix; a consuming project then
picks the matching one.

```
install/msvc/
├── include/hollow/...          public headers
├── lib/hollow.lib, hollowd.lib  static libraries
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
| `HOLLOW_BUILD_TESTS` | `ON` when top-level | Tests |
| `HOLLOW_BUILD_APPS` | `ON` when top-level | The game in `apps/` |
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

[cmake/FindNlohmannJson.cmake](cmake/FindNlohmannJson.cmake) provides the
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

A dependency of the installed library (any library hollow links, since it is
static) is also found again by `cmake/hollowConfig.cmake.in` with
`find_dependency(<Name>)`; its `Find<Name>.cmake`, if it has one, is installed
next to the package (`cmake/hollow_install.cmake`): raylib, with
`FindRaylib.cmake`. GoogleTest is used by the tests only, and nlohmann/json is
header-only and private to the library's `.cpp` files, so they need neither.

## 6. Assets

Put the game's files (textures, sounds, fonts, ...) in `assets/`. Every build
copies the folder to `<build>/bin/assets`, next to the
program, which loads from there:

```cpp
const std::string assets = std::string(GetApplicationDirectory()) + "assets/";
Texture2D player = LoadTexture((assets + "player.png").c_str());
```
