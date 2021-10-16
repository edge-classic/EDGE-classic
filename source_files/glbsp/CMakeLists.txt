project(
  edge_glbsp
  LANGUAGES C
  VERSION 0.1.0
)

set(CMAKE_C_FLAGS "-DINLINE_G=inline -DGLBSP_PLUGIN")

add_library(
  edge_glbsp
  src/analyze.c
  src/blockmap.c
  src/glbsp.c
  src/level.c
  src/node.c
  src/reject.c
  src/seg.c
  src/system.c
  src/util.c
  src/wad.c
)