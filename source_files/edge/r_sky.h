//----------------------------------------------------------------------------
//  EDGE Sky Handling Code
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

#ifndef __R_SKY__
#define __R_SKY__


// The sky map is 256*4 wide (10 bits), and angles have 32 bits
#define ANGLETOSKYSHIFT  (32 - 10)

extern const image_c *sky_image;

// true when a custom sky box is present
extern bool custom_sky_box;

typedef enum
{
	WSKY_North = 0,
	WSKY_East,
	WSKY_South,
	WSKY_West,
	WSKY_Top,
	WSKY_Bottom
}
sky_box_face_e;

void R_ComputeSkyHeights(void);

void RGL_SetupSkyMatrices(void);
void RGL_RevertSkyMatrices(void);
void RGL_BeginSky(void);
void RGL_FinishSky(void);

void RGL_DrawSkyBox(void);
void RGL_DrawSkyOriginal(void);
void RGL_DrawSkyPlane(subsector_t *sub, float h);
void RGL_DrawSkyWall(seg_t *seg, float h1, float h2);

int  RGL_UpdateSkyBoxTextures(void);
void RGL_PreCacheSky(void);


#endif /* __R_SKY__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
