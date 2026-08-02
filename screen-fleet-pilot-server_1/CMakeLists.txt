cmake_minimum_required(VERSION 3.16)
project(screen-fleet-pilot-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(screen-fleet-pilot-server 
    sources/main.cpp
)

target_include_directories(screen-fleet-pilot-server 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include 
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