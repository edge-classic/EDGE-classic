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

#ifndef __R_DEFS_H__
#define __R_DEFS_H__

// Screenwidth.
#include "dm_defs.h"

// Some more or less basic data types
// we depend on.
#include "m_math.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

// -AJA- 1999/07/10: Need this for colourmap_c.
#include "main.h"

class image_c;

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally, like some
// DOOM-alikes ("wt", "WebView") did.
// Dasho: Changed to HMM_Vec4
typedef HMM_Vec4 vertex_t;

// Forward of LineDefs, for Sectors.
struct line_s;
struct side_s;
struct subsector_s;
struct region_properties_s;

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
typedef struct touch_node_s
{
    class MapObject       *mo      = nullptr;
    struct touch_node_s *mo_next = nullptr;
    struct touch_node_s *mo_prev = nullptr;

    struct sector_s     *sec      = nullptr;
    struct touch_node_s *sec_next = nullptr;
    struct touch_node_s *sec_prev = nullptr;
} touch_node_t;

//
// Region Properties
//
// Stores the properties that affect each vertical region.
//
typedef struct region_properties_s
{
    // rendering related
    int lightlevel;

    const Colormap *colourmap; // can be nullptr

    // special type (e.g. damaging)
    int                 type;
    const SectorType *special;
    bool                secret_found = false;

    // -KM- 1998/10/29 Added gravity + friction
    float gravity;
    float friction;
    float viscosity;
    float drag;

    // pushing sector information (normally all zero)
    HMM_Vec3 push;

    HMM_Vec3 net_push = {{0, 0, 0}};

    HMM_Vec3 old_push = {{0, 0, 0}};

    // sector fog
    RGBAColor fog_color   = kRGBANoValue;
    float    fog_density = 0;
} region_properties_t;

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
typedef struct surface_s
{
    const image_c *image;

    float translucency;

    // texturing matrix (usually identity)
    HMM_Vec2  x_mat;
    HMM_Vec2  y_mat;
    BAMAngle rotation = 0;

    // current offset and scrolling deltas (world coords)
    HMM_Vec2 offset;
    HMM_Vec2 scroll;

    HMM_Vec2 net_scroll = {{0, 0}};
    HMM_Vec2 old_scroll = {{0, 0}};

    // lighting override (as in BOOM).  Usually nullptr.
    region_properties_t *override_p;

    // this only used for BOOM deep water (linetype 242)
    const Colormap *boom_colmap;

    // used for fog boundaries if needed
    bool fogwall = false;
} surface_t;

//
// ExtraFloor
//
// Stores information about a single extrafloor within a sector.
//
// -AJA- 2001/07/11: added this, replaces vert_region.
//
typedef struct extrafloor_s
{
    // links in chain.  These are sorted by increasing heights, using
    // bottom_h as the reference.  This is important, especially when a
    // liquid extrafloor overlaps a solid one: using this rule, the
    // liquid region will be higher than the solid one.
    struct extrafloor_s *higher;
    struct extrafloor_s *lower;

    struct sector_s *sector;

    // top and bottom heights of the extrafloor.  For non-THICK
    // extrafloors, these are the same.  These are generally the same as
    // in the dummy sector, EXCEPT during the process of moving the
    // extrafloor.
    float top_h, bottom_h;

    // top/bottom surfaces of the extrafloor
    surface_t *top;
    surface_t *bottom;

    // properties used for stuff below us
    region_properties_t *p;

    // type of extrafloor this is.  Only nullptr for unused extrafloors.
    // This value is cached pointer to ef_line->special->ef.
    const ExtraFloorDefinition *ef_info;

    // extrafloor linedef (frontsector == control sector).  Only nullptr
    // for unused extrafloors.
    struct line_s *ef_line;

    // link in dummy sector's controlling list
    struct extrafloor_s *ctrl_next;
} extrafloor_t;

// Vertical gap between a floor & a ceiling.
// -AJA- 1999/07/19.
//
typedef struct
{
    float f; // floor
    float c; // ceiling
} vgap_t;

typedef struct slope_plane_s
{
    // Note: z coords are relative to the floor/ceiling height
    float x1, y1, dz1;
    float x2, y2, dz2;
} slope_plane_t;

//
// The SECTORS record, at runtime.
//
typedef struct sector_s
{
    // floor and ceiling heights
    float f_h, c_h;

    surface_t floor, ceil;

    region_properties_t props;

    int tag;

    // set of extrafloors (in the global `extrafloors' array) that this
    // sector can use.  At load time we can deduce the maximum number
    // needed for extrafloors, even if they dynamically come and go.
    //
    short         exfloor_max;
    short         exfloor_used;
    extrafloor_t *exfloor_first;

    // -AJA- 2001/07/11: New multiple extrafloor code.
    //
    // Now the FLOORS ARE IMPLIED.  Unlike before, the floor below an
    // extrafloor is NOT stored in each extrafloor_t -- you must scan
    // down to find them, and use the sector's floor if you hit nullptr.
    extrafloor_t *bottom_ef;
    extrafloor_t *top_ef;

    // Liquid extrafloors are now kept in a separate list.  For many
    // purposes (especially moving sectors) they otherwise just get in
    // the way.
    extrafloor_t *bottom_liq;
    extrafloor_t *top_liq;

    // properties that are active for this sector (top-most extrafloor).
    // This may be different than the sector's actual properties (the
    // "props" field) due to flooders.
    region_properties_t *p;

    // slope information, normally nullptr
    slope_plane_t *f_slope;
    slope_plane_t *c_slope;

    // UDMF vertex slope stuff
    bool                floor_vertex_slope;
    bool                ceil_vertex_slope;
    std::vector<HMM_Vec3> floor_z_verts;
    std::vector<HMM_Vec3> ceil_z_verts;
    HMM_Vec3              floor_vs_normal;
    HMM_Vec3              ceil_vs_normal;
    HMM_Vec2              floor_vs_hilo;
    HMM_Vec2              ceil_vs_hilo;

    // linked list of extrafloors that this sector controls.  nullptr means
    // that this sector is not a controller.
    extrafloor_t *control_floors;

    // killough 3/7/98: support flat heights drawn at another sector's heights
    struct sector_s *heightsec;
    struct side_s   *heightsec_side;

    // movement thinkers, for quick look-up
    struct plane_move_s *floor_move;
    struct plane_move_s *ceil_move;

    // 0 = untraversed, 1,2 = sndlines-1
    int soundtraversed;

    // player# that made a sound (starting at 0), or -1
    int sound_player;

    // origin for any sounds played by the sector
    Position sfx_origin;

    int             linecount;
    struct line_s **lines; // [linecount] size

    // touch list: objects in or touching this sector
    touch_node_t *touch_things;

    // list of sector glow things (linked via dlnext/dlprev)
    MapObject *glow_things;

    // sky height for GL renderer
    float sky_h;

    // keep track of vertical sight gaps within the sector.  This is
    // just a much more convenient form of the info in the extrafloor
    // list.
    //
    short max_gaps;
    short sight_gap_num;

    vgap_t *sight_gaps;

    // if == validcount, already checked
    int validcount;

    // -AJA- 1999/07/29: Keep sectors with same tag in a list.
    struct sector_s *tag_next;
    struct sector_s *tag_prev;

    // -AJA- 2000/03/30: Keep a list of child subsectors.
    struct subsector_s *subsectors;

    // For dynamic scroll/push/offset
    bool  old_stored;
    float orig_height;

    // Boom door lighting stuff
    int min_neighbor_light;
    int max_neighbor_light;

    float bob_depth;
    float sink_depth;
} sector_t;

//
// The SideDef.
//
typedef struct side_s
{
    surface_t top;
    surface_t middle;
    surface_t bottom;

    // Sector the SideDef is facing.
    sector_t *sector;

    // midmasker Y offset
    float midmask_offset;
} side_t;

//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

#define SECLIST_MAX 11

typedef struct
{
    unsigned short num;
    unsigned short sec[SECLIST_MAX];
} vertex_seclist_t;

//
// LINEDEF
//

typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t *v1;
    vertex_t *v2;

    // Precalculated v2 - v1 for side checking.
    float dx;
    float dy;
    float length;

    // Animation related.
    int flags;
    int tag;
    int count;

    const LineType *special;

    // Visual appearance: SideDefs.
    // side[1] will be nullptr if one sided.
    side_t *side[2];

    // Front and back sector.
    // Note: kinda redundant (could be retrieved from sidedefs), but it
    // simplifies the code.
    sector_t *frontsector;
    sector_t *backsector;

    // Neat. Another bounding box, for the extent of the LineDef.
    float bbox[4];

    // To aid move clipping.
    slopetype_t slopetype;

    // if == validcount, already checked
    int validcount;

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
    short max_gaps;
    short gap_num;

    vgap_t *gaps;

    const LineType *slide_door;

    // slider thinker, normally nullptr
    struct slider_move_s *slider_move;

    struct line_s *portal_pair;

    bool old_stored = false;
} line_t;

//
// SubSector.
//
// References a Sector.
// Basically, this is a list of LineSegs, indicating the visible walls
// that define all sides of a convex BSP leaf.
//
typedef struct subsector_s
{
    // link in sector list
    struct subsector_s *sec_next;

    sector_t     *sector;
    struct seg_s *segs;

    // list of mobjs in subsector
    MapObject *thinglist;

    // pointer to bounding box (usually in parent node)
    float *bbox;

    // -AJA- 2004/04/20: used when emulating deep-water TRICK
    struct sector_s *deep_ref;
} subsector_t;

//
// The LineSeg
//
// Defines part of a wall that faces inwards on a convex BSP leaf.
//
typedef struct seg_s
{
    vertex_t *v1;
    vertex_t *v2;

    BAMAngle angle;

    float length;

    // link in subsector list.
    // (NOTE: sorted in clockwise order)
    struct seg_s *sub_next;

    // -AJA- 1999/12/20: Reference to partner seg, or nullptr if the seg
    //       lies along a one-sided line.
    struct seg_s *partner;

    // -AJA- 1999/09/23: Reference to subsector on each side of seg,
    //       back_sub is nullptr for one-sided segs.
    //       (Addendum: back_sub is obsolete with new `partner' field)
    subsector_t *front_sub;
    subsector_t *back_sub;

    // -AJA- 1999/09/23: For "True BSP rendering", we keep track of the
    //       `minisegs' which define all the non-wall borders of the
    //       subsector.  Thus all the segs (normal + mini) define a
    //       closed convex polygon.  When the `miniseg' field is true,
    //       all the fields below it are unused.
    //
    bool miniseg;

    float offset;

    side_t *sidedef;
    line_t *linedef;

    int side; // 0 for front, 1 for back

    // Sector references.
    // backsector is nullptr for one sided lines

    sector_t *frontsector;
    sector_t *backsector;

    // compact list of sectors touching each vertex (can be nullptr)
    vertex_seclist_t *nb_sec[2];
} seg_t;

// Partition line.
typedef struct divline_s
{
    float x;
    float y;
    float dx;
    float dy;
} divline_t;

//
// BSP node.
//
typedef struct node_s
{
    divline_t div;
    float     div_len;

    // bit NF_V5_SUBSECTOR set for a subsector.
    unsigned int children[2];

    // Bounding boxes for this node.
    float bbox[2][4];
} node_t;

typedef struct secanim_s
{
    sector_t         *target             = nullptr;
    struct sector_s  *scroll_sec_ref     = nullptr;
    const LineType *scroll_special_ref = nullptr;
    line_s           *scroll_line_ref    = nullptr;
    HMM_Vec2            floor_scroll       = {{0, 0}};
    HMM_Vec2            ceil_scroll        = {{0, 0}};
    HMM_Vec3            push               = {{0, 0, 0}};
    bool              permanent          = false;
    float             last_height        = 0.0f;
} secanim_t;

typedef struct lineanim_s
{
    line_t           *target             = nullptr;
    struct sector_s  *scroll_sec_ref     = nullptr;
    const LineType *scroll_special_ref = nullptr;
    line_s           *scroll_line_ref    = nullptr;
    float             side0_xspeed       = 0.0;
    float             side1_xspeed       = 0.0;
    float             side0_yspeed       = 0.0;
    float             side1_yspeed       = 0.0;
    float             side0_xoffspeed    = 0.0;
    float             side0_yoffspeed    = 0.0;
    float             dynamic_dx         = 0.0;
    float             dynamic_dy         = 0.0;
    bool              permanent          = false;
    float             last_height        = 0.0f;
} lineanim_t;

typedef struct lightanim_s
{
    struct sector_s *light_sec_ref  = nullptr;
    line_s          *light_line_ref = nullptr;
} lightanim_t;

#endif /*__R_DEFS__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
