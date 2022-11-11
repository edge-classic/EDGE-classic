project(
  libymfm
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-O2 -Wall")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /EHs")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-strict-aliasing")
endif()

add_library(
  libymfm
  ymfm_adpcm.cpp
  ymfm_interface.cpp
  ymfm_misc.cpp
  ymfm_opl.cpp
  ymfm_opm.cpp
  ymfm_opn.cpp
  ymfm_opq.cpp
  ymfm_opz.cpp
  ymfm_pcm.cpp
  ymfm_ssg.cpp
)