##########################################
# Emu de MIDI
##########################################

add_library(libemidi
    source/CEnvelope.cpp
    source/device/emu2413.c
    source/device/emu2212.c
    source/device/emu2149.c
    source/CMIDIModule.cpp
    source/CSccDevice.cpp
    source/CPSGDrum.cpp
    source/CMIDIMessage.cpp
    source/COpllDevice.cpp
    source/CSMFPlay.cpp
)

target_include_directories(libemidi PUBLIC ./source)