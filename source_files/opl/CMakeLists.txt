project(
  opl
  LANGUAGES CXX
  VERSION 0.1.0
)

add_library(opl
  midifile.cc
  opl3.cc
)
