##########################################
# epi
##########################################

add_library(
  edge_epi
  file_sub.cc
  file.cc
  file_memory.cc
  filesystem.cc
  image_data.cc
  image_blur.cc
  image_hq2x.cc
  image_funcs.cc
  math_angle.cc
  math_color.cc
  math_crc.cc
  math_md5.cc
  path.cc
  sound_data.cc
  sound_gather.cc
  sound_types.cc
  str_compare.cc
  str_lexer.cc
  str_util.cc
)

target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/almostequals)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/stb)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/libgme/gme)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/m4p)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/superfasthash)
if(MSVC OR WIN32_CLANG)
  target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_compile_options(edge_epi PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)
