find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Tests link against the libraries, never against the uno_server executable.
add_executable(uno_tests smoke_test.cpp)
target_link_libraries(uno_tests PRIVATE uno_core uno_app uno_net Catch2::Catch2WithMain)
uno_set_warnings(uno_tests)

# PRE_TEST: tests are listed when ctest runs, not at build time (the build stays fast and
# does not need the runtime DLLs or sanitizer environment).
catch_discover_tests(uno_tests DISCOVERY_MODE PRE_TEST)
