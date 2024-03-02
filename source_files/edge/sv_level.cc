//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Level Data)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// This file handles:
//    surface_t      [SURF]
//    side_t         [SIDE]
//    line_t         [LINE]
//
//    region_properties_t  [RPRP]
//    extrafloor_t         [EXFL]
//    sector_t             [SECT]
//



#include <stdio.h>
#include <stdlib.h>

#include "str_util.h"
#include "str_compare.h"
#include "colormap.h"

#include "r_image.h"
#include "sv_chunk.h"
#include "sv_main.h"

#undef SF
#define SF SVFIELD

// forward decls.
int   SV_SideCountElems(void);
int   SV_SideFindElem(Side *elem);
void *SV_SideGetElem(int index);
void  SV_SideCreateElems(int num_elems);
void  SV_SideFinaliseElems(void);

int   SV_LineCountElems(void);
int   SV_LineFindElem(Line *elem);
void *SV_LineGetElem(int index);
void  SV_LineCreateElems(int num_elems);
void  SV_LineFinaliseElems(void);

int   SV_ExfloorCountElems(void);
int   SV_ExfloorFindElem(Extrafloor *elem);
void *SV_ExfloorGetElem(int index);
void  SV_ExfloorCreateElems(int num_elems);
void  SV_ExfloorFinaliseElems(void);

int   SV_SectorCountElems(void);
int   SV_SectorFindElem(Sector *elem);
void *SV_SectorGetElem(int index);
void  SV_SectorCreateElems(int num_elems);
void  SV_SectorFinaliseElems(void);

bool SR_LevelGetImage(void *storage, int index, void *extra);
bool SR_LevelGetColmap(void *storage, int index, void *extra);
bool SR_LevelGetSurface(void *storage, int index, void *extra);
bool SR_LevelGetSurfPtr(void *storage, int index, void *extra);
bool SR_LineGetSpecial(void *storage, int index, void *extra);
bool SR_SectorGetSpecial(void *storage, int index, void *extra);
bool SR_SectorGetProps(void *storage, int index, void *extra);
bool SR_SectorGetPropRef(void *storage, int index, void *extra);

void SR_LevelPutImage(void *storage, int index, void *extra);
void SR_LevelPutColmap(void *storage, int index, void *extra);
void SR_LevelPutSurface(void *storage, int index, void *extra);
void SR_LevelPutSurfPtr(void *storage, int index, void *extra);
void SR_LinePutSpecial(void *storage, int index, void *extra);
void SR_SectorPutSpecial(void *storage, int index, void *extra);
void SR_SectorPutProps(void *storage, int index, void *extra);
void SR_SectorPutPropRef(void *storage, int index, void *extra);

bool SR_SideGetSide(void *storage, int index, void *extra);
void SR_SidePutSide(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  SURFACE STRUCTURE
//
static MapSurface sv_dummy_surface;

#define SV_F_BASE sv_dummy_surface

static savefield_t sv_fields_surface[] = {
    SF(image, "image", 1, SVT_STRING, SR_LevelGetImage, SR_LevelPutImage),
    SF(translucency, "translucency", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),

    SF(offset, "offset", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),
    SF(scroll, "scroll", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),
    SF(x_matrix, "x_mat", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),
    SF(y_matrix, "y_mat", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),

    SF(net_scroll, "net_scroll", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),
    SF(old_scroll, "old_scroll", 1, SVT_VEC2, SR_GetVec2, SR_PutVec2),

    SF(override_properties, "override_p", 1, SVT_STRING, SR_SectorGetPropRef, SR_SectorPutPropRef),

    SVFIELD_END};

savestruct_t sv_struct_surface = {
    nullptr,              // link in list
    "surface_t",       // structure name
    "surf",            // start marker
    sv_fields_surface, // field descriptions
    SVDUMMY,           // dummy base
    true,              // define_me
    nullptr               // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  SIDE STRUCTURE
//
static Side sv_dummy_side;

#define SV_F_BASE sv_dummy_side

static savefield_t sv_fields_side[] = {
    SF(top, "top", 1, SVT_STRUCT("surface_t"), SR_LevelGetSurface, SR_LevelPutSurface),
    SF(middle, "middle", 1, SVT_STRUCT("surface_t"), SR_LevelGetSurface, SR_LevelPutSurface),
    SF(bottom, "bottom", 1, SVT_STRUCT("surface_t"), SR_LevelGetSurface, SR_LevelPutSurface),

    // NOT HERE:
    //   sector: value is kept from level load.

    SVFIELD_END};

savestruct_t sv_struct_side = {
    nullptr,           // link in list
    "side_t",       // structure name
    "side",         // start marker
    sv_fields_side, // field descriptions
    SVDUMMY,        // dummy base
    true,           // define_me
    nullptr            // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_side = {
    nullptr,            // link in list
    "level_sides",         // array name
    &sv_struct_side, // array type
    true,            // define_me
    true,            // allow_hub

    SV_SideCountElems,    // count routine
    SV_SideGetElem,       // index routine
    SV_SideCreateElems,   // creation routine
    SV_SideFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------
//
//  LINE STRUCTURE
//
static Line sv_dummy_line;

#define SV_F_BASE sv_dummy_line

static savefield_t sv_fields_line[] = {
    SF(flags, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt), SF(tag, "tag", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(count, "count", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(side, "side", 1, SVT_INDEX("level_sides"), SR_SideGetSide, SR_SidePutSide),
    SF(special, "special", 1, SVT_STRING, SR_LineGetSpecial, SR_LinePutSpecial),
    SF(slide_door, "slide_door", 1, SVT_STRING, SR_LineGetSpecial, SR_LinePutSpecial),
    SF(old_stored, "old_stored", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),

    // NOT HERE:
    //   (many): values are kept from level load.
    //   gap stuff: regenerated from sector heights.
    //   valid_count: only a temporary value for some routines.
    //   slider_move: regenerated by a pass of the active part list.

    SVFIELD_END};

savestruct_t sv_struct_line = {
    nullptr,           // link in list
    "line_t",       // structure name
    "line",         // start marker
    sv_fields_line, // field descriptions
    SVDUMMY,        // dummy base
    true,           // define_me
    nullptr            // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_line = {
    nullptr,            // link in list
    "level_lines",         // array name
    &sv_struct_line, // array type
    true,            // define_me
    true,            // allow_hub

    SV_LineCountElems,    // count routine
    SV_LineGetElem,       // index routine
    SV_LineCreateElems,   // creation routine
    SV_LineFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------
//
//  REGION_PROPERTIES STRUCTURE
//
static RegionProperties sv_dummy_regprops;

#define SV_F_BASE sv_dummy_regprops

static savefield_t sv_fields_regprops[] = {
    SF(light_level, "lightlevel_i", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(colourmap, "colourmap", 1, SVT_STRING, SR_LevelGetColmap, SR_LevelPutColmap),

    SF(type, "type", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(special, "special", 1, SVT_STRING, SR_SectorGetSpecial, SR_SectorPutSpecial),
    SF(secret_found, "secret_found", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),

    SF(gravity, "gravity", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(friction, "friction", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(viscosity, "viscosity", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(drag, "drag", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(push, "push", 1, SVT_VEC3, SR_GetVec3, SR_PutVec3),
    SF(net_push, "net_push", 1, SVT_VEC3, SR_GetVec3, SR_PutVec3),
    SF(old_push, "old_push", 1, SVT_VEC3, SR_GetVec3, SR_PutVec3),
    SF(fog_color, "fog_color", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(fog_density, "fog_density", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),

    SVFIELD_END};

savestruct_t sv_struct_regprops = {
    nullptr,                  // link in list
    "region_properties_t", // structure name
    "rprp",                // start marker
    sv_fields_regprops,    // field descriptions
    SVDUMMY,               // dummy base
    true,                  // define_me
    nullptr                   // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  EXTRAFLOOR STRUCTURE
//
static Extrafloor sv_dummy_exfloor;

#define SV_F_BASE sv_dummy_exfloor

static savefield_t sv_fields_exfloor[] = {
    SF(higher, "higher", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(lower, "lower", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(sector, "sector", 1, SVT_INDEX("level_sectors"), SR_SectorGetSector, SR_SectorPutSector),

    SF(top_height, "top_h", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(bottom_height, "bottom_h", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(top, "top", 1, SVT_STRING, SR_LevelGetSurfPtr, SR_LevelPutSurfPtr),
    SF(bottom, "bottom", 1, SVT_STRING, SR_LevelGetSurfPtr, SR_LevelPutSurfPtr),

    SF(properties, "p", 1, SVT_STRING, SR_SectorGetPropRef, SR_SectorPutPropRef),
    SF(extrafloor_line, "extrafloor_line", 1, SVT_INDEX("level_lines"), SR_LineGetLine, SR_LinePutLine),
    SF(control_sector_next, "control_sector_next", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),

    // NOT HERE:
    //   - sector: can be regenerated.
    //   - ef_info: cached value, regenerated from extrafloor_line.

    SVFIELD_END};

savestruct_t sv_struct_exfloor = {
    nullptr,              // link in list
    "extrafloor_t",    // structure name
    "exfl",            // start marker
    sv_fields_exfloor, // field descriptions
    SVDUMMY,           // dummy base
    true,              // define_me
    nullptr               // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_exfloor = {
    nullptr,               // link in list
    "level_extrafloors",      // array name
    &sv_struct_exfloor, // array type
    true,               // define_me
    true,               // allow_hub

    SV_ExfloorCountElems,    // count routine
    SV_ExfloorGetElem,       // index routine
    SV_ExfloorCreateElems,   // creation routine
    SV_ExfloorFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------
//
//  SECTOR STRUCTURE
//
static Sector sv_dummy_sector;

#define SV_F_BASE sv_dummy_sector

static savefield_t sv_fields_sector[] = {
    SF(floor, "floor", 1, SVT_STRUCT("surface_t"), SR_LevelGetSurface, SR_LevelPutSurface),
    SF(ceiling, "ceil", 1, SVT_STRUCT("surface_t"), SR_LevelGetSurface, SR_LevelPutSurface),
    SF(floor_height, "floor_height", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat), SF(ceiling_height, "ceiling_height", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),

    SF(properties, "props", 1, SVT_STRUCT("region_properties_t"), SR_SectorGetProps, SR_SectorPutProps),
    SF(active_properties, "p", 1, SVT_STRING, SR_SectorGetPropRef, SR_SectorPutPropRef),

    SF(extrafloor_used, "extrafloor_used", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(control_floors, "control_floors", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(sound_player, "sound_player", 1, SVT_INT, SR_GetInt, SR_PutInt),

    SF(bottom_extrafloor, "bottom_extrafloor", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(top_extrafloor, "top_extrafloor", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(bottom_liquid, "bottom_liquid", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(top_liquid, "top_liquid", 1, SVT_INDEX("level_extrafloors"), SR_SectorGetEF, SR_SectorPutEF),
    SF(old_stored, "old_stored", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),

    // NOT HERE:
    //   - floor_move, ceiling_move: can be regenerated
    //   - (many): values remaining from level load are OK
    //   - soundtraversed & valid_count: temp values, don't need saving

    SVFIELD_END};

savestruct_t sv_struct_sector = {
    nullptr,             // link in list
    "sector_t",       // structure name
    "sect",           // start marker
    sv_fields_sector, // field descriptions
    SVDUMMY,          // dummy base
    true,             // define_me
    nullptr              // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_sector = {
    nullptr,              // link in list
    "level_sectors",         // array name
    &sv_struct_sector, // array type
    true,              // define_me
    true,              // allow_hub

    SV_SectorCountElems,    // count routine
    SV_SectorGetElem,       // index routine
    SV_SectorCreateElems,   // creation routine
    SV_SectorFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------

int SV_SideCountElems(void)
{
    return total_level_sides;
}

void *SV_SideGetElem(int index)
{
    if (index < 0 || index >= total_level_sides)
    {
        LogWarning("LOADGAME: Invalid Side: %d\n", index);
        index = 0;
    }

    return level_sides + index;
}

int SV_SideFindElem(Side *elem)
{
    SYS_ASSERT(level_sides <= elem && elem < (level_sides + total_level_sides));

    return elem - level_sides;
}

void SV_SideCreateElems(int num_elems)
{
    /* nothing much to do -- sides created from level load, and defaults
     * are initialised there.
     */

    if (num_elems != total_level_sides)
        FatalError("LOADGAME: SIDE MISMATCH !  (%d != %d)\n", num_elems, total_level_sides);
}

void SV_SideFinaliseElems(void)
{
    /* nothing to do */
}

//----------------------------------------------------------------------------

extern std::vector<SlidingDoorMover *> active_sliders;

int SV_LineCountElems(void)
{
    return total_level_lines;
}

void *SV_LineGetElem(int index)
{
    if (index < 0 || index >= total_level_lines)
    {
        LogWarning("LOADGAME: Invalid Line: %d\n", index);
        index = 0;
    }

    return level_lines + index;
}

int SV_LineFindElem(Line *elem)
{
    SYS_ASSERT(level_lines <= elem && elem < (level_lines + total_level_lines));

    return elem - level_lines;
}

void SV_LineCreateElems(int num_elems)
{
    // nothing much to do -- lines are created from level load,
    // and defaults are initialised there.

    if (num_elems != total_level_lines)
        FatalError("LOADGAME: LINE MISMATCH !  (%d != %d)\n", num_elems, total_level_lines);
}

//
// NOTE: line gaps done in Sector finaliser.
//
void SV_LineFinaliseElems(void)
{
    for (int i = 0; i < total_level_lines; i++)
    {
        Line *ld = level_lines + i;
        Side *s1, *s2;

        s1 = ld->side[0];
        s2 = ld->side[1];

        // check for animation
        if (s1 && (s1->top.scroll.X || s1->top.scroll.Y || s1->middle.scroll.X || s1->middle.scroll.Y ||
                   s1->bottom.scroll.X || s1->bottom.scroll.Y || s1->top.net_scroll.X || s1->top.net_scroll.Y ||
                   s1->middle.net_scroll.X || s1->middle.net_scroll.Y || s1->bottom.net_scroll.X ||
                   s1->bottom.net_scroll.Y || s1->top.old_scroll.X || s1->top.old_scroll.Y || s1->middle.old_scroll.X ||
                   s1->middle.old_scroll.Y || s1->bottom.old_scroll.X || s1->bottom.old_scroll.Y))
        {
            AddSpecialLine(ld);
        }

        if (s2 && (s2->top.scroll.X || s2->top.scroll.Y || s2->middle.scroll.X || s2->middle.scroll.Y ||
                   s2->bottom.scroll.X || s2->bottom.scroll.Y || s2->top.net_scroll.X || s2->top.net_scroll.Y ||
                   s2->middle.net_scroll.X || s2->middle.net_scroll.Y || s2->bottom.net_scroll.X ||
                   s2->bottom.net_scroll.Y || s2->top.old_scroll.X || s2->top.old_scroll.Y || s2->middle.old_scroll.X ||
                   s2->middle.old_scroll.Y || s2->bottom.old_scroll.X || s2->bottom.old_scroll.Y))
        {
            AddSpecialLine(ld);
        }
    }

    // scan active parts, regenerate slider_move field
    std::vector<SlidingDoorMover *>::iterator SMI;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end(); SMI++)
    {
        SYS_ASSERT((*SMI)->line);

        (*SMI)->line->slider_move = (*SMI);
    }
}

//----------------------------------------------------------------------------

int SV_ExfloorCountElems(void)
{
    return total_level_extrafloors;
}

void *SV_ExfloorGetElem(int index)
{
    if (index < 0 || index >= total_level_extrafloors)
    {
        LogWarning("LOADGAME: Invalid Extrafloor: %d\n", index);
        index = 0;
    }

    return level_extrafloors + index;
}

int SV_ExfloorFindElem(Extrafloor *elem)
{
    SYS_ASSERT(level_extrafloors <= elem && elem < (level_extrafloors + total_level_extrafloors));

    return elem - level_extrafloors;
}

void SV_ExfloorCreateElems(int num_elems)
{
    /* nothing much to do -- extrafloors are created from level load, and
     * defaults are initialised there.
     */

    if (num_elems != total_level_extrafloors)
        FatalError("LOADGAME: Extrafloor MISMATCH !  (%d != %d)\n", num_elems, total_level_extrafloors);
}

void SV_ExfloorFinaliseElems(void)
{
    int i;

    // need to regenerate the ef_info fields
    for (i = 0; i < total_level_extrafloors; i++)
    {
        Extrafloor *ef = level_extrafloors + i;

        // skip unused extrafloors
        if (ef->extrafloor_line == nullptr)
            continue;

        if (!ef->extrafloor_line->special || !(ef->extrafloor_line->special->ef_.type_ & kExtraFloorTypePresent))
        {
            LogWarning("LOADGAME: Missing Extrafloor Special !\n");
            ef->extrafloor_definition = &linetypes.Lookup(0)->ef_;
            continue;
        }

        ef->extrafloor_definition = &ef->extrafloor_line->special->ef_;
    }
}

//----------------------------------------------------------------------------

extern std::vector<PlaneMover *> active_planes;

int SV_SectorCountElems(void)
{
    return total_level_sectors;
}

void *SV_SectorGetElem(int index)
{
    if (index < 0 || index >= total_level_sectors)
    {
        LogWarning("LOADGAME: Invalid Sector: %d\n", index);
        index = 0;
    }

    return level_sectors + index;
}

int SV_SectorFindElem(Sector *elem)
{
    SYS_ASSERT(level_sectors <= elem && elem < (level_sectors + total_level_sectors));

    return elem - level_sectors;
}

void SV_SectorCreateElems(int num_elems)
{
    // nothing much to do -- sectors are created from level load,
    // and defaults are initialised there.

    if (num_elems != total_level_sectors)
        FatalError("LOADGAME: SECTOR MISMATCH !  (%d != %d)\n", num_elems, total_level_sectors);

    // clear animate list
}

void SV_SectorFinaliseElems(void)
{
    for (int i = 0; i < total_level_sectors; i++)
    {
        Sector *sec = level_sectors + i;

        RecomputeGapsAroundSector(sec);
        ///---	P_RecomputeTilesInSector(sec);
        FloodExtraFloors(sec);

        // check for animation
        if (sec->floor.scroll.X || sec->floor.scroll.Y || sec->ceiling.scroll.X || sec->ceiling.scroll.Y ||
            sec->floor.net_scroll.X || sec->floor.net_scroll.Y || sec->ceiling.net_scroll.X || sec->ceiling.net_scroll.Y ||
            sec->floor.old_scroll.X || sec->floor.old_scroll.Y || sec->ceiling.old_scroll.X || sec->ceiling.old_scroll.Y)
        {
            AddSpecialSector(sec);
        }
    }

    extern std::vector<LineAnimation> line_animations;

    for (size_t i = 0; i < line_animations.size(); i++)
    {
        if (line_animations[i].scroll_sector_reference)
        {
            line_animations[i].scroll_sector_reference->ceiling_move  = nullptr;
            line_animations[i].scroll_sector_reference->floor_move = nullptr;
        }
    }

    extern std::vector<LightAnimation> light_animations;

    for (size_t i = 0; i < light_animations.size(); i++)
    {
        if (light_animations[i].light_sector_reference)
        {
            light_animations[i].light_sector_reference->ceiling_move = nullptr;
        }
    }

    // scan active parts, regenerate floor_move and ceiling_move
    std::vector<PlaneMover *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        PlaneMover *pmov = *PMI;

        SYS_ASSERT(pmov->sector);

        if (pmov->is_ceiling)
            pmov->sector->ceiling_move = pmov;
        else
            pmov->sector->floor_move = pmov;
    }
}

//----------------------------------------------------------------------------

bool SR_LevelGetSurface(void *storage, int index, void *extra)
{
    MapSurface *dest = (MapSurface *)storage + index;

    if (!sv_struct_surface.counterpart)
        return true;

    return SV_LoadStruct(dest, sv_struct_surface.counterpart);
}

void SR_LevelPutSurface(void *storage, int index, void *extra)
{
    MapSurface *src = (MapSurface *)storage + index;

    // force fogwall recreation when loading a save
    if (src->fog_wall)
        src->image = nullptr;

    SV_SaveStruct(src, &sv_struct_surface);
}

bool SR_LevelGetSurfPtr(void *storage, int index, void *extra)
{
    MapSurface **dest = (MapSurface **)storage + index;

    const char *str;
    int         num;

    str = SV_GetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':')
        FatalError("SR_LevelGetSurfPtr: invalid surface string `%s'\n", str);

    num = strtol(str + 2, nullptr, 0);

    if (num < 0 || num >= total_level_sectors)
    {
        LogWarning("SR_LevelGetSurfPtr: bad sector ref %d\n", num);
        num = 0;
    }

    if (str[0] == 'F')
        (*dest) = &level_sectors[num].floor;
    else if (str[0] == 'C')
        (*dest) = &level_sectors[num].ceiling;
    else
        FatalError("SR_LevelGetSurfPtr: invalid surface plane `%s'\n", str);

    SV_FreeString(str);
    return true;
}

//
// Format of the string:
//
//    <floor/ceil>  `:'  <sector num>
//
// The first character is `F' for the floor surface of the sector,
// otherwise `C' for its ceiling.
//
void SR_LevelPutSurfPtr(void *storage, int index, void *extra)
{
    MapSurface *src = ((MapSurface **)storage)[index];

    char buffer[64];
    int  i;

    if (!src)
    {
        SV_PutString(nullptr);
        return;
    }

    // not optimal, but safe
    for (i = 0; i < total_level_sectors; i++)
    {
        if (src == &level_sectors[i].floor)
        {
            sprintf(buffer, "F:%d", i);
            SV_PutString(buffer);
            return;
        }
        else if (src == &level_sectors[i].ceiling)
        {
            sprintf(buffer, "C:%d", i);
            SV_PutString(buffer);
            return;
        }
    }

    LogWarning("SR_LevelPutSurfPtr: surface %p not found !\n", src);
    SV_PutString("F:0");
}

bool SR_LevelGetImage(void *storage, int index, void *extra)
{
    const image_c **dest = (const image_c **)storage + index;
    const char     *str;

    str = SV_GetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':')
        LogWarning("SR_LevelGetImage: invalid image string `%s'\n", str);

    (*dest) = W_ImageParseSaveString(str[0], str + 2);

    SV_FreeString(str);
    return true;
}

//
// Format of the string is:
//
//   <type char>  `:'  <name>
//
// The type character is `F' for flat, `T' for texture, etc etc..
// Also `*' is valid and means that type is not important.  Some
// examples: "F:FLAT10" and "T:STARTAN3".
//
void SR_LevelPutImage(void *storage, int index, void *extra)
{
    const image_c *src = ((const image_c **)storage)[index];

    char buffer[64];

    if (!src)
    {
        SV_PutString(nullptr);
        return;
    }

    W_ImageMakeSaveString(src, buffer, buffer + 2);
    buffer[1] = ':';

    SV_PutString(buffer);
}

bool SR_LevelGetColmap(void *storage, int index, void *extra)
{
    const Colormap **dest = (const Colormap **)storage + index;
    const char         *str;

    str = SV_GetString();

    if (str)
        (*dest) = colormaps.Lookup(str);
    else
        (*dest) = nullptr;

    // -AJA- 2008/03/15: backwards compatibility
    if (*dest && epi::StringCaseCompareASCII((*dest)->name_, "NORMAL") == 0)
        *dest = nullptr;

    SV_FreeString(str);
    return true;
}

//
// The string is the name of the colourmap.
//
void SR_LevelPutColmap(void *storage, int index, void *extra)
{
    const Colormap *src = ((const Colormap **)storage)[index];

    if (src)
        SV_PutString(src->name_.c_str());
    else
        SV_PutString(nullptr);
}

bool SR_LineGetSpecial(void *storage, int index, void *extra)
{
    const LineType **dest = (const LineType **)storage + index;
    const char        *str;

    str = SV_GetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[0] != ':')
        FatalError("SR_LineGetSpecial: invalid special `%s'\n", str);

    (*dest) = P_LookupLineType(strtol(str + 1, nullptr, 0));

    SV_FreeString(str);
    return true;
}

//
// Format of the string will usually be a colon followed by the
// linedef number (e.g. ":123").  Alternatively it can be the ddf
// name, but this shouldn't be needed currently (reserved for future
// use).
//
void SR_LinePutSpecial(void *storage, int index, void *extra)
{
    const LineType *src = ((const LineType **)storage)[index];

    if (!src)
    {
        SV_PutString(nullptr);
        return;
    }

    std::string s = epi::StringFormat(":%d", src->number_);

    SV_PutString(s.c_str());
}

bool SR_SectorGetSpecial(void *storage, int index, void *extra)
{
    const SectorType **dest = (const SectorType **)storage + index;
    const char          *str;

    str = SV_GetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[0] != ':')
        FatalError("SR_SectorGetSpecial: invalid special `%s'\n", str);

    (*dest) = P_LookupSectorType(strtol(str + 1, nullptr, 0));

    SV_FreeString(str);
    return true;
}

//
// Format of the string will usually be a colon followed by the
// sector number (e.g. ":123").  Alternatively it can be the ddf
// name, but this shouldn't be needed currently (reserved for future
// use).
//
void SR_SectorPutSpecial(void *storage, int index, void *extra)
{
    const SectorType *src = ((const SectorType **)storage)[index];

    if (!src)
    {
        SV_PutString(nullptr);
        return;
    }

    std::string s = epi::StringFormat(":%d", src->number_);

    SV_PutString(s.c_str());
}

//----------------------------------------------------------------------------

bool SR_SectorGetProps(void *storage, int index, void *extra)
{
    RegionProperties *dest = (RegionProperties *)storage + index;

    if (!sv_struct_regprops.counterpart)
        return true;

    return SV_LoadStruct(dest, sv_struct_regprops.counterpart);
}

void SR_SectorPutProps(void *storage, int index, void *extra)
{
    RegionProperties *src = (RegionProperties *)storage + index;

    SV_SaveStruct(src, &sv_struct_regprops);
}

bool SR_SectorGetPropRef(void *storage, int index, void *extra)
{
    RegionProperties **dest = (RegionProperties **)storage + index;

    const char *str;
    int         num;

    str = SV_GetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    num = strtol(str, nullptr, 0);

    if (num < 0 || num >= total_level_sectors)
    {
        LogWarning("SR_SectorGetPropRef: bad sector ref %d\n", num);
        num = 0;
    }

    (*dest) = &level_sectors[num].properties;

    SV_FreeString(str);
    return true;
}

//
// Format of the string is just the sector number containing the
// properties.
//
void SR_SectorPutPropRef(void *storage, int index, void *extra)
{
    RegionProperties *src = ((RegionProperties **)storage)[index];

    char buffer[64];
    int  i;

    if (!src)
    {
        SV_PutString(nullptr);
        return;
    }

    // not optimal, but safe
    for (i = 0; i < total_level_sectors; i++)
    {
        if (&level_sectors[i].properties == src)
            break;
    }

    if (i >= total_level_sectors)
    {
        LogWarning("SR_SectorPutPropRef: properties %p not found !\n", src);
        i = 0;
    }

    sprintf(buffer, "%d", i);
    SV_PutString(buffer);
}

bool SR_LineGetLine(void *storage, int index, void *extra)
{
    Line **dest = (Line **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (Line *)((swizzle == 0) ? nullptr : SV_LineGetElem(swizzle - 1));
    return true;
}

void SR_LinePutLine(void *storage, int index, void *extra)
{
    Line *elem = ((Line **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_LineFindElem(elem) + 1;

    SV_PutInt(swizzle);
}

bool SR_SideGetSide(void *storage, int index, void *extra)
{
    Side **dest = (Side **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (Side *)((swizzle == 0) ? nullptr : SV_SideGetElem(swizzle - 1));
    return true;
}

void SR_SidePutSide(void *storage, int index, void *extra)
{
    Side *elem = ((Side **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_SideFindElem(elem) + 1;

    SV_PutInt(swizzle);
}

bool SR_SectorGetSector(void *storage, int index, void *extra)
{
    Sector **dest = (Sector **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (Sector *)((swizzle == 0) ? nullptr : SV_SectorGetElem(swizzle - 1));
    return true;
}

void SR_SectorPutSector(void *storage, int index, void *extra)
{
    Sector *elem = ((Sector **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_SectorFindElem(elem) + 1;

    SV_PutInt(swizzle);
}

bool SR_SectorGetEF(void *storage, int index, void *extra)
{
    Extrafloor **dest = (Extrafloor **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (Extrafloor *)((swizzle == 0) ? nullptr : SV_ExfloorGetElem(swizzle - 1));
    return true;
}

void SR_SectorPutEF(void *storage, int index, void *extra)
{
    Extrafloor *elem = ((Extrafloor **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_ExfloorFindElem(elem) + 1;

    SV_PutInt(swizzle);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
