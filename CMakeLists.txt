# Minimum CMake version required by this project.
cmake_minimum_required(VERSION 3.16)

# Project definition and language settings.
project(net LANGUAGES CXX)

# Enforce modern C++ usage across all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for tooling (clang-tidy/VS Code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: provide handy targets for formatting and static analysis
find_program(CLANG_FORMAT_EXE NAMES clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

# Dependencies
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

file(GLOB_RECURSE ALL_CXX_HEADERS
	${CMAKE_SOURCE_DIR}/include/*.h
	${CMAKE_SOURCE_DIR}/include/*.hh
	${CMAKE_SOURCE_DIR}/include/*.hpp
	${CMAKE_SOURCE_DIR}/include/*.hxx)
file(GLOB_RECURSE ALL_CXX_SOURCES
	${CMAKE_SOURCE_DIR}/src/*.c
	${CMAKE_SOURCE_DIR}/src/*.cc
	${CMAKE_SOURCE_DIR}/src/*.cpp
	${CMAKE_SOURCE_DIR}/src/*.cxx
	${CMAKE_SOURCE_DIR}/*.c
	${CMAKE_SOURCE_DIR}/*.cc
	${CMAKE_SOURCE_DIR}/*.cpp
	${CMAKE_SOURCE_DIR}/*.cxx)

if(CLANG_FORMAT_EXE)
	add_custom_target(format
		COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_HEADERS} ${ALL_CXX_SOURCES}
		COMMENT "Formatting C/C++ sources with clang-format")
endif()

# tidy 目标在定义完 net_core 之后再创建

# Core static library
add_library(net_core STATIC
	src/Socket.cpp
	src/Log.cpp
	src/Channel.cpp
	src/EventLoop.cpp
	src/EpollPoller.cpp
	src/Acceptor.cpp
	src/TcpServer.cpp
	src/TcpConnection.cpp
)
target_include_directories(net_core PUBLIC include)
target_compile_options(net_core PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(net_core
	PUBLIC
		spdlog::spdlog
		fmt::fmt
)
target_compile_definitions(net_core
	PRIVATE
		SERVER_LOG_BASE="${CMAKE_SOURCE_DIR}/Log"
)

if(CLANG_TIDY_EXE)
	# 仅对核心库源码运行 tidy，排除 old/ 与 demo/test 以减少干扰
	get_target_property(NET_CORE_SRCS net_core SOURCES)
	add_custom_target(tidy
		COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${NET_CORE_SRCS}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Running clang-tidy on net_core sources")
endif()

# Demos link against core
add_executable(socket_server_demo
	test/socket_server_demo.cpp
)
target_link_libraries(socket_server_demo PRIVATE net_core)
target_compile_options(socket_server_demo PRIVATE -Wall -Wextra -pedantic)

add_executable(socket_client_demo
	test/socket_client_demo.cpp
)
target_link_libraries(socket_client_demo PRIVATE net_core)
target_compile_options(socket_client_demo PRIVATE -Wall -Wextra -pedantic)

# Minimal build/link test
add_executable(core_build_test
	test/core_build_test.cpp
)
target_link_libraries(core_build_test PRIVATE net_core)
target_compile_options(core_build_test PRIVATE -Wall -Wextra -pedantic)

add_executable(tcp_server_demo
	test/tcp_server_demo.cpp
)
target_link_libraries(tcp_server_demo PRIVATE net_core)
target_compile_options(tcp_server_demo PRIVATE -Wall -Wextra -pedantic)
