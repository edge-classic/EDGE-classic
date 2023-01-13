project(
  fmmidi
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_library(fmmidi
  filter.cpp
  midisynth.cpp
)