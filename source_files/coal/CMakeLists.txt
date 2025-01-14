##########################################
# coal
##########################################

add_library(
  coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)

target_link_libraries(coal PRIVATE almostequals epi stb)

target_include_directories(coal PUBLIC ./)

target_compile_options(coal PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
