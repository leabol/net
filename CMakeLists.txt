# Minimum CMake version required by this project.
cmake_minimum_required(VERSION 3.16)

# Project definition and language settings.
project(net LANGUAGES CXX)

# Enforce modern C++ usage across all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(socket_server_demo
	test/socket_server_demo.cpp
	src/Socket.cpp
	src/Log.cpp
)
target_include_directories(socket_server_demo PRIVATE include)
target_compile_options(socket_server_demo PRIVATE -Wall -Wextra -pedantic)

add_executable(socket_client_demo
	test/socket_client_demo.cpp
	src/Socket.cpp
	src/Log.cpp
)
target_include_directories(socket_client_demo PRIVATE include)
target_compile_options(socket_client_demo PRIVATE -Wall -Wextra -pedantic)