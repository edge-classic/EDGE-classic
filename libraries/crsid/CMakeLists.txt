##########################################
# crsid
##########################################

add_library(
  crsid
  libcRSID.c
)

target_include_directories(crsid PUBLIC ./)