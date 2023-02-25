##########################################
# AJBSP
##########################################

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fwrapv -fno-rtti -fno-exceptions")
endif()

add_library(edge_ajbsp
	bsp_level.cc
	bsp_misc.cc
	bsp_node.cc
	bsp_utility.cc
	bsp_wad.cc
)

target_include_directories(edge_ajbsp PRIVATE ../epi)
target_include_directories(edge_ajbsp PRIVATE ../miniz)