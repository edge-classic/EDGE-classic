//----------------------------------------------------------------------------
//  EDGE Blockmap utility functions
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

#ifndef __P_BLOCKMAP_H__
#define __P_BLOCKMAP_H__

// #include "epi/arrays.h"

// mapblocks are used to check movement
// against lines and things
#define BLOCKMAP_UNIT  128
#define LIGHTMAP_UNIT  512

#define BMAP_END  ((unsigned short) 0xFFFF)

extern int bmap_width;   // in mapblocks
extern int bmap_height;

extern float bmap_orgx;  // origin of block map
extern float bmap_orgy;

#define BLOCKMAP_GET_X(x)  ((int) ((x) - bmap_orgx) / BLOCKMAP_UNIT)
#define BLOCKMAP_GET_Y(y)  ((int) ((y) - bmap_orgy) / BLOCKMAP_UNIT)

#define LIGHTMAP_GET_X(x)  ((int) ((x) - bmap_orgx) / LIGHTMAP_UNIT)
#define LIGHTMAP_GET_Y(y)  ((int) ((y) - bmap_orgy) / LIGHTMAP_UNIT)


#define PT_ADDLINES  1
#define PT_ADDTHINGS 2

typedef struct intercept_s
{
	float frac;  // along trace line

	// one of these will be NULL
	mobj_t *thing;
	line_t *line;
}
intercept_t;

extern divline_t trace;


/* FUNCTIONS */

void P_CreateThingBlockMap(void);
void P_DestroyBlockMap(void);

void P_SetThingPosition(mobj_t * mo);
void P_UnsetThingPosition(mobj_t * mo);
void P_UnsetThingFinally(mobj_t * mo);
void P_ChangeThingPosition(mobj_t * mo, float x, float y, float z);
void P_FreeSectorTouchNodes(sector_t *sec);

void P_GenerateBlockMap(int min_x, int min_y, int max_x, int max_y);

bool P_BlockLinesIterator(float x1, float y1, float x2, float y2,
		                  bool (* func)(line_t *, void *),
						  void *data = NULL);

bool P_BlockThingsIterator(float x1, float y1, float x2, float y2,
		                   bool (* func)(mobj_t *, void *),
						   void *data = NULL);

void P_DynamicLightIterator(float x1, float y1, float z1,
		                    float x2, float y2, float z2,
		                    void (* func)(mobj_t *, void *),
						    void *data = NULL);

void P_SectorGlowIterator(sector_t *sec,
		                  float x1, float y1, float z1,
		                  float x2, float y2, float z2,
		                  void (* func)(mobj_t *, void *),
						  void *data = NULL);

float P_InterceptVector(divline_t * v2, divline_t * v1);

bool P_PathTraverse(float x1, float y1, float x2, float y2, int flags,
		            bool (* func)(intercept_t *, void *),
					void *data = NULL);


#endif // __P_BLOCKMAP_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
