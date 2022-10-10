project(
  edge_epi
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
  edge_epi
  arrays.cc
  exe_path.cc
  file_sub.cc
  file.cc
  filesystem.cc
  image_data.cc
  image_hq2x.cc
  image_funcs.cc
  math_angle.cc
  math_bbox.cc
  math_crc.cc
  math_md5.cc
  math_oddity.cc
  math_vector.cc
  mus_2_midi.cc
  path.cc
  sound_data.cc
  sound_gather.cc
  sound_types.cc
  str_compare.cc
  str_lexer.cc
  str_util.cc
)
target_include_directories(edge_epi PRIVATE ../whereami)
target_include_directories(edge_epi PRIVATE ../stb)
target_include_directories(edge_epi PRIVATE ../libgme/gme)
target_include_directories(edge_epi PRIVATE ../libmodplug/src)

target_link_libraries(edge_epi PUBLIC whereami)
