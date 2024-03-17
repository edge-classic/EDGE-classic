//------------------------------------------------------------------------
//  Common Doom Engine/Format Definitions
//----------------------------------------------------------------------------
//
//  Copyright (C) 2007-2024  The EDGE Team
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
//------------------------------------------------------------------------

// This is here to provide a shared file for both AJBSP and the main program
// instead of defining the same structures twice - Dasho

#pragma once

#include <stdint.h>

// Indicate a leaf.
constexpr uint32_t kLeafSubsector = (uint32_t)(1 << 31);

/* ----- The wad structures ---------------------- */

// wad header
#pragma pack(push, 1)
struct RawWadHeader
{
    char magic[4];

    uint32_t total_entries;
    uint32_t directory_start;
};
#pragma pack(pop)

// directory entry
#pragma pack(push, 1)
struct RawWadEntry
{
    uint32_t position;
    uint32_t size;

    char name[8];
};
#pragma pack(pop)

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum LumpOrder
{
    kLumpLabel = 0,  // A separator name, ExMx or MAPxx
    kLumpThings,     // Monsters, items..
    kLumpLinedefs,   // LineDefs, from editing
    kLumpSidedefs,   // SideDefs, from editing
    kLumpVertexes,   // Vertices, edited and BSP splits generated
    kLumpSegs,       // LineSegs, from LineDefs split by BSP
    kLumpSubSectors, // SubSectors, list of LineSegs
    kLumpNodes,      // BSP nodes
    kLumpSectors,    // Sectors, from editing
    kLumpReject,     // LUT, sector-sector visibility
    kLumpBlockmap,   // LUT, motion clipping, walls/grid element
    kLumpBehavior    // Hexen scripting stuff
};

/* ----- The level structures ---------------------- */
#pragma pack(push, 1)
struct RawVertex
{
    int16_t x, y;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawV2Vertex
{
    int32_t x, y;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawLinedef
{
    uint16_t start; // from this vertex...
    uint16_t end;   // ... to this vertex
    uint16_t flags; // linedef flags (impassible, etc)
    uint16_t type;  // special type (0 for none, 97 for teleporter, etc)
    int16_t  tag;   // this linedef activates the sector with same tag
    uint16_t right; // right sidedef
    uint16_t left;  // left sidedef (only if this line adjoins 2 sectors)
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawSidedef
{
    int16_t x_offset;      // X offset for texture
    int16_t y_offset;      // Y offset for texture

    char upper_texture[8]; // texture name for the part above
    char lower_texture[8]; // texture name for the part below
    char mid_texture[8];   // texture name for the regular part

    uint16_t sector;       // adjacent sector
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawSector
{
    int16_t floor_height;   // floor height
    int16_t ceiling_height; // ceiling height

    char floor_texture[8];  // floor texture
    char ceil_texture[8];   // ceiling texture

    uint16_t light;         // light level (0-255)
    uint16_t type;          // special type (0 = normal, 9 = secret, ...)
    int16_t  tag;           // sector activated by a linedef with same tag
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawThing
{
    int16_t  x, y;    // position of thing
    int16_t  angle;   // angle thing faces (degrees)
    uint16_t type;    // type of thing
    uint16_t options; // when appears, deaf, etc..
};
#pragma pack(pop)

/* ----- The BSP tree structures ----------------------- */
#pragma pack(push, 1)
struct RawBoundingBox
{
    int16_t maximum_y, minimum_y;
    int16_t minimum_x, maximum_x;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawV5Node
{
    // this structure used by ZDoom nodes too

    int16_t        x, y;                           // starting point
    int16_t        delta_x, delta_y;               // offset to ending point
    RawBoundingBox bounding_box_1, bounding_box_2; // bounding rectangles
    uint32_t       right, left;                    // children: Node or SSector (if high bit is set)
};
#pragma pack(pop)

/* ----- Graphical structures ---------------------- */
#pragma pack(push, 1)
struct RawPatchDefinition
{
    int16_t x_origin;
    int16_t y_origin;

    uint16_t pname;    // index into PNAMES
    uint16_t stepdir;  // NOT USED
    uint16_t colormap; // NOT USED
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawStrifePatchDefinition
{
    int16_t  x_origin;
    int16_t  y_origin;
    uint16_t pname; // index into PNAMES
};
#pragma pack(pop)

// Texture definition.
//
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
//
#pragma pack(push, 1)
struct RawTexture
{
    char name[8];

    uint16_t masked; // NOT USED
    uint8_t  scale_x;
    uint8_t  scale_y;
    uint16_t width;
    uint16_t height;
    uint32_t column_dir; // NOT USED
    uint16_t patch_count;

    RawPatchDefinition patches[1];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RawStrifeTexture
{
    char name[8];

    uint16_t masked; // NOT USED
    uint8_t  scale_x;
    uint8_t  scale_y;
    uint16_t width;
    uint16_t height;
    uint16_t patch_count;

    RawStrifePatchDefinition patches[1];
};
#pragma pack(pop)

// Patches.
//
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
//
#pragma pack(push, 1)
struct Patch
{
    // bounding box size
    int16_t width;
    int16_t height;

    // pixels to the left of origin
    int16_t left_offset;

    // pixels below the origin
    int16_t top_offset;

    uint32_t column_offset[1]; // only [width] used
};
#pragma pack(pop)

//
// LineDef attributes.
//

enum LineFlag
{
    // solid, is an obstacle
    kLineFlagBlocking = 0x0001,

    // blocks monsters only
    kLineFlagBlockMonsters = 0x0002,

    // backside will not be present at all if not two sided
    kLineFlagTwoSided = 0x0004,

    // If a texture is pegged, the texture will have
    // the end exposed to air held constant at the
    // top or bottom of the texture (stairs or pulled
    // down things) and will move with a height change
    // of one of the neighbor sectors.
    //
    // Unpegged textures allways have the first row of
    // the texture at the top pixel of the line for both
    // top and bottom textures (use next to windows).

    // upper texture unpegged
    kLineFlagUpperUnpegged = 0x0008,

    // lower texture unpegged
    kLineFlagLowerUnpegged = 0x0010,

    // in AutoMap: don't map as two sided: IT'S A SECRET!
    kLineFlagSecret = 0x0020,

    // sound rendering: don't let sound cross two of these
    kLineFlagSoundBlock = 0x0040,

    // don't draw on the automap at all
    kLineFlagDontDraw = 0x0080,

    // set as if already seen, thus drawn in automap
    kLineFlagMapped = 0x0100,

    // -AJA- this one is from Boom. Allows multiple lines to
    //       be pushed simultaneously.
    kLineFlagBoomPassThrough = 0x0200,

    // 0x0400 is Eternity's 3DMidTex flag - Dasho

    // Clear extended line flags (BOOM or later spec); needed to repair
    // mapping/editor errors with historical maps (i.e., E2M7)
    kLineFlagClearBoomFlags = 0x0800,

    // MBF21
    kLineFlagBlockGroundedMonsters = 0x1000,

    // MBF21
    kLineFlagBlockPlayers = 0x2000,

    // ----- internal flags -----

    kLineFlagMirror = (1 << 16),

    // -AJA- These two from XDoom.
    // Dasho - Moved to internal flag range to make room for MBF21 stuff
    kLineFlagShootBlock = (1 << 17),

    kLineFlagSightBlock = (1 << 18),
};

constexpr int16_t kBoomGeneralizedLineFirst = 0x2f80;
constexpr int16_t kBoomGeneralizedLineLast  = 0x7fff;

inline bool IsBoomGeneralizedLine(int16_t line)
{
    return (line >= kBoomGeneralizedLineFirst && line <= kBoomGeneralizedLineLast);
}

//
// Sector attributes.
//

enum BoomSectorFlag
{
    kBoomSectorFlagTypeMask   = 0x001F,
    kBoomSectorFlagDamageMask = 0x0060,
    kBoomSectorFlagSecret     = 0x0080,
    kBoomSectorFlagFriction   = 0x0100,
    kBoomSectorFlagPush       = 0x0200,
    kBoomSectorFlagNoSounds   = 0x0400,
    kBoomSectorFlagQuietPlane = 0x0800
};

constexpr int16_t kBoomFlagBits = 0x0FE0;

//
// Thing attributes.
//

enum ThingOption
{
    kThingEasy            = 1,
    kThingMedium          = 2,
    kThingHard            = 4,
    kThingAmbush          = 8,
    kThingNotSinglePlayer = 16,
    kThingNotDeathmatch   = 32,
    kThingNotCooperative  = 64,
    kThingFriend          = 128,
    kThingReserved        = 256,
};

constexpr int16_t kExtrafloorMask     = 0x3C00;
constexpr uint8_t kExtrafloorBitShift = 10;

//
// Polyobject stuff
//
constexpr uint8_t kHexenPolyobjectStart    = 1;
constexpr uint8_t kHexenPolyobjectExplicit = 5;

// -JL- ZDoom polyobj thing types
constexpr int16_t kZDoomPolyobjectAnchorType     = 9300;
constexpr int16_t kZDoomPolyobjectSpawnType      = 9301;
constexpr int16_t kZDoomPolyobjectSpawnCrushType = 9302;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
