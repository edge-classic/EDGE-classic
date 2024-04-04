##########################################
# edge
##########################################

set (EDGE_SOURCE_FILES
  i_movie.cc
  i_ctrl.cc
  i_video.cc
  i_sound.cc
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
  im_filter.cc
  im_data.cc
  im_funcs.cc
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
  r_state.cc
  r_colormap.cc
  r_modes.cc
  r_mdl.cc
  r_md2.cc
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
  s_m4p.cc
  s_opl.cc
  s_rad.cc
  s_sid.cc
  s_wav.cc
  s_flac.cc
  sv_chunk.cc
  sv_glob.cc
  sv_level.cc
  sv_load.cc
  sv_main.cc
  sv_misc.cc
  sv_mobj.cc
  sv_play.cc
  sv_save.cc
  snd_data.cc
  snd_gather.cc
  snd_types.cc
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
  script/lua_debugger.cc
  script/compat/lua_vm.cc
  script/compat/lua_compat.cc
  script/compat/lua_core.cc
  script/compat/lua_game.cc
  script/compat/lua_player.cc
  script/compat/lua_hud.cc)

if (EMSCRIPTEN)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_web.cc)
else()
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_main.cc)
endif()

set (EDGE_LINK_LIBRARIES
  edge_ajbsp  
  edge_ddf
  edge_deh
  edge_epi
  ${SDL2_LIBRARIES}
  crsid
  fluidlite
  libRAD
  m4p
  miniz
  edge_coal
  lua
  utf8proc
)

if (WIN32)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} w32_res.rc)
endif()

if (EDGE_PROFILING)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} TracyClient)
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

target_include_directories(edge-classic PRIVATE ./)

if (NOT EDGE_GL_ES2)
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/glad/include/glad)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} glad ${OPENGL_LIBRARIES})
else()
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/gl4es/include ${EDGE_LIBRARY_DIR}/gl4es/include/GL)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} gl4es)
endif()

target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/almostequals)
target_include_directories(edge-classic PRIVATE ../ajbsp)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/crsid)
target_include_directories(edge-classic PRIVATE ../ddf)
target_include_directories(edge-classic PRIVATE ../dehacked)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/dr_libs)
target_include_directories(edge-classic PRIVATE ../epi)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/fluidlite/include)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/hmm)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/libRAD)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/m4p)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/minivorbis)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/miniz)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/pl_mpeg)
target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/stb)
if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_link_libraries(edge-classic PRIVATE ${EDGE_LINK_LIBRARIES})

target_compile_options(edge-classic PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNINGS}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNINGS}>
)

set(COPY_FILES "")

if (NOT EMSCRIPTEN)

  set (DEST_DIR "${CMAKE_SOURCE_DIR}")
  list(APPEND COPY_FILES "$<TARGET_FILE:edge-classic>")

else()

  set (SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/source_files/edge")
  set (DEST_DIR "${CMAKE_SOURCE_DIR}/web/site")

  target_link_options(edge-classic PRIVATE "--pre-js=${DEST_DIR}/edge-classic-data.js")

  list(APPEND COPY_FILES "${SOURCE_DIR}/edge-classic.js" "${SOURCE_DIR}/edge-classic.wasm" "${SOURCE_DIR}/edge-classic.wasm.map")

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

# Copy appropriate SDL2.dll to local install directory when built with MSVC/Clang
if (WIN32)
  if(MSVC OR CLANG)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/x64/SDL2.dll")
    else ()
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/x86/SDL2.dll")
    endif ()
  endif()
endif()

if (COPY_FILES)
  add_custom_command( TARGET edge-classic POST_BUILD COMMAND_EXPAND_LISTS COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COPY_FILES} ${DEST_DIR})
endif()

