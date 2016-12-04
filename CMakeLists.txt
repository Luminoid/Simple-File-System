cmake_minimum_required(VERSION 3.6)
project(SimpleFileSystem)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(SOURCE_FILES fs.c disk.c)
add_executable(SimpleFileSystem ${SOURCE_FILES})