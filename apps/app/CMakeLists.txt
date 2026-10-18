# The game: the main loop, the rooms' logic (scripts/) and the screens. The
# systems it is built from are in the library (hollow::hollow).

add_executable(
    game
    main.cpp
    game.cpp
    fight.cpp
    reaper.cpp
    revive.cpp
    scripts/campsite.cpp
    scripts/checkpoint.cpp
    scripts/creature.cpp
    scripts/creature_room.cpp
    scripts/scripts.cpp
    scripts/summit.cpp
    walkthrough.cpp
)

# The program carries the game's name: "The Hollow Dawn.exe" (the target is
# still "game" in CMake, the tasks and the tests).
set_target_properties(game PROPERTIES OUTPUT_NAME "The Hollow Dawn")

# The logo as the program's own icon, and its name in Windows (game.rc, game.ico).
if(WIN32)
    target_sources(game PRIVATE game.rc)
endif()

target_link_libraries(
    game
    PRIVATE
        hollow::hollow
)

target_compile_options(
    game
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# A Windows program without a console window in Release builds (single-config
# generators such as Ninja; the console stays in Debug for messages).
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(game PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
        target_link_options(game PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

# The game loads its files from assets/ next to the program, so every build
# copies the project's assets/ folder there (<build>/bin/assets). It is its own
# target so that new or changed art is copied even when no code changed.
add_custom_target(hollow_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:game>/assets"
    COMMENT "Copying assets/ next to the game"
    VERBATIM
)
add_dependencies(game hollow_assets)

# A short run in CTest reads and checks every data file in assets/ (--check
# doesn't open a window, so it also runs where there is no display). A
# mistake in a JSON file fails this test with the file and the place.
if(HOLLOW_BUILD_TESTS)
    add_test(
        NAME hollow.app
        COMMAND game --check
    )

    # Whole playthroughs with virtual input (tests/walkthrough/): the game
    # must reach its end on a careful and on a careless route. They open a
    # hidden window, so they need a desktop (label "window"; skip them on a
    # machine without one: ctest -LE window).
    foreach(route IN ITEMS full careless)
        add_test(
            NAME hollow.walkthrough.${route}
            COMMAND game --walkthrough "${PROJECT_SOURCE_DIR}/tests/walkthrough/${route}.txt"
        )
        set_tests_properties(hollow.walkthrough.${route} PROPERTIES LABELS window TIMEOUT 120)
    endforeach()
endif()
