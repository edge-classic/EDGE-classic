//----------------------------------------------------------------------------
//  EDGE Moving Object Header
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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
// IMPORTANT NOTE: Altering anything within the mobj_t will most likely
//                 require changes to p_saveg.c and the save-game object
//                 (savegmobj_t); if you experience any problems with
//                 savegames, check here!
//

#ifndef __P_MOBJ_H__
#define __P_MOBJ_H__

#include "types.h"
#include "m_math.h"

#include <unordered_set>

// forward decl.
class atkdef_c;
class mobjtype_c;
class image_c;
class abstract_shader_c;

struct mobj_s;
struct player_s;
struct rad_script_s;
struct region_properties_s;
struct state_s;
struct subsector_s;
struct touch_node_s;

extern std::unordered_set<const mobjtype_c *> seen_monsters;

#define STOPSPEED   		0.07f
#define OOF_SPEED   		9.0f //Lobo: original value 20.0f too high, almost never played oof

//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost always set
// from state_t structures.
//
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
//
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.  
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

// Directions
typedef enum
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,

	DI_NODIR,

	DI_SLOWTURN,
	DI_FASTTURN,
	DI_WALKING,
	DI_EVASIVE
}
dirtype_e;

typedef struct
{
	// location on the map.  `z' can take the special values ONFLOORZ
	// and ONCEILINGZ.
	float x, y, z;

	// direction thing faces
	angle_t angle;
	angle_t vertangle;

	// type of thing
	const mobjtype_c *info;

	// certain flags (mainly MF_AMBUSH).
	int flags;

	// tag number (from Hexen map format)
	int tag;
}
spawnpoint_t;


struct position_c
{
public:
	float x, y, z;
};

typedef struct dlight_state_s
{
	float r;  // radius
	float target;  // target radius
	rgbcol_t color;
///--- const image_c *image;
	abstract_shader_c *shader;
}
dlight_state_t;


// Map Object definition.
typedef struct mobj_s mobj_t;

struct mobj_s : public position_c
{
	const mobjtype_c *info;

	angle_t angle;      // orientation
	angle_t vertangle;  // looking up or down

	// For movement checking.
	float radius;
	float height;

	// Momentum, used to update position.
	vec3_t mom;

	// current subsector
	struct subsector_s *subsector;

	// properties from extrafloor the thing is in
	struct region_properties_s *props;

	// The closest interval over all contacted Sectors.
	float floorz;
	float ceilingz;
	float dropoffz;

	// This is the current speed of the object.
	// if fastparm, it is already calculated.
	float speed;
	int fuse;

	// Thing's health level
	float health;

	// state tic counter
	int tics;
	int tic_skip;

	const struct state_s *state;
	const struct state_s *next_state;

	// flags (Old and New)
	int flags;
	int extendedflags;
	int hyperflags;
	int mbf21flags;

	int model_skin;
	int model_last_frame;

	// tag ID (for special operations)
	int tag;

	// Movement direction, movement generation (zig-zagging).
	dirtype_e movedir;  // 0-7

	// when 0, select a new dir
	int movecount;

	// Reaction time: if non 0, don't attack yet.
	// Used by player to freeze a bit after teleporting.
	int reactiontime;

	// If >0, the target will be chased
	// no matter what (even if shot)
	int threshold;

	// Additional info record for player avatars only.
	struct player_s *player;

	// Player number last looked for.
	int lastlook;

	// For respawning.
	spawnpoint_t spawnpoint;

	float origheight;

	// current visibility and target visibility
	float visibility;
	float vis_target;

	// current attack to be made
	const atkdef_c *currentattack;

	// spread count for Ordered spreaders
	int spreadcount;

	// If == validcount, already checked.
	int validcount;

	// -ES- 1999/10/25 Reference Count. DO NOT TOUCH.
	// All the following mobj references should be set only
	// through P_MobjSetX, where X is the field name. This is useful because
	// it sets the pointer to NULL if the mobj is removed, this protects us
	// from a crash.
	int refcount;

	// source of the mobj, used for projectiles (i.e. the shooter)
	mobj_t * source;

	// target of the mobj
	mobj_t * target;

	// current spawned fire of the mobj
	mobj_t * tracer;

	// if exists, we are supporting/helping this object
	mobj_t * supportobj;
	int side;

	// objects that is above and below this one.  If there were several,
	// then the closest one (in Z) is chosen.  We are riding the below
	// object if the head height == our foot height.  We are being
	// ridden if our head == the above object's foot height.
	//
	mobj_t * above_mo;
	mobj_t * below_mo;

	// these delta values give what position from the ride_em thing's
	// center that we are sitting on.
	float ride_dx, ride_dy;

	// -AJA- 1999/09/25: Path support.
	struct rad_script_s *path_trigger;

	// if we're on a ladder, this is the linedef #, otherwise -1.
	int on_ladder;

	dlight_state_t dlight;

	// monster reload support: count the number of shots
	int shot_count;

	// hash values for TUNNEL missiles
	u32_t tunnel_hash[2];

	// position interpolation (disabled when lerp_num <= 1)
	short lerp_num;
	short lerp_pos;

	vec3_t lerp_from;

	// touch list: sectors this thing is in or touches
	struct touch_node_s *touch_sectors;

	// linked list (mobjlisthead)
	mobj_t *next, *prev;

	// Interaction info, by BLOCKMAP.
	// Links in blocks (if needed).
	mobj_t *bnext, *bprev;

	// More list: links in subsector (if needed)
	mobj_t *snext, *sprev;

	// One more: link in dynamic light blockmap
	mobj_t *dlnext, *dlprev;

public:
	bool isRemoved() const;

	void SetTracer(mobj_t *ref);
	void SetSource(mobj_t *ref);
	void SetTarget(mobj_t *ref);
	void SetSupportObj(mobj_t *ref);
	void SetAboveMo(mobj_t *ref);
	void SetBelowMo(mobj_t *ref);
	void SetRealSource(mobj_t *ref);

	void ClearStaleRefs();
};

// Item-in-Respawn-que Structure -ACB- 1998/07/30
typedef struct iteminque_s
{
	spawnpoint_t spawnpoint;
	int time;
	struct iteminque_s *next;
	struct iteminque_s *prev;
}
iteminque_t;

// useful macro for the vertical center of an object
#define MO_MIDZ(mo)  ((mo)->z + (mo)->height / 2)

#endif  /*__P_MOBJ_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
