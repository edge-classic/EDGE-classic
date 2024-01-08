//------------------------------------------------------------------------
// STRUCT : Doom structures, raw on-disk layout
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#ifndef __DM_STRUCTS_H__
#define __DM_STRUCTS_H__

/* ----- The wad structures ---------------------- */

// wad header
typedef struct raw_wad_header_s
{
    char identification[4];

    uint32_t num_entries;
    uint32_t dir_start;
} raw_wad_header_t;

// directory entry
typedef struct raw_wad_entry_s
{
    uint32_t pos;
    uint32_t size;

    char name[8];
} raw_wad_entry_t;

// blockmap
typedef struct raw_blockmap_header_s
{
    int16_t x_origin, y_origin;
    int16_t x_blocks, y_blocks;
} raw_blockmap_header_t;

/* ----- The level structures ---------------------- */

typedef struct raw_vertex_s
{
    int16_t x, y;
} raw_vertex_t;

typedef struct raw_v2_vertex_s
{
    int32_t x, y;
} raw_v2_vertex_t;

typedef struct raw_linedef_s
{
    uint16_t start;   // from this vertex...
    uint16_t end;     // ... to this vertex
    uint16_t flags;   // linedef flags (impassible, etc)
    uint16_t special; // special type (0 for none, 97 for teleporter, etc)
    int16_t tag;     // this linedef activates the sector with same tag
    uint16_t side_R;  // right sidedef
    uint16_t side_L;  // left sidedef (only if this line adjoins 2 sectors)
} raw_linedef_t;

typedef struct raw_hexen_linedef_s
{
    uint16_t start;   // from this vertex...
    uint16_t end;     // ... to this vertex
    uint16_t flags;   // linedef flags (impassible, etc)
    uint8_t  special; // special type
    uint8_t  args[5]; // special arguments
    uint16_t side_R;  // right sidedef
    uint16_t side_L;  // left sidedef
} raw_hexen_linedef_t;

typedef struct raw_sidedef_s
{
    int16_t x_offset; // X offset for texture
    int16_t y_offset; // Y offset for texture

    char upper_tex[8]; // texture name for the part above
    char lower_tex[8]; // texture name for the part below
    char mid_tex[8];   // texture name for the regular part

    uint16_t sector; // adjacent sector
} raw_sidedef_t;

typedef struct raw_sector_s
{
    int16_t floor_h; // floor height
    int16_t ceil_h;  // ceiling height

    char floor_tex[8]; // floor texture
    char ceil_tex[8];  // ceiling texture

    uint16_t light;   // light level (0-255)
    uint16_t special; // special behaviour (0 = normal, 9 = secret, ...)
    int16_t tag;     // sector activated by a linedef with same tag
} raw_sector_t;

typedef struct raw_thing_s
{
    int16_t x, y;    // position of thing
    int16_t angle;   // angle thing faces (degrees)
    uint16_t type;    // type of thing
    uint16_t options; // when appears, deaf, etc..
} raw_thing_t;

// -JL- Hexen thing definition
typedef struct raw_hexen_thing_s
{
    int16_t tid;     // tag id (for scripts/specials)
    int16_t x, y;    // position
    int16_t height;  // start height above floor
    int16_t angle;   // angle thing faces
    uint16_t type;    // type of thing
    uint16_t options; // when appears, deaf, dormant, etc..

    uint8_t special; // special type
    uint8_t args[5]; // special arguments
} raw_hexen_thing_t;

/* ----- The BSP tree structures ----------------------- */

typedef struct raw_seg_s
{
    uint16_t start;   // from this vertex...
    uint16_t end;     // ... to this vertex
    uint16_t angle;   // angle (0 = east, 16384 = north, ...)
    uint16_t linedef; // linedef that this seg goes along
    uint16_t flip;    // true if not the same direction as linedef
    uint16_t dist;    // distance from starting point
} raw_seg_t;

typedef struct raw_gl_seg_s
{
    uint16_t start;   // from this vertex...
    uint16_t end;     // ... to this vertex
    uint16_t linedef; // linedef that this seg goes along, or -1
    uint16_t side;    // 0 if on right of linedef, 1 if on left
    uint16_t partner; // partner seg number, or -1
} raw_gl_seg_t;

// Indicates a GL-specific vertex
#define SF_GL_VERTEX (uint16_t)(1 << 15)

typedef struct raw_v3_seg_s
{
    uint32_t start;   // from this vertex...
    uint32_t end;     // ... to this vertex
    uint16_t linedef; // linedef that this seg goes along, or -1
    uint16_t side;    // 0 if on right of linedef, 1 if on left
    uint32_t partner; // partner seg number, or -1
} raw_v3_seg_t;

// Indicates a GL-specific vertex (V3 and V5 formats)
#define SF_GL_VERTEX_V3 (uint32_t)(1 << 30)
#define SF_GL_VERTEX_V5 (uint32_t)(1 << 31)

typedef struct raw_bbox_s
{
    int16_t maxy, miny;
    int16_t minx, maxx;
} raw_bbox_t;

typedef struct raw_node_s
{
    int16_t      x, y;        // starting point
    int16_t      dx, dy;      // offset to ending point
    raw_bbox_t bbox[2];     // bounding rectangles
    uint16_t      children[2]; // children: Node or SSector (if high bit is set)
} raw_node_t;

// Indicate a leaf.
#define NF_SUBSECTOR (uint16_t)(1 << 15)

typedef struct raw_subsec_s
{
    uint16_t num;   // number of Segs in this Sub-Sector
    uint16_t first; // first Seg
} raw_subsec_t;

typedef struct raw_v3_subsec_s
{
    uint32_t num;   // number of Segs in this Sub-Sector
    uint32_t first; // first Seg
} raw_v3_subsec_t;

typedef struct raw_v5_node_s
{
    int16_t      x, y;        // starting point
    int16_t      dx, dy;      // offset to ending point
    raw_bbox_t bbox[2];     // bounding rectangles
    uint32_t      children[2]; // children: Node or SSector (if high bit is set)
} raw_v5_node_t;

// Indicate a leaf.
#define NF_V5_SUBSECTOR (uint32_t)(1 << 31)

/* ----- Graphical structures ---------------------- */

typedef struct
{
    int16_t x_origin;
    int16_t y_origin;

    uint16_t pname;    // index into PNAMES
    uint16_t stepdir;  // NOT USED
    uint16_t colormap; // NOT USED
} raw_patchdef_t;

typedef struct
{
    int16_t x_origin;
    int16_t y_origin;

    uint16_t pname; // index into PNAMES
} raw_strife_patchdef_t;

// Texture definition.
//
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
//
typedef struct
{
    char name[8];

    uint16_t flags;   // these from ZDoom (NOT USED in vanilla)
    uint8_t  scale_x; //
    uint8_t  scale_y; //

    uint16_t width;
    uint16_t height;
    uint32_t column_dir; // NOT USED
    uint16_t patch_count;

    raw_patchdef_t patches[1];
} raw_texture_t;

typedef struct
{
    char name[8];

    uint16_t flags;   // these from ZDoom (NOT USED in vanilla)
    uint8_t  scale_x; //
    uint8_t  scale_y; //

    uint16_t width;
    uint16_t height;
    uint16_t patch_count;

    raw_strife_patchdef_t patches[1];
} raw_strife_texture_t;

#endif /* __DM_STRUCTS_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
