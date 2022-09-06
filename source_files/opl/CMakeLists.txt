project(
  opl
  LANGUAGES CXX
  VERSION 0.1.0
)

add_library(opl
  genmidi.cc
  midifile.cc
  opl_player.cc
  opl3.cc
)
