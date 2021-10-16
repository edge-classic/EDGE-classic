//----------------------------------------------------------------------------
//  EDGE Sprite Management
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __W_SPRITES_H__
#define __W_SPRITES_H__

#include "e_player.h"
#include "r_defs.h"

//      
// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites.  The base name is NNNNFx or NNNNFxFx,
// with x indicating the rotation, x = 0, 1-15.
//
// Horizontal flipping is used to save space, thus NNNNF2F8 defines a
// mirrored patch (F8 is the mirrored one).
//
// Some sprites will only have one picture used for all views: NNNNF0.
// In that case, the `rotated' field is false.
//
class spriteframe_c
{
public:

	// whether this frame has been completed.  Completed frames cannot
	// be replaced by sprite lumps in older wad files.
	bool finished;
  
    // 1  = not rotated, we don't have to determine the angle for the
    //      sprite.  This is an optimisation.
	// 8  = normal DOOM rotations.
	// 16 = EDGE extended rotations using [9ABCDEFG].
	int rots;
  
	// Flip bits (1 = flip) to use for each view angle
	byte flip[16];
  
	// Images for each view angle
	const image_c *images[16];

	bool is_weapon;

public:
	spriteframe_c() : finished(false), rots(0), is_weapon(false)
	{
		for (int j = 0; j < 16; j++)
		{
			flip[j] = 0;
			images[j] = NULL;
		}
	}
};


/* Functions */

void W_InitSprites(void);

bool W_CheckSpritesExist(const state_group_t& group);
void W_PrecacheSprites(void);

spriteframe_c *W_GetSpriteFrame(int spr_num, int framenum);

#endif // __W_SPRITES_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
