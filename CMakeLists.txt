cmake_minimum_required(VERSION 3.10)

project(cpp_thread_pool CXX)

set(CXX_STANDARD 17)

include_directories(src)

add_executable(test src/main.cpp)