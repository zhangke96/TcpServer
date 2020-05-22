cmake_minimum_required(VERSION 3.8)
project(TcpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -Wall)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
ADD_DEFINITIONS(-DTCP_SERVER_TEST)
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}")

add_executable(TcpServer TcpServer/TcpServer_test.cpp TcpServer/log.cpp TcpServer/tool.cpp TcpServer/TcpServer.cpp)
target_link_libraries(TcpServer Threads::Threads)
