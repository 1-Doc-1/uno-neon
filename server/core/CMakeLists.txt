# uno_core: pure game domain. No network, no clock, no global randomness, no I/O.
configure_file(src/build_info.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.cpp" @ONLY)

add_library(uno_core STATIC
    src/deck.cpp
    src/piles.cpp
    src/round.cpp
    src/turn_order.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.cpp")
target_include_directories(uno_core PUBLIC include)
target_compile_features(uno_core PUBLIC cxx_std_23)
uno_set_warnings(uno_core)
