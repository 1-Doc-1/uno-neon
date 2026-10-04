find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
include(Catch)

# Tests link against the libraries, never against the uno_server executable.
add_executable(uno_tests
    card_test.cpp
    application_declare_uno_test.cpp
    application_draw_amount_test.cpp
    application_draw_rule_test.cpp
    application_lifecycle_test.cpp
    application_match_test.cpp
    application_presentation_test.cpp
    application_rooms_test.cpp
    application_uno_test.cpp
    client_event_test.cpp
    codec_test.cpp
    crypto_random_source_test.cpp
    deck_test.cpp
    nickname_test.cpp
    origin_policy_test.cpp
    piles_test.cpp
    playability_test.cpp
    player_view_test.cpp
    protocol_contract_test.cpp
    quiet_logs.cpp
    random_test.cpp
    rate_limiter_test.cpp
    scoring_test.cpp
    server_integration_test.cpp
    simulation_test.cpp
    match_test.cpp
    round_apply_test.cpp
    round_declare_uno_test.cpp
    round_draw_amount_test.cpp
    round_draw_rule_test.cpp
    round_effects_test.cpp
    round_end_test.cpp
    round_removal_test.cpp
    round_test.cpp
    round_uno_test.cpp
    round_wild_draw_four_test.cpp
    turn_order_test.cpp
    uws_scheduler_test.cpp
    smoke_test.cpp
    test_hooks_test.cpp
    websocket_server_test.cpp
    no_dialogs.cpp
    support/test_client.cpp)
target_link_libraries(uno_tests PRIVATE
    uno_core uno_app uno_net uno_bootstrap uno_testing Catch2::Catch2WithMain nlohmann_json::nlohmann_json spdlog::spdlog)
target_include_directories(uno_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# Windows sockets, for the raw test client of the real server.
if(WIN32)
    target_link_libraries(uno_tests PRIVATE ws2_32)
endif()
# Shared with the Angular client: the examples of the protocol (SPEC §8.1).
target_compile_definitions(uno_tests PRIVATE UNO_PROTOCOL_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/../protocol/examples")
uno_set_warnings(uno_tests)

# PRE_TEST: tests are listed when ctest runs, not at build time (the build stays fast and
# does not need the runtime DLLs or sanitizer environment).
catch_discover_tests(uno_tests DISCOVERY_MODE PRE_TEST)
