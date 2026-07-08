cmake_minimum_required(VERSION 3.27)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project 
project(m0rtis)

# Library
add_library(m0rtis src/m0rtis.cpp)

target_include_directories(m0rtis PUBLIC include)

# Tests
enable_testing()

add_executable(test_main tests/test_main.cpp)
target_link_libraries(test_main PRIVATE m0rtis)
add_test(NAME test_main COMMAND test_main)
