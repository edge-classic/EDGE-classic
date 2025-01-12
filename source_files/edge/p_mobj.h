//----------------------------------------------------------------------------
//  EDGE Moving Object Header
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
// IMPORTANT NOTE: Altering anything within the MapObject will most likely
//                 require changes to p_saveg.c and the save-game object
//                 (savegmobj_t); if you experience any problems with
//                 savegames, check here!
//

#pragma once

#include <unordered_set>

#include "con_var.h"
#include "ddf_types.h"
#include "m_math.h"

// forward decl.
class AttackDefinition;
class MapObjectDefinition;
class Image;
class AbstractShader;

class Player;
struct RADScript;
struct RegionProperties;
struct State;
struct Subsector;
struct TouchNode;
struct Line;

extern std::unordered_set<const MapObjectDefinition *> seen_monsters;

extern bool time_stop_active;

extern ConsoleVariable gravity_factor;

constexpr float kStopSpeed = 0.07f;

//
// NOTES: MapObject
//
// MapObjects are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which Patch
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost always set
// from State structures.
//
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
//
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for Patchs grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the MapObject.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when MapObjects are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The MapObject->flags element has various bit flags
// used by the simulation.
//
// Every MapObject is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any MapObject that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable MapObject that has its origin contained.
//
// A valid MapObject is a MapObject that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// kMapObjectFlagNoSector flag set (the subsector_t needs to be valid
// even if kMapObjectFlagNoSector is set), and is linked into a blockmap
// block or has the kMapObjectFlagNoBlockmap flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

// Directions
enum DirectionType
{
    kDirectionEast,
    kDirectionNorthEast,
    kDirectionNorth,
    kDirectionNorthWest,
    kDirectionWest,
    kDirectionSouthwest,
    kDirectionSouth,
    kDirectionSoutheast,

    kDirectionNone,

    kDirectionSlowTurn,
    kDirectionFastTurn,
    kDirectionWalking,
    kDirectionEvasive
};

struct SpawnPoint
{
    // location on the map.  `z' can take the special values kOnFloorZ
    // and kOnCeilingZ.
    float x, y, z;

    // direction thing faces
    BAMAngle angle;
    BAMAngle vertical_angle;

    // type of thing
    const MapObjectDefinition *info;

    // certain flags (mainly kMapObjectFlagAmbush).
    int flags;

    // tag number
    int tag;
};

struct DynamicLightState
{
    float           r;      // radius
    float           target; // target radius
    RGBAColor       color;
    AbstractShader *shader;
    Line           *glow_wall     = nullptr;
    bool            bad_wall_glow = false;
};

constexpr float kInvalidPosition = -999999.0f;

// Map Object definition.
struct Position
{
    float x, y, z;
};

class MapObject : public Position
{
  public:
    const MapObjectDefinition *info_ = nullptr;

    BAMAngle angle_          = 0; // orientation
    BAMAngle vertical_angle_ = 0; // looking up or down

    // For movement checking.
    float radius_ = 0;
    float height_ = 0;
    float scale_  = 1.0f;
    float aspect_ = 1.0f;
    float alpha_  = 1.0f;

    // Momentum, used to update position.
    HMM_Vec3 momentum_ = {{0, 0, 0}};

    // Track hover phase for time stop shenanigans
    float phase_ = 0.0f;

    // current subsector
    struct Subsector *subsector_ = nullptr;

    // properties from extrafloor the thing is in
    struct RegionProperties *region_properties_ = nullptr;

    // Uncapped stuff - Dasho
    float    old_x_              = 0;
    float    old_y_              = 0;
    BAMAngle old_angle_          = 0;
    BAMAngle old_vertical_angle_ = 0;

    // Vert slope stuff maybe
    float old_z_       = 0;
    float old_floor_z_ = 0;
    bool  on_slope_    = false;

    // The closest interval over all contacted Sectors.
    float floor_z_   = 0;
    float ceiling_z_ = 0;
    float dropoff_z_ = 0;

    // This is the current speed of the object.
    // if fast_monsters, it is already calculated.
    float speed_ = 0;
    int   fuse_  = 0;

    // When this times out we go to "MORPH" state
    int morph_timeout_ = 0;

    // Thing's health level
    float health_       = 0;
    float spawn_health_ = 0;

    // state tic counter
    int tics_     = 0;
    int tic_skip_ = 0;

    const struct State *state_      = nullptr;
    const struct State *next_state_ = nullptr;

    // flags (Old and New)
    int flags_          = 0;
    int extended_flags_ = 0;
    int hyper_flags_    = 0;
    int mbf21_flags_    = 0;

    int   model_skin_       = 0;
    int   model_last_frame_ = 0;
    float model_scale_      = 1.0f;
    float model_aspect_     = 1.0f;

    // tag ID (for special operations)
    int         tag_                  = 0;
    std::string wait_until_dead_tags_ = "";

    // Movement direction, movement generation (zig-zagging).
    DirectionType move_direction_ = kDirectionEast; // 0-7

    // when 0, select a new dir
    int move_count_ = 0;

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int reaction_time_ = 0;

    // If >0, the target will be chased
    // no matter what (even if shot)
    int threshold_ = 0;

    // Additional info record for player avatars only.
    class Player *player_ = nullptr;

    // Player number last looked for.
    int last_look_ = 0;

    // For respawning.
    SpawnPoint spawnpoint_ = {0, 0, 0, 0, 0, nullptr, 0, 0};

    float original_height_ = 0;

    // current visibility and target visibility
    float visibility_        = 0;
    float target_visibility_ = 0;

    float pain_chance_ = 0;

    // current attack to be made
    const AttackDefinition *current_attack_ = nullptr;

    // spread count for Ordered spreaders
    int spread_count_ = 0;

    // If == valid_count, already checked.
    int valid_count_ = 0;

    // -ES- 1999/10/25 Reference Count.
    // All the following mobj references should be set *only* via the
    // SetXX() methods, where XX is the field name. This is useful because
    // it sets the pointer to nullptr if the mobj is removed, which protects
    // us from a crash.
    int reference_count_ = 0;

    // source of the mobj, used for projectiles (i.e. the shooter)
    MapObject *source_ = nullptr;

    // target of the mobj
    MapObject *target_ = nullptr;

    // current spawned fire of the mobj
    MapObject *tracer_ = nullptr;

    // if exists, we are supporting/helping this object
    MapObject *support_object_ = nullptr;
    int        side_           = 0;

    // objects that is above and below this one.  If there were several,
    // then the closest one (in Z) is chosen.  We are riding the below
    // object if the head height == our foot height.  We are being
    // ridden if our head == the above object's foot height.
    //
    MapObject *above_object_ = nullptr;
    MapObject *below_object_ = nullptr;

    // these delta values give what position from the ride_em thing's
    // center that we are sitting on.
    float ride_delta_x_ = 0;
    float ride_delta_y_ = 0;

    // -AJA- 1999/09/25: Path support.
    struct RADScript *path_trigger_ = nullptr;

    // if we're on a ladder, this is the linedef #, otherwise -1.
    int on_ladder_ = -1;

    DynamicLightState dynamic_light_ = {0, 0, 0, nullptr};

    // monster reload support: count the number of shots
    int shot_count_ = 0;

    // hash values for TUNNEL missiles
    uint32_t tunnel_hash_[2] = {0, 0};

    // position interpolation (disabled when lerp_num <= 1)
    short interpolation_number_   = 0;
    short interpolation_position_ = 0;

    HMM_Vec3 interpolation_from_ = {{0, 0, 0}};

    // touch list: sectors this thing is in or touches
    struct TouchNode *touch_sectors_ = nullptr;

    // linked list (map_object_list_head)
    MapObject *next_     = nullptr;
    MapObject *previous_ = nullptr;

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    MapObject *blockmap_next_     = nullptr;
    MapObject *blockmap_previous_ = nullptr;

    // More list: links in subsector (if needed)
    MapObject *subsector_next_     = nullptr;
    MapObject *subsector_previous_ = nullptr;

    // One more: link in dynamic light blockmap
    MapObject *dynamic_light_next_     = nullptr;
    MapObject *dynamic_light_previous_ = nullptr;

    // Player number last heard.
    int last_heard_ = 0;

    bool is_voodoo_ = false;

    bool slope_sight_hit_ = false;

    // Uncapped test - Dasho
    bool interpolate_ = false;

  public:
    bool IsRemoved() const;

    void SetTracer(MapObject *ref);
    void SetSource(MapObject *ref);
    void SetTarget(MapObject *ref);
    void SetSupportObject(MapObject *ref);
    void SetAboveObject(MapObject *ref);
    void SetBelowObject(MapObject *ref);
    void SetRealSource(MapObject *ref);

    bool IsSpawning();

    void AddMomentum(float xm, float ym, float zm);

    void ClearStaleReferences();

    // Stores what this mobj was before being MORPHed/BECOMEing
    const MapObjectDefinition *pre_become_ = nullptr;
};

// Item-in-Respawn-que Structure -ACB- 1998/07/30
struct RespawnQueueItem
{
    SpawnPoint               spawnpoint = {0, 0, 0, 0, 0, nullptr, 0, 0};
    int                      time       = 0;
    struct RespawnQueueItem *next       = nullptr;
    struct RespawnQueueItem *previous   = nullptr;
};

inline float MapObjectMidZ(MapObject *mo)
{
    return (mo->z + mo->height_ / 2);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
