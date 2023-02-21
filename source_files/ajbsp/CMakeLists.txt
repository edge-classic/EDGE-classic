# AJBSP CMake Script
cmake_minimum_required(VERSION 3.2.2)

project(edge_ajbsp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-O2 -fno-strict-aliasing -fwrapv -fno-rtti")
if(WIN32 AND NOT MSYS AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -Wall")
endif()

add_library(edge_ajbsp
	bsp_level.cc
	bsp_misc.cc
	bsp_node.cc
	bsp_utility.cc
	bsp_wad.cc
)

target_include_directories(edge_ajbsp PRIVATE ../epi)
target_include_directories(edge_ajbsp PRIVATE ../miniz)