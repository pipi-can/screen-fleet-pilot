cmake_minimum_required(VERSION 3.16)
project(screen-fleet-pilot-common LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sfp_common STATIC
    sources/logmgr.cpp
)

target_include_directories(sfp_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)
