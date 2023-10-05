cmake_minimum_required(VERSION 3.20)
include(FetchContent)
project(TeslaBLE
        VERSION 1.0.0
        DESCRIPTION "CPP Tesla BLE Library"
        LANGUAGES CXX C
)

set(MBEDTLS_AS_SUBPROJECT ON)
set(ENABLE_PROGRAMS OFF)
set(ENABLE_TESTING OFF)
set(BUILD_EXAMPLES ON)

FetchContent_Declare(
        nanopb
        GIT_REPOSITORY https://github.com/nanopb/nanopb.git
        GIT_TAG 0.4.7
        GIT_SHALLOW    TRUE
)
FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.4.1
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nanopb mbedtls)

set(CMAKE_MODULE_PATH ${nanopb_SOURCE_DIR}/extra)
find_package(Nanopb REQUIRED)

#include_directories(${NANOPB_INCLUDE_DIRS})
# disabled because of platformio library
#nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS src/vcsec.proto)

add_library(TeslaBLE
        STATIC
        src/client.cpp
        src/utils.cpp
        src/vcsec.pb.c
        ${nanopb_SOURCE_DIR}/pb_decode.c
        ${nanopb_SOURCE_DIR}/pb_encode.c
        ${nanopb_SOURCE_DIR}/pb_common.c
)

target_include_directories(
        TeslaBLE
        PRIVATE
        src/
        ${NANOPB_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
        PUBLIC
        include/
        ${NANOPB_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(TeslaBLE PUBLIC mbedcrypto mbedtls mbedx509)
set_target_properties(TeslaBLE PROPERTIES PUBLIC_HEADER include/client.h)
set_target_properties(TeslaBLE PROPERTIES PUBLIC_HEADER include/vcsec.pb.h)