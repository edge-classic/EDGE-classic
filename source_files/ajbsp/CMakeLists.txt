##########################################
# AJBSP
##########################################

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwrapv")
endif()

add_library(edge_ajbsp
	bsp_level.cc
	bsp_misc.cc
	bsp_node.cc
	bsp_utility.cc
	bsp_wad.cc
)

target_link_libraries(edge_ajbsp PRIVATE almostequals edge_epi HandmadeMath miniz stb)

target_include_directories(edge_ajbsp PUBLIC ./)

target_compile_options(edge_ajbsp PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
