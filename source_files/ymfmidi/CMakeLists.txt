##########################################
# ymfmidi
##########################################

add_library(ymfmidi
  patches.cpp
  patchnames.cpp
  ymf_player.cpp
)

target_include_directories(ymfmidi PRIVATE ../libymfm)
