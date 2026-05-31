cmake_minimum_required(VERSION 3.20)

project(sockets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sockets
    src/main.cpp
    lib/net/socket/socket.cpp
    lib/net/http/server.cpp
    lib/net/http/response.cpp
    lib/net/http/parser.cpp
    lib/net/http/event_loop.cpp
)

target_include_directories(sockets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(sockets PRIVATE ws2_32 mswsock)
endif()

add_executable(sockets_tests
    tests/main.cpp
    tests/http/parser_content_length_tests.cpp
    tests/http/parser_keep_alive_tests.cpp
    tests/http/parser_chunked_tests.cpp
    lib/net/http/parser.cpp
)

target_include_directories(sockets_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
