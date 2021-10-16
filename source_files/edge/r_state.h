//----------------------------------------------------------------------------
//  EDGE Refresh internal state variables
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

#ifndef __R_STATE_H__
#define __R_STATE_H__

// Need data structure definitions.
#include "w_flat.h"
#include "m_math.h"
#include "r_defs.h"

//
// Lookup tables for map data.
//
extern int numvertexes;
extern vec2_t *vertexes;

extern int num_gl_vertexes;
extern vec2_t *gl_vertexes;

extern int numsegs;
extern seg_t *segs;

extern int numsectors;
extern sector_t *sectors;

extern int numsubsectors;
extern subsector_t *subsectors;

extern int numextrafloors;
extern extrafloor_t *extrafloors;
     
extern int numnodes;
extern node_t *nodes;

extern int numlines;
extern line_t *lines;

extern int numsides;
extern side_t *sides;

extern int numvertgaps;
extern vgap_t *vertgaps;

//
// POV data.
//
extern float viewx;
extern float viewy;
extern float viewz;

extern angle_t viewangle;

// -ES- 1999/03/20 Added these.
// Angles that are used for linedef clipping.
// Nearly the same as leftangle/rightangle, but slightly rounded to fit
// viewangletox lookups, and converted to BAM format.

// angles used for clipping
extern angle_t clip_left, clip_right;

// the scope of the clipped area (clip_left-clip_right).
// ANG180 disables polar clipping
extern angle_t clip_scope;

// the most extreme angles of the view
extern float view_x_slope, view_y_slope;

#endif /* __R_STATE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
