#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_WINMM
#ifndef EDGE_FLAC_SUPPORT
#define MA_NO_FLAC
#endif
#ifndef EDGE_MP3_SUPPORT
#define MA_NO_MP3
#endif
#ifndef EDGE_WAV_SUPPORT
#define MA_NO_WAV
#endif
// Unlike the above, this one is unconditional. If OGG support is enabled,
// our own custom decoder will be used
#define MA_NO_VORBIS
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"