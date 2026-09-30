# uno_app: rooms, sessions and use cases. Depends on uno_core only.
add_library(uno_app STATIC src/server_config.cpp)
target_include_directories(uno_app PUBLIC include)
target_link_libraries(uno_app PUBLIC uno_core)
uno_set_warnings(uno_app)
