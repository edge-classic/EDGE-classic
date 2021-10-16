project(
  edge_deh
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-DDEH_EDGE_PLUGIN")

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