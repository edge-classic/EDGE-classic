project(
  edge_coal
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-DWIN32")

add_library(
  edge_coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)