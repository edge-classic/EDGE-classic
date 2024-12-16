##########################################
# epi
##########################################

add_library(
  edge_epi
  epi_color.cc
  epi_crc.cc
  epi_ename.cc
  epi_file.cc
  epi_filesystem.cc
  epi_lexer.cc
  epi_md5.cc
  epi_str_compare.cc
  epi_str_util.cc
  epi_scanner.cpp
)

target_link_libraries(edge_epi PRIVATE almostequals HandmadeMath stb xxhash)

if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(edge_epi PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_include_directories(edge_epi PUBLIC ./)

target_compile_options(edge_epi PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)
