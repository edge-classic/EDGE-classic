//----------------------------------------------------------------------------
//  EDGE Automap Functions
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

#include "con_var.h"
#include "e_event.h"
#include "p_mobj.h"

extern bool   automap_active;
extern bool   rotate_map;
extern bool   automap_keydoor_blink;
extern ConsoleVariable automap_keydoor_text;

struct AutomapPoint
{
    float x, y;
};

struct AutomapLine
{
    AutomapPoint a, b;
};

void AutomapInitLevel(void);

// Called by main loop.
bool AutomapResponder(event_t *ev);

// Called by main loop.
void AutomapTicker(void);

// Called to draw the automap on the screen.
void AutomapRender(float x, float y, float w, float h, mobj_t *focus);

// Called to force the automap to quit
// if the level is completed while it is up.
void AutomapStop(void);

// color setting API

// NOTE: these numbers here must match the COAL API script
enum AutomapColor
{
    kAutomapColorGrid = 0,
    kAutomapColorAllmap,
    kAutomapColorWall,
    kAutomapColorStep,
    kAutomapColorLedge,
    kAutomapColorCeil,
    kAutomapColorSecret,
    kAutomapColorPlayer,
    kAutomapColorMonster,
    kAutomapColorCorpse,
    kAutomapColorItem,
    kAutomapColorMissile,
    kAutomapColorScenery,
    kTotalAutomapColors
};

void AutomapSetColor(int which, RGBAColor color);

// NOTE: the bit numbers here must match the COAL API script
enum AutomapState
{
    kAutomapStateGrid      = (1 << 0),  // draw the grid
    kAutomapStateFollow    = (1 << 4),  // follow the player
    kAutomapStateRotate    = (1 << 5),  // rotate the map (disables grid)
    kAutomapStateHideLines = (1 << 6),  // turn off all line drawing
    kAutomapStateThings    = (1 << 3),  // draw all objects
    kAutomapStateWalls     = (1 << 2),  // draw all walls (like IDDT)
    kAutomapStateAllmap    = (1 << 1),  // draw like Allmap powerup
};

enum AutomapArrowStyle
{
    kAutomapArrowStyleDoom,
    kAutomapArrowStyleHeretic,
    kTotalAutomapArrowStyles
};

void AutomapSetArrow(AutomapArrowStyle type);

void AutomapGetState(int *state, float *zoom);
void AutomapSetState(int state, float zoom);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
