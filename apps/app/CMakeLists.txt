# The game: a raylib program that uses the library.

add_executable(
    hollow_app
    main.cpp
)

target_link_libraries(
    hollow_app
    PRIVATE
        hollow::hollow
        raylib
)

# The game loads its files from assets/ next to the program, so every build
# copies the project's assets/ folder there (<build>/bin/assets).
add_custom_command(
    TARGET hollow_app
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:hollow_app>/assets"
    COMMENT "Copying assets/ next to hollow_app"
    VERBATIM
)

# A short run in CTest checks that the program starts and ends cleanly;
# --check doesn't open a window, so it also runs where there is no display.
if(HOLLOW_BUILD_TESTS)
    add_test(
        NAME hollow.app
        COMMAND hollow_app --check
    )
endif()
