cmake_minimum_required(VERSION 3.12..3.20)

add_subdirectory(source_files/coal)
add_subdirectory(source_files/ddf)
add_subdirectory(source_files/deh_edge)
add_subdirectory(source_files/epi)
add_subdirectory(source_files/glbsp)
add_subdirectory(source_files/glew/build/cmake)
add_subdirectory(source_files/libjpeg)
add_subdirectory(source_files/libpng)
add_subdirectory(source_files/libogg)
add_subdirectory(source_files/libvorbis)
add_subdirectory(source_files/zlib)

project(
  edge135
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-O2 -ffast-math -fno-strict-aliasing -DINLINE_G=inline -Wall")
if(WIN32 AND NOT MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")
  set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:WINDOWS")
endif()
if(MSYS)
  set(CMAKE_EXE_LINKER_FLAGS "-static -mwindows")
endif()

if(WIN32 OR MSYS)
add_executable(
  edge135
	source_files/edge/i_main.cc     
	source_files/edge/i_ctrl.cc     
	source_files/edge/i_video.cc    
	source_files/edge/i_sound.cc    
	source_files/edge/i_net.cc      
	source_files/edge/am_map.cc     
	source_files/edge/con_con.cc    
	source_files/edge/con_main.cc   
	source_files/edge/con_link.cc   
	source_files/edge/con_var.cc    
	source_files/edge/e_input.cc    
	source_files/edge/e_main.cc     
	source_files/edge/e_player.cc   
	source_files/edge/f_finale.cc   
	source_files/edge/f_interm.cc   
	source_files/edge/g_game.cc     
	source_files/edge/hu_draw.cc    
	source_files/edge/hu_font.cc    
	source_files/edge/hu_stuff.cc   
	source_files/edge/hu_style.cc   
	source_files/edge/l_glbsp.cc    
	source_files/edge/l_deh.cc      
	source_files/edge/m_argv.cc     
	source_files/edge/m_bbox.cc     
	source_files/edge/m_cheat.cc    
	source_files/edge/m_math.cc     
	source_files/edge/m_menu.cc     
	source_files/edge/m_misc.cc     
	source_files/edge/m_option.cc   
	source_files/edge/m_netgame.cc  
	source_files/edge/m_random.cc   
	source_files/edge/n_bcast.cc    
	source_files/edge/n_reliable.cc 
	source_files/edge/n_network.cc  
	source_files/edge/p_action.cc   
	source_files/edge/p_blockmap.cc 
	source_files/edge/p_bot.cc      
	source_files/edge/p_enemy.cc    
	source_files/edge/p_inter.cc    
	source_files/edge/p_lights.cc   
	source_files/edge/p_map.cc      
	source_files/edge/p_maputl.cc   
	source_files/edge/p_mobj.cc     
	source_files/edge/p_plane.cc    
	source_files/edge/p_setup.cc    
	source_files/edge/p_sight.cc    
	source_files/edge/p_spec.cc     
	source_files/edge/p_switch.cc   
	source_files/edge/p_tick.cc     
	source_files/edge/p_user.cc     
	source_files/edge/p_forces.cc   
	source_files/edge/p_telept.cc   
	source_files/edge/p_weapon.cc   
	source_files/edge/rad_act.cc    
	source_files/edge/rad_pars.cc   
	source_files/edge/rad_trig.cc   
	source_files/edge/r_draw.cc     
	source_files/edge/r_shader.cc   
	source_files/edge/r_render.cc   
	source_files/edge/r_effects.cc  
	source_files/edge/r_main.cc     
	source_files/edge/r_occlude.cc  
	source_files/edge/m_logo.cc     
	source_files/edge/r_things.cc   
	source_files/edge/r_units.cc    
	source_files/edge/r_wipe.cc     
	source_files/edge/r_misc.cc     
	source_files/edge/r_sky.cc      
	source_files/edge/r_colormap.cc 
	source_files/edge/r_modes.cc    
	source_files/edge/r_md2.cc      
	source_files/edge/r_image.cc    
	source_files/edge/r_doomtex.cc  
	source_files/edge/r_texgl.cc    
	source_files/edge/s_blit.cc     
	source_files/edge/s_cache.cc    
	source_files/edge/s_sound.cc 
	source_files/edge/s_mp3.cc   
	source_files/edge/s_music.cc    
	source_files/edge/s_ogg.cc      
	source_files/edge/s_tsf.cc    
	source_files/edge/sv_chunk.cc   
	source_files/edge/sv_glob.cc    
	source_files/edge/sv_level.cc   
	source_files/edge/sv_load.cc    
	source_files/edge/sv_main.cc    
	source_files/edge/sv_misc.cc    
	source_files/edge/sv_mobj.cc    
	source_files/edge/sv_play.cc    
	source_files/edge/sv_save.cc    
	source_files/edge/w_flat.cc     
	source_files/edge/w_model.cc    
	source_files/edge/w_sprite.cc   
	source_files/edge/w_texture.cc  
	source_files/edge/w_wad.cc      
	source_files/edge/z_zone.cc     
	source_files/edge/vm_coal.cc    
	source_files/edge/vm_hud.cc     
	source_files/edge/vm_player.cc  
	source_files/edge/w32_mus.cc
	source_files/edge/w32_music.cc
	source_files/edge/w32_net.cc
	source_files/edge/w32_res.rc
	source_files/edge/w32_system.cc
)
else()
add_executable(
  edge135
	source_files/edge/i_main.cc     
	source_files/edge/i_ctrl.cc     
	source_files/edge/i_video.cc    
	source_files/edge/i_sound.cc    
	source_files/edge/i_net.cc      
	source_files/edge/am_map.cc     
	source_files/edge/con_con.cc    
	source_files/edge/con_main.cc   
	source_files/edge/con_link.cc   
	source_files/edge/con_var.cc    
	source_files/edge/e_input.cc    
	source_files/edge/e_main.cc     
	source_files/edge/e_player.cc   
	source_files/edge/f_finale.cc   
	source_files/edge/f_interm.cc   
	source_files/edge/g_game.cc     
	source_files/edge/hu_draw.cc    
	source_files/edge/hu_font.cc    
	source_files/edge/hu_stuff.cc   
	source_files/edge/hu_style.cc   
	source_files/edge/l_glbsp.cc    
	source_files/edge/l_deh.cc      
	source_files/edge/m_argv.cc     
	source_files/edge/m_bbox.cc     
	source_files/edge/m_cheat.cc    
	source_files/edge/m_math.cc     
	source_files/edge/m_menu.cc     
	source_files/edge/m_misc.cc     
	source_files/edge/m_option.cc   
	source_files/edge/m_netgame.cc  
	source_files/edge/m_random.cc   
	source_files/edge/n_bcast.cc    
	source_files/edge/n_reliable.cc 
	source_files/edge/n_network.cc  
	source_files/edge/p_action.cc   
	source_files/edge/p_blockmap.cc 
	source_files/edge/p_bot.cc      
	source_files/edge/p_enemy.cc    
	source_files/edge/p_inter.cc    
	source_files/edge/p_lights.cc   
	source_files/edge/p_map.cc      
	source_files/edge/p_maputl.cc   
	source_files/edge/p_mobj.cc     
	source_files/edge/p_plane.cc    
	source_files/edge/p_setup.cc    
	source_files/edge/p_sight.cc    
	source_files/edge/p_spec.cc     
	source_files/edge/p_switch.cc   
	source_files/edge/p_tick.cc     
	source_files/edge/p_user.cc     
	source_files/edge/p_forces.cc   
	source_files/edge/p_telept.cc   
	source_files/edge/p_weapon.cc   
	source_files/edge/rad_act.cc    
	source_files/edge/rad_pars.cc   
	source_files/edge/rad_trig.cc   
	source_files/edge/r_draw.cc     
	source_files/edge/r_shader.cc   
	source_files/edge/r_render.cc   
	source_files/edge/r_effects.cc  
	source_files/edge/r_main.cc     
	source_files/edge/r_occlude.cc  
	source_files/edge/m_logo.cc     
	source_files/edge/r_things.cc   
	source_files/edge/r_units.cc    
	source_files/edge/r_wipe.cc     
	source_files/edge/r_misc.cc     
	source_files/edge/r_sky.cc      
	source_files/edge/r_colormap.cc 
	source_files/edge/r_modes.cc    
	source_files/edge/r_md2.cc      
	source_files/edge/r_image.cc    
	source_files/edge/r_doomtex.cc  
	source_files/edge/r_texgl.cc    
	source_files/edge/s_blit.cc     
	source_files/edge/s_cache.cc    
	source_files/edge/s_sound.cc
	source_files/edge/s_mp3.cc   
	source_files/edge/s_music.cc    
	source_files/edge/s_ogg.cc      
	source_files/edge/s_tsf.cc    
	source_files/edge/sv_chunk.cc   
	source_files/edge/sv_glob.cc    
	source_files/edge/sv_level.cc   
	source_files/edge/sv_load.cc    
	source_files/edge/sv_main.cc    
	source_files/edge/sv_misc.cc    
	source_files/edge/sv_mobj.cc    
	source_files/edge/sv_play.cc    
	source_files/edge/sv_save.cc    
	source_files/edge/w_flat.cc     
	source_files/edge/w_model.cc    
	source_files/edge/w_sprite.cc   
	source_files/edge/w_texture.cc  
	source_files/edge/w_wad.cc      
	source_files/edge/z_zone.cc     
	source_files/edge/vm_coal.cc    
	source_files/edge/vm_hud.cc     
	source_files/edge/vm_player.cc  
	source_files/edge/unx_music.cc  
	source_files/edge/unx_net.cc    
	source_files/edge/unx_system.cc
)
endif()

find_package(SDL REQUIRED)
find_package(GLEW REQUIRED)

if(NOT MSYS)
find_package(OpenGL REQUIRED)
endif()

target_include_directories(edge135 PRIVATE source_files/coal)
target_include_directories(edge135 PRIVATE source_files/ddf)
target_include_directories(edge135 PRIVATE source_files/deh_edge)
target_include_directories(edge135 PRIVATE source_files/dr_mp3)
target_include_directories(edge135 PRIVATE source_files/epi)
target_include_directories(edge135 PRIVATE source_files/glew/include/GL)
target_include_directories(edge135 PRIVATE source_files/glbsp/src)
target_include_directories(edge135 PRIVATE source_files/libjpeg)
target_include_directories(edge135 PRIVATE source_files/libogg/include/ogg)
target_include_directories(edge135 PRIVATE source_files/libpng)
target_include_directories(edge135 PRIVATE source_files/libvorbis/include/vorbis)
target_include_directories(edge135 PRIVATE source_files/tinysoundfont)
target_include_directories(edge135 PRIVATE source_files/zlib)

if(WIN32 OR MSYS)
  target_compile_definitions(edge135 PRIVATE WIN32)
else()
  target_compile_definitions(edge135 PRIVATE UNIX)
endif()

# Copies executables to local install directory after build
add_custom_command(
  TARGET edge135
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:edge135>"
          "${CMAKE_CURRENT_LIST_DIR}"
)

if(WIN32 OR MSYS)
target_link_libraries(
  edge135
  PRIVATE edge_coal
          edge_ddf
          edge_deh
          edge_epi
          edge_glbsp
		  glew_s
		  opengl32
		  Ogg::ogg
		  ${SDL_LIBRARIES}
		  png_static
		  jpeg_static
		  zlibstatic
		  vorbis
		  vorbisfile
		  vorbisenc
		  winmm
		  wsock32
  )
else()
target_link_libraries(
  edge135
  PRIVATE edge_coal
          edge_ddf
          edge_deh
          edge_epi
          edge_glbsp
		  OpenGL
		  ${GLEW_LIBRARIES}
		  Ogg::ogg
		  SDL
		  vorbis
		  vorbisfile
		  vorbisenc
		  png_static
		  jpeg_static
		  zlibstatic
  )
endif()
