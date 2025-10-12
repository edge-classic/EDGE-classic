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
  p_weapon.cc
  rad_act.cc
  rad_pars.cc
  rad_trig.cc
  r_bsp.cc
  r_draw.cc
  r_shader.cc
  r_render.cc
  r_effects.cc
  r_backend.cc
  r_mirror.cc
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
  s_flac.cc
  s_mp3.cc
  s_music.cc
  s_ogg.cc
  s_sound.cc
  s_wav.cc
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
  list (APPEND EDGE_SOURCE_FILES ${EDGE_GL_SOURCE_FILES})
else()
  if (EDGE_SOKOL_D3D11)
    list (APPEND EDGE_SOKOL_SOURCE_FILES render/sokol/sokol_d3d11.cc)
  endif()

  list (APPEND EDGE_SOURCE_FILES ${EDGE_SOKOL_SOURCE_FILES})
endif()

if (EDGE_CLASSIC)
  list (APPEND 
    EDGE_SOURCE_FILES
    l_deh.cc
    p_umapinfo.cc
    s_doom.cc
    s_m4p.cc
    s_midi_ec.cc
    s_sid.cc
    vm_coal.cc
    vm_hud.cc
    vm_player.cc    
  )
else()
  list (APPEND
    EDGE_SOURCE_FILES 
    s_midi.cc  
  )
endif()

if (EMSCRIPTEN)
  list (APPEND EDGE_SOURCE_FILES i_web.cc)
else()
  list (APPEND EDGE_SOURCE_FILES i_main.cc)
endif()

if (WIN32 OR MINGW)
  list (APPEND EDGE_SOURCE_FILES w32_res.rc)
endif()

add_executable(
  edge-classic
  ${EDGE_SOURCE_FILES}
)

if(WIN32 OR MINGW)
  target_compile_definitions(edge-classic PRIVATE WIN32)
else()
  target_compile_definitions(edge-classic PRIVATE UNIX)
endif()

if(WIN32 AND (MSVC OR CLANG))
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/msvc/include)
elseif (MINGW)
  target_include_directories(edge-classic SYSTEM PRIVATE ${EDGE_LIBRARY_DIR}/sdl2/mingw/include)
endif()

target_include_directories(edge-classic PRIVATE ./)

set (EDGE_LINK_LIBRARIES
  ajbsp
  ddf
  epi
  edge_tracy
  ${SDL2_LIBRARIES}
  almostequals
  fluidlite
  HandmadeMath
  lua
  miniaudio
  minivorbis
  miniz
  pl_mpeg
  prns
  stb
)

if (NOT EMSCRIPTEN OR (EMSCRIPTEN AND EDGE_WEB_MULTITHREADED))
  list (APPEND EDGE_LINK_LIBRARIES thread)
endif()

if (EDGE_CLASSIC)
  list (APPEND
    EDGE_LINK_LIBRARIES
    coal
    dehacked
    libcRSID
    m4p
    opalmidi
  )
endif()

if (EDGE_PROFILING)
  list (APPEND EDGE_LINK_LIBRARIES TracyClient)
endif()

if (EDGE_SOKOL)
  list(APPEND EDGE_LINK_LIBRARIES sokol)
  if (MINGW)
    if (EDGE_SOKOL_GL)
      list(APPEND EDGE_LINK_LIBRARIES winmm kernel32)
    else ()
      list(APPEND EDGE_LINK_LIBRARIES d3d11 winmm kernel32)
    endif()
  endif()
else ()
  target_include_directories(edge-classic PRIVATE ${EDGE_LIBRARY_DIR}/glad/include/glad)
	list(APPEND EDGE_LINK_LIBRARIES glad OpenGL::GL)
endif()

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
  COMMAND python3 ${EMPACKAGER} ${DEST_DIR}/edge-classic.data --preload
    ${CMAKE_SOURCE_DIR}/web/preload@/
    ${CMAKE_SOURCE_DIR}/crosshairs@/crosshairs
    ${CMAKE_SOURCE_DIR}/edge_base@/edge_base
    ${CMAKE_SOURCE_DIR}/edge_defs@/edge_defs
    ${CMAKE_SOURCE_DIR}/overlays@/overlays
    ${CMAKE_SOURCE_DIR}/soundfont@/soundfont
    --js-output=${DEST_DIR}/edge-classic-data.js
    --use-preload-cache
    --no-node
    --lz4
  )

endif()

# Copy appropriate SDL2.dll to local install directory when built with MSVC/Clang or MSYS2
if (WIN32)
  if (MSYS)
    if($ENV{MSYSTEM} MATCHES "CLANG64")  
      list(APPEND COPY_FILES "/clang64/bin/SDL2.dll")
    elseif($ENV{MSYSTEM} MATCHES "UCRT64")
      list(APPEND COPY_FILES "/ucrt64/bin/SDL2.dll")
    else ()
      if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
        list(APPEND COPY_FILES "/mingw64/bin/SDL2.dll")
      else ()
        list(APPEND COPY_FILES "/mingw32/bin/SDL2.dll")
      endif ()
    endif()
  elseif(MSVC OR CLANG)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/msvc/x64/SDL2.dll")
    else ()
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/msvc/x86/SDL2.dll")
    endif ()
  elseif(MINGW)
    if (${CMAKE_SIZEOF_VOID_P} MATCHES 8)
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/mingw/x64/SDL2.dll")
    else ()
      list(APPEND COPY_FILES "${CMAKE_SOURCE_DIR}/libraries/sdl2/mingw/x86/SDL2.dll")
    endif ()
  endif()
endif()

if (COPY_FILES)
  add_custom_command( TARGET edge-classic POST_BUILD COMMAND_EXPAND_LISTS COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COPY_FILES} ${DEST_DIR})
endif()

if (UNIX AND NOT EMSCRIPTEN)
  if (EXISTS "${CMAKE_SOURCE_DIR}/edge_defs.epk")
    set(HAVE_EPKS TRUE)
  else()
    set(HAVE_EPKS FALSE)
  endif()

  set(INSTALL_BASEDIR "${CMAKE_INSTALL_PREFIX}/share/edge-classic")

  install(PROGRAMS "$<TARGET_FILE:edge-classic>" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" RENAME "edge-classic.bin")
  install(FILES "${CMAKE_SOURCE_DIR}/source_files/edge/edge-classic.png"
          DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps")

  install(PROGRAMS "${CMAKE_SOURCE_DIR}/source_files/edge/edge-classic.sh" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" RENAME "edge-classic")
  install(FILES "${CMAKE_SOURCE_DIR}/source_files/edge/edge-classic.desktop"
          DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")

  # Update launch script/desktop file with path information
  install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/bin/edge-classic\" \"\nexec \\\"${CMAKE_INSTALL_PREFIX}/bin/edge-classic.bin\\\" \\\"$@\\\" \\\"-game\\\" \\\"${INSTALL_BASEDIR}\\\"\n\")")
  install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/share/applications/edge-classic.desktop\" \"\nExec=\\\"${CMAKE_INSTALL_PREFIX}/bin/edge-classic.bin\\\" \\\"$@\\\" \\\"-game\\\" \\\"${INSTALL_BASEDIR}\\\"\")")
  install(CODE "file(APPEND \"${CMAKE_INSTALL_PREFIX}/share/applications/edge-classic.desktop\" \"\nIcon=\"${CMAKE_INSTALL_PREFIX}/share/pixmaps/edge-classic.png\"\n\")")

  install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/autoload"
              "${CMAKE_SOURCE_DIR}/crosshairs"
              "${CMAKE_SOURCE_DIR}/docs"
              "${CMAKE_SOURCE_DIR}/edge_fixes"
              "${CMAKE_SOURCE_DIR}/overlays"
              "${CMAKE_SOURCE_DIR}/soundfont"
    DESTINATION ${INSTALL_BASEDIR}
    CONFIGURATIONS Release
    PATTERN ".gitkeep" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
  )

  if (HAVE_EPKS)
    install(
      FILES "${CMAKE_SOURCE_DIR}/edge_defs.epk"
      DESTINATION ${INSTALL_BASEDIR}
      CONFIGURATIONS Release
    )
    install(
      FILES "${CMAKE_SOURCE_DIR}/edge_base/blasphemer.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/chex1.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/chex3v.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/chex3vm.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/doom.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/doom1.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/doom2.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/freedoom1.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/freedoom2.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/hacx.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/harmony.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/harmonyc.epk"    
            "${CMAKE_SOURCE_DIR}/edge_base/heretic.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/plutonia.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/rekkr.epk"
            "${CMAKE_SOURCE_DIR}/edge_base/tnt.epk"  
      DESTINATION "${INSTALL_BASEDIR}/edge_base"
      CONFIGURATIONS Release
    )
  else()
    install(
      DIRECTORY "${CMAKE_SOURCE_DIR}/edge_base"
                "${CMAKE_SOURCE_DIR}/edge_defs"
      DESTINATION ${INSTALL_BASEDIR}
      CONFIGURATIONS Release
      PATTERN ".gitkeep" EXCLUDE
      PATTERN ".gitignore" EXCLUDE
    )
  endif()
endif()