##########################################
# epi
##########################################

add_library(
  epi
  epi_color.cc
  epi_crc.cc
  epi_ename.cc
  epi_file.cc
  epi_filesystem.cc
  epi_md5.cc
  epi_str_compare.cc
  epi_str_util.cc
  epi_scanner.cpp
)

set (epi_LIBRARIES almostequals HandmadeMath stb xxhash)

if (NOT EMSCRIPTEN AND NOT MINGW)
  list(APPEND epi_LIBRARIES mimalloc)
endif()

target_link_libraries(epi PUBLIC ${epi_LIBRARIES})

if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(epi PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_include_directories(epi PUBLIC ./)

target_compile_options(epi PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
