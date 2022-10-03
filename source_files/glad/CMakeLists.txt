cmake_minimum_required(VERSION 3.2.2)

project(
  glad
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_CC_FLAGS "-O2 -Wall")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CC_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CC_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_library(glad src/gl.c)

target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
