//----------------------------------------------------------------------------
//  Sound Caching
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __S_CACHE_H__
#define __S_CACHE_H__

#include "sound_data.h"

class sfxdef_c;

void S_CacheInit(void);
// setup the sound cache system.

void S_CacheClearAll(void);
// clear all sounds from the cache.
// Must be called if the audio system parameters (sample_bits,
// stereoness) are changed.

sound_data_c *S_CacheLoad(sfxdef_c *def);
// load a sound into the cache.  If the sound has already
// been loaded, then it is simply returned (increasing the
// reference count).  Returns nullptr if the lump doesn't exist.

void S_CacheRelease(sound_data_c *data);
// we are finished with this data.  The cache system may
// free the memory when the number of references drops to 0.
// Typically though the sound is kept, as it will likely
// be needed again shortly.

#endif /* __S_CACHE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
