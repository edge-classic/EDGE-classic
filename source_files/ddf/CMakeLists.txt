##########################################
# ddf
##########################################

add_library(
  edge_ddf
  anim.cc
  attack.cc
  boom.cc
  colormap.cc
  flat.cc
  font.cc
  game.cc
  image.cc
  language.cc
  level.cc
  line.cc
  main.cc
  movie.cc
  playlist.cc
  sector.cc
  sfx.cc
  states.cc
  style.cc
  switch.cc
  thing.cc
  wadfixes.cc
  weapon.cc
)

target_include_directories(edge_ddf PRIVATE ${EDGE_LIBRARY_DIR}/almostequals)
target_include_directories(edge_ddf PRIVATE ../edge)
target_include_directories(edge_ddf PRIVATE ../epi)

target_compile_options(edge_ddf PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)
