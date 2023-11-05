##########################################
# coal
##########################################

add_library(
  edge_coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)

target_include_directories(edge_coal PRIVATE ${EDGE_LIBRARY_DIR}/almostequals)

target_compile_options(edge_coal PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)
