project(
  edge_deh
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-O2 -DINLINE_G=inline -Wall -DDEH_EDGE_PLUGIN")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_library(
  edge_deh
  ammo.cc
  attacks.cc
  buffer.cc
  convert.cc
  frames.cc
  info.cc
  main.cc
  misc.cc
  mobj.cc
  patch.cc
  rscript.cc
  sounds.cc
  storage.cc
  system.cc
  text.cc
  things.cc
  util.cc
  wad.cc
  weapons.cc
)

target_include_directories(edge_deh PRIVATE ../epi)
target_include_directories(edge_deh PRIVATE ../ddf)
