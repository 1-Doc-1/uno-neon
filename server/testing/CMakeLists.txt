# uno_testing: deterministic test doubles and fixtures for uno_core, built only with the test suite.
find_package(Catch2 3 CONFIG REQUIRED)

add_library(uno_testing STATIC src/legal_actions.cpp src/round_invariants.cpp src/seeded_random_source.cpp src/view_leak_check.cpp)
target_include_directories(uno_testing PUBLIC include)
target_link_libraries(uno_testing PUBLIC uno_core PRIVATE Catch2::Catch2)
uno_set_warnings(uno_testing)
