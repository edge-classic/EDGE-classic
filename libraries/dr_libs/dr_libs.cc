#if EDGE_WAV_SUPPORT
#define DR_WAV_NO_STDIO
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#endif

#if EDGE_MP3_SUPPORT
#define DR_MP3_NO_STDIO
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#endif

#if EDGE_FLAC_SUPPORT
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_CRC
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#endif