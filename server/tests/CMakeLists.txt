find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Tests link against the libraries, never against the uno_server executable.
add_executable(uno_tests
    card_test.cpp
    deck_test.cpp
    piles_test.cpp
    playability_test.cpp
    random_test.cpp
    scoring_test.cpp
    match_test.cpp
    round_apply_test.cpp
    round_effects_test.cpp
    round_end_test.cpp
    round_test.cpp
    round_uno_test.cpp
    round_wild_draw_four_test.cpp
    turn_order_test.cpp
    smoke_test.cpp)
target_link_libraries(uno_tests PRIVATE
    uno_core uno_app uno_net uno_testing Catch2::Catch2WithMain)
uno_set_warnings(uno_tests)

# PRE_TEST: tests are listed when ctest runs, not at build time (the build stays fast and
# does not need the runtime DLLs or sanitizer environment).
catch_discover_tests(uno_tests DISCOVERY_MODE PRE_TEST)
