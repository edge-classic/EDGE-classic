//----------------------------------------------------------------------------
//  EDGE Music Handling Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#ifndef __S_MUSIC_H__
#define __S_MUSIC_H__

/* abstract base class */
class abstract_music_c
{
public:
	abstract_music_c() { }
	virtual ~abstract_music_c() { }

	virtual void Close(void) = 0;

	virtual void Play(bool loop) = 0;
	virtual void Stop(void) = 0;

	virtual void Pause(void)  = 0;
	virtual void Resume(void) = 0;

	virtual void Ticker(void) = 0;
	virtual void Volume(float gain) = 0;
};

/* VARIABLES */

extern int mus_volume;  // 0 .. SND_SLIDER_NUM-1

extern int var_music_dev;

/* FUNCTIONS */

void S_ChangeMusic(int entrynum, bool loop);
void S_ResumeMusic(void);
void S_PauseMusic(void);
void S_StopMusic(void);
void S_MusicTicker(void);

void S_ChangeMusicVolume(void);

#endif /* __S_MUSIC_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
