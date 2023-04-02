##########################################
# coal
##########################################

add_library(
  edge_coal
  c_compile.cc
  c_execute.cc
  c_memory.cc
)

target_compile_options(edge_coal PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
