//----------------------------------------------------------------------------
//  EDGE Sound FX Handling Code
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

// Forward declarations
struct Position;
class MapObject;

// Sound Categories
// ----------------
//
// Each category has a minimum number of channels (say N).
// Sounds of a category are GUARANTEED to play when there
// are less than N sounds of that category already playing.
//
// So while more than N sounds of a category can be active at
// a time, the extra ones are "hogging" channels belonging to
// other categories, and will be kicked out (trumped) if there
// are no other free channels.
//
// The order here is significant, if the channel limit for a
// category is set to zero, then NEXT category is tried.
//
enum SoundCategory
{
    kCategoryUi = 0,   // for the user interface (menus, tips)
    kCategoryPlayer,   // for console player (pain, death, pickup)
    kCategoryWeapon,   // for console player's weapon
    kCategoryOpponent, // for all other players (DM or COOP)
    kCategoryMonster,  // for all monster sounds
    kCategoryObject,   // for all objects (esp. projectiles)
    kCategoryLevel,    // for doors, lifts and map scripts
    kTotalCategories
};

/* FX Flags */
enum SoundEffectFlag
{
    kSoundEffectNormal = 0,

    // monster bosses: sound is not diminished by distance
    kSoundEffectBoss = (1 << 1),

    // only play one instance of this sound at this location.
    kSoundEffectSingle = (1 << 2),

    // combine with kSoundEffectSingle: the already playing sound is
    // allowed to continue and the new sound it dropped.
    // Without this flag: the playing sound is cut off.
    // (has no effect without kSoundEffectSingle).
    kSoundEffectPrecious = (1 << 3),
};

constexpr float kMinimumSoundClipDistance         = 160.0f;
constexpr float kMinimumOccludedSoundClipDistance = 80.0f;

// Init/Shutdown
void InitializeSound(void);
void ShutdownSound(void);

void StartSoundEffect(const SoundEffect *sfx, int category = kCategoryUi, const Position *pos = nullptr, int flags = 0);

void StopSoundEffect(const Position *pos);
void StopSoundEffect(const SoundEffect *sfx);
void StopAllSoundEffects(void);

void ResumeSound(void);
void PauseSound(void);

void SoundTicker(void);

void PrecacheSounds(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
