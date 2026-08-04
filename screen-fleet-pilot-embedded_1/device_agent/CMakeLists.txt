cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

project(screen-fleet-pilot-embedded-device-agent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(JSON_C_ROOT /home/build/json_build)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()


# 根据 project(VERSION 1.0.0) 生成 includes/version.h 到 build 目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_BINARY_DIR}/sfp_common
)

aux_source_directory(sources SOURCES)

add_executable(screen-fleet-pilot-embedded-device-agent
    ${SOURCES}
)

target_include_directories(screen-fleet-pilot-embedded-device-agent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_BINARY_DIR}/generated
        ${JSON_C_ROOT}/include
)

target_compile_options(screen-fleet-pilot-embedded-device-agent
    PRIVATE
        -Wall
        -Wextra
)

target_compile_definitions(screen-fleet-pilot-embedded-device-agent
    PRIVATE
        LINUX
)

target_link_libraries(screen-fleet-pilot-embedded-device-agent
    PRIVATE
        sfp_common
        ${JSON_C_ROOT}/lib/libjson-c.a
)