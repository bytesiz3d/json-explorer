cmake_minimum_required(VERSION 3.20)

include(doctest)

add_executable(json-parser-tests json-parser-tests.cpp)
target_compile_definitions(json-parser-tests PUBLIC TESTS_DIR="${json-test-suite_SOURCE_DIR}/test_parsing")
target_link_libraries(json-parser-tests json-parser doctest)

doctest_discover_tests(json-parser-tests)
