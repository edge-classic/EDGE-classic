##########################################
# glad
##########################################

add_library(glad src/gl.c)

target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
