##########################################
# dehacked
##########################################

add_library(
  edge_deh
  deh_ammo.cc
  deh_buffer.cc
  deh_convert.cc
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
  deh_util.cc
  deh_wad.cc
  deh_weapons.cc
)

target_include_directories(edge_deh PRIVATE ../epi)
target_include_directories(edge_deh PRIVATE ../ddf)