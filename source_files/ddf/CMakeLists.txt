##########################################
# ddf
##########################################

add_library(
  ddf
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
  ddf_reverb.cc
  ddf_sector.cc
  ddf_sfx.cc
  ddf_states.cc
  ddf_style.cc
  ddf_switch.cc
  ddf_thing.cc
  ddf_wadfixes.cc
  ddf_weapon.cc
)

target_link_libraries(ddf PRIVATE almostequals epi HandmadeMath stb)

target_include_directories(ddf PRIVATE ../edge ../../libraries/miniaudio)

target_include_directories(ddf PUBLIC ./)

target_compile_options(ddf PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)