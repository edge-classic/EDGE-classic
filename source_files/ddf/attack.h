//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __DDF_ATK_H__
#define __DDF_ATK_H__

#include "utility.h"

#include "types.h"


// ------------------------------------------------------------------
// --------------------ATTACK TYPE STRUCTURES------------------------
// ------------------------------------------------------------------

// -KM- 1998/11/25 Added BFG SPRAY attack type.

// FIXME!!! Move enums into attackdef_t
typedef enum
{
	ATK_NONE = 0,
	ATK_PROJECTILE,
	ATK_SPAWNER,
	ATK_DOUBLESPAWNER, //Lobo 2021: doom64 pain elemental
	ATK_TRIPLESPAWNER,
	ATK_SPREADER,
	ATK_RANDOMSPREAD,
	ATK_SHOT,
	ATK_TRACKER,
	ATK_CLOSECOMBAT,
	ATK_SHOOTTOSPOT,
	ATK_SKULLFLY,
	ATK_SMARTPROJECTILE,
	ATK_SPRAY,
	NUMATKCLASS
}
attackstyle_e;

typedef enum
{
	AF_None            = 0,

	AF_TraceSmoke      = (1 << 0),
	AF_KillFailedSpawn = (1 << 1),
	AF_PrestepSpawn    = (1 << 2),
	AF_SpawnTelefrags  = (1 << 3),

	AF_NeedSight       = (1 << 4),
	AF_FaceTarget      = (1 << 5),
	AF_Player          = (1 << 6),
	AF_ForceAim        = (1 << 7),

	AF_AngledSpawn     = (1 << 8),
	AF_NoTriggerLines  = (1 << 9),
	AF_SilentToMon     = (1 << 10),
	AF_NoTarget        = (1 << 11),
	AF_Vampire         = (1 << 12),
}
attackflags_e;

// attack definition class
class atkdef_c
{
public:
	atkdef_c();
	~atkdef_c();

public:
	void Default();
	void CopyDetail(atkdef_c &src);

	// Member vars
	epi::strent_c name;

	attackstyle_e attackstyle;
	attackflags_e flags;
	struct sfx_s *initsound;
	struct sfx_s *sound;
	float accuracy_slope;
	angle_t accuracy_angle;
	float xoffset;
	float yoffset;
	angle_t angle_offset;  // -AJA- 1999/09/10.
	float slope_offset;    //
	angle_t trace_angle; // -AJA- 2005/02/08.
	float assault_speed;
	float height;
	float range;
	int count;
	int tooclose;
	float berserk_mul;  // -AJA- 2005/08/06.
	damage_c damage;

	// class of the attack.
	bitset_t attack_class;
 
	// object init state.  The integer value only becomes valid after
	// DDF_AttackCleanUp() has been called.
	int objinitstate;
	epi::strent_c objinitstate_ref;
  
	percent_t notracechance;
	percent_t keepfirechance;

	// the MOBJ that is integrated with this attack, or NULL
	const mobjtype_c *atk_mobj;

	// spawned object (for spawners).  The mobjdef pointer only becomes
	// valid after DDF_AttackCleanUp().  Can be NULL.
	const mobjtype_c *spawnedobj;
	epi::strent_c spawnedobj_ref;
	int spawn_limit;
  
	// puff object.  The mobjdef pointer only becomes valid after
	// DDF_AttackCleanUp() has been called.  Can be NULL.
	const mobjtype_c *puff;
	epi::strent_c puff_ref;

private:
	// disable copy construct and assignment operator
	explicit atkdef_c(atkdef_c &rhs) { }
	atkdef_c& operator= (atkdef_c &rhs) { return *this; }
};


class atkdef_container_c : public epi::array_c
{
public:
	atkdef_container_c();
	~atkdef_container_c();

private:
	void CleanupObject(void *obj);

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(atkdef_c *a) { return InsertObject((void*)&a); }
	atkdef_c* operator[](int idx) { return *(atkdef_c**)FetchObject(idx); } 

	// Search Functions
	atkdef_c* Lookup(const char* refname);
};


// -----EXTERNALISATIONS-----

extern atkdef_container_c atkdefs;			// -ACB- 2004/06/09 Implemented

bool DDF_ReadAtks(void *data, int size);

#endif // __DDF_ATK_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
