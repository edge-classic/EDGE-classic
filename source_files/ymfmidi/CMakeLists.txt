project(
  ymfmidi
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_library(ymfmidi
  patches.cpp
  patchnames.cpp
  ymf_player.cpp
)

target_include_directories(ymfmidi PRIVATE ../libymfm)