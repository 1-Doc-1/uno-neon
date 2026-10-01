# uno_net: adapters to the outside world (WebSocket, JSON codec, cryptographic randomness).
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(uno_net STATIC
    src/client_codec.cpp
    src/crypto_random_source.cpp
    src/crypto_runtime.cpp
    src/game_codec.cpp
    src/origin_policy.cpp
    src/server_codec.cpp
    src/server_message_sink.cpp
    src/system_clock.cpp
    src/websocket_server.cpp)
target_include_directories(uno_net PUBLIC include)
target_link_libraries(uno_net
    PUBLIC uno_app
    PRIVATE
        unofficial::uwebsockets::uwebsockets
        nlohmann_json::nlohmann_json
        unofficial-sodium::sodium
        spdlog::spdlog)
uno_set_warnings(uno_net)
