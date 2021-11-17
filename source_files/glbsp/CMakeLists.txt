project(
  edge_glbsp
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-O2 -ffast-math -fno-strict-aliasing -DINLINE_G=inline")

add_library(
  edge_glbsp
  analyze.cc
  blockmap.cc
  glbsp.cc
  level.cc
  node.cc
  reject.cc
  seg.cc
  system.cc
  util.cc
  wad.cc
)