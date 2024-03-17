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

#include "ddf_colormap.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "r_image.h"
#include "sv_chunk.h"
#include "sv_main.h"

// forward decls.
int   SV_SideCountElems(void);
int   SV_SideGetIndex(Side *elem);
void *SV_SideFindByIndex(int index);
void  SV_SideCreateElems(int num_elems);
void  SV_SideFinaliseElems(void);

int   SV_LineCountElems(void);
int   SV_LineGetIndex(Line *elem);
void *SV_LineFindByIndex(int index);
void  SV_LineCreateElems(int num_elems);
void  SV_LineFinaliseElems(void);

int   SV_ExfloorCountElems(void);
int   SV_ExfloorGetIndex(Extrafloor *elem);
void *SV_ExfloorFindByIndex(int index);
void  SV_ExfloorCreateElems(int num_elems);
void  SV_ExfloorFinaliseElems(void);

int   SV_SectorCountElems(void);
int   SV_SectorGetIndex(Sector *elem);
void *SV_SectorFindByIndex(int index);
void  SV_SectorCreateElems(int num_elems);
void  SV_SectorFinaliseElems(void);

bool SaveGameLevelGetImage(void *storage, int index, void *extra);
bool SaveGameLevelGetColormap(void *storage, int index, void *extra);
bool SaveGameLevelGetSurface(void *storage, int index, void *extra);
bool SaveGameLevelGetSurfPtr(void *storage, int index, void *extra);
bool SaveGameLineGetSpecial(void *storage, int index, void *extra);
bool SaveGameSectorGetSpecial(void *storage, int index, void *extra);
bool SaveGameSectorGetProps(void *storage, int index, void *extra);
bool SaveGameSectorGetPropRef(void *storage, int index, void *extra);

void SaveGameLevelPutImage(void *storage, int index, void *extra);
void SaveGameLevelPutColormap(void *storage, int index, void *extra);
void SaveGameLevelPutSurface(void *storage, int index, void *extra);
void SaveGameLevelPutSurfPtr(void *storage, int index, void *extra);
void SaveGameLinePutSpecial(void *storage, int index, void *extra);
void SaveGameSectorPutSpecial(void *storage, int index, void *extra);
void SaveGameSectorPutProps(void *storage, int index, void *extra);
void SaveGameSectorPutPropRef(void *storage, int index, void *extra);

bool SR_SideGetSide(void *storage, int index, void *extra);
void SR_SidePutSide(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  SURFACE STRUCTURE
//
static MapSurface dummy_map_surface;

static SaveField sv_fields_surface[] = {
    EDGE_SAVE_FIELD(dummy_map_surface, image, "image", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetImage,
                    SaveGameLevelPutImage),
    EDGE_SAVE_FIELD(dummy_map_surface, translucency, "translucency", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_surface, offset, "offset", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, scroll, "scroll", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, x_matrix, "x_mat", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, y_matrix, "y_mat", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, net_scroll, "net_scroll", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, old_scroll, "old_scroll", 1, kSaveFieldNumeric, 8, nullptr, SaveGameGetVec2,
                    SaveGamePutVec2),
    EDGE_SAVE_FIELD(dummy_map_surface, override_properties, "override_p", 1, kSaveFieldString, 0, nullptr,
                    SaveGameSectorGetPropRef, SaveGameSectorPutPropRef),
    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_surface = {
    nullptr,                          // link in list
    "surface_t",                      // structure name
    "surf",                           // start marker
    sv_fields_surface,                // field descriptions
    (const char *)&dummy_map_surface, // dummy base
    true,                             // define_me
    nullptr                           // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  SIDE STRUCTURE
//
static Side dummy_side;

static SaveField sv_fields_side[] = {EDGE_SAVE_FIELD(dummy_side, top, "top", 1, kSaveFieldStruct, 0, "surface_t",
                                                     SaveGameLevelGetSurface, SaveGameLevelPutSurface),
                                     EDGE_SAVE_FIELD(dummy_side, middle, "middle", 1, kSaveFieldStruct, 0, "surface_t",
                                                     SaveGameLevelGetSurface, SaveGameLevelPutSurface),
                                     EDGE_SAVE_FIELD(dummy_side, bottom, "bottom", 1, kSaveFieldStruct, 0, "surface_t",
                                                     SaveGameLevelGetSurface, SaveGameLevelPutSurface),

                                     // NOT HERE:
                                     //   sector: value is kept from level load.

                                     {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_side = {
    nullptr,                   // link in list
    "side_t",                  // structure name
    "side",                    // start marker
    sv_fields_side,            // field descriptions
    (const char *)&dummy_side, // dummy base
    true,                      // define_me
    nullptr                    // pointer to known struct
};

SaveArray sv_array_side = {
    nullptr,              // link in list
    "sides",              // array name
    &sv_struct_side,      // array type
    true,                 // define_me
    true,                 // allow_hub

    SV_SideCountElems,    // count routine
    SV_SideFindByIndex,   // index routine
    SV_SideCreateElems,   // creation routine
    SV_SideFinaliseElems, // finalisation routine

    nullptr,              // pointer to known array
    0                     // loaded size
};

//----------------------------------------------------------------------------
//
//  LINE STRUCTURE
//
static Line dummy_line;

static SaveField sv_fields_line[] = {
    EDGE_SAVE_FIELD(dummy_line, flags, "flags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_line, tag, "tag", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_line, count, "count", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_line, side, "side", 1, kSaveFieldIndex, 4, "sides", SR_SideGetSide, SR_SidePutSide),
    EDGE_SAVE_FIELD(dummy_line, special, "special", 1, kSaveFieldString, 0, nullptr, SaveGameLineGetSpecial,
                    SaveGameLinePutSpecial),
    EDGE_SAVE_FIELD(dummy_line, slide_door, "slide_door", 1, kSaveFieldString, 0, nullptr, SaveGameLineGetSpecial,
                    SaveGameLinePutSpecial),
    EDGE_SAVE_FIELD(dummy_line, old_stored, "old_stored", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),

    // NOT HERE:
    //   (many): values are kept from level load.
    //   gap stuff: regenerated from sector heights.
    //   valid_count: only a temporary value for some routines.
    //   slider_move: regenerated by a pass of the active part list.

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_line = {
    nullptr,                   // link in list
    "line_t",                  // structure name
    "line",                    // start marker
    sv_fields_line,            // field descriptions
    (const char *)&dummy_line, // dummy base
    true,                      // define_me
    nullptr                    // pointer to known struct
};

SaveArray sv_array_line = {
    nullptr,              // link in list
    "lines",              // array name
    &sv_struct_line,      // array type
    true,                 // define_me
    true,                 // allow_hub

    SV_LineCountElems,    // count routine
    SV_LineFindByIndex,   // index routine
    SV_LineCreateElems,   // creation routine
    SV_LineFinaliseElems, // finalisation routine

    nullptr,              // pointer to known array
    0                     // loaded size
};

//----------------------------------------------------------------------------
//
//  REGION_PROPERTIES STRUCTURE
//
static RegionProperties dummy_region_properties;

static SaveField sv_fields_regprops[] = {
    EDGE_SAVE_FIELD(dummy_region_properties, light_level, "lightlevel_i", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_region_properties, colourmap, "colourmap", 1, kSaveFieldString, 0, nullptr,
                    SaveGameLevelGetColormap, SaveGameLevelPutColormap),
    EDGE_SAVE_FIELD(dummy_region_properties, type, "type", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_region_properties, special, "special", 1, kSaveFieldString, 0, nullptr,
                    SaveGameSectorGetSpecial, SaveGameSectorPutSpecial),
    EDGE_SAVE_FIELD(dummy_region_properties, secret_found, "secret_found", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetBoolean, SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_region_properties, gravity, "gravity", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_region_properties, friction, "friction", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_region_properties, viscosity, "viscosity", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_region_properties, drag, "drag", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_region_properties, push, "push", 1, kSaveFieldNumeric, 12, nullptr, SaveGameGetVec3,
                    SaveGamePutVec3),
    EDGE_SAVE_FIELD(dummy_region_properties, net_push, "net_push", 1, kSaveFieldNumeric, 12, nullptr, SaveGameGetVec3,
                    SaveGamePutVec3),
    EDGE_SAVE_FIELD(dummy_region_properties, old_push, "old_push", 1, kSaveFieldNumeric, 12, nullptr, SaveGameGetVec3,
                    SaveGamePutVec3),
    EDGE_SAVE_FIELD(dummy_region_properties, fog_color, "fog_color", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_region_properties, fog_density, "fog_density", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_regprops = {
    nullptr,                                // link in list
    "region_properties_t",                  // structure name
    "rprp",                                 // start marker
    sv_fields_regprops,                     // field descriptions
    (const char *)&dummy_region_properties, // dummy base
    true,                                   // define_me
    nullptr                                 // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  EXTRAFLOOR STRUCTURE
//
static Extrafloor dummy_extrafloor;

static SaveField sv_fields_exfloor[] = {
    EDGE_SAVE_FIELD(dummy_extrafloor, higher, "higher", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_extrafloor, lower, "lower", 1, kSaveFieldIndex, 4, "extrafloors", SaveGameSectorGetExtrafloor,
                    SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_extrafloor, sector, "sector", 1, kSaveFieldIndex, 4, "sectors", SaveGameGetSector,
                    SaveGamePutSector),
    EDGE_SAVE_FIELD(dummy_extrafloor, top_height, "top_h", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_extrafloor, bottom_height, "bottom_h", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_extrafloor, top, "top", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetSurfPtr,
                    SaveGameLevelPutSurfPtr),
    EDGE_SAVE_FIELD(dummy_extrafloor, bottom, "bottom", 1, kSaveFieldString, 0, nullptr, SaveGameLevelGetSurfPtr,
                    SaveGameLevelPutSurfPtr),
    EDGE_SAVE_FIELD(dummy_extrafloor, properties, "p", 1, kSaveFieldString, 0, nullptr, SaveGameSectorGetPropRef,
                    SaveGameSectorPutPropRef),
    EDGE_SAVE_FIELD(dummy_extrafloor, extrafloor_line, "ef_line", 1, kSaveFieldIndex, 4, "lines", SaveGameGetLine,
                    SaveGamePutLine),
    EDGE_SAVE_FIELD(dummy_extrafloor, control_sector_next, "ctrl_next", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),

    // NOT HERE:
    //   - sector: can be regenerated.
    //   - ef_info: cached value, regenerated from extrafloor_line.

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_exfloor = {
    nullptr,                         // link in list
    "extrafloor_t",                  // structure name
    "exfl",                          // start marker
    sv_fields_exfloor,               // field descriptions
    (const char *)&dummy_extrafloor, // dummy base
    true,                            // define_me
    nullptr                          // pointer to known struct
};

SaveArray sv_array_exfloor = {
    nullptr,                 // link in list
    "extrafloors",           // array name
    &sv_struct_exfloor,      // array type
    true,                    // define_me
    true,                    // allow_hub

    SV_ExfloorCountElems,    // count routine
    SV_ExfloorFindByIndex,   // index routine
    SV_ExfloorCreateElems,   // creation routine
    SV_ExfloorFinaliseElems, // finalisation routine

    nullptr,                 // pointer to known array
    0                        // loaded size
};

//----------------------------------------------------------------------------
//
//  SECTOR STRUCTURE
//
static Sector dummy_sector;

static SaveField sv_fields_sector[] = {
    EDGE_SAVE_FIELD(dummy_sector, floor, "floor", 1, kSaveFieldStruct, 0, "surface_t", SaveGameLevelGetSurface,
                    SaveGameLevelPutSurface),
    EDGE_SAVE_FIELD(dummy_sector, ceiling, "ceil", 1, kSaveFieldStruct, 0, "surface_t", SaveGameLevelGetSurface,
                    SaveGameLevelPutSurface),
    EDGE_SAVE_FIELD(dummy_sector, floor_height, "f_h", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_sector, ceiling_height, "c_h", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),

    EDGE_SAVE_FIELD(dummy_sector, properties, "props", 1, kSaveFieldStruct, 0, "region_properties_t",
                    SaveGameSectorGetProps, SaveGameSectorPutProps),
    EDGE_SAVE_FIELD(dummy_sector, active_properties, "p", 1, kSaveFieldString, 0, nullptr, SaveGameSectorGetPropRef,
                    SaveGameSectorPutPropRef),

    EDGE_SAVE_FIELD(dummy_sector, extrafloor_used, "exfloor_used", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_sector, control_floors, "control_floors", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_sector, sound_player, "sound_player", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    EDGE_SAVE_FIELD(dummy_sector, bottom_extrafloor, "bottom_ef", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_sector, top_extrafloor, "top_ef", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_sector, bottom_liquid, "bottom_liq", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_sector, top_liquid, "top_liq", 1, kSaveFieldIndex, 4, "extrafloors",
                    SaveGameSectorGetExtrafloor, SaveGameSectorPutExtrafloor),
    EDGE_SAVE_FIELD(dummy_sector, old_stored, "old_stored", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),

    // NOT HERE:
    //   - floor_move, ceiling_move: can be regenerated
    //   - (many): values remaining from level load are OK
    //   - soundtraversed & valid_count: temp values, don't need saving

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_sector = {
    nullptr,                     // link in list
    "sector_t",                  // structure name
    "sect",                      // start marker
    sv_fields_sector,            // field descriptions
    (const char *)&dummy_sector, // dummy base
    true,                        // define_me
    nullptr                      // pointer to known struct
};

SaveArray sv_array_sector = {
    nullptr,                // link in list
    "sectors",              // array name
    &sv_struct_sector,      // array type
    true,                   // define_me
    true,                   // allow_hub

    SV_SectorCountElems,    // count routine
    SV_SectorFindByIndex,   // index routine
    SV_SectorCreateElems,   // creation routine
    SV_SectorFinaliseElems, // finalisation routine

    nullptr,                // pointer to known array
    0                       // loaded size
};

//----------------------------------------------------------------------------

int SV_SideCountElems(void)
{
    return total_level_sides;
}

void *SV_SideFindByIndex(int index)
{
    if (index < 0 || index >= total_level_sides)
    {
        LogWarning("LOADGAME: Invalid Side: %d\n", index);
        index = 0;
    }

    return level_sides + index;
}

int SV_SideGetIndex(Side *elem)
{
    EPI_ASSERT(level_sides <= elem && elem < (level_sides + total_level_sides));

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
{ /* nothing to do */
}

//----------------------------------------------------------------------------

extern std::vector<SlidingDoorMover *> active_sliders;

int SV_LineCountElems(void)
{
    return total_level_lines;
}

void *SV_LineFindByIndex(int index)
{
    if (index < 0 || index >= total_level_lines)
    {
        LogWarning("LOADGAME: Invalid Line: %d\n", index);
        index = 0;
    }

    return level_lines + index;
}

int SV_LineGetIndex(Line *elem)
{
    EPI_ASSERT(level_lines <= elem && elem < (level_lines + total_level_lines));

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
        EPI_ASSERT((*SMI)->line);

        (*SMI)->line->slider_move = (*SMI);
    }
}

//----------------------------------------------------------------------------

int SV_ExfloorCountElems(void)
{
    return total_level_extrafloors;
}

void *SV_ExfloorFindByIndex(int index)
{
    if (index < 0 || index >= total_level_extrafloors)
    {
        LogWarning("LOADGAME: Invalid Extrafloor: %d\n", index);
        index = 0;
    }

    return level_extrafloors + index;
}

int SV_ExfloorGetIndex(Extrafloor *elem)
{
    EPI_ASSERT(level_extrafloors <= elem && elem < (level_extrafloors + total_level_extrafloors));

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

void *SV_SectorFindByIndex(int index)
{
    if (index < 0 || index >= total_level_sectors)
    {
        LogWarning("LOADGAME: Invalid Sector: %d\n", index);
        index = 0;
    }

    return level_sectors + index;
}

int SV_SectorGetIndex(Sector *elem)
{
    EPI_ASSERT(level_sectors <= elem && elem < (level_sectors + total_level_sectors));

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
            sec->floor.net_scroll.X || sec->floor.net_scroll.Y || sec->ceiling.net_scroll.X ||
            sec->ceiling.net_scroll.Y || sec->floor.old_scroll.X || sec->floor.old_scroll.Y ||
            sec->ceiling.old_scroll.X || sec->ceiling.old_scroll.Y)
        {
            AddSpecialSector(sec);
        }
    }

    extern std::vector<LineAnimation> line_animations;

    for (size_t i = 0; i < line_animations.size(); i++)
    {
        if (line_animations[i].scroll_sector_reference)
        {
            line_animations[i].scroll_sector_reference->ceiling_move = nullptr;
            line_animations[i].scroll_sector_reference->floor_move   = nullptr;
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

        EPI_ASSERT(pmov->sector);

        if (pmov->is_ceiling)
            pmov->sector->ceiling_move = pmov;
        else
            pmov->sector->floor_move = pmov;
    }
}

//----------------------------------------------------------------------------

bool SaveGameLevelGetSurface(void *storage, int index, void *extra)
{
    MapSurface *dest = (MapSurface *)storage + index;

    if (!sv_struct_surface.counterpart)
        return true;

    return SaveGameStructLoad(dest, sv_struct_surface.counterpart);
}

void SaveGameLevelPutSurface(void *storage, int index, void *extra)
{
    MapSurface *src = (MapSurface *)storage + index;

    // force fogwall recreation when loading a save
    if (src->fog_wall)
        src->image = nullptr;

    SaveGameStructSave(src, &sv_struct_surface);
}

bool SaveGameLevelGetSurfPtr(void *storage, int index, void *extra)
{
    MapSurface **dest = (MapSurface **)storage + index;

    const char *str;
    int         num;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':')
        FatalError("SaveGameLevelGetSurfPtr: invalid surface string `%s'\n", str);

    num = strtol(str + 2, nullptr, 0);

    if (num < 0 || num >= total_level_sectors)
    {
        LogWarning("SaveGameLevelGetSurfPtr: bad sector ref %d\n", num);
        num = 0;
    }

    if (str[0] == 'F')
        (*dest) = &level_sectors[num].floor;
    else if (str[0] == 'C')
        (*dest) = &level_sectors[num].ceiling;
    else
        FatalError("SaveGameLevelGetSurfPtr: invalid surface plane `%s'\n", str);

    SaveChunkFreeString(str);
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
void SaveGameLevelPutSurfPtr(void *storage, int index, void *extra)
{
    MapSurface *src = ((MapSurface **)storage)[index];

    char buffer[64];
    int  i;

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // not optimal, but safe
    for (i = 0; i < total_level_sectors; i++)
    {
        if (src == &level_sectors[i].floor)
        {
            sprintf(buffer, "F:%d", i);
            SaveChunkPutString(buffer);
            return;
        }
        else if (src == &level_sectors[i].ceiling)
        {
            sprintf(buffer, "C:%d", i);
            SaveChunkPutString(buffer);
            return;
        }
    }

    LogWarning("SaveGameLevelPutSurfPtr: surface %p not found !\n", src);
    SaveChunkPutString("F:0");
}

bool SaveGameLevelGetImage(void *storage, int index, void *extra)
{
    const Image **dest = (const Image **)storage + index;
    const char   *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[1] != ':')
        LogWarning("SaveGameLevelGetImage: invalid image string `%s'\n", str);

    (*dest) = ImageParseSaveString(str[0], str + 2);

    SaveChunkFreeString(str);
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
void SaveGameLevelPutImage(void *storage, int index, void *extra)
{
    const Image *src = ((const Image **)storage)[index];

    char buffer[64];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    ImageMakeSaveString(src, buffer, buffer + 2);
    buffer[1] = ':';

    SaveChunkPutString(buffer);
}

bool SaveGameLevelGetColormap(void *storage, int index, void *extra)
{
    const Colormap **dest = (const Colormap **)storage + index;
    const char      *str;

    str = SaveChunkGetString();

    if (str)
        (*dest) = colormaps.Lookup(str);
    else
        (*dest) = nullptr;

    // -AJA- 2008/03/15: backwards compatibility
    if (*dest && epi::StringCaseCompareASCII((*dest)->name_, "NORMAL") == 0)
        *dest = nullptr;

    SaveChunkFreeString(str);
    return true;
}

//
// The string is the name of the colourmap.
//
void SaveGameLevelPutColormap(void *storage, int index, void *extra)
{
    const Colormap *src = ((const Colormap **)storage)[index];

    if (src)
        SaveChunkPutString(src->name_.c_str());
    else
        SaveChunkPutString(nullptr);
}

bool SaveGameLineGetSpecial(void *storage, int index, void *extra)
{
    const LineType **dest = (const LineType **)storage + index;
    const char      *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[0] != ':')
        FatalError("SaveGameLineGetSpecial: invalid special `%s'\n", str);

    (*dest) = LookupLineType(strtol(str + 1, nullptr, 0));

    SaveChunkFreeString(str);
    return true;
}

//
// Format of the string will usually be a colon followed by the
// linedef number (e.g. ":123").  Alternatively it can be the ddf
// name, but this shouldn't be needed currently (reserved for future
// use).
//
void SaveGameLinePutSpecial(void *storage, int index, void *extra)
{
    const LineType *src = ((const LineType **)storage)[index];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    std::string s = epi::StringFormat(":%d", src->number_);

    SaveChunkPutString(s.c_str());
}

bool SaveGameSectorGetSpecial(void *storage, int index, void *extra)
{
    const SectorType **dest = (const SectorType **)storage + index;
    const char        *str;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    if (str[0] != ':')
        FatalError("SaveGameSectorGetSpecial: invalid special `%s'\n", str);

    (*dest) = LookupSectorType(strtol(str + 1, nullptr, 0));

    SaveChunkFreeString(str);
    return true;
}

//
// Format of the string will usually be a colon followed by the
// sector number (e.g. ":123").  Alternatively it can be the ddf
// name, but this shouldn't be needed currently (reserved for future
// use).
//
void SaveGameSectorPutSpecial(void *storage, int index, void *extra)
{
    const SectorType *src = ((const SectorType **)storage)[index];

    if (!src)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    std::string s = epi::StringFormat(":%d", src->number_);

    SaveChunkPutString(s.c_str());
}

//----------------------------------------------------------------------------

bool SaveGameSectorGetProps(void *storage, int index, void *extra)
{
    RegionProperties *dest = (RegionProperties *)storage + index;

    if (!sv_struct_regprops.counterpart)
        return true;

    return SaveGameStructLoad(dest, sv_struct_regprops.counterpart);
}

void SaveGameSectorPutProps(void *storage, int index, void *extra)
{
    RegionProperties *src = (RegionProperties *)storage + index;

    SaveGameStructSave(src, &sv_struct_regprops);
}

bool SaveGameSectorGetPropRef(void *storage, int index, void *extra)
{
    RegionProperties **dest = (RegionProperties **)storage + index;

    const char *str;
    int         num;

    str = SaveChunkGetString();

    if (!str)
    {
        (*dest) = nullptr;
        return true;
    }

    num = strtol(str, nullptr, 0);

    if (num < 0 || num >= total_level_sectors)
    {
        LogWarning("SaveGameSectorGetPropRef: bad sector ref %d\n", num);
        num = 0;
    }

    (*dest) = &level_sectors[num].properties;

    SaveChunkFreeString(str);
    return true;
}

//
// Format of the string is just the sector number containing the
// properties.
//
void SaveGameSectorPutPropRef(void *storage, int index, void *extra)
{
    RegionProperties *src = ((RegionProperties **)storage)[index];

    char buffer[64];
    int  i;

    if (!src)
    {
        SaveChunkPutString(nullptr);
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
        LogWarning("SaveGameSectorPutPropRef: properties %p not found !\n", src);
        i = 0;
    }

    sprintf(buffer, "%d", i);
    SaveChunkPutString(buffer);
}

bool SaveGameGetLine(void *storage, int index, void *extra)
{
    Line **dest = (Line **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (Line *)((swizzle == 0) ? nullptr : SV_LineFindByIndex(swizzle - 1));
    return true;
}

void SaveGamePutLine(void *storage, int index, void *extra)
{
    Line *elem = ((Line **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_LineGetIndex(elem) + 1;

    SaveChunkPutInteger(swizzle);
}

bool SR_SideGetSide(void *storage, int index, void *extra)
{
    Side **dest = (Side **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (Side *)((swizzle == 0) ? nullptr : SV_SideFindByIndex(swizzle - 1));
    return true;
}

void SR_SidePutSide(void *storage, int index, void *extra)
{
    Side *elem = ((Side **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_SideGetIndex(elem) + 1;

    SaveChunkPutInteger(swizzle);
}

bool SaveGameGetSector(void *storage, int index, void *extra)
{
    Sector **dest = (Sector **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (Sector *)((swizzle == 0) ? nullptr : SV_SectorFindByIndex(swizzle - 1));
    return true;
}

void SaveGamePutSector(void *storage, int index, void *extra)
{
    Sector *elem = ((Sector **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_SectorGetIndex(elem) + 1;

    SaveChunkPutInteger(swizzle);
}

bool SaveGameSectorGetExtrafloor(void *storage, int index, void *extra)
{
    Extrafloor **dest = (Extrafloor **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (Extrafloor *)((swizzle == 0) ? nullptr : SV_ExfloorFindByIndex(swizzle - 1));
    return true;
}

void SaveGameSectorPutExtrafloor(void *storage, int index, void *extra)
{
    Extrafloor *elem = ((Extrafloor **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_ExfloorGetIndex(elem) + 1;

    SaveChunkPutInteger(swizzle);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
