##########################################
# xxHash
##########################################

add_library(xxhash xxhash.cc)

target_include_directories(xxhash PUBLIC ./)