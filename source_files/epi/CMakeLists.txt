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
  math_color.cc
  math_crc.cc
  math_md5.cc
  str_compare.cc
  str_lexer.cc
  str_util.cc
)

target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/almostequals)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/stb)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/libgrapheme)
target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/superfasthash)
if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_compile_options(edge_epi PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)
