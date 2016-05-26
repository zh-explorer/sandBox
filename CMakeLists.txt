cmake_minimum_required(VERSION 3.4)
project(sandBox)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.c)
add_executable(sandBox ${SOURCE_FILES})