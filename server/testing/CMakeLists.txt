# uno_testing: deterministic test doubles for uno_core, built only with the test suite.
add_library(uno_testing STATIC src/seeded_random_source.cpp)
target_include_directories(uno_testing PUBLIC include)
target_link_libraries(uno_testing PUBLIC uno_core)
uno_set_warnings(uno_testing)
