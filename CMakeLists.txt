# Minimum CMake version required by this project.
cmake_minimum_required(VERSION 3.16)

# Project definition and language settings.
project(net LANGUAGES CXX)

# Enforce modern C++ usage across all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build the server executable and include shared headers.
add_executable(server
	src/server.cpp
	src/EventLoop.cpp
	src/EpollPoller.cpp
	src/SocketBase.cpp
	src/ServerSocket.cpp
	src/TcpStream.cpp
)
target_include_directories(server PRIVATE include)
target_compile_options(server PRIVATE -Wall -Wextra -pedantic)