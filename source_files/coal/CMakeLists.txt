##########################################
# coal
##########################################

add_library(
  edge_coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)

target_link_libraries(edge_coal PRIVATE almostequals edge_epi stb)

target_include_directories(edge_coal PUBLIC ./)

target_compile_options(edge_coal PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
