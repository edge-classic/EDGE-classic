project(
  timidity
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-DINLINE_G=inline")

add_library(
  timidity
  common.cc
  instrum.cc
  loadmidi.cc
  mix.cc
  playmidi.cc
  resample.cc
  tables.cc
  timidity.cc
)

target_include_directories(timidity PRIVATE ../epi)