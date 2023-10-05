cmake_minimum_required(VERSION 3.22)
project(load_key)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(../../ TeslaBLE)
add_executable(load_key main.cpp)
target_link_libraries(load_key PRIVATE TeslaBLE)
