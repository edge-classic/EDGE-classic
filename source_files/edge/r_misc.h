//----------------------------------------------------------------------------
//  EDGE Main Rendering Organisation Code
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
//
// -KM- 1998/09/27 Dynamic Colourmaps.
//

#pragma once

#include "con_var.h"
#include "r_defs.h"

//
// POV related.
//
// Used for Boom 242 height_sector checks
enum ViewHeightZone
{
    kHeightZoneNone,
    kHeightZoneA,
    kHeightZoneB,
    kHeightZoneC
};

extern float    view_cosine;
extern float    view_sine;
extern BAMAngle view_vertical_angle;

extern Subsector        *view_subsector;
extern RegionProperties *view_properties;
extern ViewHeightZone    view_height_zone;

extern int view_window_x;
extern int view_window_y;
extern int view_window_width;
extern int view_window_height;

extern HMM_Vec3 view_forward;
extern HMM_Vec3 view_up;
extern HMM_Vec3 view_right;

extern int valid_count;

extern int line_count;

// -ES- 1999/03/29 Added these
extern BAMAngle normal_field_of_view, zoomed_field_of_view;
extern bool     view_is_zoomed;

extern ConsoleVariable field_of_view;

extern int render_frame_count;

extern MapObject *background_camera_map_object;

extern ConsoleVariable pixel_aspect_ratio;
extern ConsoleVariable monitor_aspect_ratio;

// Values/tables adapted from Quake 3 GPL release
constexpr uint16_t kSineTableSize = 1024;
constexpr uint16_t kSineTableMask = kSineTableSize - 1;

extern float sine_table[kSineTableSize];

//
// Utility functions.
BAMAngle          PointToAngle(float x1, float y1, float x2, float y2, bool precise = false);
float             PointToDistance(float x1, float y1, float x2, float y2);
Subsector        *PointInSubsector(float x, float y);
RegionProperties *GetPointProperties(Subsector *sub, float z);

//
// REFRESH - the actual rendering functions.
//

// Renders the view for the next frame.
void RenderView(int x, int y, int w, int h, MapObject *camera, bool full_height, float expand_w);

// Called by startup code.
void RendererStartup(void);
// Called by shutdown code
void RendererShutdown(void);

// Called during level shutdown
void RendererShutdownLevel();

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
