//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Weapons)
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
//
// Player Weapons Setup and Parser Code
//
// -KM- 1998/11/25 File Written
//
#include "local.h"

#include "weapon.h"

#include "p_action.h"

#undef  DF
#define DF  DDF_FIELD

static weapondef_c *dynamic_weapon;

weapondef_container_c weapondefs;

static void DDF_WGetAmmo(const char *info, void *storage);
static void DDF_WGetUpgrade(const char *info, void *storage);
static void DDF_WGetSpecialFlags(const char *info, void *storage);

#undef  DDF_CMD_BASE
#define DDF_CMD_BASE  dummy_weapon
static weapondef_c dummy_weapon;

static const commandlist_t weapon_commands[] =
{
	DF("AMMOTYPE", ammo[0], DDF_WGetAmmo),
	DF("AMMOPERSHOT", ammopershot[0], DDF_MainGetNumeric),
	DF("CLIPSIZE", clip_size[0], DDF_MainGetNumeric),
	DF("AUTOMATIC", autofire[0], DDF_MainGetBoolean),
	DF("ATTACK", attack[0], DDF_MainRefAttack),
	DF("SPECIAL", specials[0], DDF_WGetSpecialFlags),

	DF("SEC_AMMOTYPE", ammo[1], DDF_WGetAmmo),
	DF("SEC_AMMOPERSHOT", ammopershot[1], DDF_MainGetNumeric),
	DF("SEC_CLIPSIZE", clip_size[1], DDF_MainGetNumeric),
	DF("SEC_AUTOMATIC", autofire[1], DDF_MainGetBoolean),
	DF("SEC_ATTACK", attack[1], DDF_MainRefAttack),
	DF("SEC_SPECIAL", specials[1], DDF_WGetSpecialFlags),

	DF("EJECT_ATTACK", eject_attack, DDF_MainRefAttack),
	DF("FREE", autogive, DDF_MainGetBoolean),
	DF("BINDKEY", bind_key, DDF_MainGetNumeric),
	DF("PRIORITY", priority, DDF_MainGetNumeric),
	DF("DANGEROUS", dangerous, DDF_MainGetBoolean),
	DF("UPGRADES", upgrade_weap, DDF_WGetUpgrade),
	DF("IDLE_SOUND", idle, DDF_MainLookupSound),
	DF("ENGAGED_SOUND", engaged, DDF_MainLookupSound),
	DF("HIT_SOUND", hit, DDF_MainLookupSound),
	DF("START_SOUND", start, DDF_MainLookupSound),
	DF("NOTHRUST", nothrust, DDF_MainGetBoolean),
	DF("FEEDBACK", feedback, DDF_MainGetBoolean),
	DF("KICK", kick, DDF_MainGetFloat),
	DF("ZOOM_FOV", zoom_fov, DDF_MainGetAngle),
	DF("REFIRE_INACCURATE", refire_inacc, DDF_MainGetBoolean),
	DF("SHOW_CLIP", show_clip, DDF_MainGetBoolean),
	DF("SHARED_CLIP", shared_clip, DDF_MainGetBoolean),
	DF("BOBBING", bobbing, DDF_MainGetPercent),
	DF("SWAYING", swaying, DDF_MainGetPercent),
	DF("IDLE_WAIT", idle_wait, DDF_MainGetTime),
	DF("IDLE_CHANCE", idle_chance, DDF_MainGetPercent),
	DF("MODEL_SKIN", model_skin, DDF_MainGetNumeric),
	DF("MODEL_ASPECT", model_aspect, DDF_MainGetFloat),
	DF("MODEL_BIAS", model_bias, DDF_MainGetFloat),
	DF("MODEL_FORWARD", model_forward, DDF_MainGetFloat),
	DF("MODEL_SIDE", model_side, DDF_MainGetFloat),

	// -AJA- backwards compatibility cruft...
	DF("SECOND_ATTACK", attack[1], DDF_MainRefAttack),

	DF("SOUND1", sound1, DDF_MainLookupSound),
	DF("SOUND2", sound2, DDF_MainLookupSound),
	DF("SOUND3", sound3, DDF_MainLookupSound),

	DDF_CMD_END
};


static const state_starter_t weapon_starters[] =
{
	DDF_STATE("UP",        "UP",        up_state),
	DDF_STATE("DOWN",      "DOWN",      down_state),
	DDF_STATE("READY",     "READY",     ready_state),
	DDF_STATE("EMPTY",     "EMPTY",     empty_state),
	DDF_STATE("IDLE",      "READY",     idle_state),
	DDF_STATE("CROSSHAIR", "CROSSHAIR", crosshair),
	DDF_STATE("ZOOM",      "ZOOM",      zoom_state),

	DDF_STATE("ATTACK",    "READY",     attack_state[0]),
	DDF_STATE("RELOAD",    "READY",     reload_state[0]),
	DDF_STATE("DISCARD",   "READY",     discard_state[0]),
	DDF_STATE("WARMUP",    "ATTACK",    warmup_state[0]),
	DDF_STATE("FLASH",     "REMOVE",    flash_state[0]),

	DDF_STATE("SECATTACK", "READY",     attack_state[1]),
	DDF_STATE("SECRELOAD", "READY",     reload_state[1]),
	DDF_STATE("SECDISCARD","READY",     discard_state[1]),
	DDF_STATE("SECWARMUP", "SECATTACK", warmup_state[1]),
	DDF_STATE("SECFLASH",  "REMOVE",    flash_state[1]),

	DDF_STATE_END
};


static const actioncode_t weapon_actions[] =
{
	{"NOTHING", NULL, NULL},

	{"RAISE",             A_Raise, NULL},
	{"LOWER",             A_Lower, NULL},
	{"READY",             A_WeaponReady, NULL},
	{"EMPTY",             A_WeaponEmpty, NULL},
	{"SHOOT",             A_WeaponShoot, DDF_StateGetAttack},
	{"EJECT",             A_WeaponEject, DDF_StateGetAttack},
	{"REFIRE",            A_ReFire, NULL},
	{"NOFIRE",            A_NoFire, NULL},
	{"NOFIRE_RETURN",     A_NoFireReturn, NULL},
	{"KICK",              A_WeaponKick, DDF_StateGetFloat},
	{"CHECKRELOAD",       A_CheckReload, NULL},
	{"PLAYSOUND",         A_WeaponPlaySound, DDF_StateGetSound},
	{"KILLSOUND",         A_WeaponKillSound, NULL},
	{"SET_SKIN",          A_WeaponSetSkin,   DDF_StateGetInteger},
	{"JUMP",              A_WeaponJump, DDF_StateGetJump},
	{"UNZOOM",            A_WeaponUnzoom, NULL},
	
	{"DJNE",              A_WeaponDJNE, DDF_StateGetJump},

	{"RTS_ENABLE_TAGGED", A_WeaponEnableRadTrig,  DDF_StateGetInteger},
	{"RTS_DISABLE_TAGGED",A_WeaponDisableRadTrig, DDF_StateGetInteger},
	{"SEC_SHOOT",         A_WeaponShootSA, DDF_StateGetAttack},
	{"SEC_REFIRE",        A_ReFireSA, NULL},
	{"SEC_NOFIRE",        A_NoFireSA, NULL},
	{"SEC_NOFIRE_RETURN", A_NoFireReturnSA, NULL},
	{"SEC_CHECKRELOAD",   A_CheckReloadSA, NULL},

	// flash-related actions
	{"FLASH",             A_GunFlash, NULL},
	{"SEC_FLASH",         A_GunFlashSA, NULL},
	{"LIGHT0",            A_Light0, NULL},
	{"LIGHT1",            A_Light1, NULL},
	{"LIGHT2",            A_Light2, NULL},
	{"TRANS_SET",         A_WeaponTransSet,  DDF_StateGetPercent},
	{"TRANS_FADE",        A_WeaponTransFade, DDF_StateGetPercent},

	// crosshair-related actions
	{"SETCROSS",          A_SetCrosshair, DDF_StateGetFrame},
	{"TARGET_JUMP",       A_TargetJump, DDF_StateGetFrame},
	{"FRIEND_JUMP",       A_FriendJump, DDF_StateGetFrame},

	// -AJA- backwards compatibility cruft...
	{"SOUND1",           A_SFXWeapon1, NULL},
	{"SOUND2",           A_SFXWeapon2, NULL},
	{"SOUND3",           A_SFXWeapon3, NULL},

	{NULL, NULL, NULL}
};


const specflags_t ammo_types[] =
{
    {"NOAMMO",  AM_NoAmmo, 0},

    {"BULLETS",  AM_Bullet,  0},
    {"SHELLS",   AM_Shell,   0},
    {"ROCKETS",  AM_Rocket,  0},
    {"CELLS",    AM_Cell,    0},
    {"PELLETS",  AM_Pellet,  0},
    {"NAILS",    AM_Nail,    0},
    {"GRENADES", AM_Grenade, 0},
    {"GAS",      AM_Gas,     0},

    {"AMMO1",  AM_Bullet,  0},
    {"AMMO2",  AM_Shell,   0},
    {"AMMO3",  AM_Rocket,  0},
    {"AMMO4",  AM_Cell,    0},
    {"AMMO5",  AM_Pellet,  0},
    {"AMMO6",  AM_Nail,    0},
    {"AMMO7",  AM_Grenade, 0},
    {"AMMO8",  AM_Gas,     0},

    {"AMMO9",  AM_9,  0},
    {"AMMO10", AM_10, 0},
    {"AMMO11", AM_11, 0},
    {"AMMO12", AM_12, 0},
    {"AMMO13", AM_13, 0},
    {"AMMO14", AM_14, 0},
    {"AMMO15", AM_15, 0},
    {"AMMO16", AM_16, 0},
	{"AMMO17", AM_17, 0},
    {"AMMO18", AM_18, 0},
    {"AMMO19", AM_19, 0},
    {"AMMO20", AM_20, 0},
	{"AMMO21", AM_21, 0},
    {"AMMO22", AM_22, 0},
    {"AMMO23", AM_23, 0},
    {"AMMO24", AM_24, 0},
	
    {NULL, 0, 0}
};



//
//  DDF PARSE ROUTINES
//

static void WeaponStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New weapon entry is missing a name!");
		name = "WEAPON_WITH_NO_NAME";
	}

	dynamic_weapon = weapondefs.Lookup(name);

	if (extend)
	{
		if (! dynamic_weapon)
			DDF_Error("Unknown weapon to extend: %s\n", name);

		DDF_StateBeginRange(dynamic_weapon->state_grp);
		return;
	}

	// replaces an existing entry?
	if (dynamic_weapon)
	{
		dynamic_weapon->Default();
	}
	else
	{
		// not found, create a new one
		dynamic_weapon = new weapondef_c;
		dynamic_weapon->name = name;

		weapondefs.Insert(dynamic_weapon);
	}

	DDF_StateBeginRange(dynamic_weapon->state_grp);
}


static void WeaponDoTemplate(const char *contents)
{
	weapondef_c *other = weapondefs.Lookup(contents);

	if (!other || other == dynamic_weapon)
		DDF_Error("Unknown weapon template: '%s'\n", contents);

	dynamic_weapon->CopyDetail(*other);

	DDF_StateBeginRange(dynamic_weapon->state_grp);
}


static void WeaponParseField(const char *field, const char *contents,
    int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("WEAPON_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_CompareName(field, "TEMPLATE") == 0)
	{
		WeaponDoTemplate(contents);
		return;
	}

	if (DDF_MainParseField(weapon_commands, field, contents, (byte *)dynamic_weapon))
		return;

	if (DDF_MainParseState((byte *)dynamic_weapon, dynamic_weapon->state_grp,
	                       field, contents, index, is_last, true /* is_weapon */,
						   weapon_starters, weapon_actions))
		return;

	DDF_WarnError("Unknown weapons.ddf command: %s\n", field);
}


static void WeaponFinishEntry(void)
{
	if (! dynamic_weapon->state_grp.back().first)
		DDF_Error("Weapon `%s' has missing states.\n",
			dynamic_weapon->name.c_str());

	DDF_StateFinishRange(dynamic_weapon->state_grp);

	// check stuff...
	int ATK;

	for (ATK = 0; ATK < 2; ATK++)
	{
		if (dynamic_weapon->ammopershot[ATK] < 0)
		{
			DDF_WarnError("Bad %sAMMOPERSHOT value for weapon: %d\n",
					ATK ? "SEC_" : "", dynamic_weapon->ammopershot[ATK]);
			dynamic_weapon->ammopershot[ATK] = 0;
		}

		// zero values for ammopershot really mean infinite ammo
		if (dynamic_weapon->ammopershot[ATK] == 0)
			dynamic_weapon->ammo[ATK] = AM_NoAmmo;

		if (dynamic_weapon->clip_size[ATK] < 0)
		{
			DDF_WarnError("Bad %sCLIPSIZE value for weapon: %d\n",
					ATK ? "SEC_" : "", dynamic_weapon->clip_size[ATK]);
			dynamic_weapon->clip_size[ATK] = 0;
		}

		// check if clip_size + ammopershot makes sense
		if (dynamic_weapon->clip_size[ATK] > 0 && dynamic_weapon->ammo[ATK] != AM_NoAmmo &&
			(dynamic_weapon->clip_size[ATK] < dynamic_weapon->ammopershot[ATK] ||
			 (dynamic_weapon->clip_size[ATK] % dynamic_weapon->ammopershot[ATK] != 0)))
		{
			DDF_WarnError("%sAMMOPERSHOT=%d incompatible with %sCLIPSIZE=%d\n",
				ATK ? "SEC_" : "", dynamic_weapon->ammopershot[ATK],
				ATK ? "SEC_" : "", dynamic_weapon->clip_size[ATK]);
			dynamic_weapon->ammopershot[ATK] = 1;
		}

		// DISCARD states require the PARTIAL special
		if (dynamic_weapon->discard_state[ATK] &&
			! (dynamic_weapon->specials[ATK] & WPSP_Partial))
		{
			DDF_Error("Cannot use %sDISCARD states with NO_PARTIAL special.\n",
				ATK ? "SEC_" : "");
		}
	}

	if (dynamic_weapon->shared_clip)
	{
		if (dynamic_weapon->clip_size[0] == 0)
			DDF_Error("SHARED_CLIP requires a clip weapon (missing CLIPSIZE)\n");

		if (dynamic_weapon->attack_state[1] == 0)
			DDF_Error("SHARED_CLIP used without secondary attack states.\n");

		if (dynamic_weapon->ammo[1] != AM_NoAmmo ||
			dynamic_weapon->ammopershot[1] != 0 ||
			dynamic_weapon->clip_size[1] != 0)
		{
			DDF_Error("SHARED_CLIP cannot be used with SEC_AMMO or SEC_CLIPSIZE commands.\n");
		}
	}

	if (dynamic_weapon->model_skin < 0 || dynamic_weapon->model_skin > 9)
		DDF_Error("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n",
			dynamic_weapon->model_skin);

	// backwards compatibility
	if (dynamic_weapon->priority < 0)
	{
		DDF_WarnError("Using PRIORITY=-1 in weapons.ddf is obsolete !\n");

		dynamic_weapon->dangerous = true;
		dynamic_weapon->priority = 10;
	}
}


static void WeaponClearAll(void)
{
	// not safe to delete weapons, there are (integer) references

	// not using SetDisabledCount() since it breaks castle.wad

	epi::array_iterator_c it;

	for (it = weapondefs.GetIterator(0); it.IsValid(); it++)
	{
		weapondef_c *wd = ITERATOR_TO_TYPE(it, weapondef_c*);

		if (wd)
		{
			wd->no_cheat = true;
			wd->autogive = false;
		}
	}
}


//
// DDF_ReadWeapons
//
bool DDF_ReadWeapons(void *data, int size)
{
	readinfo_t weapons;

	weapons.memfile = (char*)data;
	weapons.memsize = size;
	weapons.tag = "WEAPONS";
	weapons.entries_per_dot = 1;

	if (weapons.memfile)
	{
		weapons.message = NULL;
		weapons.filename = NULL;
		weapons.lumpname = "DDFWEAP";
	}
	else
	{
		weapons.message = "DDF_InitWeapons";
		weapons.filename = "weapons.ddf";
		weapons.lumpname = NULL;
	}

	weapons.start_entry  = WeaponStartEntry;
	weapons.parse_field  = WeaponParseField;
	weapons.finish_entry = WeaponFinishEntry;
	weapons.clear_all    = WeaponClearAll;

	return DDF_MainReadFile(&weapons);
}

void DDF_WeaponInit(void)
{
	weapondefs.Clear();
}

void DDF_WeaponCleanUp(void)
{
	// Trim down the required to size
	weapondefs.Trim();
}

static void DDF_WGetAmmo(const char *info, void *storage)
{
	int *ammo = (int *)storage;
	int flag_value;

	switch (DDF_MainCheckSpecialFlag(info, ammo_types, &flag_value,
				false, false))
	{
		case CHKF_Positive:
		case CHKF_Negative:
			(*ammo) = flag_value;
			break;

		case CHKF_User:
		case CHKF_Unknown:
			DDF_WarnError("Unknown Ammo type '%s'\n", info);
			break;
	}
}


static void DDF_WGetUpgrade(const char *info, void *storage)
{
	weapondef_c **dest = (weapondef_c **)storage;

	*dest = weapondefs.Lookup(info);

	if (*dest == NULL)
		DDF_Warning("Unknown weapon: %s\n", info);
}

static specflags_t weapon_specials[] =
{
    {"SILENT_TO_MONSTERS", WPSP_SilentToMon, 0},
    {"ANIMATED",  WPSP_Animated, 0},
    {"SWITCH",  WPSP_SwitchAway, 0},
	{"TRIGGER", WPSP_Trigger, 0},
	{"FRESH",   WPSP_Fresh, 0},
	{"MANUAL",  WPSP_Manual, 0},
	{"PARTIAL", WPSP_Partial, 0},
    {NULL, WPSP_None, 0}
};

//
// DDF_WGetSpecialFlags
//
static void DDF_WGetSpecialFlags(const char *info, void *storage)
{
	int flag_value;

	weapon_flag_e *dest = (weapon_flag_e *) storage;

	switch (DDF_MainCheckSpecialFlag(info, weapon_specials, &flag_value,
				true, false))
	{
		case CHKF_Positive:
			*dest = (weapon_flag_e)(*dest | flag_value);
			break;

		case CHKF_Negative:
			*dest = (weapon_flag_e)(*dest & ~flag_value);
			break;

		case CHKF_User:
		case CHKF_Unknown:
			DDF_WarnError("DDF_WGetSpecialFlags: Unknown Special: %s", info);
			return;
	}
}

//
// DDF_WeaponIsUpgrade
//
// Checks whether first weapon is an upgrade of second one,
// including indirectly (e.g. an upgrade of an upgrade).
//
bool DDF_WeaponIsUpgrade(weapondef_c *weap, weapondef_c *old)
{
	if (!weap || !old || weap == old)
		return false;
	
	for (int loop = 0; loop < 10; loop++)
	{
		if (! weap->upgrade_weap)
			return false;

		if (weap->upgrade_weap == old)
			return true;
	
		weap = weap->upgrade_weap;
	}

	return false;
}

// --> Weapon Definition

//
// weapondef_c Constructor
//
weapondef_c::weapondef_c() : name(), state_grp()
{
	Default();
}


//
// weapondef_c Destructor
//
weapondef_c::~weapondef_c()
{
}

//
// weapondef_c::CopyDetail()
//
void weapondef_c::CopyDetail(weapondef_c &src)
{
	state_grp.clear();

	for (unsigned int i = 0; i < src.state_grp.size(); i++)
		state_grp.push_back(src.state_grp[i]);

	for (int ATK = 0; ATK < 2; ATK++)
	{
		attack[ATK] = src.attack[ATK];
		ammo[ATK]   = src.ammo[ATK];
		ammopershot[ATK] = src.ammopershot[ATK];
		autofire[ATK]  = src.autofire[ATK];
		clip_size[ATK] = src.clip_size[ATK];
		specials[ATK]  = src.specials[ATK];

		attack_state[ATK]  = src.attack_state[ATK];
		reload_state[ATK]  = src.reload_state[ATK];
		discard_state[ATK] = src.discard_state[ATK];
		warmup_state[ATK]  = src.warmup_state[ATK];
		flash_state[ATK]   = src.flash_state[ATK];
	}

	kick = src.kick;

	up_state    = src.up_state;
	down_state  = src.down_state;
	ready_state = src.ready_state;
	empty_state = src.empty_state;
	idle_state  = src.idle_state;
	crosshair   = src.crosshair;
	zoom_state  = src.zoom_state;

	no_cheat = src.no_cheat;

	autogive = src.autogive;
	feedback = src.feedback;
	upgrade_weap = src.upgrade_weap;

	priority = src.priority;
	dangerous = src.dangerous;

	eject_attack = src.eject_attack;

	idle = src.idle;
	engaged = src.engaged;
	hit = src.hit;
	start = src.start;

	sound1 = src.sound1;
	sound2 = src.sound2;
	sound3 = src.sound3;

	nothrust = src.nothrust;

  	bind_key = src.bind_key;

	zoom_fov = src.zoom_fov;
	refire_inacc = src.refire_inacc;
	show_clip = src.show_clip;
	shared_clip = src.shared_clip;

	bobbing = src.bobbing;
	swaying = src.swaying;
	idle_wait = src.idle_wait;
	idle_chance = src.idle_chance;

	model_skin = src.model_skin;
	model_aspect = src.model_aspect;
	model_bias = src.model_bias;
	model_forward = src.model_forward;
	model_side = src.model_side;
}

//
// weapondef_c::Default()
//
void weapondef_c::Default(void)
{
	state_grp.clear();

	for (int ATK = 0; ATK < 2; ATK++)
	{
		attack[ATK] = NULL;
		ammo[ATK]   = AM_NoAmmo;
		ammopershot[ATK] = 0;
		clip_size[ATK]   = 0;
		autofire[ATK]    = false;

		attack_state[ATK]  = 0;
		reload_state[ATK]  = 0;
		discard_state[ATK] = 0;
		warmup_state[ATK]  = 0;
		flash_state[ATK]   = 0;
	}

	specials[0] = DEFAULT_WPSP;
	specials[1] = (weapon_flag_e)(DEFAULT_WPSP & ~WPSP_SwitchAway);

	kick = 0.0f;

	up_state = 0;
	down_state= 0;
	ready_state = 0;
	empty_state = 0;
	idle_state = 0;

	crosshair = 0;
	zoom_state = 0;

	no_cheat = false;

	autogive = false;
	feedback = false;
	upgrade_weap = NULL;
	priority = 0;
	dangerous = false;

	eject_attack = NULL;
	idle = NULL;
	engaged = NULL;
	hit = NULL;
	start = NULL;

	sound1 = NULL;
	sound2 = NULL;
	sound3 = NULL;

	nothrust = false;
	bind_key = -1;
	zoom_fov = 0;
	refire_inacc = false;
	show_clip = false;
	shared_clip = false;

	bobbing = PERCENT_MAKE(100);
	swaying = PERCENT_MAKE(100);
	idle_wait = 15 * TICRATE;
	idle_chance = PERCENT_MAKE(12);

	model_skin = 1;
	model_aspect = 1.0f;
	model_bias = 0.0f;
	model_forward = 0.0f;
	model_side = 0.0f;
}


// --> Weapon Definition Container

//
// weapondef_container_c Constructor
//
weapondef_container_c::weapondef_container_c() 
	: epi::array_c(sizeof(weapondef_c*))
{
}

//
// weapondef_container_c Destructor
//
weapondef_container_c::~weapondef_container_c()
{
	Clear();
}

//
// weapondef_container_c::CleanupObject()
//
void weapondef_container_c::CleanupObject(void *obj)
{
	weapondef_c *w = *(weapondef_c**)obj;

	if (w)
		delete w;

	return;
}

//
// weapondef_container_c::FindFirst()
//
int weapondef_container_c::FindFirst(const char *name, int startpos)
{
	epi::array_iterator_c it;
	weapondef_c *w;

	if (startpos>0)
		it = GetIterator(startpos);
	else
		it = GetBaseIterator();

	while (it.IsValid())
	{
		w = ITERATOR_TO_TYPE(it, weapondef_c*);
		if (DDF_CompareName(w->name.c_str(), name) == 0)
		{
			return it.GetPos();
		}

		it++;
	}

	return -1;
}

//
// weapondef_container_c::Lookup()
//
weapondef_c* weapondef_container_c::Lookup(const char* refname)
{
	int idx = FindFirst(refname, 0);
	if (idx >= 0)
		return (*this)[idx];

	return NULL;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
