//----------------------------------------------------------------------------
//  EDGE Rendering Definitions Header
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

#include "ddf_main.h"
#include "dm_defs.h"
#include "m_math.h"
#include "p_mobj.h"

#if defined(EDGE_SOKOL)
#define THREAD_U64 uint64_t
#include "thread.h"
#endif

class Image;

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally, like some
// DOOM-alikes ("wt", "WebView") did.
// Dasho: Changed to HMM_Vec4
typedef HMM_Vec4 Vertex;

// Forward of LineDefs, for Sectors.
struct Line;
struct Subsector;
struct RegionProperties;

//
// Touch Node
//
// -AJA- Used for remembering things that are inside or touching
// sectors.  The idea is blatantly copied from BOOM: there are two
// lists running through each node, (a) list for things, to remember
// what sectors they are in/touch, (b) list for sectors, holding what
// things are in or touch them.
//
// NOTE: we use the same optimisation: in P_UnsetThingPos we just
// clear all the `mo' fields to nullptr.  During P_SetThingPos we find
// the first nullptr `mo' field (i.e. as an allocation).  The interesting
// part is that we only need to unlink the node from the sector list
// (and relink) if the sector in that node is different.  Thus saving
// work for the common case where the sector(s) don't change.
//
// CAVEAT: this means that very little should be done in between
// P_UnsetThingPos and P_SetThingPos calls, ideally just load some new
// x/y position.  Avoid especially anything that scans the sector
// touch lists.
//
struct TouchNode
{
    class MapObject  *map_object          = nullptr;
    struct TouchNode *map_object_next     = nullptr;
    struct TouchNode *map_object_previous = nullptr;

    struct Sector    *sector          = nullptr;
    struct TouchNode *sector_next     = nullptr;
    struct TouchNode *sector_previous = nullptr;
};

//
// Region Properties
//
// Stores the properties that affect each vertical region.
//
struct RegionProperties
{
    // rendering related
    int light_level;

    const Colormap *colourmap; // can be nullptr

    // special type (e.g. damaging)
    int               type;
    const SectorType *special;
    bool              secret_found = false;

    // -KM- 1998/10/29 Added gravity + friction
    float gravity;
    float friction;
    float movefactor;
    float viscosity;
    float drag;

    // pushing sector information (normally all zero)
    HMM_Vec3 push;
    bool     push_constant = false;

    HMM_Vec3 net_push = {{0, 0, 0}};

    HMM_Vec3 old_push = {{0, 0, 0}};

    // sector fog
    RGBAColor fog_color   = kRGBANoValue;
    float     fog_density = 0;
};

//
// Surface
//
// Stores the texturing information about a single "surface", which is
// either a wall part or a ceiling/floor.  Doesn't include position
// info -- that is elsewhere.
//
// Texture coordinates are computed from World coordinates via:
//   wx += offset.x
//   wy += offset.y
//
//   tx = wx * x_mat.x + wy * x_mat.y
//   ty = wx * y_mat.x + wy * y_mat.y
//
struct MapSurface
{
    const Image *image;

    float translucency;

    // texturing matrix (usually identity)
    HMM_Vec2 x_matrix;
    HMM_Vec2 y_matrix;
    BAMAngle rotation = 0;

    // current offset and scrolling deltas (world coords)
    HMM_Vec2 offset;
    HMM_Vec2 old_offset;
    HMM_Vec2 scroll;

    HMM_Vec2 net_scroll = {{0, 0}};
    HMM_Vec2 old_scroll = {{0, 0}};

    // lighting override (as in BOOM).  Usually nullptr.
    RegionProperties *override_properties;

    // this only used for BOOM deep water (linetype 242)
    const Colormap *boom_colormap;

    // used for fog boundaries if needed
    bool fog_wall = false;
};

//
// ExtraFloor
//
// Stores information about a single extrafloor within a sector.
//
// -AJA- 2001/07/11: added this, replaces vert_region.
//
struct Extrafloor
{
    // links in chain.  These are sorted by increasing heights, using
    // bottom_h as the reference.  This is important, especially when a
    // liquid extrafloor overlaps a solid one: using this rule, the
    // liquid region will be higher than the solid one.
    struct Extrafloor *higher;
    struct Extrafloor *lower;

    struct Sector *sector;

    // top and bottom heights of the extrafloor.  For non-THICK
    // extrafloors, these are the same.  These are generally the same as
    // in the dummy sector, EXCEPT during the process of moving the
    // extrafloor.
    float top_height, bottom_height;

    // top/bottom surfaces of the extrafloor
    MapSurface *top;
    MapSurface *bottom;

    // properties used for stuff below us
    RegionProperties *properties;

    // type of extrafloor this is.  Only nullptr for unused extrafloors.
    // This value is cached pointer to extrafloor_line->special->ef.
    const ExtraFloorDefinition *extrafloor_definition;

    // extrafloor linedef (frontsector == control sector).  Only nullptr
    // for unused extrafloors.
    struct Line *extrafloor_line;

    // link in dummy sector's controlling list
    struct Extrafloor *control_sector_next;
};

// Vertical gap between a floor & a ceiling.
// -AJA- 1999/07/19.
//
struct VerticalGap
{
    float floor;
    float ceiling;
};

struct SlopePlane
{
    // Note: z coords are relative to the floor/ceiling height
    float x1, y1, delta_z1;
    float x2, y2, delta_z2;
};

//
// The SECTORS record, at runtime.
//
struct Sector
{
    float floor_height, ceiling_height;

    // Uncapped test - Dasho
    float old_floor_height, old_ceiling_height;
    float interpolated_floor_height, interpolated_ceiling_height;

    MapSurface floor, ceiling;

    RegionProperties properties;

    int tag;

    // set of extrafloors (in the global `extrafloors' array) that this
    // sector can use.  At load time we can deduce the maximum number
    // needed for extrafloors, even if they dynamically come and go.
    //
    short       extrafloor_maximum;
    short       extrafloor_used;
    Extrafloor *extrafloor_first;

    // -AJA- 2001/07/11: New multiple extrafloor code.
    //
    // Now the FLOORS ARE IMPLIED.  Unlike before, the floor below an
    // extrafloor is NOT stored in each extrafloor_t -- you must scan
    // down to find them, and use the sector's floor if you hit nullptr.
    Extrafloor *bottom_extrafloor;
    Extrafloor *top_extrafloor;

    // Liquid extrafloors are now kept in a separate list.  For many
    // purposes (especially moving sectors) they otherwise just get in
    // the way.
    Extrafloor *bottom_liquid;
    Extrafloor *top_liquid;

    // properties that are active for this sector (top-most extrafloor).
    // This may be different than the sector's actual properties (the
    // "props" field) due to flooders.
    RegionProperties *active_properties;

    // slope information, normally nullptr
    SlopePlane *floor_slope;
    SlopePlane *ceiling_slope;

    // UDMF vertex slope stuff
    bool     floor_vertex_slope;
    bool     ceiling_vertex_slope;
    HMM_Vec3 floor_z_vertices[3];
    HMM_Vec3 ceiling_z_vertices[3];
    HMM_Vec3 floor_vertex_slope_normal;
    HMM_Vec3 ceiling_vertex_slope_normal;
    HMM_Vec2 floor_vertex_slope_high_low;
    HMM_Vec2 ceiling_vertex_slope_high_low;

    // linked list of extrafloors that this sector controls.  nullptr means
    // that this sector is not a controller.
    Extrafloor *control_floors;

    // killough 3/7/98: support flat heights drawn at another sector's heights
    struct Sector *height_sector;
    struct Side   *height_sector_side;

    // movement thinkers, for quick look-up
    struct PlaneMover *floor_move;
    struct PlaneMover *ceiling_move;

    // 0 = untraversed, 1,2 = sndlines-1
    int sound_traversed;

    // player# that made a sound (starting at 0), or -1
    int sound_player;

    // origin for any sounds played by the sector
    Position sound_effects_origin;

    // DDF reverb effect to use
    // Will override dynamic reverb
    ddf::ReverbDefinition *sound_reverb;

    int           line_count;
    struct Line **lines; // [line_count] size

    // touch list: objects in or touching this sector
    TouchNode *touch_things;

    // list of sector glow things (linked via dlnext/dlprev)
    MapObject *glow_things;

    // sky height for GL renderer
    float sky_height;

    // keep track of vertical sight gaps within the sector.  This is
    // just a much more convenient form of the info in the extrafloor
    // list.
    //
    short maximum_gaps;
    short sight_gap_number;

    VerticalGap *sight_gaps;

    // if == valid_count, already checked
    int valid_count;

    // -AJA- 1999/07/29: Keep sectors with same tag in a list.
    struct Sector *tag_next;
    struct Sector *tag_previous;

    // -AJA- 2000/03/30: Keep a list of child subsectors.
    struct Subsector *subsectors;

    // For dynamic scroll/push/offset
    bool  old_stored;
    float original_height;

    // Boom door lighting stuff
    int minimum_neighbor_light;
    int maximum_neighbor_light;

    float bob_depth;
    float sink_depth;
};

//
// The SideDef.
//
struct Side
{
    MapSurface top;
    MapSurface middle;
    MapSurface bottom;

    // Sector the SideDef is facing.
    Sector *sector;

    // midmasker Y offset
    float middle_mask_offset;
};

//
// Move clipping aid for LineDefs.
//
enum LineClippingSlope
{
    kLineClipHorizontal,
    kLineClipVertical,
    kLineClipPositive,
    kLineClipNegative
};

constexpr uint8_t kVertexSectorListMaximum = 11;

struct VertexSectorList
{
    unsigned short total;
    unsigned short sectors[kVertexSectorListMaximum];
};

//
// LINEDEF
//

struct Line
{
    // Vertices, from v1 to v2.
    Vertex *vertex_1;
    Vertex *vertex_2;

    // Precalculated v2 - v1 for side checking.
    float delta_x;
    float delta_y;
    float length;

    // Animation related.
    int flags;
    int tag;
    int count;

    const LineType *special;

    // Visual appearance: SideDefs.
    // side[1] will be nullptr if one sided.
    Side *side[2];

    // Front and back sector.
    // Note: kinda redundant (could be retrieved from sidedefs), but it
    // simplifies the code.
    Sector *front_sector;
    Sector *back_sector;

    // Neat. Another bounding box, for the extent of the LineDef.
    float bounding_box[4];

    // To aid move clipping.
    LineClippingSlope slope_type;

    // if == valid_count, already checked
    int valid_count;

    // whether this linedef is "blocking" for rendering purposes.
    // Always true for 1s lines.  Always false when both sides of the
    // line reference the same sector.
    //
    bool blocked;

    // -AJA- 1999/07/19: Extra floor support.  We now keep track of the
    // gaps between the front & back sectors here, instead of computing
    // them each time in P_LineOpening() -- which got a lot more complex
    // due to extra floors.  Now they only need to be recomputed when
    // one of the sectors changes height.  The pointer here points into
    // the single global array `vertgaps'.
    //
    short maximum_gaps;
    short gap_number;

    VerticalGap *gaps;

    const LineType *slide_door;

    // slider thinker, normally nullptr
    struct SlidingDoorMover *slider_move;

    Line *portal_pair;

    bool old_stored = false;
};

//
// SubSector.
//
// References a Sector.
// Basically, this is a list of LineSegs, indicating the visible walls
// that define all sides of a convex BSP leaf.
//
struct Subsector
{
    // link in sector list
    Subsector *sector_next;

    Sector     *sector;
    struct Seg *segs;

    // list of mobjs in subsector
    MapObject *thing_list;

    // pointer to bounding box (usually in parent node)
    float *bounding_box;

    // -AJA- 2004/04/20: used when emulating deep-water TRICK
    Sector *deep_water_reference;
};

//
// The LineSeg
//
// Defines part of a wall that faces inwards on a convex BSP leaf.
//
struct Seg
{
    Vertex *vertex_1;
    Vertex *vertex_2;

    BAMAngle angle;

    float length;

    // link in subsector list.
    // (NOTE: sorted in clockwise order)
    struct Seg *subsector_next;

    // -AJA- 1999/12/20: Reference to partner seg, or nullptr if the seg
    //       lies along a one-sided line.
    struct Seg *partner;

    // -AJA- 1999/09/23: Reference to subsector on each side of seg,
    //       back_sub is nullptr for one-sided segs.
    //       (Addendum: back_sub is obsolete with new `partner' field)
    Subsector *front_subsector;
    Subsector *back_subsector;

    // -AJA- 1999/09/23: For "True BSP rendering", we keep track of the
    //       `minisegs' which define all the non-wall borders of the
    //       subsector.  Thus all the segs (normal + mini) define a
    //       closed convex polygon.  When the `miniseg' field is true,
    //       all the fields below it are unused.
    //
    bool miniseg;

    float offset;

    Side *sidedef;
    Line *linedef;

    int side; // 0 for front, 1 for back

    // Sector references.
    // backsector is nullptr for one sided lines

    Sector *front_sector;
    Sector *back_sector;

    // compact list of sectors touching each vertex (can be nullptr)
    VertexSectorList *vertex_sectors[2];
};

// Partition line.
struct DividingLine
{
    float x;
    float y;
    float delta_x;
    float delta_y;
};

//
// BSP node.
//
struct BSPNode
{
    DividingLine divider;
    float        divider_length;

    // bit kLeafSubsector set for a subsector.
    unsigned int children[2];

    // Bounding boxes for this node.
    float bounding_boxes[2][4];
};

struct SectorAnimation
{
    Sector         *target                   = nullptr;
    struct Sector  *scroll_sector_reference  = nullptr;
    const LineType *scroll_special_reference = nullptr;
    Line           *scroll_line_reference    = nullptr;
    HMM_Vec2        floor_scroll             = {{0, 0}};
    HMM_Vec2        ceil_scroll              = {{0, 0}};
    HMM_Vec3        push                     = {{0, 0, 0}};
    bool            permanent                = false;
    float           last_height              = 0.0f;
};

struct LineAnimation
{
    Line           *target                   = nullptr;
    struct Sector  *scroll_sector_reference  = nullptr;
    const LineType *scroll_special_reference = nullptr;
    Line           *scroll_line_reference    = nullptr;
    float           side_0_x_speed           = 0.0;
    float           side_1_x_speed           = 0.0;
    float           side_0_y_speed           = 0.0;
    float           side_1_y_speed           = 0.0;
    float           side_0_x_offset_speed    = 0.0;
    float           side_0_y_offset_speed    = 0.0;
    float           dynamic_delta_x          = 0.0;
    float           dynamic_delta_y          = 0.0;
    bool            permanent                = false;
    float           last_height              = 0.0f;
};

struct LightAnimation
{
    struct Sector *light_sector_reference = nullptr;
    Line          *light_line_reference   = nullptr;
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
