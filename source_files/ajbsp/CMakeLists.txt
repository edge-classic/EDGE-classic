##########################################
# AJBSP
##########################################

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwrapv")
endif()

add_library(ajbsp
	bsp_level.cc
	bsp_misc.cc
	bsp_node.cc
	bsp_utility.cc
	bsp_wad.cc
)

target_link_libraries(ajbsp PRIVATE almostequals epi HandmadeMath miniz stb)

target_include_directories(ajbsp PUBLIC ./)

target_compile_options(ajbsp PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)
