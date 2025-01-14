##########################################
# opalmidi
##########################################

add_library(
  opalmidi
  opal.cpp
  opalmidi.cpp
  patches.cpp
)

target_include_directories(opalmidi PUBLIC ./)