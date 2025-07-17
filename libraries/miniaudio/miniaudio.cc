#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
// We use our own custom minivorbis decoder
#define MA_NO_VORBIS
#define MA_USE_STDINT
#ifdef _WIN32
// We want to use the apartment model in order
// to prevent conflicts with SDL
#define MA_COINIT_VALUE COINIT_APARTMENTTHREADED
// If using Sokol we already require
// Windows 7+ so build only for WASAPI in that case
#ifdef EDGE_SOKOL
#define MA_NO_WINMM
#define MA_NO_DSOUND
#endif
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define MINIAUDIO_FREEVERB_IMPLEMENTATION
#define VERBLIB_IMPLEMENTATION
#include "miniaudio_freeverb.h"