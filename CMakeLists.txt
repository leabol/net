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

# Build the client executable and reuse the same include directory.
add_executable(client
	clent.cpp
	src/SocketBase.cpp
	src/ClientSocket.cpp
	src/TcpStream.cpp
)
target_include_directories(client PRIVATE include)
target_compile_options(client PRIVATE -Wall -Wextra -pedantic)

# Performance test for the epoll poller.
add_executable(epoll_perf_test
	tests/epoll_perf_test.cpp
	src/EpollPoller.cpp
	src/SocketBase.cpp
)
target_include_directories(epoll_perf_test PRIVATE include)
target_compile_options(epoll_perf_test PRIVATE -Wall -Wextra -pedantic)
