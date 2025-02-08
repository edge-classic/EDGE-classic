
add_library(sokol sokol.cc)

set (SOKOL_LINK_LIBRARIES imgui)

if (EDGE_SOKOL_GL)
    set (SOKOL_LINK_LIBRARIES ${SOKOL_LINK_LIBRARIES} OpenGL::GL)
elseif (EDGE_SOKOL_GLES3 AND NOT EMSCRIPTEN)
    set (SOKOL_LINK_LIBRARIES ${SOKOL_LINK_LIBRARIES} OpenGL::GLES3)
endif()

target_link_libraries(sokol PUBLIC ${SOKOL_LINK_LIBRARIES})

target_include_directories(sokol PUBLIC ./)
target_compile_definitions(sokol PUBLIC SOKOL_NO_DEPRECATED SOKOL_IMGUI_NO_SOKOL_APP)

if (EDGE_SOKOL)
    if(EDGE_SOKOL_GL)
        target_compile_definitions (sokol PUBLIC SOKOL_GLCORE)
    elseif(EDGE_SOKOL_GLES3)
        target_compile_definitions (sokol PUBLIC SOKOL_GLES3)
    elseif(EDGE_SOKOL_D3D11)
        target_compile_definitions (sokol PUBLIC SOKOL_D3D11)
    endif()
endif()
