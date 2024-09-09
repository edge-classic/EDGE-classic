##########################################
# libRAD
##########################################

add_library(
  libRAD
  opal.cpp
  radplay.cpp
  patches.cpp
  radmidi.cpp
)

target_include_directories(libRAD PUBLIC ./)