##########################################
# libvwad
##########################################

add_library(
  libvwad
  vwadvfs.c
)

target_include_directories(libvwad PUBLIC ./)