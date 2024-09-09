##########################################
# ddf
##########################################

add_library(
  edge_ddf
  ddf_anim.cc
  ddf_attack.cc
  ddf_boom.cc
  ddf_colormap.cc
  ddf_flat.cc
  ddf_font.cc
  ddf_game.cc
  ddf_image.cc
  ddf_language.cc
  ddf_level.cc
  ddf_line.cc
  ddf_main.cc
  ddf_movie.cc
  ddf_playlist.cc
  ddf_sector.cc
  ddf_sfx.cc
  ddf_states.cc
  ddf_style.cc
  ddf_switch.cc
  ddf_thing.cc
  ddf_wadfixes.cc
  ddf_weapon.cc
)

target_link_libraries(edge_ddf PRIVATE almostequals edge_epi HandmadeMath sokol_color)

target_include_directories(edge_ddf PRIVATE ../edge)

target_include_directories(edge_ddf PUBLIC ./)

target_compile_options(edge_ddf PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)