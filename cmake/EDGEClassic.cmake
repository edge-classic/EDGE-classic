##########################################
# Edge Classic - CMake Utilities
##########################################

set (EDGE_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/libraries)

# target for generating epk packs
add_custom_target(GenerateEPKS)

# defs/game/mod packs
set (EPK_BASE_SOURCE
    "edge_defs"    
    "edge_base/blasphemer"
    "edge_base/chex1"
    "edge_base/chex3v"
    "edge_base/chex3vm"
    "edge_base/doom"
    "edge_base/doom1"
    "edge_base/doom2"
    "edge_base/freedoom1"
    "edge_base/freedoom2"
    "edge_base/hacx"
    "edge_base/harmony"
    "edge_base/harmonyc"    
    "edge_base/heretic"
    "edge_base/plutonia"
    "edge_base/rekkr"
    "edge_base/tnt"    
) 

function( add_epk EPK_PATH )
    add_custom_command(
        TARGET GenerateEPKS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/${EPK_PATH}.epk" --format=zip "."
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${EPK_PATH}"
    )
endfunction()

foreach( epk ${EPK_BASE_SOURCE})
    add_epk(${epk})
endforeach( epk )

