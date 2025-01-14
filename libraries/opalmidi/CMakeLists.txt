##########################################
# opalmidi
##########################################

set(OPALMIDI_SOURCE_FILES opal.cpp)

# If only IMF is supported, we don't need the MIDI components
if(EDGE_OPL_SUPPORT)
  set(OPALMIDI_SOURCE_FILES ${OPALMIDI_SOURCE_FILES} opalmidi.cpp patches.cpp)
endif()

add_library(
  opalmidi
  ${OPALMIDI_SOURCE_FILES}
)

target_include_directories(opalmidi PUBLIC ./)