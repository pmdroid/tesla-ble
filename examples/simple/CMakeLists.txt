cmake_minimum_required(VERSION 3.22)
project(simple)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../ TeslaBLE)
add_executable(simple main.cpp)
target_link_libraries(simple PRIVATE TeslaBLE)
