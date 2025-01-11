##########################################
# miniaudio
##########################################

add_library(miniaudio miniaudio.cc)

target_include_directories(miniaudio PUBLIC ./)