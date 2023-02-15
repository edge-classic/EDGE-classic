cmake_minimum_required(VERSION 3.2.2)

project(
  crsid
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_CC_FLAGS "-O2 -Wall")

add_library(
  crsid
  libcRSID.c
)
