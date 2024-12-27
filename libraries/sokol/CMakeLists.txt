
add_library(sokol sokol.cc)

target_link_libraries(sokol imgui)

if (CMAKE_HOST_UNIX)
    target_link_libraries(sokol GL)
endif()

target_include_directories(sokol PUBLIC ./)
target_compile_definitions(sokol PUBLIC SOKOL_NO_DEPRECATED SOKOL_TRACE_HOOKS SOKOL_IMGUI_NO_SOKOL_APP)

if (EDGE_SOKOL)
    if(EDGE_SOKOL_GL)
        target_compile_definitions (sokol PUBLIC SOKOL_GLCORE)
    elseif(EDGE_SOKOL_D3D11)
        target_compile_definitions (sokol PUBLIC SOKOL_D3D11)
    endif()
endif()
