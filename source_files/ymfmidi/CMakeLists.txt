project(
  ymfmidi
  LANGUAGES CXX
  VERSION 0.1.0
)

add_library(ymfmidi
  patches.cpp
  patchnames.cpp
  ymf_player.cpp
)

target_include_directories(ymfmidi PRIVATE ../libymfm)