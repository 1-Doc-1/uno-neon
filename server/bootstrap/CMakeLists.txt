# uno_bootstrap: what the composition root decides from its environment, kept out of main.cpp so it can be tested.
add_library(uno_bootstrap STATIC src/test_hooks.cpp)
target_include_directories(uno_bootstrap PUBLIC include)
target_link_libraries(uno_bootstrap PUBLIC uno_core uno_app uno_net)
# Only a binary built with UNO_ENABLE_TEST_HOOKS can be configured for end-to-end tests (ADR 0016, ADR 0023).
if(UNO_ENABLE_TEST_HOOKS)
    target_compile_definitions(uno_bootstrap PUBLIC UNO_ENABLE_TEST_HOOKS)
    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries(uno_bootstrap PRIVATE uno_testing spdlog::spdlog)
endif()
uno_set_warnings(uno_bootstrap)
