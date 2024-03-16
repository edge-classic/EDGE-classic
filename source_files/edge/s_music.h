//----------------------------------------------------------------------------
//  EDGE Music Handling Code
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

#pragma once

#include "con_var.h"

/* abstract base class */
class AbstractMusicPlayer
{
  public:
    AbstractMusicPlayer()
    {
    }
    virtual ~AbstractMusicPlayer()
    {
    }

    virtual void Close(void) = 0;

    virtual void Play(bool loop) = 0;
    virtual void Stop(void)      = 0;

    virtual void Pause(void)  = 0;
    virtual void Resume(void) = 0;

    virtual void Ticker(void) = 0;
};

/* VARIABLES */

extern ConsoleVariable music_volume;
extern int             entry_playing;
extern bool            pc_speaker_mode;

/* FUNCTIONS */

void ChangeMusic(int entry_number, bool loop);
void ResumeMusic(void);
void PauseMusic(void);
void StopMusic(void);
void MusicTicker(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
