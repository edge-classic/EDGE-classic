//----------------------------------------------------------------------------
//  EDGE Main Rendering Organisation Code
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
//
// -KM- 1998/09/27 Dynamic Colourmaps
//

#include "i_defs.h"
#include "i_defs_gl.h"

#include <math.h>

#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "m_misc.h"
#include "n_network.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_units.h"


cvar_c r_fov;
cvar_c r_zoomfov;

int viewwindow_x;
int viewwindow_y;
int viewwindow_w;
int viewwindow_h;

angle_t viewangle = 0;
angle_t viewvertangle = 0;

vec3_t viewforward;
vec3_t viewup;
vec3_t viewright;

angle_t normalfov, zoomedfov;
bool viewiszoomed = false;

// increment every time a check is made
int validcount = 1;

// just for profiling purposes
int framecount;
int linecount;

subsector_t *viewsubsector;
region_properties_t *view_props;

float viewx;
float viewy;
float viewz;

float viewcos;
float viewsin;

player_t *viewplayer;

mobj_t *background_camera_mo = NULL;

//
// precalculated math tables
//

angle_t viewanglebaseoffset;
angle_t viewangleoffset;

int telept_starttic;
int telept_active = 0;

int telept_effect = 0;
int telept_flash = 1;
int telept_reverse = 0;

int var_invul_fx;

//
// To get a global angle from cartesian coordinates,
// the coordinates are flipped until they are in
// the first octant of the coordinate system, then
// the y (<=x) is scaled and divided by x to get a
// tangent (slope) value which is looked up in the
// tantoangle[] table.
//
angle_t R_PointToAngle(float x1, float y1, float x, float y)
{
	x -= x1;
	y -= y1;

	if ((x == 0) && (y == 0))
		return 0;

	if (x >= 0)
	{
		// x >=0
		if (y >= 0)
		{
			// y>= 0

			if (x > y)
			{
				// octant 0
				return M_ATan(y / x);
			}
			else
			{
				// octant 1
				return ANG90 - 1 - M_ATan(x / y);
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x > y)
			{
				// octant 8
				// -ACB- 1999/09/27 Fixed MSVC Compiler warning
				return 0 - M_ATan(y / x);
			}
			else
			{
				// octant 7
				return ANG270 + M_ATan(x / y);
			}
		}
	}
	else
	{
		// x<0
		x = -x;

		if (y >= 0)
		{
			// y>= 0
			if (x > y)
			{
				// octant 3
				return ANG180 - 1 - M_ATan(y / x);
			}
			else
			{
				// octant 2
				return ANG90 + M_ATan(x / y);
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x > y)
			{
				// octant 4
				return ANG180 + M_ATan(y / x);
			}
			else
			{
				// octant 5
				return ANG270 - 1 - M_ATan(x / y);
			}
		}
	}
}


float R_PointToDist(float x1, float y1, float x2, float y2)
{
	angle_t angle;
	float dx;
	float dy;
	float temp;
	float dist;

	dx = (float)fabs(x2 - x1);
	dy = (float)fabs(y2 - y1);

	if (dx == 0.0f)
		return dy;
	else if (dy == 0.0f)
		return dx;

	if (dy > dx)
	{
		temp = dx;
		dx = dy;
		dy = temp;
	}

	angle = M_ATan(dy / dx) + ANG90;

	// use as cosine
	dist = dx / M_Sin(angle);

	return dist;
}



//
// Called once at startup, to initialise some rendering stuff.
//
void R_Init(void)
{
	E_ProgressMessage(language["RefreshDaemon"]);

	framecount = 0;

	// -AJA- 1999/07/01: Setup colour tables.
	V_InitColour();

	RGL_LoadLights();
}


subsector_t *R_PointInSubsector(float x, float y)
{
	node_t *node;
	int side;
	unsigned int nodenum;

	nodenum = root_node;

	while (!(nodenum & NF_V5_SUBSECTOR))
	{
		node = &nodes[nodenum];
		side = P_PointOnDivlineSide(x, y, &node->div);
		nodenum = node->children[side];
	}

	return &subsectors[nodenum & ~NF_V5_SUBSECTOR];
}


region_properties_t *R_PointGetProps(subsector_t *sub, float z)
{
	extrafloor_t *S, *L, *C;
	float floor_h;

	// traverse extrafloors upwards

	floor_h = sub->sector->f_h;

	S = sub->sector->bottom_ef;
	L = sub->sector->bottom_liq;

	while (S || L)
	{
		if (!L || (S && S->bottom_h < L->bottom_h))
		{
			C = S;  S = S->higher;
		}
		else
		{
			C = L;  L = L->higher;
		}

		SYS_ASSERT(C);

		// ignore liquids in the middle of THICK solids, or below real
		// floor or above real ceiling
		//
		if (C->bottom_h < floor_h || C->bottom_h > sub->sector->c_h)
			continue;

		if (z < C->top_h)
			return C->p;

		floor_h = C->top_h;
	}

	// extrafloors were exhausted, must be top area
	return sub->sector->p;
}

void R_StartFading(int start, int range)
{
	telept_active = true;
	telept_starttic = start + leveltime;
}


//----------------------------------------------------------------------------

static std::vector<drawthing_t  *> drawthings;
static std::vector<drawfloor_t  *> drawfloors;
static std::vector<drawseg_c    *> drawsegs;
static std::vector<drawsub_c    *> drawsubs;
static std::vector<drawmirror_c *> drawmirrors;

static int drawthing_pos;
static int drawfloor_pos;
static int drawseg_pos;
static int drawsub_pos;
static int drawmirror_pos;


//
// R2_InitUtil
//
// One-time initialisation routine.
//
void R2_InitUtil(void)
{
}

// bsp clear function

void R2_ClearBSP(void)
{
	drawthing_pos  = 0;
	drawfloor_pos  = 0;
	drawseg_pos    = 0;
	drawsub_pos    = 0;
	drawmirror_pos = 0;
}

void R2_FreeupBSP(void)
{
	int i;

	for (i=0; i < (int)drawthings .size(); i++) delete drawthings [i];
	for (i=0; i < (int)drawfloors .size(); i++) delete drawfloors [i];
	for (i=0; i < (int)drawsegs   .size(); i++) delete drawsegs   [i];
	for (i=0; i < (int)drawsubs   .size(); i++) delete drawsubs   [i];
	for (i=0; i < (int)drawmirrors.size(); i++) delete drawmirrors[i];

	drawthings .erase(drawthings .begin(), drawthings .end());
	drawfloors .erase(drawfloors .begin(), drawfloors .end());
	drawsegs   .erase(drawsegs   .begin(), drawsegs   .end());
	drawsubs   .erase(drawsubs   .begin(), drawsubs   .end());
	drawmirrors.erase(drawmirrors.begin(), drawmirrors.end());

	R2_ClearBSP();
}


drawthing_t *R_GetDrawThing()
{
	if (drawthing_pos >= (int)drawthings.size())
	{
		drawthings.push_back(new drawthing_t);
	}

	return drawthings[drawthing_pos++];
}

drawfloor_t *R_GetDrawFloor()
{
	if (drawfloor_pos >= (int)drawfloors.size())
	{
		drawfloors.push_back(new drawfloor_t);
	}

	return drawfloors[drawfloor_pos++];
}

drawseg_c *R_GetDrawSeg()
{
	if (drawseg_pos >= (int)drawsegs.size())
	{
		drawsegs.push_back(new drawseg_c);
	}

	return drawsegs[drawseg_pos++];
}

drawsub_c *R_GetDrawSub()
{
	if (drawsub_pos >= (int)drawsubs.size())
	{
		drawsubs.push_back(new drawsub_c);
	}

	return drawsubs[drawsub_pos++];
}

drawmirror_c *R_GetDrawMirror()
{
	if (drawmirror_pos >= (int)drawmirrors.size())
	{
		drawmirrors.push_back(new drawmirror_c);
	}

	return drawmirrors[drawmirror_pos++];
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
