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
// -KM- 1998/09/27 Dynamic Colourmaps.
//

#ifndef __R_MAIN_H__
#define __R_MAIN_H__

#include "w_flat.h"
#include "r_defs.h"

//
// POV related.
//
extern float viewcos;
extern float viewsin;
extern angle_t viewvertangle;

extern subsector_t *viewsubsector;
extern region_properties_t *view_props;

extern int viewwindow_x;
extern int viewwindow_y;
extern int viewwindow_w;
extern int viewwindow_h;

extern vec3_t viewforward;
extern vec3_t viewup;
extern vec3_t viewright;

extern int validcount;

extern int linecount;

// -ES- 1999/03/29 Added these
extern angle_t normalfov, zoomedfov;
extern bool viewiszoomed;

extern cvar_c r_fov;

extern int framecount;

extern struct mobj_s *background_camera_mo;


#define DOOM_SCREEN_ASPECT  (320.0f / 200.0f)
#define DOOM_PIXEL_ASPECT   (5.0f / 6.0f)

extern cvar_c v_pixelaspect;
extern cvar_c v_monitorsize;

// Values/tables adapted from Quake 3 GPL release
#define FUNCTABLE_SIZE 1024
#define FUNCTABLE_MASK FUNCTABLE_SIZE - 1
#define DEG2RAD( a ) (( a * M_PI ) / 180.0f)

extern float *r_sintable;
extern float *r_squaretable;
extern float *r_sawtoothtable;
extern float *r_inversesawtoothtable;
extern float *r_triangletable;

//
// Utility functions.
angle_t R_PointToAngle(float x1, float y1, float x2, float y2);
float R_PointToDist(float x1, float y1, float x2, float y2);
float R_ScaleFromGlobalAngle(angle_t visangle);
subsector_t *R_PointInSubsector(float x, float y);
region_properties_t *R_PointGetProps(subsector_t *sub, float z);
void R_InitShaderTables();

//
// REFRESH - the actual rendering functions.
//

// Renders the view for the next frame.
void R_Render(int x, int y, int w, int h, mobj_t *camera,
              bool full_height, float expand_w);

// Called by startup code.
void R_Init(void);
// Called by shutdown code
void R_Shutdown(void);

// -ES- 1998/09/11 Added these prototypes.
void R_SetViewSize(int blocks);

#endif /* __R_MAIN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
