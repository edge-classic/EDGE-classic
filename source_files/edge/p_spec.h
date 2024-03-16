//----------------------------------------------------------------------------
//  EDGE Specials Lines, Elevator & Floor Code
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
// -KM-  1998/09/01 Lines.ddf
// -ACB- 1998/09/13 Cleaned Up.
// -ACB- 2001/01/14 Added Elevator Types
//

#pragma once

#include "main.h"
#include "r_defs.h"
#include "r_image.h"

struct LightSpecial
{
    // type of light effect
    const LightSpecialDefinition *type;

    Sector *sector;

    // countdown value to next change, or 0 if disabled
    int count;

    // dark and bright levels
    int minimum_light;
    int maximum_light;

    // current direction for GLOW type, -1 down, +1 up
    int direction;

    // countdown value for FADE type
    int fade_count;
};

enum ButtonPosition
{
    kButtonNone,
    kButtonTop,
    kButtonMiddle,
    kButtonBottom
};

struct Button
{
    Line               *line;
    ButtonPosition      where;
    const Image        *button_image;
    int                 button_timer;
    struct SoundEffect *off_sound;
};

enum PlaneDirection
{
    kPlaneDirectionUp     = +1,
    kPlaneDirectionWait   = 0,
    kPlaneDirectionDown   = -1,
    kPlaneDirectionStasis = -2
};

struct PlaneMover
{
    const PlaneMoverDefinition *type;
    Sector                     *sector;

    bool is_ceiling;
    bool is_elevator;

    float start_height;
    float destination_height;
    float elevator_height;
    float speed;
    int   crush; // damage, or 0 for no crushing

    // 1 = up, 0 = waiting at top, -1 = down
    int direction;
    int old_direction;

    int tag;

    // tics to wait when fully open
    int waited;

    bool sound_effect_started;

    int          new_special;
    const Image *new_image;

    bool nuke_me = false; // for changers already at their dest height
};

struct SlidingDoorMover
{
    const SlidingDoor *info;
    Line              *line;

    // current distance it has opened
    float opening;

    // target distance
    float target;

    // length of line
    float line_length;

    // 1 = opening, 0 = waiting, -1 = closing
    int direction;

    // tics to wait at the top
    int waited;

    bool sound_effect_started;
    bool final_open;
};

struct Force
{
    bool is_point;
    bool is_wind;

    HMM_Vec3 point;

    float radius;
    float magnitude;

    HMM_Vec2 direction; // wind/current

    Sector *sector;     // the affected sector
};

// End-level timer (-TIMER option)
extern bool level_timer;
extern int  level_time_count;

extern LineType donut[2];

// at map load
void SpawnMapSpecials1(void);
void SpawnMapSpecials2(int autotag);

// every tic
void UpdateSpecials(bool extra_tic);

// when needed
bool UseSpecialLine(MapObject *thing, Line *line, int side, float open_bottom, float open_top);
bool CrossSpecialLine(Line *ld, int side, MapObject *thing);
void ShootSpecialLine(Line *ld, int side, MapObject *thing);
void RemoteActivation(MapObject *thing, int typenum, int tag, int side, LineTrigger method);
void PlayerInSpecialSector(class Player *pl, Sector *sec, bool should_choke = true);

// Utilities...
int     LineIsTwoSided(int sector, int line);
Side   *GetLineSidedef(int currentSector, int line, int side);
Sector *GetLineSector(int currentSector, int line, int side);
Sector *GetLineSectorAdjacent(const Line *line, const Sector *sec, bool ignore_selfref = false);

// Info Needs....
float   FindSurroundingHeight(const TriggerHeightReference ref, const Sector *sec);
float   FindRaiseToTexture(Sector *sec); // -KM- 1998/09/01 New func, old inline
Sector *FindSectorFromTag(int tag);
int     FindMinimumSurroundingLight(Sector *sector, int max);

// start an action...
bool RunSectorLight(Sector *sec, const LightSpecialDefinition *type);

void RunActivePlanes(void);
void RunActiveSliders(void);

void AddActivePlane(PlaneMover *pmov);
void AddActiveSlider(SlidingDoorMover *smov);
void DestroyAllPlanes(void);
void DestroyAllSliders(void);

void AddSpecialLine(Line *ld);
void AddSpecialSector(Sector *sec);
void SectorChangeSpecial(Sector *sec, int new_type);

void          RunLights(void);
LightSpecial *NewLight(void);
void          DestroyAllLights(void);

void RunLineTagLights(int tag, int bright);
bool RunDonutSpecial(Sector *s1, struct SoundEffect *sfx[4]);
bool TeleportMapObject(Line *line, int tag, MapObject *thing, const TeleportDefinition *def);
bool RunManualPlaneMover(Line *line, MapObject *thing, const PlaneMoverDefinition *type);

bool RunPlaneMover(Sector *sec, const PlaneMoverDefinition *type, Sector *model);
bool RunSlidingDoor(Line *door, Line *act_line, MapObject *thing, const LineType *special);
bool SectorIsLowering(Sector *sec);

void RunForces(bool extra_tic);
void DestroyAllForces(void);
void AddPointForce(Sector *sec, float length);
void AddSectorForce(Sector *sec, bool is_wind, float x_mag, float y_mag);

void RunAmbientSounds(void);
void AddAmbientSounds(Sector *sec, struct SoundEffect *sfx);
void DestroyAllAmbientSounds(void);

//
//  P_SWITCH
//
void InitializeSwitchList(void);
void ChangeSwitchTexture(Line *line, bool useAgain, LineSpecial specials, bool noSound);
void ClearButtons(void);
void UpdateButtons(void);
bool ButtonIsPressed(Line *ld);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
