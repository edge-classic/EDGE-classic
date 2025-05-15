##########################################
# glad
##########################################

add_library(glad src/gl.c)

target_include_directories(glad SYSTEM PUBLIC ./include)