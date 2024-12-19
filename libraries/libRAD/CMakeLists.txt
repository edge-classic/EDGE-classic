##########################################
# libRAD
##########################################

add_library(
  libRAD
  opal.cpp
  radplay.cpp
)

target_include_directories(libRAD PUBLIC ./)