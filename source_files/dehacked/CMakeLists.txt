##########################################
# dehacked
##########################################

add_library(
  dehacked
  deh_ammo.cc
  deh_buffer.cc
  deh_field.cc
  deh_frames.cc
  deh_info.cc
  deh_main.cc
  deh_misc.cc
  deh_mobj.cc
  deh_music.cc
  deh_patch.cc
  deh_rscript.cc
  deh_sounds.cc
  deh_sprites.cc
  deh_system.cc
  deh_text.cc
  deh_things.cc
  deh_wad.cc
  deh_weapons.cc
)

target_link_libraries(dehacked PRIVATE ddf epi stb)

target_include_directories(dehacked PUBLIC ./)

target_compile_options(dehacked PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
)