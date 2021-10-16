//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Screen effects)
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

#ifndef __RGL_FX_H__
#define __RGL_FX_H__

#include "e_player.h"
#include "r_image.h"


class screen_effect_def_c;


class screen_fx_slot_c
{
public:
	screen_fx_slot_c();
	~screen_fx_slot_c() { }

	const screen_effect_def_c *def;

	bool active;

	// number of powerup this is linked to, or -1 if none
	int power_link;
	
	// tics remaining (not used if linked)
	int time;
};


void RGL_RainbowEffect(player_t *player);
void RGL_ColourmapEffect(player_t *player);
void RGL_PaletteEffect(player_t *player);


//
//  FUZZY Emulation
//

extern const image_c * fuzz_image;

void FUZZ_Update(void);
void FUZZ_Adjust(vec2_t *tc, mobj_t *mo);


#endif /* __RGL_FX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
