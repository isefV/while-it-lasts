# Tests, written with GoogleTest (cmake/FindGoogleTest.cmake). Each TEST() is
# registered with CTest as its own test: ctest --test-dir <build> -R Counter

add_executable(
    hollow_tests
    hollow_test.cpp
)

target_link_libraries(
    hollow_tests
    PRIVATE
        hollow::hollow
        GTest::gtest_main
)

include(GoogleTest)

# PRE_TEST: the tests are listed when ctest runs, not while building, so a
# shared hollow only has to be next to the test program by then.
gtest_discover_tests(
    hollow_tests
    DISCOVERY_MODE PRE_TEST
)
