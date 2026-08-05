cmake_minimum_required(VERSION 3.16)
project(screen-fleet-pilot-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
find_library(JSONC_STATIC libjson-c.a)


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
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_BINARY_DIR}/sfp_common
)

aux_source_directory(${PROJECT_SOURCE_DIR}/sources SOURCES)

add_executable(screen-fleet-pilot-server
    ${SOURCES}
)

target_include_directories(screen-fleet-pilot-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        ${CMAKE_BINARY_DIR}/generated
)

target_compile_options(screen-fleet-pilot-server 
    PRIVATE
        -Wall
        -Wextra
)

target_compile_definitions(screen-fleet-pilot-server 
    PRIVATE
        LINUX
)

target_link_libraries(screen-fleet-pilot-server
    PRIVATE
        sfp_common
        sqlite3
        ${JSONC_STATIC}
)