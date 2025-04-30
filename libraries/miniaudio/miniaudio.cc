#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
// We use our own custom minivorbis decoder
#define MA_NO_VORBIS
#define MA_USE_STDINT
#ifdef _WIN32
// If using Sokol or Mimalloc we already require
// Windows 7+ so build only for WASAPI in that case
#if defined (EDGE_SOKOL) || defined (EDGE_MIMALLOC)
#define MA_NO_WINMM
#define MA_NO_DSOUND
#endif
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define MINIAUDIO_FREEVERB_IMPLEMENTATION
#define VERBLIB_IMPLEMENTATION
#include "miniaudio_freeverb.h"