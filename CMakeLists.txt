cmake_minimum_required(VERSION 3.10)

project(m0rtis-protocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(m0rtis_proto INTERFACE)
target_include_directories(m0rtis_proto INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
add_subdirectory(m0rtis_hub)
add_subdirectory(m0rtis_node)
