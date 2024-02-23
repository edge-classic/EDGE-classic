//----------------------------------------------------------------------------
//  EDGE MP3 Music Player (HEADER)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2021-2024 The EDGE Team.
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
// -ACB- 2004/08/18 Written
//
#ifndef __MP3PLAYER_H__
#define __MP3PLAYER_H__

#include "s_music.h"

#include "sound_data.h"

/* FUNCTIONS */

AbstractMusicPlayer *S_PlayMP3Music(uint8_t *data, int length, bool looping);

bool S_LoadMP3Sound(sound_data_c *buf, const uint8_t *data, int length);

#endif /* __MP3PLAYER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
