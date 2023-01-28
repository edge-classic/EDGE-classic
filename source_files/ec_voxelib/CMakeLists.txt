project(
  ec_voxelib
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_FLAGS "-O2 -Wall")
if(WIN32 AND NOT MSYS AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_library(
  ec_voxelib
  ec_voxelib.cc
)
