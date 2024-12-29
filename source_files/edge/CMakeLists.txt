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
  r_backend.cc
  r_occlude.cc
  r_things.cc
  r_units.cc
  r_wipe.cc
  r_misc.cc
  r_sky.cc  
  r_colormap.cc
  r_modes.cc
  r_image.cc
  r_doomtex.cc
  r_texgl.cc
  s_blit.cc
  s_cache.cc
  s_sound.cc
  s_music.cc
  s_ogg.cc
  s_emidi.cc
  s_tsf.cc
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
  script/lua_debugger.cc
  script/compat/lua_vm.cc
  script/compat/lua_compat.cc
  script/compat/lua_core.cc
  script/compat/lua_game.cc
  script/compat/lua_player.cc
  script/compat/lua_hud.cc)
  
set (EDGE_GL_SOURCE_FILES
  render/gl/gl_backend.cc
  render/gl/gl_md2.cc
  render/gl/gl_mdl.cc
  render/gl/gl_sky.cc
  render/gl/gl_state.cc
  render/gl/gl_units.cc
)

set (EDGE_SOKOL_SOURCE_FILES
  render/sokol/sokol_backend.cc
  render/sokol/sokol_gl.cc  
  render/sokol/sokol_images.cc
  render/sokol/sokol_md2.cc
  render/sokol/sokol_mdl.cc
  render/sokol/sokol_pipeline.cc
  render/sokol/sokol_sky.cc
  render/sokol/sokol_state.cc
  render/sokol/sokol_units.cc
)

if (NOT EDGE_SOKOL)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} ${EDGE_GL_SOURCE_FILES})
else()

  if (EDGE_SOKOL_D3D11)
    set (EDGE_SOKOL_SOURCE_FILES ${EDGE_SOKOL_SOURCE_FILES} render/sokol/sokol_d3d11.cc)
  endif()

  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} ${EDGE_SOKOL_SOURCE_FILES})

endif()

if (EDGE_COAL_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} vm_coal.cc vm_hud.cc vm_player.cc)
endif()

if (EDGE_DEHACKED_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} l_deh.cc)
endif()

if (EDGE_DOOM_SFX_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_doom.cc)
endif()

if (EDGE_FLAC_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_flac.cc)
endif()

if (EDGE_IMF_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_imf.cc)
endif()

if (EDGE_MP3_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_mp3.cc)
endif()

if (EDGE_OGG_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_ogg.cc)
endif()

if (EDGE_RAD_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_rad.cc)
endif()

if (EDGE_SID_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_sid.cc)
endif()

if (EDGE_TRACKER_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_m4p.cc)
endif()

if (EDGE_WAV_SUPPORT)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} s_wav.cc)
endif()

if (EMSCRIPTEN)
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_web.cc)
else()
  set (EDGE_SOURCE_FILES ${EDGE_SOURCE_FILES} i_main.cc)
endif()

set (EDGE_LINK_LIBRARIES
  edge_ajbsp
  edge_ddf
  edge_epi
  edge_tracy
  ${SDL2_LIBRARIES}
  almostequals
  dr_libs
  HandmadeMath
  libemidi
  lua
  miniz
  pl_mpeg
  prns
  stb
  tsf
  xxhash
)

if (EDGE_COAL_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} edge_coal)
endif()

if (EDGE_DEHACKED_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} edge_deh)
endif()

if (EDGE_IMF_SUPPORT OR EDGE_RAD_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} libRAD)
endif()

if (EDGE_SID_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} crsid)
endif()

if (EDGE_TRACKER_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} m4p)
endif()

if (EDGE_VWAD_SUPPORT)
  set (EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} libvwad)
endif()

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

if (EDGE_SOKOL)
  set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} sokol imgui)
elseif (NOT EDGE_GL_ES2)
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/glad/include/glad)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} glad ${OPENGL_LIBRARIES})
else()
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/gl4es/include ${EDGE_LIBRARY_DIR}/gl4es/include/GL)
	set(EDGE_LINK_LIBRARIES ${EDGE_LINK_LIBRARIES} gl4es)
endif()

if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/include)
endif()

target_include_directories(edge-classic PRIVATE ./)

target_link_libraries(edge-classic PRIVATE ${EDGE_LINK_LIBRARIES})

target_compile_options(edge-classic PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:${EDGE_WARNING_LEVEL}>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${EDGE_WARNING_LEVEL}>
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

# Copy appropriate SDL2.dll to local install directory when built with MSVC/Clang or MSYS2
if (WIN32)
  if(MSVC OR CLANG)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/x64/SDL2.dll")
    else ()
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/lib/x86/SDL2.dll")
    endif ()
  elseif(MSYS)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      list(APPEND COPY_FILES "/mingw64/bin/SDL2.dll")
    else ()
      list(APPEND COPY_FILES "/mingw32/bin/SDL2.dll")
    endif ()
  endif()
endif()

if (COPY_FILES)
  add_custom_command( TARGET edge-classic POST_BUILD COMMAND_EXPAND_LISTS COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COPY_FILES} ${DEST_DIR})
endif()

