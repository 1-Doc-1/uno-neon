# uno_app: rooms, sessions and use cases. Depends on uno_core only.
find_package(spdlog CONFIG REQUIRED)

add_library(uno_app STATIC
    src/application.cpp
    src/application_lifecycle.cpp
    src/bot_strategy.cpp
    src/id_generator.cpp
    src/nickname.cpp
    src/room.cpp
    src/room_repository.cpp
    src/room_settings.cpp
    src/server_config.cpp)
target_include_directories(uno_app PUBLIC include)
target_link_libraries(uno_app PUBLIC uno_core PRIVATE spdlog::spdlog)
uno_set_warnings(uno_app)
