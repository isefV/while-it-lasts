# Tests, written with GoogleTest (cmake/FindGoogleTest.cmake). Each TEST() is
# registered with CTest as its own test: ctest --test-dir <build> -R Condition

add_executable(
    while_it_lasts_tests
    script_test.cpp
    dialogue_test.cpp
    text_test.cpp
    story_test.cpp
    actor_test.cpp
)

target_link_libraries(
    while_it_lasts_tests
    PRIVATE
        while_it_lasts::while_it_lasts
        GTest::gtest_main
)

# story_test.cpp reads the game's real data from the source tree.
target_compile_definitions(
    while_it_lasts_tests
    PRIVATE
        WHILE_IT_LASTS_TEST_ASSETS="${PROJECT_SOURCE_DIR}/assets/"
)

include(GoogleTest)

# PRE_TEST: the tests are listed when ctest runs, not while building.
gtest_discover_tests(
    while_it_lasts_tests
    DISCOVERY_MODE PRE_TEST
)
