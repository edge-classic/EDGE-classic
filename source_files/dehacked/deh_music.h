//------------------------------------------------------------------------
//  MUSIC Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#pragma once

#include <string>

namespace dehacked
{

// This file diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum MusicType
{
    kmus_None,
    kmus_e1m1,
    kmus_e1m2,
    kmus_e1m3,
    kmus_e1m4,
    kmus_e1m5,
    kmus_e1m6,
    kmus_e1m7,
    kmus_e1m8,
    kmus_e1m9,
    kmus_e2m1,
    kmus_e2m2,
    kmus_e2m3,
    kmus_e2m4,
    kmus_e2m5,
    kmus_e2m6,
    kmus_e2m7,
    kmus_e2m8,
    kmus_e2m9,
    kmus_e3m1,
    kmus_e3m2,
    kmus_e3m3,
    kmus_e3m4,
    kmus_e3m5,
    kmus_e3m6,
    kmus_e3m7,
    kmus_e3m8,
    kmus_e3m9,
    kmus_inter,
    kmus_intro,
    kmus_bunny,
    kmus_victor,
    kmus_introa,
    kmus_runnin,
    kmus_stalks,
    kmus_countd,
    kmus_betwee,
    kmus_doom,
    kmus_the_da,
    kmus_shawn,
    kmus_ddtblu,
    kmus_in_cit,
    kmus_dead,
    kmus_stlks2,
    kmus_theda2,
    kmus_doom2,
    kmus_ddtbl2,
    kmus_runni2,
    kmus_dead2,
    kmus_stlks3,
    kmus_romero,
    kmus_shawn2,
    kmus_messag,
    kmus_count2,
    kmus_ddtbl3,
    kmus_ampie,
    kmus_theda3,
    kmus_adrian,
    kmus_messg2,
    kmus_romer2,
    kmus_tense,
    kmus_shawn3,
    kmus_openin,
    kmus_evil,
    kmus_ultima,
    kmus_read_m,
    kmus_dm2ttl,
    kmus_dm2int,
    kTotalMusicTypes
};

namespace music
{
void Init();
void Shutdown();

// this returns true if the string was found.
bool ReplaceMusic(const char *before, const char *after);

void AlterBexMusic(const char *new_val);

void ConvertMUS();
} // namespace music

} // namespace dehacked