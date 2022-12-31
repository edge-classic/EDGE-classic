cmake_minimum_required(VERSION 3.2.2)

project(
  miniz
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_CC_FLAGS "-O2 -Wall")
if(WIN32 AND NOT MSYS AND NOT MINGW)
  set(CMAKE_CC_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CC_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_library(
  miniz
  miniz.c 
  mini_gzip.c)
