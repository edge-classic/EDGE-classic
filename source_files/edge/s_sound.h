//----------------------------------------------------------------------------
//  EDGE Sound FX Handling Code
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

#ifndef __S_SOUND_H__
#define __S_SOUND_H__

// Forward declarations
class position_c;
struct mobj_s;
struct sfx_s;

// for the sliders
#define SND_SLIDER_NUM  20

extern float slider_to_gain[SND_SLIDER_NUM];

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
typedef enum
{
	SNCAT_UI = 0,     // for the user interface (menus, tips)
	SNCAT_Player,     // for console player (pain, death, pickup)
	SNCAT_Weapon,     // for console player's weapon
	SNCAT_Opponent,   // for all other players (DM or COOP)
	SNCAT_Monster,    // for all monster sounds
	SNCAT_Object,     // for all objects (esp. projectiles)
	SNCAT_Level,      // for doors, lifts and map scripts

	SNCAT_NUMTYPES
}
sound_category_e;


/* FX Flags */
typedef enum
{
	FX_NORMAL = 0,

	// monster bosses: sound is not diminished by distance
	FX_Boss = (1 << 1),
	
	// only play one instance of this sound at this location.
	FX_Single = (1 << 2),

	// combine with FX_Single: the already playing sound is
	// allowed to continue and the new sound it dropped.
	// Without this flag: the playing sound is cut off.
	// (has no effect without FX_Single).
	FX_Precious = (1 << 3),

}
fx_flag_e;


// Vars
extern int sfx_volume;  // 0 .. SND_SLIDER_NUM-1


// Init/Shutdown
void S_Init(void);
void S_Shutdown(void);

void S_StartFX(struct sfx_s *sfx, int category = SNCAT_UI, position_c *pos = NULL, int flags = 0);

void S_StopFX(position_c *pos);
void S_StopLevelFX(void);

void S_ResumeSound(void);
void S_PauseSound(void);

void S_SoundTicker(void);

void S_ChangeSoundVolume(void);
void S_ChangeChannelNum(void);

#endif /* __S_SOUND_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
