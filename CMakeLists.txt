cmake_minimum_required(VERSION 3.20)

project(sockets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------------
# Descarga e integración automática de Asio (Standalone)
# -------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-36-0 # Versión estable de Asio
)
FetchContent_MakeAvailable(asio)

# -------------------------------------------------------------------
# Configuración del ejecutable principal
# -------------------------------------------------------------------
add_executable(sockets
    src/main.cpp
    lib/net/socket/socket.cpp
    lib/net/http/server.cpp
    lib/net/http/response.cpp
    lib/net/http/parser.cpp
    lib/net/http/event_loop_win.cpp
    lib/net/http/event_loop_linux.cpp
)

target_include_directories(sockets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Vincular Asio y los hilos del sistema requeridos por la librería
find_package(Threads REQUIRED)
target_link_libraries(sockets PRIVATE asio Threads::Threads)

if (WIN32)
    # ws2_32 y mswsock siguen siendo necesarios en Windows para Asio
    target_link_libraries(sockets PRIVATE ws2_32 mswsock)
endif()

# -------------------------------------------------------------------
# Configuración de las pruebas (Tests)
# -------------------------------------------------------------------
add_executable(sockets_tests
    tests/main.cpp
    tests/http/parser_content_length_tests.cpp
    tests/http/parser_keep_alive_tests.cpp
    tests/http/parser_chunked_tests.cpp
    lib/net/http/parser.cpp
)

target_include_directories(sockets_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# También vinculamos Asio a los tests por si tus parsers o utilidades lo requieren
target_link_libraries(sockets_tests PRIVATE asio Threads::Threads)

if (WIN32)
    target_link_libraries(sockets_tests PRIVATE ws2_32 mswsock)
endif()