//----------------------------------------------------------------------------
//  EDGE Sprite Management
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
class SpriteFrame
{
   public:
    // whether this frame has been completed.  Completed frames cannot
    // be replaced by sprite lumps in older wad files.
    bool finished_;

    // 1  = not rotated, we don't have to determine the angle for the
    //      sprite.  This is an optimisation.
    // 8  = normal DOOM rotations.
    // 16 = EDGE extended rotations using [9ABCDEFG].
    int rotations_;

    // Flip bits (1 = flip) to use for each view angle
    uint8_t flip_[16];

    // Images for each view angle
    const Image *images_[16];

    bool is_weapon_;

   public:
    SpriteFrame() : finished_(false), rotations_(0), is_weapon_(false)
    {
        for (int j = 0; j < 16; j++)
        {
            flip_[j]   = 0;
            images_[j] = nullptr;
        }
    }
};

/* Functions */

void InitializeSprites(void);

bool CheckSpritesExist(const std::vector<StateRange> &group);
void PrecacheSprites(void);

SpriteFrame *GetSpriteFrame(int spr_num, int framenum);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
