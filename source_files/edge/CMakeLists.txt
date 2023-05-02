##########################################
# edge
##########################################

set (EDGE_SOURCE_FILES  
  i_ctrl.cc
  i_video.cc
  i_sound.cc
  i_net.cc
  am_map.cc
  bot_nav.cc
  bot_think.cc
  con_con.cc
  con_main.cc
  con_var.cc
  e_input.cc
  e_main.cc
  e_player.cc
  f_finale.cc
  f_interm.cc
  g_game.cc
  hu_draw.cc
  hu_font.cc
  hu_stuff.cc
  hu_style.cc
  i_system.cc
  l_ajbsp.cc
  l_deh.cc
  m_argv.cc
  m_bbox.cc
  m_cheat.cc
  m_math.cc
  m_menu.cc
  m_misc.cc
  m_option.cc
  m_netgame.cc
  m_random.cc
  n_bcast.cc
  n_reliable.cc
  n_network.cc
  p_action.cc
  p_blockmap.cc
  p_enemy.cc
  p_inter.cc
  p_lights.cc
  p_map.cc
  p_maputl.cc
  p_mobj.cc
  p_plane.cc
  p_setup.cc
  p_sight.cc
  p_spec.cc
  p_switch.cc
  p_tick.cc
  p_user.cc
  p_forces.cc
  p_telept.cc
  p_umapinfo.cc
  p_weapon.cc
  rad_act.cc
  rad_pars.cc
  rad_trig.cc
  r_draw.cc
  r_shader.cc
  r_render.cc
  r_effects.cc
  r_main.cc
  r_occlude.cc
  r_things.cc
  r_units.cc
  r_wipe.cc
  r_misc.cc
  r_sky.cc
  r_colormap.cc
  r_modes.cc
  r_mdl.cc
  r_md2.cc
  r_voxel.cc
  r_image.cc
  r_doomtex.cc
  r_texgl.cc
  s_blit.cc
  s_cache.cc
  s_sound.cc
  s_mp3.cc
  s_music.cc
  s_ogg.cc
  s_fluid.cc
  s_gme.cc
  s_m4p.cc
  s_opl.cc
  s_sid.cc
  s_vgm.cc
  s_wav.cc
  s_flac.cc
  s_fmm.cc
  sv_chunk.cc
  sv_glob.cc
  sv_level.cc
  sv_load.cc
  sv_main.cc
  sv_misc.cc
  sv_mobj.cc
  sv_play.cc
  sv_save.cc
  w_files.cc
  w_flat.cc
  w_model.cc
  w_epk.cc
  w_sprite.cc
  w_texture.cc
  w_wad.cc
  vm_coal.cc
  vm_hud.cc
  vm_player.cc
)

if (EMSCRIPTEN)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_web.cc)
else()
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_main.cc)
endif()

set (EDGE_LINK_LIBRARIES
  edge_ajbsp
  edge_coal
  edge_ddf
  edge_deh
  edge_epi
  ${SDL2_LIBRARIES}
  crsid
  ec_voxelib
  fmmidi
  fluidlite
  gme
  m4p
  miniz
  libymfm
  ymfmidi
)

if (WIN32)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} w32_res.rc)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} wsock32)
endif()

add_executable(
  edge-classic
  ${EDGE_SOURCE_FILES}
)

if(WIN32)
  target_compile_definitions(edge-classic PRIVATE WIN32)
else()
  target_compile_definitions(edge-classic PRIVATE UNIX)
endif()

if (NOT EDGE_GL_ES2)
  target_include_directories(edge-classic PRIVATE ../glad/include/glad)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} glad ${OPENGL_LIBRARIES})
else()	
  target_include_directories(edge-classic PRIVATE ../gl4es/include ../gl4es/include/GL)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} gl4es)
endif()


target_include_directories(edge-classic PRIVATE ../ajbsp)
target_include_directories(edge-classic PRIVATE ../coal)
target_include_directories(edge-classic PRIVATE ../crsid)
target_include_directories(edge-classic PRIVATE ../ddf)
target_include_directories(edge-classic PRIVATE ../dehacked)
target_include_directories(edge-classic PRIVATE ../dr_libs)
target_include_directories(edge-classic PRIVATE ../ec_voxelib)
target_include_directories(edge-classic PRIVATE ../epi)
target_include_directories(edge-classic PRIVATE ../fluidlite/include)
target_include_directories(edge-classic PRIVATE ../fmmidi)
target_include_directories(edge-classic PRIVATE ../libgme/gme)
target_include_directories(edge-classic PRIVATE ../libymfm)
target_include_directories(edge-classic PRIVATE ../m4p)
target_include_directories(edge-classic PRIVATE ../minimp3)
target_include_directories(edge-classic PRIVATE ../minivorbis)
target_include_directories(edge-classic PRIVATE ../miniz)
target_include_directories(edge-classic PRIVATE ../stb)
target_include_directories(edge-classic PRIVATE ../ymfmidi)
if(MSVC OR WIN32_CLANG)
  target_include_directories(edge-classic PRIVATE ../sdl2/include)
endif()

target_link_libraries(edge-classic PRIVATE ${EDGE_LINK_LIBRARIES})

target_compile_options(edge-classic PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

#Assemble EPKs
if (BUILD_EPKS)
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge-defs.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_defs"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/blasphemer.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/blasphemer"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/doom.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/doom"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/doom1.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/doom1"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/doom2.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/doom2"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/freedoom1.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/freedoom1"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/freedoom2.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/freedoom2"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/hacx.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/hacx"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/harmony.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/harmony"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/heretic.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/heretic"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/plutonia.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/plutonia"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/rekkr.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/rekkr"
    )
  add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/edge_base/tnt.epk" --format=zip "."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base/tnt"
    )
endif()
  

if (NOT EMSCRIPTEN)
  
  set (DEST_DIR "${CMAKE_SOURCE_DIR}")

  # Copies executable to local install directory after build
  add_custom_command(
  TARGET edge-classic
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:edge-classic>"
      "${DEST_DIR}"
  )
else()
  set (SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/source_files/edge")
  set (DEST_DIR "${CMAKE_SOURCE_DIR}/web/site")

  target_link_options(edge-classic PRIVATE "--pre-js=${DEST_DIR}/edge-classic-data.js")
  
  add_custom_command(
  TARGET edge-classic
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy 
      "${SOURCE_DIR}/edge-classic.js"
      "${SOURCE_DIR}/edge-classic.wasm"
      "${SOURCE_DIR}/edge-classic.wasm.map"      
      "${DEST_DIR}"
  )
  
  add_custom_command(
  TARGET edge-classic
  PRE_BUILD
  COMMAND python ${EMPACKAGER} ${DEST_DIR}/edge-classic.data --preload 
    ${CMAKE_SOURCE_DIR}/web/preload@/ 
    ${CMAKE_SOURCE_DIR}/edge_base@/edge_base
    ${CMAKE_SOURCE_DIR}/edge_defs@/edge_defs
    ${CMAKE_SOURCE_DIR}/soundfont@/soundfont
    --js-output=${DEST_DIR}/edge-classic-data.js 
    --use-preload-cache
    --no-node
    --lz4
  )  

endif()

if (WIN32)
  # Copy appropriate SDL2.dll to local install directory when built with MSVC
  if(MSVC)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/source_files/sdl2/lib/x64/SDL2.dll"
        "${DEST_DIR}"
    )
    else ()
      add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/source_files/sdl2/lib/x86/SDL2.dll"
        "${DEST_DIR}"
    )
    endif ()
  # Copy appropriate SDL2.dll to local install directory when built with MinGW
  elseif(MinGW)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
    add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "/mingw64/bin/SDL2.dll"
        "${DEST_DIR}"
    )
    else ()
    add_custom_command(
    TARGET edge-classic
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "/mingw32/bin/SDL2.dll"
        "${DEST_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy "/mingw32/bin/libgcc_s_dw2-1.dll"
        "${DEST_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy "/mingw32/bin/libstdc++-6.dll"
        "${DEST_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy "/mingw32/bin/libwinpthread-1.dll"
        "${DEST_DIR}"
    )
    endif ()
  endif()

endif()