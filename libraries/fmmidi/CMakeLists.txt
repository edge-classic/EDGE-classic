##########################################
# fmmidi
##########################################

add_library(fmmidi
  filter.cpp
  midisynth.cpp
)

target_include_directories(fmmidi PUBLIC ./)