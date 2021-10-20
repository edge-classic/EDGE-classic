project(
  edge_coal
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-O2 -ffast-math -fno-strict-aliasing -DINLINE_G=inline -Wall")

add_library(
  edge_coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)