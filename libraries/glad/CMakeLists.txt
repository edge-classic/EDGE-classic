##########################################
# glad
##########################################

add_library(glad src/glad.c)

target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})

target_include_directories(glad SYSTEM PUBLIC ./include)