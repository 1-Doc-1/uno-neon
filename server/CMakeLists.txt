cmake_minimum_required(VERSION 3.28...4.3)

project(uno_neon
    VERSION 0.1.0
    DESCRIPTION "Authoritative UNO Néon game server"
    LANGUAGES CXX)

option(UNO_BUILD_TESTS "Build the Catch2 test suite" ON)
option(UNO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(UNO_SANITIZERS "Instrument every target with sanitizers" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules in this project: skip the dependency scan (faster builds, quieter MSVC output).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# Dependency rule (hexagonal architecture): net -> app -> core, never the reverse.
add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(net)

find_package(spdlog CONFIG REQUIRED)

add_executable(uno_server main.cpp)
target_link_libraries(uno_server PRIVATE uno_core uno_app uno_net spdlog::spdlog)
uno_set_warnings(uno_server)

if(UNO_BUILD_TESTS)
    enable_testing()
    # Test doubles (SeededRandomSource): never linked into the production binary (SPEC §7.4).
    add_subdirectory(testing)
    add_subdirectory(tests)
endif()
