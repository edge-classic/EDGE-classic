project(
  edge_ddf
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-O2 -ffast-math -fno-strict-aliasing -DINLINE_G=inline -Wall")

add_library(
  edge_ddf
  anim.cc
  attack.cc
  boom.cc
  colormap.cc
  font.cc
  game.cc
  image.cc
  language.cc
  level.cc
  line.cc
  main.cc
  playlist.cc
  sector.cc
  sfx.cc
  states.cc
  style.cc
  switch.cc
  thing.cc
  weapon.cc
)

target_include_directories(edge_ddf PRIVATE ../edge)
target_include_directories(edge_ddf PRIVATE ../epi)