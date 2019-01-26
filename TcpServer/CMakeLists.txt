cmake_minimum_required(VERSION 3.8)
project(TcpServer)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
ADD_DEFINITIONS(-DTCP_SERVER_TEST)

add_executable(TcpServer TcpServer_test.cpp log.cpp tool.cpp TcpServer.cpp)
target_link_libraries(TcpServer Threads::Threads)
