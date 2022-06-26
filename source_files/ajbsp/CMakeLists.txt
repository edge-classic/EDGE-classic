# AJBSP CMake Script
cmake_minimum_required(VERSION 3.2.2)

project(edge_ajbsp)

set(CMAKE_CXX_FLAGS "-O2 -std=c++03 -fno-strict-aliasing -fwrapv -fno-exceptions -fno-rtti")

add_library(
	edge_ajbsp
	ajbsp.cc
	bsp_level.cc
	bsp_node.cc
	bsp_util.cc
	lib_file.cc
	lib_udmf.cc
	lib_util.cc
	w_wad.cc
)