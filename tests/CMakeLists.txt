cmake_minimum_required(VERSION 3.10)
# Note : must be included by master CMakeLists.txt
# Tests in static libraries might not get registered, see https://github.com/onqtam/doctest/blob/master/doc/markdown/faq.md#why-are-my-tests-in-a-static-library-not-getting-registered
# For this reason, and because it is interesting to have individual
# test executables for each library, it is suggested not to put tests directly in the libraries (even though doctest advocates this usage)
# Creating multiple executables is of course not mandatory, and one could use the same executable with various command lines to filter what tests to run.

add_executable(failtest failtest.cpp)
target_link_libraries(failtest doctest::doctest)

add_test(
    # Use some per-module/project prefix so that it is easier to run only tests for this module
    NAME VALHALLA.failtest
    COMMAND failtest ${TEST_RUNNER_PARAMS}
)
set_tests_properties(
    VALHALLA.failtest
    PROPERTIES
        WILL_FAIL TRUE # We expect this test to fail
)

add_executable(successtest successtest.cpp)
target_link_libraries(successtest doctest::doctest)

add_test(
    NAME VALHALLA.successtest
    COMMAND successtest ${TEST_RUNNER_PARAMS}
)
