//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Things)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// This file handles:
//    mobj_t        [MOBJ]
//    spawnspot_t   [SPWN]
//    iteminque_t   [ITMQ]
//

#include "i_defs.h"

#include "p_setup.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "z_zone.h"


#undef SF
#define SF  SVFIELD


// forward decls.
int SV_MobjCountElems(void);
int SV_MobjFindElem(mobj_t *elem);
void * SV_MobjGetElem(int index);
void SV_MobjCreateElems(int num_elems);
void SV_MobjFinaliseElems(void);

int SV_ItemqCountElems(void);
int SV_ItemqFindElem(iteminque_t *elem);
void * SV_ItemqGetElem(int index);
void SV_ItemqCreateElems(int num_elems);
void SV_ItemqFinaliseElems(void);

bool SR_MobjGetPlayer(void *storage, int index, void *extra);
bool SR_MobjGetMobj(void *storage, int index, void *extra);
bool SR_MobjGetType(void *storage, int index, void *extra);
bool SR_MobjGetState(void *storage, int index, void *extra);
bool SR_MobjGetSpawnPoint(void *storage, int index, void *extra);
bool SR_MobjGetAttack(void *storage, int index, void *extra);

void SR_MobjPutPlayer(void *storage, int index, void *extra);
void SR_MobjPutMobj(void *storage, int index, void *extra);
void SR_MobjPutType(void *storage, int index, void *extra);
void SR_MobjPutState(void *storage, int index, void *extra);
void SR_MobjPutSpawnPoint(void *storage, int index, void *extra);
void SR_MobjPutAttack(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  MOBJ STRUCTURE AND ARRAY
//
static mobj_t sv_dummy_mobj;

#define SV_F_BASE  sv_dummy_mobj

static savefield_t sv_fields_mobj[] =
{
	SF(x, "x", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(y, "y", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(z, "z", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(angle, "angle", 1, SVT_ANGLE, SR_GetAngle, SR_PutAngle),
	SF(floorz, "floorz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(ceilingz, "ceilingz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(dropoffz, "dropoffz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(radius, "radius", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(height, "height", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(mom, "mom", 1, SVT_VEC3, SR_GetVec3, SR_PutVec3),
	SF(health, "health", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(speed, "speed", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(fuse, "fuse", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(info, "info", 1, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
	SF(state, "state", 1, SVT_STRING, SR_MobjGetState, SR_MobjPutState),
	SF(next_state, "next_state", 1, SVT_STRING, SR_MobjGetState, SR_MobjPutState),
	SF(tics, "tics", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(flags, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(extendedflags, "extendedflags", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(hyperflags, "hyperflags", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(movedir, "movedir", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(movecount, "movecount", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(reactiontime, "reactiontime", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(threshold, "threshold", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(model_skin, "model_skin", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(tag, "tag", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(side, "side", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(player, "player", 1, SVT_INDEX("players"), 
		SR_MobjGetPlayer, SR_MobjPutPlayer),
	SF(spawnpoint, "spawnpoint", 1, SVT_STRUCT("spawnpoint_t"), 
		SR_MobjGetSpawnPoint, SR_MobjPutSpawnPoint),
	SF(origheight, "origheight", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(visibility, "visibility", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(vis_target, "vis_target", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(vertangle, "vertangle", 1, SVT_FLOAT, SR_GetAngleFromSlope, SR_PutAngleToSlope),
	SF(spreadcount, "spreadcount", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(currentattack, "currentattack", 1, SVT_STRING, SR_MobjGetAttack, SR_MobjPutAttack),
	SF(source, "source", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(target, "target", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(tracer, "tracer", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(supportobj, "supportobj", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(above_mo, "above_mo", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(below_mo, "below_mo", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
	SF(ride_dx, "ride_dx", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(ride_dy, "ride_dy", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(on_ladder, "on_ladder", 1, SVT_INT, SR_GetInt, SR_PutInt),
	SF(path_trigger, "path_trigger", 1, SVT_STRING,
	    SR_TriggerGetScript, SR_TriggerPutScript),
	SF(dlight.r, "dlight_qty", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(dlight.target, "dlight_target", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(dlight.color, "dlight_color", 1, SVT_RGBCOL, SR_GetRGB, SR_PutRGB),
	SF(shot_count, "shot_count", 1, SVT_INT, SR_GetInt, SR_PutInt),

	// NOT HERE:
	//   subsector & region: these are regenerated.
	//   next,prev,snext,sprev,bnext,bprev: links are regenerated.
	//   tunnel_hash: would be meaningless, and not important.
	//   lastlookup: being reset to zero won't hurt.
	//   ...

	SVFIELD_END
};

savestruct_t sv_struct_mobj =
{
	NULL,            // link in list
	"mobj_t",        // structure name
	"mobj",          // start marker
	sv_fields_mobj,  // field descriptions
	SVDUMMY,         // dummy base
	true,            // define_me
	NULL             // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_mobj =
{
	NULL,             // link in list
	"mobjs",          // array name
	&sv_struct_mobj,  // array type
	true,             // define_me
	true,             // allow_hub

	SV_MobjCountElems,     // count routine
	SV_MobjGetElem,        // index routine
	SV_MobjCreateElems,    // creation routine
	SV_MobjFinaliseElems,  // finalisation routine

	NULL,     // pointer to known array
	0         // loaded size
};


//----------------------------------------------------------------------------
//
//  SPAWNPOINT STRUCTURE
//
static spawnpoint_t sv_dummy_spawnpoint;

#define SV_F_BASE  sv_dummy_spawnpoint

static savefield_t sv_fields_spawnpoint[] =
{
	SF(x, "x", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(y, "y", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(z, "z", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
	SF(angle, "angle", 1, SVT_ANGLE, SR_GetAngle, SR_PutAngle),
	SF(vertangle, "slope", 1, SVT_FLOAT, SR_GetAngleFromSlope, SR_PutAngleToSlope),
	SF(info, "info", 1, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
	SF(flags, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt),

	SVFIELD_END
};

savestruct_t sv_struct_spawnpoint =
{
	NULL,                 // link in list
	"spawnpoint_t",       // structure name
	"spwn",               // start marker
	sv_fields_spawnpoint, // field descriptions
	SVDUMMY,              // dummy base
	true,                 // define_me
	NULL                  // pointer to known struct
};

#undef SV_F_BASE


//----------------------------------------------------------------------------
//
//  ITEMINQUE STRUCTURE AND ARRAY
//
static iteminque_t sv_dummy_iteminque;

#define SV_F_BASE  sv_dummy_iteminque

static savefield_t sv_fields_iteminque[] =
{
	SF(spawnpoint, "spawnpoint", 1, SVT_STRUCT("spawnpoint_t"), 
		SR_MobjGetSpawnPoint, SR_MobjPutSpawnPoint),
	SF(time, "time", 1, SVT_INT, SR_GetInt, SR_PutInt),

	// NOT HERE:
	//   next,prev: links are regenerated.

	SVFIELD_END
};

savestruct_t sv_struct_iteminque =
{
	NULL,                 // link in list
	"iteminque_t",        // structure name
	"itmq",               // start marker
	sv_fields_iteminque,  // field descriptions
	SVDUMMY,              // dummy base
	true,                 // define_me
	NULL                  // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_iteminque =
{
	NULL,                  // link in list
	"itemquehead",         // array name
	&sv_struct_iteminque,  // array type
	true,                  // define_me
	true,             // allow_hub

	SV_ItemqCountElems,     // count routine
	SV_ItemqGetElem,        // index routine
	SV_ItemqCreateElems,    // creation routine
	SV_ItemqFinaliseElems,  // finalisation routine

	NULL,  // pointer to known array
	0      // loaded size
};


//----------------------------------------------------------------------------

//
// SV_MobjCountElems
//
int SV_MobjCountElems(void)
{
	mobj_t *cur;
	int count=0;

	for (cur=mobjlisthead; cur; cur=cur->next)
		count++;

	return count;
}

//
// SV_MobjGetElem
//
// The index here starts at 0.
//
void *SV_MobjGetElem(int index)
{
	mobj_t *cur;

	for (cur=mobjlisthead; cur && index > 0; cur=cur->next)
		index--;

	if (!cur)
		I_Error("LOADGAME: Invalid Mobj: %d\n", index);

	SYS_ASSERT(index == 0);

	return cur;
}

//
// SV_MobjFindElem
//
// Returns the index number (starts at 0 here).
// 
int SV_MobjFindElem(mobj_t *elem)
{
	mobj_t *cur;
	int index;

	for (cur=mobjlisthead, index=0; cur && cur != elem; cur=cur->next)
		index++;

	if (!cur)
		I_Error("LOADGAME: No such MobjPtr: %p\n", elem);

	return index;
}

//
// SV_MobjCreateElems
//
void SV_MobjCreateElems(int num_elems)
{
	// free existing mobjs
	if (mobjlisthead)
		P_RemoveAllMobjs();

	SYS_ASSERT(mobjlisthead == NULL);

	for (; num_elems > 0; num_elems--)
	{
		mobj_t *cur = Z_New(mobj_t, 1);

		Z_Clear(cur, mobj_t, 1);

		cur->next = mobjlisthead;
		cur->prev = NULL;

		if (mobjlisthead)
			mobjlisthead->prev = cur;

		mobjlisthead = cur;

		// initialise defaults
		cur->info  = NULL;
		cur->state = cur->next_state = states+1;

		cur->model_skin = 1;
		cur->model_last_frame = -1;
	}
}

//
// SV_MobjFinaliseElems
//
void SV_MobjFinaliseElems(void)
{
	mobj_t *mo;

	for (mo=mobjlisthead; mo; mo=mo->next)
	{
		if (! mo->info)
			mo->info = mobjtypes.Lookup(0);  // template

		P_SetThingPosition(mo);

		// handle reference counts

#define REF_COUNT_FIELD(field)  \
	if (mo->field) mo->field->refcount++;

		REF_COUNT_FIELD(tracer);
		REF_COUNT_FIELD(source);
		REF_COUNT_FIELD(target);
		REF_COUNT_FIELD(supportobj);
		REF_COUNT_FIELD(above_mo);
		REF_COUNT_FIELD(below_mo);

#undef REF_COUNT_FIELD
		
		// sanity checks
	}
}


//----------------------------------------------------------------------------

//
// SV_ItemqCountElems
//
int SV_ItemqCountElems(void)
{
	iteminque_t *cur;
	int count=0;

	for (cur=itemquehead; cur; cur=cur->next)
		count++;

	return count;
}

//
// SV_ItemqGetElem
//
// The index value starts at 0.
//
void *SV_ItemqGetElem(int index)
{
	iteminque_t *cur;

	for (cur=itemquehead; cur && index > 0; cur=cur->next)
		index--;

	if (!cur)
		I_Error("LOADGAME: Invalid ItemInQue: %d\n", index);

	SYS_ASSERT(index == 0);
	return cur;
}

//
// SV_ItemqFindElem
//
// Returns the index number (starts at 0 here).
// 
int SV_ItemqFindElem(iteminque_t *elem)
{
	iteminque_t *cur;
	int index;

	for (cur=itemquehead, index=0; cur && cur != elem; cur=cur->next)
		index++;

	if (!cur)
		I_Error("LOADGAME: No such ItemInQue ptr: %p\n", elem);

	return index;
}

//
// SV_ItemqCreateElems
//
void SV_ItemqCreateElems(int num_elems)
{
	P_RemoveItemsInQue();

	itemquehead = NULL;

	for (; num_elems > 0; num_elems--)
	{
		iteminque_t *cur = Z_New(iteminque_t, 1);

		Z_Clear(cur, iteminque_t, 1);

		cur->next = itemquehead;
		cur->prev = NULL;

		if (itemquehead)
			itemquehead->prev = cur;

		itemquehead = cur;

		// initialise defaults: leave blank
	}
}

//
// SV_ItemqFinaliseElems
//
void SV_ItemqFinaliseElems(void)
{
	iteminque_t *cur, *next;

	// remove any dead wood
	for (cur = itemquehead; cur; cur = next)
	{
		next = cur->next;

		if (cur->spawnpoint.info)
			continue;

		I_Warning("LOADGAME: discarding empty ItemInQue\n");

		if (next)
			next->prev = cur->prev;

		if (cur->prev)
			cur->prev->next = next;
		else
			itemquehead = next;

		Z_Free(cur);
	}
}


//----------------------------------------------------------------------------

bool SR_MobjGetPlayer(void *storage, int index, void *extra)
{
	player_t ** dest = (player_t **)storage + index;

	int swizzle = SV_GetInt();

	*dest = (swizzle == 0) ? NULL : (player_t*)SV_PlayerGetElem(swizzle - 1);
	return true;
}

void SR_MobjPutPlayer(void *storage, int index, void *extra)
{
	player_t *elem = ((player_t **)storage)[index];

	int swizzle = (elem == NULL) ? 0 : SV_PlayerFindElem(elem) + 1;

	SV_PutInt(swizzle);
}

bool SR_MobjGetMobj(void *storage, int index, void *extra)
{
	mobj_t ** dest = (mobj_t **)storage + index;

	int swizzle = SV_GetInt();

	*dest = (swizzle == 0) ? NULL : (mobj_t*)SV_MobjGetElem(swizzle - 1);
	return true;
}

void SR_MobjPutMobj(void *storage, int index, void *extra)
{
	mobj_t *elem = ((mobj_t **)storage)[index];

	int swizzle;

	swizzle = (elem == NULL) ? 0 : SV_MobjFindElem(elem) + 1;
	SV_PutInt(swizzle);
}

bool SR_MobjGetType(void *storage, int index, void *extra)
{
	mobjtype_c ** dest = (mobjtype_c **)storage + index;

	const char *name = SV_GetString();

	if (! name)
	{
		*dest = NULL;
		return true;
	}

	// special handling for projectiles (attacks)
	if (strncmp(name, "atk:", 4) == 0)
	{
		const atkdef_c *atk = atkdefs.Lookup(name+4);

		if (atk)
			*dest = (mobjtype_c *)atk->atk_mobj;
	}
	else
		*dest = (mobjtype_c *)mobjtypes.Lookup(name);

	if (! *dest)
	{
		// Note: a missing 'info' field will be fixed up later
		I_Warning("LOADGAME: no such thing type '%s'\n",  name);
	}

	SV_FreeString(name);
	return true;
}

void SR_MobjPutType(void *storage, int index, void *extra)
{
	mobjtype_c *info = ((mobjtype_c **)storage)[index];

	SV_PutString((info == NULL) ? NULL : info->name.c_str());
}

bool SR_MobjGetSpawnPoint(void *storage, int index, void *extra)
{
	spawnpoint_t *dest = (spawnpoint_t *)storage + index;

	if (sv_struct_spawnpoint.counterpart)
		return SV_LoadStruct(dest, sv_struct_spawnpoint.counterpart);

	return true;  // presumably
}

void SR_MobjPutSpawnPoint(void *storage, int index, void *extra)
{
	spawnpoint_t *src = (spawnpoint_t *)storage + index;

	SV_SaveStruct(src, &sv_struct_spawnpoint);
}

bool SR_MobjGetAttack(void *storage, int index, void *extra)
{
	atkdef_c ** dest = (atkdef_c **)storage + index;

	const char *name = SV_GetString();

	// Intentional Const Override
	*dest = (name == NULL) ? NULL : (atkdef_c *)atkdefs.Lookup(name);

	SV_FreeString(name);
	return true;
}

void SR_MobjPutAttack(void *storage, int index, void *extra)
{
	atkdef_c *info = ((atkdef_c **)storage)[index];

	SV_PutString((info == NULL) ? NULL : info->name.c_str());
}


//----------------------------------------------------------------------------

//
// SR_MobjGetState
//
bool SR_MobjGetState(void *storage, int index, void *extra)
{
	state_t ** dest = (state_t **)storage + index;

	char buffer[256];
	char *base_p, *off_p;
	int base, offset;

	const char *swizzle;
	const mobj_t *mo = (mobj_t *) sv_current_elem;
	const mobjtype_c *actual;

	SYS_ASSERT(mo);

	swizzle = SV_GetString();

	if (!swizzle || !mo->info)
	{
		*dest = NULL;
		return true;
	}

	Z_StrNCpy(buffer, swizzle, 256-1);
	SV_FreeString(swizzle);

	// separate string at `:' characters

	base_p = strchr(buffer, ':');

	if (base_p == NULL || base_p[0] == 0)
		I_Error("Corrupt savegame: bad state 1/2: `%s'\n", buffer);

	*base_p++ = 0;

	off_p = strchr(base_p, ':');

	if (off_p == NULL || off_p[0] == 0)
		I_Error("Corrupt savegame: bad state 2/2: `%s'\n", base_p);

	*off_p++ = 0;

	// find thing that contains the state
	actual = mo->info;

	if (buffer[0] != '*')
	{
		// Do we care about those in the disabled group?
		actual = mobjtypes.Lookup(buffer);
		if (!actual)
			I_Error("LOADGAME: no such thing %s for state %s:%s\n",
			buffer, base_p, off_p);
	}

	// find base state
	offset = strtol(off_p, NULL, 0) - 1;

	base = DDF_StateFindLabel(actual->state_grp, base_p, true /* quiet */);

	if (! base)
	{
		I_Warning("LOADGAME: no such label `%s' for state.\n", base_p);
		offset = 0;

		if (actual->idle_state)
			base = actual->idle_state;
		else if (actual->spawn_state)
			base = actual->spawn_state;
		else if (actual->meander_state)
			base = actual->meander_state;
		else if (actual->state_grp.size() > 0)
			base = actual->state_grp[0].first;
		else
			base = 1;
	}

#if 0
	L_WriteDebug("Unswizzled state `%s:%s:%s' -> %d\n", 
		buffer, base_p, off_p, base + offset);
#endif

	*dest = states + base + offset;

	return true;
}

//
// SR_MobjPutState
//
// The format of the string is:
//
//    THING `:' BASE `:' OFFSET
//
// where THING is usually just "*" for the current thing, but can
// refer to another ddf thing (e.g. "IMP").  BASE is the nearest
// labelled state (e.g. "SPAWN"), or "*" as offset from the thing's
// first state (unlikely to be needed).  OFFSET is the integer offset
// from the base state (e.g. "5"), which BTW starts at 1 (like the ddf
// format).
//
// Alternatively, the string can be NULL, which means the state
// pointer should be NULL.
//
// P.S: we go to all this trouble to try and get reasonable behaviour
// when loading with different DDF files than what we saved with.
// Typical example: a new item, monster or weapon gets added to our
// DDF files causing all state numbers to be shifted upwards.
//
void SR_MobjPutState(void *storage, int index, void *extra)
{
	state_t *S = ((state_t **)storage)[index];

	char swizzle[256];

	int s_num, base;

	const mobj_t *mo = (mobj_t *) sv_current_elem;
	const mobjtype_c *actual;

	SYS_ASSERT(mo);

	if (S == NULL || !mo->info)
	{
		SV_PutString(NULL);
		return;
	}

	// object has no states ?
	if (mo->info->state_grp.empty())
	{
		I_Warning("SAVEGAME: object [%s] has no states !!\n", mo->info->name.c_str());
		SV_PutString(NULL);
		return;
	}

	// get state number, check if valid
	s_num = (int)(S - states);

	if (s_num < 0 || s_num >= num_states)
	{
		I_Warning("SAVEGAME: object [%s] is in invalid state %d\n", 
			mo->info->name.c_str(), s_num);

		if (mo->info->idle_state)
			s_num = mo->info->idle_state;
		else if (mo->info->spawn_state)
			s_num = mo->info->spawn_state;
		else if (mo->info->meander_state)
			s_num = mo->info->meander_state;
		else
		{
			SV_PutString("*:*:1");
			return;
		}
	}

	// state gone AWOL into another object ?
	actual = mo->info;

	if (! DDF_StateGroupHasState(actual->state_grp, s_num))
	{
		I_Warning("SAVEGAME: object [%s] is in AWOL state %d\n",
			mo->info->name.c_str(), s_num);

		epi::array_iterator_c it;

		// look for real object
		for (it = mobjtypes.GetBaseIterator(); it.IsValid(); it++)
		{
			actual = ITERATOR_TO_TYPE(it, mobjtype_c*);

			if (DDF_StateGroupHasState(actual->state_grp, s_num))
				break;
		}

		if (it.IsValid())
		{
			I_Warning("-- ARGH: state %d cannot be found !!\n", s_num);
			SV_PutString("*:*:1");
			return;
		}

		if (! actual->name)
		{
			I_Warning("-- OOPS: state %d found in unnamed object !!\n", s_num);
			SV_PutString("*:*:1");
			return;
		}
	}

	// find the nearest base state
	base = s_num;

	while (! states[base].label &&
	       DDF_StateGroupHasState(actual->state_grp, base-1))
	{
		base--;
	}

	sprintf(swizzle, "%s:%s:%d", 
		(actual == mo->info) ? "*" : actual->name.c_str(), 
		states[base].label ? states[base].label : "*",
		1 + s_num - base);

#if 0
	L_WriteDebug("Swizzled state %d of [%s] -> `%s'\n", 
		s_num, mo->info->name, swizzle);
#endif

	SV_PutString(swizzle);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
