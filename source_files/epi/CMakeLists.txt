project(
  edge_epi
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-O2 -ffast-math -fno-strict-aliasing -DINLINE_G=inline -Wall")

add_library(
  edge_epi
  arrays.cc
  bytearray.cc
  epi_win32.cc
  exe_path.cc
  file_memory.cc
  file_sub.cc
  file.cc
  filesystem.cc
  image_data.cc
  image_hq2x.cc
  image_funcs.cc
  math_angle.cc
  math_bbox.cc
  math_color.cc
  math_crc.cc
  math_matrix.cc
  math_md5.cc
  math_oddity.cc
  math_random.cc
  math_vector.cc
  mersenne_twist.cc
  mus_2_midi.cc
  path.cc
  sound_data.cc
  sound_gather.cc
  sound_wav.cc
  str_format.cc
  timestamp.cc
  utility.cc
)

target_include_directories(edge_epi PRIVATE ../stb)