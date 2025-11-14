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

if(CLANG_TIDY_EXE)
	add_custom_target(tidy
		COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${ALL_CXX_SOURCES}
		COMMENT "Running clang-tidy on sources")
endif()

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
