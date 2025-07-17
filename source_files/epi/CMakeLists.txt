##########################################
# epi
##########################################

add_library(
  epi
  epi.cc
  epi_color.cc
  epi_crc.cc
  epi_file.cc
  epi_filesystem.cc
  epi_md5.cc
  epi_str_compare.cc
  epi_str_hash.cc
  epi_str_util.cc
  epi_scanner.cpp
)

set (EPI_LIBRARIES almostequals HandmadeMath stb)

target_link_libraries(epi PUBLIC ${EPI_LIBRARIES})

if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(epi PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/msvc/include)
elseif (MINGW)
  target_include_directories(epi SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/mingw/include)
endif()

target_include_directories(epi PUBLIC ./)

target_compile_options(epi PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
