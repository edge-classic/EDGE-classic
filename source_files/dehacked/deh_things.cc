//------------------------------------------------------------------------
//  THING Conversion
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <string>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_info.h"
#include "deh_field.h"
#include "deh_frames.h"
#include "deh_misc.h"
#include "deh_mobj.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_things.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_util.h"
#include "deh_wad.h"
#include "deh_weapons.h"


namespace Deh_Edge
{

#define DEBUG_MONST  0


#define EF_DISLOYAL    'D'
#define EF_TRIG_HAPPY  'H'
#define EF_BOSSMAN     'B'
#define EF_LOUD        'L'
#define EF_NO_RAISE    'R'
#define EF_NO_GRUDGE   'G'
#define EF_NO_ITEM_BK  'I'


#define CAST_MAX  20


extern mobjinfo_t mobjinfo[NUMMOBJTYPES_COMPAT];

extern mobjinfo_t brain_explode_mobj;

std::vector<mobjinfo_t *> new_mobjinfo;


namespace Things
{
	mobjinfo_t * NewMobj(int mt_num);
	const mobjinfo_t * OldMobj(int mt_num);
	const mobjinfo_t * NewMobjElseOld(int mt_num);
}


//----------------------------------------------------------------------------
//
//  ATTACKS
//
//----------------------------------------------------------------------------

#define KF_FACE_TARG  'F'
#define KF_SIGHT      'S'
#define KF_KILL_FAIL  'K'
#define KF_NO_TRACE   't'
#define KF_TOO_CLOSE  'c'
#define KF_KEEP_FIRE  'e'
#define KF_PUFF_SMK   'p'


namespace Attacks
{
	bool got_one;
	bool flag_got_one;

	class scratch_atk_c
	{
	public:
		int damage;
		std::string sfx;
		std::string fullname;

		scratch_atk_c(int _damage, const char *_sfx, const char *_name) :
			damage(_damage), sfx(_sfx), fullname(_name)
		{ }

		~scratch_atk_c()
		{ }
	};

	std::vector<scratch_atk_c *> scratchers;

	const char * AddScratchAttack(int damage, const char *sfx);

	void BeginLump(void)
	{
		WAD::NewLump(DDF_Attack);

		WAD::Printf("<ATTACKS>\n\n");
	}

	void FinishLump(void)
	{
		WAD::Printf("\n");
	}

	typedef struct
	{
		int mt_num;
		const char *atk_type;
		int atk_height;
		int translucency;
		const char *flags;
	}
	attackextra_t;

	const attackextra_t attack_extra[] =
	{
		{ MT_FIRE,       "TRACKER", 0, 75, "FS" },
		{ MT_TRACER,     "PROJECTILE", 48, 75, "cptF" },
		{ MT_FATSHOT,    "FIXED_SPREADER", 32, 75, "" },
		{ MT_TROOPSHOT,  "PROJECTILE", 32, 75, "F" },
		{ MT_BRUISERSHOT,"PROJECTILE", 32, 75, "F" },
		{ MT_HEADSHOT,   "PROJECTILE", 32, 75, "F" },
		{ MT_ARACHPLAZ,  "PROJECTILE", 16, 50, "eF" },
		{ MT_ROCKET,     "PROJECTILE", 44, 75, "FK" },
		{ MT_PLASMA,     "PROJECTILE", 32, 75, "eK" },
		{ MT_BFG,        "PROJECTILE", 32, 50, "K" },
		{ MT_EXTRABFG,   "SPRAY", 0, 75, "" },
		{ MT_SPAWNSHOT,  "SHOOTTOSPOT", 16, 100, "" },

		{ -1, NULL, 0, 0, "" }
	};


	void HandleSounds(const mobjinfo_t *info, int mt_num)
	{
		if (info->seesound != sfx_None)
			WAD::Printf("LAUNCH_SOUND = \"%s\";\n", Sounds::GetSound(info->seesound));

		if (info->deathsound != sfx_None)
			WAD::Printf("DEATH_SOUND = \"%s\";\n", Sounds::GetSound(info->deathsound));

		if (info->rip_sound != sfx_None)
			WAD::Printf("RIP_SOUND = \"%s\";\n", Sounds::GetSound(info->rip_sound));

		if (mt_num == MT_FIRE)
		{
			WAD::Printf("ATTEMPT_SOUND = \"%s\";\n", Sounds::GetSound(sfx_vilatk));
			WAD::Printf("ENGAGED_SOUND = \"%s\";\n", Sounds::GetSound(sfx_barexp));
		}

		if (mt_num == MT_FATSHOT)
			WAD::Printf("ATTEMPT_SOUND = \"%s\";\n", Sounds::GetSound(sfx_manatk));
	}


	void HandleFrames(const mobjinfo_t *info, int mt_num)
	{
		Frames::ResetGroups();

		// special cases...

		if (mt_num == MT_SPAWNSHOT)
		{
			// EDGE merges MT_SPAWNSHOT and MT_SPAWNFIRE into a single
			// attack ("BRAIN_CUBE").

			int count = 0;

			const mobjinfo_t * spawnfire = Things::NewMobjElseOld(MT_SPAWNFIRE);
			assert(spawnfire);

			count += Frames::BeginGroup('D', spawnfire->spawnstate);
			count += Frames::BeginGroup('S', info->spawnstate);

			if (count != 2)
				PrintWarn("Brain cube is missing spawn/fire states.\n");

			if (count == 0)
				return;

			Frames::SpreadGroups();

			Frames::OutputGroup('S');
			Frames::OutputGroup('D');

			return;
		}

		// --- collect states into groups ---

		int count = 0;

		count += Frames::BeginGroup('D', info->deathstate);
		count += Frames::BeginGroup('E', info->seestate);
		count += Frames::BeginGroup('S', info->spawnstate);

		if (count == 0)
		{
			PrintWarn("Attack [%s] has no states.\n", Things::GetMobjName(mt_num) + 1);
			return;
		}

		Frames::SpreadGroups();

		Frames::OutputGroup('S');
		Frames::OutputGroup('E');
		Frames::OutputGroup('D');
	}


	void AddAtkSpecial(const char *name)
	{
		if (! flag_got_one)
		{
			flag_got_one = true;
			WAD::Printf("ATTACK_SPECIAL = ");
		}
		else
			WAD::Printf(",");

		WAD::Printf("%s", name);
	}


	void HandleAtkSpecials(const mobjinfo_t *info, int mt_num,
		const attackextra_t *ext, bool plr_rocket)
	{
		flag_got_one = false;

		if (strchr(ext->flags, KF_FACE_TARG) && ! plr_rocket)
			AddAtkSpecial("FACE_TARGET");

		if (strchr(ext->flags, KF_SIGHT))
			AddAtkSpecial("NEED_SIGHT");

		if (strchr(ext->flags, KF_KILL_FAIL))
			AddAtkSpecial("KILL_FAILED_SPAWN");

		if (strchr(ext->flags, KF_PUFF_SMK))
			AddAtkSpecial("SMOKING_TRACER");

		if (flag_got_one)
			WAD::Printf(";\n");
	}


	void CheckPainElemental(void)
	{
		// two attacks which refer to the LOST_SOUL's missile states
		// (ELEMENTAL_SPAWNER and ELEMENTAL_DEATHSPAWN).  check if
		// those states are still valid, recreate attacks if not.

		const mobjinfo_t *skull = Things::NewMobjElseOld(MT_SKULL);
		assert(skull);

		if (Frames::CheckMissileState(skull->missilestate))
			return;

		// need to write out new versions

		if (! got_one)
		{
			got_one = true;
			BeginLump();
		}

		const char *spawn_at = NULL;

		if (skull->seestate != S_NULL)
			spawn_at = "CHASE:1";
		else if (skull->missilestate != S_NULL)
			spawn_at = "MISSILE:1";
		else if (skull->meleestate != S_NULL)
			spawn_at = "MELEE:1";
		else
			spawn_at = "IDLE:1";

		WAD::Printf("[ELEMENTAL_SPAWNER]\n");
		WAD::Printf("ATTACKTYPE = SPAWNER;\n");
		WAD::Printf("ATTACK_HEIGHT = 8;\n");
		WAD::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
		WAD::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
		WAD::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);

		WAD::Printf("SPAWN_LIMIT = 21;\n");

		WAD::Printf("\n");
		WAD::Printf("[ELEMENTAL_DEATHSPAWN]\n");
		WAD::Printf("ATTACKTYPE = TRIPLE_SPAWNER;\n");
		WAD::Printf("ATTACK_HEIGHT = 8;\n");
		WAD::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
		WAD::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
		WAD::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);
	}

	void ConvertAttack(const mobjinfo_t *info, int mt_num, bool plr_rocket);
	void ConvertScratch(const scratch_atk_c *atk);
}


const char * Attacks::AddScratchAttack(int damage, const char *sfx)
{
	const char *safe_sfx = "QUIET";
	if (sfx != NULL)
		safe_sfx = StrSanitize(sfx);

	static char namebuf[256];
	snprintf(namebuf, sizeof(namebuf), "SCRATCH_%s_%d", safe_sfx, damage);

	// already have it?
	for (size_t i = 0 ; i < scratchers.size() ; i++)
	{
		if (strcmp(scratchers[i]->fullname.c_str(), namebuf) == 0)
			return namebuf;
	}

	scratch_atk_c * atk = new scratch_atk_c(damage, sfx ? sfx : "", namebuf);
	scratchers.push_back(atk);

	return namebuf;
}


void Attacks::ConvertScratch(const scratch_atk_c *atk)
{
	if (! got_one)
	{
		got_one = true;
		BeginLump();
	}

	WAD::Printf("[%s]\n", atk->fullname.c_str());

	WAD::Printf("ATTACKTYPE=CLOSECOMBAT;\n");
	WAD::Printf("DAMAGE.VAL=%d;\n", atk->damage);
	WAD::Printf("DAMAGE.MAX=%d;\n", atk->damage);
	WAD::Printf("ATTACKRANGE=80;\n");
	WAD::Printf("ATTACK_SPECIAL=FACE_TARGET;\n");

	if (atk->sfx != "")
	{
		WAD::Printf("ENGAGED_SOUND=%s;\n", atk->sfx.c_str());
	}

	WAD::Printf("\n");
}


void Attacks::ConvertAttack(const mobjinfo_t *info, int mt_num, bool plr_rocket)
{
	if (info->name[0] != '*')  // thing?
		return;

	// MT_SPAWNFIRE is handled specially (in other code)
	if (mt_num == MT_SPAWNFIRE)
		return;

	if (! got_one)
	{
		got_one = true;
		BeginLump();
	}

	if (plr_rocket)
		WAD::Printf("[%s]\n", "PLAYER_MISSILE");
	else
		WAD::Printf("[%s]\n", Things::GetMobjName(mt_num) + 1);

	// find attack in the extra table...
	const attackextra_t *ext = NULL;

	for (int j = 0; attack_extra[j].atk_type; j++)
		if (attack_extra[j].mt_num == mt_num)
		{
			ext = attack_extra + j;
			break;
		}

	if (! ext)
		InternalError("Missing attack %s in extra table.\n", Things::GetMobjName(mt_num) + 1);

	WAD::Printf("ATTACKTYPE = %s;\n", ext->atk_type);

	WAD::Printf("RADIUS = %1.1f;\n", F_FIXED(info->radius));
	WAD::Printf("HEIGHT = %1.1f;\n", F_FIXED(info->height));

	if (info->spawnhealth != 1000)
		WAD::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

	if (info->speed != 0)
		WAD::Printf("SPEED = %s;\n", Things::GetSpeed(info->speed));

	if (info->mass != 100)
		WAD::Printf("MASS = %d;\n", info->mass);

	if (mt_num == MT_BRUISERSHOT)
		WAD::Printf("FAST = 1.4;\n");
	else if (mt_num == MT_TROOPSHOT || mt_num == MT_HEADSHOT)
		WAD::Printf("FAST = 2.0;\n");

	if (plr_rocket)
		WAD::Printf("ATTACK_HEIGHT = 32;\n");
	else if (ext->atk_height != 0)
		WAD::Printf("ATTACK_HEIGHT = %d;\n", ext->atk_height);

	if (mt_num == MT_FIRE)
	{
		WAD::Printf("DAMAGE.VAL = 20;\n");
		WAD::Printf("EXPLODE_DAMAGE.VAL = 70;\n");
	}
	else if (mt_num == MT_EXTRABFG)
	{
		WAD::Printf("DAMAGE.VAL   = 65;\n");
		WAD::Printf("DAMAGE.ERROR = 50;\n");
	}
	else if (info->damage > 0)
	{
		WAD::Printf("DAMAGE.VAL = %d;\n", info->damage);
		WAD::Printf("DAMAGE.MAX = %d;\n", info->damage * 8);
	}

	if (mt_num == MT_BFG)
		WAD::Printf("SPARE_ATTACK = BFG9000_SPRAY;\n");

	if (ext->translucency != 100)
		WAD::Printf("TRANSLUCENCY = %d%%;\n", ext->translucency);

	if (strchr(ext->flags, KF_PUFF_SMK))
		WAD::Printf("PUFF = SMOKE;\n");

	if (strchr(ext->flags, KF_TOO_CLOSE))
		WAD::Printf("TOO_CLOSE_RANGE = 196;\n");

	if (strchr(ext->flags, KF_NO_TRACE))
	{
		WAD::Printf("NO_TRACE_CHANCE = 50%%;\n");

		WAD::Printf("TRACE_ANGLE = 9;\n");
	}

	if (strchr(ext->flags, KF_KEEP_FIRE))
		WAD::Printf("KEEP_FIRING_CHANCE = 4%%;\n");

	HandleAtkSpecials(info, mt_num, ext, plr_rocket);
	HandleSounds(info, mt_num);
	HandleFrames(info, mt_num);

	WAD::Printf("\n");

	Things::HandleFlags(info, mt_num, 0);
	Things::HandleMBF21Flags(info, mt_num, 0);

	if (Frames::attack_slot[0] || Frames::attack_slot[1] ||
	    Frames::attack_slot[2])
	{
		PrintWarn("Attack [%s] contained an attacking action.\n", Things::GetMobjName(mt_num) + 1);
		Things::HandleAttacks(info, mt_num);
	}

	if (Frames::act_flags & AF_EXPLODE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = 128;\n");

	WAD::Printf("\n");
}


//----------------------------------------------------------------------------
//
//  THINGS
//
//----------------------------------------------------------------------------

const int height_fixes[] =
{
	MT_MISC14, 60, MT_MISC29, 78, MT_MISC30, 58, MT_MISC31, 46,
	MT_MISC33, 38, MT_MISC34, 50, MT_MISC38, 56, MT_MISC39, 48,
	MT_MISC41, 96, MT_MISC42, 96, MT_MISC43, 96, MT_MISC44, 72,
	MT_MISC45, 72, MT_MISC46, 72, MT_MISC70, 64, MT_MISC72, 52,
	MT_MISC73, 40, MT_MISC74, 64, MT_MISC75, 64, MT_MISC76, 120,

	MT_MISC36, 56, MT_MISC37, 56, MT_MISC47, 56, MT_MISC48, 128,
	MT_MISC35, 56, MT_MISC40, 56, MT_MISC50, 56, MT_MISC77, 42,

	-1, -1  /* the end */
};


namespace Things
{
	int cast_mobjs[CAST_MAX];

	void BeginLump();
	void FinishLump();

	bool CheckIsMonster(const mobjinfo_t *info, int mt_num, int player, bool use_act_flags);
}


void Things::Init()
{
	new_mobjinfo.clear();
	Attacks::scratchers.clear();
}


void Things::Shutdown()
{
	for (size_t i = 0 ; i < new_mobjinfo.size() ; i++)
		if (new_mobjinfo[i] != NULL)
			delete new_mobjinfo[i];

	new_mobjinfo.clear();

	for (size_t k = 0 ; k < Attacks::scratchers.size() ; k++)
		delete Attacks::scratchers[k];

	Attacks::scratchers.clear();
}


void Things::BeginLump()
{
	WAD::NewLump(DDF_Thing);

	WAD::Printf("<THINGS>\n\n");
}

void Things::FinishLump(void)
{
	WAD::Printf("\n");
}


void Things::MarkThing(int mt_num)
{
	assert(mt_num >= 0);

	// handle merged things/attacks
	if (mt_num == MT_TFOG)
		MarkThing(MT_TELEPORTMAN);

	if (mt_num == MT_SPAWNFIRE)
		MarkThing(MT_SPAWNSHOT);

	// fill any missing slots with NULLs, including the one we want
	while ((int)new_mobjinfo.size() < mt_num+1)
	{
		new_mobjinfo.push_back(NULL);
	}

	// already have a modified entry?
	if (new_mobjinfo[mt_num] != NULL)
		return;

	// create new entry, copy original info if we have it
	mobjinfo_t * entry = new mobjinfo_t;
	new_mobjinfo[mt_num] = entry;

	if (mt_num < NUMMOBJTYPES_COMPAT)
	{
		memcpy(entry, &mobjinfo[mt_num], sizeof(mobjinfo_t));
	}
	else
	{
		memset(entry, 0, sizeof(mobjinfo_t));

		entry->name = "X";  // only needed to differentiate from an attack
		entry->doomednum = -1;

		// DEHEXTRA things have a default doomednum
		if (MT_EXTRA00 <= mt_num && mt_num <= MT_EXTRA99)
		{
			entry->doomednum = mt_num;
		}
	}
}


void Things::UseThing(int mt_num)
{
	// only create something when our standard DDF lacks it
	if (mt_num >= MT_DOGS)
		MarkThing(mt_num);
}


void Things::MarkAllMonsters()
{
	for (int i = 0; i < NUMMOBJTYPES_COMPAT; i++)
	{
		if (i == MT_PLAYER)
			continue;

		const mobjinfo_t *mobj = &mobjinfo[i];

		if (CheckIsMonster(mobj, i, 0, false))
			MarkThing(i);
	}
}


mobjinfo_t * Things::GetModifiedMobj(int mt_num)
{
	MarkThing(mt_num);

	return &mobjinfo[mt_num];
}


const char * Things::GetMobjName(int mt_num)
{
	assert(mt_num >= 0);

	if (mt_num < NUMMOBJTYPES_COMPAT)
		return mobjinfo[mt_num].name;

	static char buffer[64];

	if (MT_EXTRA00 <= mt_num && mt_num <= MT_EXTRA99)
		snprintf(buffer, sizeof(buffer), "MT_EXTRA%02d", mt_num - MT_EXTRA00);
	else
		snprintf(buffer, sizeof(buffer), "DEHACKED_%d", mt_num + 1);

	return buffer;
}


void Things::SetPlayerHealth(int new_value)
{
	MarkThing(MT_PLAYER);

	new_mobjinfo[MT_PLAYER]->spawnhealth = new_value;
}


const mobjinfo_t * Things::OldMobj(int mt_num)
{
	if (mt_num < NUMMOBJTYPES_COMPAT)
		return &mobjinfo[mt_num];

	return NULL;
}


mobjinfo_t * Things::NewMobj(int mt_num)
{
	if (mt_num < (int)new_mobjinfo.size())
		return new_mobjinfo[mt_num];

	return NULL;
}


const mobjinfo_t * Things::NewMobjElseOld(int mt_num)
{
	const mobjinfo_t * info = NewMobj(mt_num);
	if (info != NULL)
		return info;

	return OldMobj(mt_num);
}


int Things::GetMobjMBF21Flags(int mt_num)
{
	const mobjinfo_t *info = NewMobjElseOld(mt_num);
	if (info == NULL)
		return 0;
	return info->mbf21_flags;
}


bool Things::IsSpawnable(int mt_num)
{
	// attacks are not spawnable via A_Spawn
	if (mt_num < NUMMOBJTYPES_COMPAT && mobjinfo[mt_num].name[0] == '*')
		return false;

	const mobjinfo_t *info = NewMobjElseOld(mt_num);
	if (info == NULL)
		return false;

	return info->doomednum > 0;
}


const char * Things::AddScratchAttack(int damage, const char *sfx)
{
	return Attacks::AddScratchAttack(damage, sfx);
}


namespace Things
{
	typedef struct
	{
		long long int flag;          // flag in mobjinfo_t (MF_XXX), 0 if ignored
		const char *bex;   // name in a DEHACKED or BEX file
		const char *conv;  // edge name, NULL if none, can be multiple
	}
	flagname_t;

	const flagname_t flag_list[] =
	{
		{ MF_SPECIAL,      "SPECIAL",       "SPECIAL" },
		{ MF_SOLID,        "SOLID",         "SOLID" },
		{ MF_SHOOTABLE,    "SHOOTABLE",     "SHOOTABLE" },
		{ MF_NOSECTOR,     "NOSECTOR",      "NOSECTOR" },
		{ MF_NOBLOCKMAP,   "NOBLOCKMAP",    "NOBLOCKMAP" },
		{ MF_AMBUSH,       "AMBUSH",        "AMBUSH" },
		{ 0,               "JUSTHIT",       NULL },
		{ 0,               "JUSTATTACKED",  NULL },
		{ MF_SPAWNCEILING, "SPAWNCEILING",  "SPAWNCEILING" },
		{ MF_NOGRAVITY,    "NOGRAVITY",     "NOGRAVITY" },
		{ MF_DROPOFF,      "DROPOFF",       "DROPOFF" },
		{ MF_PICKUP,       "PICKUP",        "PICKUP" },
		{ MF_NOCLIP,       "NOCLIP",        "NOCLIP" },
		{ MF_SLIDE,        "SLIDE",         "SLIDER" },
		{ MF_FLOAT,        "FLOAT",         "FLOAT" },
		{ MF_TELEPORT,     "TELEPORT",      "TELEPORT" },
		{ MF_MISSILE,      "MISSILE",       "MISSILE" },
		{ MF_DROPPED,      "DROPPED",       "DROPPED" },
		{ MF_SHADOW,       "SHADOW",        "FUZZY"  },
		{ MF_NOBLOOD,      "NOBLOOD",       "DAMAGESMOKE" },
		{ MF_CORPSE,       "CORPSE",        "CORPSE" },
		{ 0,               "INFLOAT",       NULL },
		{ MF_COUNTKILL,    "COUNTKILL",     "COUNT_AS_KILL" },
		{ MF_COUNTITEM,    "COUNTITEM",     "COUNT_AS_ITEM" },
		{ MF_SKULLFLY,     "SKULLFLY",      "SKULLFLY" },
		{ MF_NOTDMATCH,    "NOTDMATCH",     "NODEATHMATCH" },
		{ MF_TRANSLATION1, "TRANSLATION1",  NULL },
		{ MF_TRANSLATION2, "TRANSLATION2",  NULL },
		{ MF_TRANSLATION1, "TRANSLATION",   NULL },  // bug compat
		{ MF_TOUCHY,       "TOUCHY",        "TOUCHY" },
		{ MF_BOUNCES,      "BOUNCES",       "BOUNCE" },
		{ MF_FRIEND,  	   "FRIEND",   		NULL },
		{ MF_TRANSLUCENT,  "TRANSLUCENT",   NULL },
		{ MF_TRANSLUCENT,  "TRANSLUC50",    NULL },
		// BOOM and MBF flags...
		//{ MF_STEALTH,      "STEALTH",       "STEALTH" },

		{ MF_UNUSED1,       "UNUSED1",       NULL },
		{ MF_UNUSED2,       "UNUSED2",       NULL },
		{ MF_UNUSED3,       "UNUSED3",       NULL },
		{ MF_UNUSED4,       "UNUSED4",       NULL },

		{ 0, NULL, NULL }  // End sentinel
	};

	const flagname_t mbf21flag_list[] =
	{
		{ MBF21_LOGRAV,          "LOGRAV",         "LOGRAV" },
		{ MBF21_DMGIGNORED,      "DMGIGNORED",     "NEVERTARGETED"  },
		{ MBF21_NORADIUSDMG,     "NORADIUSDMG",    "EXPLODE_IMMUNE" },
		{ MBF21_HIGHERMPROB,     "HIGHERMPROB",    "TRIGGER_HAPPY"  },  // FIXME: not quite the same
		{ MBF21_RANGEHALF,       "RANGEHALF",      "TRIGGER_HAPPY"  },
		{ MBF21_NOTHRESHOLD,     "NOTHRESHOLD",    "NOGRUDGE"       },
		{ MBF21_BOSS,            "BOSS",           "BOSSMAN"        },
		{ MBF21_RIP,             "RIP",            "TUNNEL"         },
		{ MBF21_FULLVOLSOUNDS,   "FULLVOLSOUNDS",  "ALWAYS_LOUD"    },

		// flags which don't produce an Edge special
		{ MBF21_SHORTMRANGE,     "SHORTMRANGE",    NULL },
		{ MBF21_LONGMELEE,       "LONGMELEE",      NULL },
		{ MBF21_FORCERADIUSDMG,  "FORCERADIUSDMG", NULL },

		{ MBF21_MAP07BOSS1,      "MAP07BOSS1",     NULL },
		{ MBF21_MAP07BOSS2,      "MAP07BOSS2",     NULL },
		{ MBF21_E1M8BOSS,        "E1M8BOSS",       NULL },
		{ MBF21_E2M8BOSS,        "E2M8BOSS",       NULL },
		{ MBF21_E3M8BOSS,        "E3M8BOSS",       NULL },
		{ MBF21_E4M6BOSS,        "E4M6BOSS",       NULL },
		{ MBF21_E4M8BOSS,        "E4M8BOSS",       NULL },

		{ 0, NULL, NULL }  // End sentinel
	};

	// these are extra flags we add for certain monsters.
	// they do not correspond to anything in DEHACKED / BEX / MBF21.
	const flagname_t extflaglist[] =
	{
		{ EF_DISLOYAL,    NULL,  "DISLOYAL,ATTACK_HURTS" },  // must be first
		{ EF_TRIG_HAPPY,  NULL,  "TRIGGER_HAPPY" },
		{ EF_BOSSMAN,     NULL,  "BOSSMAN" },
		{ EF_LOUD,        NULL,  "ALWAYS_LOUD" },
		{ EF_NO_RAISE,    NULL,  "NO_RESURRECT" },
		{ EF_NO_GRUDGE,   NULL,  "NO_GRUDGE,NEVERTARGETED" },
		{ EF_NO_ITEM_BK,  NULL,  "NO_RESPAWN" },

		{ 0, NULL, NULL }  // End sentinel
	};


	int ParseBits(const flagname_t *list, char *bit_str)
	{
		int new_flags = 0;

		// these delimiters are the same as what Boom/MBF uses
		static const char *delims = "+|, \t\f\r";

		for (char *token = strtok(bit_str, delims);
			 token != NULL;
			 token = strtok(NULL, delims))
		{
			// tokens should be non-empty
			assert(token[0] != 0);

			if (isdigit(token[0]))
			{
				int flags;

				if (sscanf(token, " %i ", &flags) == 1)
					new_flags |= flags;
				else
					PrintWarn("Line %d: unreadable BITS value: %s\n", Patch::line_num, token);

				continue;
			}

			// find the name in the given list
			int i;
			for (i = 0 ; list[i].bex != NULL ; i++)
				if (StrCaseCmp(token, list[i].bex) == 0)
					break;

			if (list[i].bex == NULL)
			{
				PrintWarn("Line %d: unknown BITS mnemonic: %s\n", Patch::line_num, token);
				continue;
			}

			new_flags |= list[i].flag;
		}

		return new_flags;
	}

	bool CheckIsMonster(const mobjinfo_t *info, int mt_num, int player,
		bool use_act_flags)
	{
		if (player > 0)
			return false;

		if (info->doomednum <= 0)
			return false;

		if (info->name[0] == '*')
			return false;

		if (info->flags & MF_COUNTKILL)
			return true;

		if (info->flags & (MF_SPECIAL | MF_COUNTITEM))
			return false;

		int score = 0;

		// values determined by statistical analysis of major DEH patches
		// (Standard DOOM, Batman, Mordeth, Wheel-of-Time, Osiris).

		if (info->flags & MF_SOLID) score += 25;
		if (info->flags & MF_SHOOTABLE) score += 72;

		if (info->painstate) score += 91;
		if (info->missilestate || info->meleestate) score += 91;
		if (info->deathstate) score += 72;
		if (info->raisestate) score += 31;

		if (use_act_flags)
		{
			if (Frames::act_flags & AF_CHASER) score += 78;
			if (Frames::act_flags & AF_FALLER) score += 61;
		}

		if (info->speed > 0) score += 87;

#if (DEBUG_MONST)
		Debug_PrintMsg("[%20.20s:%-4d] %c%c%c%c%c %c%c%c%c %c%c %d = %d\n",
			GetMobjName(mt_num), info->doomednum,
			(info->flags & MF_SOLID) ? 'S' : '-',
			(info->flags & MF_SHOOTABLE) ? 'H' : '-',
			(info->flags & MF_FLOAT) ? 'F' : '-',
			(info->flags & MF_MISSILE) ? 'M' : '-',
			(info->flags & MF_NOBLOOD) ? 'B' : '-',
			(info->painstate) ? 'p' : '-',
			(info->deathstate) ? 'd' : '-',
			(info->raisestate) ? 'r' : '-',
			(info->missilestate || info->meleestate) ? 'm' : '-',
			(Frames::act_flags & AF_CHASER) ? 'C' : '-',
			(Frames::act_flags & AF_FALLER) ? 'F' : '-',
			info->speed, score);
#endif

		return score >= (use_act_flags ? 370 : 300);
	}

	const char *GetExtFlags(int mt_num, int player)
	{
		if (player > 0)
			return "D";

		switch (mt_num)
		{
			case MT_INS:
			case MT_INV: return "I";

			case MT_POSSESSED:
			case MT_SHOTGUY:
			case MT_CHAINGUY: return "D";

			case MT_SKULL: return "DHM";
			case MT_UNDEAD: return "H";

			case MT_VILE: return "GR";
			case MT_CYBORG: return "BHR";
			case MT_SPIDER: return "BHR";

			case MT_BOSSSPIT: return "B";
			case MT_BOSSBRAIN: return "L";

			default:
				break;
		}

		return "";
	}

	void AddOneFlag(const mobjinfo_t *info, const char *name, bool& got_a_flag)
	{
		if (! got_a_flag)
		{
			got_a_flag = true;

			if (info->name[0] == '*')
				WAD::Printf("PROJECTILE_SPECIAL = ");
			else
				WAD::Printf("SPECIAL = ");
		}
		else
			WAD::Printf(",");

		WAD::Printf("%s", name);
	}

	void HandleFlags(const mobjinfo_t *info, int mt_num, int player)
	{
		int i;
		int cur_f = info->flags;
		bool got_a_flag = false;

		// strangely absent from MT_PLAYER
		if (player)
			cur_f |= MF_SLIDE;

		// this can cause EDGE 1.27 to crash
		if (! player)
			cur_f &= ~MF_PICKUP;

		// EDGE requires teleportman in sector. (DOOM uses thinker list)
		if (mt_num == MT_TELEPORTMAN)
			cur_f &= ~MF_NOSECTOR;

		// special workaround for negative MASS values
		if (info->mass < 0)
			cur_f |= MF_SPAWNCEILING | MF_NOGRAVITY;

		bool is_monster = CheckIsMonster(info, mt_num, player, true);
		bool force_disloyal = (is_monster && Misc::monster_infight == 221);

		for (i = 0 ; flag_list[i].bex != NULL ; i++)
		{
			if (0 == (cur_f & flag_list[i].flag))
				continue;

			if (flag_list[i].conv != NULL)
				AddOneFlag(info, flag_list[i].conv, got_a_flag);
		}

		const char *eflags = GetExtFlags(mt_num, player);

		for (i = 0 ; extflaglist[i].bex != NULL; i++)
		{
			char ch = (char) extflaglist[i].flag;

			if (! strchr(eflags, ch))
				continue;

			if (ch == EF_DISLOYAL)
			{
				force_disloyal = true;
				continue;
			}

			AddOneFlag(info, extflaglist[i].conv, got_a_flag);
		}

		if (force_disloyal)
			AddOneFlag(info, extflaglist[0].conv, got_a_flag);

		if (is_monster)
			AddOneFlag(info, "MONSTER", got_a_flag);

		AddOneFlag(info, "DEHACKED_COMPAT", got_a_flag);

		if (got_a_flag)
			WAD::Printf(";\n");

		if (cur_f & MF_TRANSLATION)
		{
			if ((cur_f & MF_TRANSLATION) == 0x4000000)
				WAD::Printf("PALETTE_REMAP = PLAYER_DK_GREY;\n");
			else if ((cur_f & MF_TRANSLATION) == 0x8000000)
				WAD::Printf("PALETTE_REMAP = PLAYER_BROWN;\n");
			else
				WAD::Printf("PALETTE_REMAP = PLAYER_DULL_RED;\n");
		}

		if (cur_f & MF_TRANSLUCENT)
		{
			WAD::Printf("TRANSLUCENCY = 50%%;\n");
		}

		if ((cur_f & MF_FRIEND) && ! player)
		{
			WAD::Printf("SIDE = 16777215;\n");
		}
	}

	void HandleMBF21Flags(const mobjinfo_t *info, int mt_num, int player)
	{
		int i;
		int cur_f = info->mbf21_flags;
		bool got_a_flag = false;

		for (i = 0; mbf21flag_list[i].bex != NULL; i++)
		{
			if (0 == (cur_f & mbf21flag_list[i].flag))
				continue;

			if (mbf21flag_list[i].conv != NULL)
				AddOneFlag(info, mbf21flag_list[i].conv, got_a_flag);
		}

		AddOneFlag(info, "MBF21_COMPAT", got_a_flag);

		if (got_a_flag)
			WAD::Printf(";\n");
	}

	void FixHeights()
	{
		for (int i = 0 ; height_fixes[i] >= 0 ; i += 2)
		{
			int mt_num = height_fixes[i];
			int new_h  = height_fixes[i + 1];

			assert(mt_num < NUMMOBJTYPES_COMPAT);

			// if the thing was not modified, nothing to do here
			if (mt_num >= (int)new_mobjinfo.size())
				continue;

			mobjinfo_t *info = new_mobjinfo[mt_num];
			if (info == NULL)
				continue;

			/* Kludge for Aliens TC (and others) that put these things on
			 * the ceiling -- they need the 16 height for correct display,
			 */
			if (info->flags & MF_SPAWNCEILING)
				continue;

			if (info->height != 16*FRACUNIT)
				continue;

			info->height = new_h * FRACUNIT;
		}
	}


	void CollectTheCast()
	{
		for (int i = 0 ; i < CAST_MAX ; i++)
			cast_mobjs[i] = -1;

		for (int mt_num = 0 ; mt_num < NUMMOBJTYPES_COMPAT ; mt_num++)
		{
			int order = 0;

			// cast objects are required to have CHASE and DEATH states
			const mobjinfo_t *info = NewMobjElseOld(mt_num);

			if (info->seestate == S_NULL || info->deathstate == S_NULL)
				continue;

			switch (mt_num)
			{
				case MT_PLAYER:    order =  1; break;
				case MT_POSSESSED: order =  2; break;
				case MT_SHOTGUY:   order =  3; break;
				case MT_CHAINGUY:  order =  4; break;
				case MT_TROOP:     order =  5; break;
				case MT_SERGEANT:  order =  6; break;
				case MT_SKULL:     order =  7; break;
				case MT_HEAD:      order =  8; break;
				case MT_KNIGHT:    order =  9; break;
				case MT_BRUISER:   order = 10; break;
				case MT_BABY:      order = 11; break;
				case MT_PAIN:      order = 12; break;
				case MT_UNDEAD:    order = 13; break;
				case MT_FATSO:     order = 14; break;
				case MT_VILE:      order = 15; break;
				case MT_SPIDER:    order = 16; break;
				case MT_CYBORG:    order = 17; break;

				default: continue;
			}

			cast_mobjs[order] = mt_num;
		}
	}


	const char *GetSpeed(int speed)
	{
		// Interestingly, speed is fixed point for attacks, but
		// plain int for things.  Here we automatically handle both.

		static char num_buf[128];

		if (speed >= 1024)
			sprintf(num_buf, "%1.2f", F_FIXED(speed));
		else
			sprintf(num_buf, "%d", speed);

		return num_buf;
	}


	void HandleSounds(const mobjinfo_t *info, int mt_num)
	{
		if (info->activesound != sfx_None)
		{
			if (info->flags & MF_PICKUP)
				WAD::Printf("PICKUP_SOUND = \"%s\";\n", Sounds::GetSound(info->activesound));
			else
				WAD::Printf("ACTIVE_SOUND = \"%s\";\n", Sounds::GetSound(info->activesound));
		}
		else if (mt_num == MT_TELEPORTMAN)
			WAD::Printf("ACTIVE_SOUND = \"%s\";\n", Sounds::GetSound(sfx_telept));

		if (info->seesound != sfx_None)
			WAD::Printf("SIGHTING_SOUND = \"%s\";\n", Sounds::GetSound(info->seesound));
		else if (mt_num == MT_BOSSSPIT)
			WAD::Printf("SIGHTING_SOUND = \"%s\";\n", Sounds::GetSound(sfx_bossit));

		if (info->attacksound != sfx_None && info->meleestate != S_NULL)
		{
			WAD::Printf("STARTCOMBAT_SOUND = \"%s\";\n", Sounds::GetSound(info->attacksound));
		}

		if (info->painsound != sfx_None)
			WAD::Printf("PAIN_SOUND = \"%s\";\n", Sounds::GetSound(info->painsound));

		if (info->deathsound != sfx_None)
			WAD::Printf("DEATH_SOUND = \"%s\";\n", Sounds::GetSound(info->deathsound));

		if (info->rip_sound != sfx_None)
			WAD::Printf("RIP_SOUND = \"%s\";\n", Sounds::GetSound(info->rip_sound));
	}


	void HandleFrames(const mobjinfo_t *info, int mt_num)
	{
		Frames::ResetGroups();

		// special cases...

		if (mt_num == MT_TELEPORTMAN)
		{
			WAD::Printf("TRANSLUCENCY = 50%%;\n");
			WAD::Printf("\n");
			WAD::Printf("STATES(IDLE) = %s:A:-1:NORMAL:TRANS_SET(0%%);\n",
				Sprites::GetSprite(SPR_TFOG));

			// EDGE doesn't use the TELEPORT_FOG object, instead it uses
			// the CHASE states of the TELEPORT_FLASH object (i.e. the one
			// used to find the destination in the target sector).

			const mobjinfo_t *tfog = NewMobjElseOld(MT_TFOG);
			assert(tfog);

			if (0 == Frames::BeginGroup('E', tfog->spawnstate))
			{
				PrintWarn("Teleport fog has no spawn states.\n");
				return;
			}

			Frames::SpreadGroups();
			Frames::OutputGroup('E');

			return;
		}

		// --- collect states into groups ---

		int count = 0;

		// do more important states AFTER less important ones
		count += Frames::BeginGroup('R', info->raisestate);
		count += Frames::BeginGroup('X', info->xdeathstate);
		count += Frames::BeginGroup('D', info->deathstate);
		count += Frames::BeginGroup('P', info->painstate);
		count += Frames::BeginGroup('M', info->missilestate);
		count += Frames::BeginGroup('L', info->meleestate);
		count += Frames::BeginGroup('E', info->seestate);
		count += Frames::BeginGroup('S', info->spawnstate);

		if (count == 0)
		{
			// only occurs with special/invisible objects, currently only
			// with teleport target (handled above) and brain spit targets.

			if (mt_num != MT_BOSSTARGET)
				PrintWarn("Mobj [%s:%d] has no states.\n", GetMobjName(mt_num), info->doomednum);

			WAD::Printf("TRANSLUCENCY = 0%%;\n");

			WAD::Printf("\n");
			WAD::Printf("STATES(IDLE) = %s:A:-1:NORMAL:NOTHING;\n",
				Sprites::GetSprite(SPR_CAND));

			return;
		}

		Frames::SpreadGroups();

		Frames::OutputGroup('S');
		Frames::OutputGroup('E');
		Frames::OutputGroup('L');
		Frames::OutputGroup('M');
		Frames::OutputGroup('P');
		Frames::OutputGroup('D');
		Frames::OutputGroup('X');
		Frames::OutputGroup('R');

		// the A_VileChase action is another special case
		if (Frames::act_flags & AF_RAISER)
		{
			if (Frames::BeginGroup('H', S_VILE_HEAL1) > 0)
			{
				Frames::SpreadGroups();
				Frames::OutputGroup('H');
			}
		}
	}


	const int NUMPLAYERS = 8;

	typedef struct
	{
		const char *name;
		int num;
		const char *remap;
	}
	playerinfo_t;

	const playerinfo_t player_info[NUMPLAYERS] =
	{
		{ "OUR_HERO",    1, "PLAYER_GREEN" },
		{ "PLAYER2",     2, "PLAYER_DK_GREY" },
		{ "PLAYER3",     3, "PLAYER_BROWN" },
		{ "PLAYER4",     4, "PLAYER_DULL_RED" },
		{ "PLAYER5",  4001, "PLAYER_ORANGE" },
		{ "PLAYER6",  4002, "PLAYER_LT_GREY" },
		{ "PLAYER7",  4003, "PLAYER_LT_RED" },
		{ "PLAYER8",  4004, "PLAYER_PINK"  }
	};

	void HandlePlayer(const mobjinfo_t *info, int player)
	{
		if (player <= 0)
			return;

		assert(player <= NUMPLAYERS);

		const playerinfo_t *pi = player_info + (player - 1);

		WAD::Printf("PLAYER = %d;\n", player);
		WAD::Printf("SIDE = %d;\n", 1 << (player - 1));
		WAD::Printf("PALETTE_REMAP = %s;\n", pi->remap);

		WAD::Printf("INITIAL_BENEFIT = \n");
		WAD::Printf("    BULLETS.LIMIT(%d), ", Ammo::plr_max[am_bullet]);
		WAD::Printf(    "SHELLS.LIMIT(%d), ",  Ammo::plr_max[am_shell]);
		WAD::Printf(    "ROCKETS.LIMIT(%d), ", Ammo::plr_max[am_rocket]);
		WAD::Printf(    "CELLS.LIMIT(%d),\n",  Ammo::plr_max[am_cell]);
		WAD::Printf("    PELLETS.LIMIT(%d), ", 200);
		WAD::Printf(    "NAILS.LIMIT(%d), ",   100);
		WAD::Printf(    "GRENADES.LIMIT(%d), ", 50);
		WAD::Printf(    "GAS.LIMIT(%d),\n",    300);

		WAD::Printf("    AMMO9.LIMIT(%d), ",   100);
		WAD::Printf(    "AMMO10.LIMIT(%d), ",  200);
		WAD::Printf(    "AMMO11.LIMIT(%d), ",  50);
		WAD::Printf(    "AMMO12.LIMIT(%d),\n", 300);
		WAD::Printf("    AMMO13.LIMIT(%d), ",  100);
		WAD::Printf(    "AMMO14.LIMIT(%d), ",  200);
		WAD::Printf(    "AMMO15.LIMIT(%d), ",  50);
		WAD::Printf(    "AMMO16.LIMIT(%d),\n", 300);

		WAD::Printf("    BULLETS(%d);\n",      Misc::init_ammo);
	}


	typedef struct
	{
		int spr_num;
		const char *benefit;
		int par_num;
		int amount, limit;
		const char *ldf;
		int sound;
	}
	pickupitem_t;

	const pickupitem_t pickup_item[] =
	{
		// Health & Armor....
		{ SPR_BON1, "HEALTH", 2, 1,200, "GotHealthPotion", sfx_itemup },
		{ SPR_STIM, "HEALTH", 2, 10,100, "GotStim", sfx_itemup },  
		{ SPR_MEDI, "HEALTH", 2, 25,100, "GotMedi", sfx_itemup },  
		{ SPR_BON2, "GREEN_ARMOUR", 2, 1,200, "GotArmourHelmet", sfx_itemup },  
		{ SPR_ARM1, "GREEN_ARMOUR", 2, 100,100, "GotArmour", sfx_itemup },  
		{ SPR_ARM2, "BLUE_ARMOUR", 2, 200,200, "GotMegaArmour", sfx_itemup },  

		// Keys....
		{ SPR_BKEY, "KEY_BLUECARD",   0, 0,0, "GotBlueCard", sfx_itemup },
		{ SPR_YKEY, "KEY_YELLOWCARD", 0, 0,0, "GotYellowCard", sfx_itemup },
		{ SPR_RKEY, "KEY_REDCARD",    0, 0,0, "GotRedCard", sfx_itemup },
		{ SPR_BSKU, "KEY_BLUESKULL",  0, 0,0, "GotBlueSkull", sfx_itemup },
		{ SPR_YSKU, "KEY_YELLOWSKULL",0, 0,0, "GotYellowSkull", sfx_itemup },
		{ SPR_RSKU, "KEY_REDSKULL",   0, 0,0, "GotRedSkull", sfx_itemup },

		// Ammo....
		{ SPR_CLIP, "BULLETS", 1, 10,0, "GotClip", sfx_itemup },
		{ SPR_AMMO, "BULLETS", 1, 50,0, "GotClipBox", sfx_itemup },
		{ SPR_SHEL, "SHELLS",  1, 4,0,  "GotShells", sfx_itemup },
		{ SPR_SBOX, "SHELLS",  1, 20,0, "GotShellBox", sfx_itemup },
		{ SPR_ROCK, "ROCKETS", 1, 1,0,  "GotRocket", sfx_itemup },
		{ SPR_BROK, "ROCKETS", 1, 5,0,  "GotRocketBox", sfx_itemup },
		{ SPR_CELL, "CELLS",   1, 20,0, "GotCell", sfx_itemup },
		{ SPR_CELP, "CELLS",   1, 100,0, "GotCellPack", sfx_itemup },

		// Powerups....
		{ SPR_SOUL, "HEALTH", 2, 100,200, "GotSoul", sfx_getpow },  
		{ SPR_PMAP, "POWERUP_AUTOMAP", 0, 0,0, "GotMap", sfx_getpow },
		{ SPR_PINS, "POWERUP_PARTINVIS", 2, 100,100, "GotInvis", sfx_getpow },  
		{ SPR_PINV, "POWERUP_INVULNERABLE", 2, 30,30, "GotInvulner", sfx_getpow },  
		{ SPR_PVIS, "POWERUP_LIGHTGOGGLES", 2, 120,120, "GotVisor", sfx_getpow },  
		{ SPR_SUIT, "POWERUP_ACIDSUIT", 2, 60,60, "GotSuit", sfx_getpow },  

		// Weapons....
		{ SPR_CSAW, "CHAINSAW", 0, 0,0, "GotChainSaw", sfx_wpnup },
		{ SPR_SHOT, "SHOTGUN,SHELLS", 1, 8,0, "GotShotGun", sfx_wpnup },
		{ SPR_SGN2, "SUPERSHOTGUN,SHELLS", 1, 8,0, "GotDoubleBarrel", sfx_wpnup },
		{ SPR_MGUN, "CHAINGUN,BULLETS", 1, 20,0, "GotChainGun", sfx_wpnup },
		{ SPR_LAUN, "ROCKET_LAUNCHER,ROCKETS", 1, 2,0, "GotRocketLauncher", sfx_wpnup },
		{ SPR_PLAS, "PLASMA_RIFLE,CELLS", 1, 40,0, "GotPlasmaGun", sfx_wpnup },
		{ SPR_BFUG, "BFG9000,CELLS", 1, 40,0, "GotBFG", sfx_wpnup },
		
		{ -1, NULL, 0,0,0, NULL }
	};


	void HandleItem(const mobjinfo_t *info, int mt_num)
	{
		if (! (info->flags & MF_SPECIAL))
			return;

		if (info->spawnstate == S_NULL)
			return;

		int spr_num = Frames::GetStateSprite(info->spawnstate);

		// special cases:

		if (spr_num == SPR_PSTR) // Berserk
		{
			WAD::Printf("PICKUP_BENEFIT = POWERUP_BERSERK(60:60),HEALTH(100:100);\n");
			WAD::Printf("PICKUP_MESSAGE = GotBerserk;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_getpow));
			WAD::Printf("PICKUP_EFFECT = SWITCH_WEAPON(FIST);\n");
			return;
		}
		else if (spr_num == SPR_MEGA)  // Megasphere
		{
			WAD::Printf("PICKUP_BENEFIT = ");
			WAD::Printf("HEALTH(%d:%d),", Misc::mega_health, Misc::mega_health);
			WAD::Printf("BLUE_ARMOUR(%d:%d);\n", Misc::max_armour, Misc::max_armour);
			WAD::Printf("PICKUP_MESSAGE = GotMega;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_getpow));
			return;
		}
		else if (spr_num == SPR_BPAK)  // Backpack full of AMMO
		{
			WAD::Printf("PICKUP_BENEFIT = \n");
			WAD::Printf("    BULLETS.LIMIT(%d), ", 2 * Ammo::plr_max[am_bullet]);
			WAD::Printf("    SHELLS.LIMIT(%d),\n", 2 * Ammo::plr_max[am_shell]);
			WAD::Printf("    ROCKETS.LIMIT(%d), ", 2 * Ammo::plr_max[am_rocket]);
			WAD::Printf("    CELLS.LIMIT(%d),\n",  2 * Ammo::plr_max[am_cell]);
			WAD::Printf("    BULLETS(10), SHELLS(4), ROCKETS(1), CELLS(20);\n");
			WAD::Printf("PICKUP_MESSAGE = GotBackpack;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_itemup));
			return;
		}

		int i;

		for (i = 0; pickup_item[i].benefit != NULL; i++)
		{
			if (spr_num == pickup_item[i].spr_num)
				break;
		}

		const pickupitem_t *pu = pickup_item + i;

		if (pu->benefit == NULL)  // not found
		{
			PrintWarn("Unknown pickup sprite \"%s\" for item [%s]\n",
				Sprites::GetOriginalName(spr_num), GetMobjName(mt_num));
			return;
		}

		int amount = pu->amount;
		int limit  = pu->limit;

		// handle patchable amounts

		switch (spr_num)
		{
			// Armor & health...
			case SPR_BON2:   // "ARMOUR_HELMET"  
				limit  = Misc::max_armour;
				break;

			case SPR_ARM1:   // "GREEN_ARMOUR"
				amount = Misc::green_armour_class * 100;
				limit  = Misc::max_armour;
				break;

			case SPR_ARM2:   // "BLUE_ARMOUR"  
				amount = Misc::blue_armour_class * 100;
				limit  = Misc::max_armour;
				break;

			case SPR_BON1:    // "HEALTH_POTION"
				limit  = Misc::max_health;  // Note: *not* MEDIKIT
				break;

			case SPR_SOUL:   // "SOULSPHERE"  
				amount = Misc::soul_health;
				limit  = Misc::soul_limit;
				break;

			// Ammo...
			case SPR_CLIP:   // "CLIP"
			case SPR_AMMO:   // "BOX_OF_BULLETS"  
				amount = Ammo::pickups[am_bullet];
				break;

			case SPR_SHEL:   // "SHELLS"  
			case SPR_SBOX:   // "BOX_OF_SHELLS"  
				amount = Ammo::pickups[am_shell];
				break;

			case SPR_ROCK:   // "ROCKET"  
			case SPR_BROK:   // "BOX_OF_ROCKETS"  
				amount = Ammo::pickups[am_rocket];
				break;

			case SPR_CELL:   // "CELLS"  
			case SPR_CELP:   // "CELL_PACK"  
				amount = Ammo::pickups[am_cell];
				break;

			default:
				break;
		}

		// big boxes of ammo
		if (spr_num == SPR_AMMO || spr_num == SPR_BROK ||
			spr_num == SPR_CELP || spr_num == SPR_SBOX)
		{
			amount *= 5;
		}

		if (pu->par_num == 2 && amount > limit)
			amount = limit;

		WAD::Printf("PICKUP_BENEFIT = %s", pu->benefit);

		if (pu->par_num == 1)
			WAD::Printf("(%d)", amount);
		else if (pu->par_num == 2)
			WAD::Printf("(%d:%d)", amount, limit);

		WAD::Printf(";\n");
		WAD::Printf("PICKUP_MESSAGE = %s;\n", pu->ldf);

		if (info->activesound == sfx_None)
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(pu->sound));
	}


	const char *cast_titles[17] =
	{
	    "OurHeroName",     "ZombiemanName",
        "ShotgunGuyName",  "HeavyWeaponDudeName",
        "ImpName",         "DemonName",
	    "LostSoulName",    "CacodemonName",
        "HellKnightName",  "BaronOfHellName",
        "ArachnotronName", "PainElementalName",
        "RevenantName",    "MancubusName",
        "ArchVileName",    "SpiderMastermindName",
        "CyberdemonName"
	};


	void HandleCastOrder(const mobjinfo_t *info, int mt_num, int player)
	{
		if (player >= 2)
			return;

		int pos   = 1;
		int order = 1;

		for (; pos < CAST_MAX ; pos++)
		{
			// ignore missing members (ensure real order is contiguous)
			if (cast_mobjs[pos] < 0)
				continue;

			if (cast_mobjs[pos] == mt_num)
				break;

			order++;
		}

		if (pos >= CAST_MAX)  // not found
			return;

		WAD::Printf("CASTORDER = %d;\n", order);
		WAD::Printf("CAST_TITLE = %s;\n", cast_titles[pos - 1]);
	}



	void HandleDropItem(const mobjinfo_t *info, int mt_num);
	void HandleAttacks(const mobjinfo_t *info, int mt_num);
	void ConvertMobj(const mobjinfo_t *info, int mt_num, int player,
		bool brain_missile, bool& got_one);
}


void Things::HandleDropItem(const mobjinfo_t *info, int mt_num)
{
	const char *item = NULL;

	switch (mt_num)
	{
		case MT_WOLFSS:
		case MT_POSSESSED: item = "CLIP"; break;

		case MT_SHOTGUY:   item = "SHOTGUN"; break;
		case MT_CHAINGUY:  item = "CHAINGUN"; break;

		default:
			return;
	}

	assert(item);

	WAD::Printf("DROPITEM = \"%s\";\n", item);
}


void Things::HandleAttacks(const mobjinfo_t *info, int mt_num)
{
	if (Frames::attack_slot[Frames::RANGE])
	{
		WAD::Printf("RANGE_ATTACK = %s;\n", Frames::attack_slot[Frames::RANGE]);
		WAD::Printf("MINATTACK_CHANCE = 25%%;\n");
	}

	if (Frames::attack_slot[Frames::COMBAT])
	{
		WAD::Printf("CLOSE_ATTACK = %s;\n", Frames::attack_slot[Frames::COMBAT]);
	}
	else if (info->meleestate && info->name[0] != '*')
	{
		PrintWarn("No close attack in melee states of [%s].\n", GetMobjName(mt_num));
		WAD::Printf("CLOSE_ATTACK = DEMON_CLOSECOMBAT; // dummy attack\n");
	}

	if (Frames::attack_slot[Frames::SPARE])
		WAD::Printf("SPARE_ATTACK = %s;\n", Frames::attack_slot[Frames::SPARE]);
}


void Things::ConvertMobj(const mobjinfo_t *info, int mt_num, int player,
	bool brain_missile, bool& got_one)
{
	if (info->name[0] == '*')  // attack
		return;

	if (! got_one)
	{
		got_one = true;
		BeginLump();
	}

	const char * ddf_name = GetMobjName(mt_num);

	if (brain_missile)
		ddf_name = info->name;

	if (player > 0)
		WAD::Printf("[%s:%d]\n", player_info[player-1].name, player_info[player-1].num);
	else if (info->doomednum < 0)
		WAD::Printf("[%s]\n", ddf_name);
	else
		WAD::Printf("[%s:%d]\n", ddf_name, info->doomednum);

	WAD::Printf("RADIUS = %1.1f;\n", F_FIXED(info->radius));
	WAD::Printf("HEIGHT = %1.1f;\n", F_FIXED(info->height));

	if (info->spawnhealth != 1000)
		WAD::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

	if (player > 0)
		WAD::Printf("SPEED = 1;\n");
	else if (info->speed != 0)
		WAD::Printf("SPEED = %s;\n", GetSpeed(info->speed));

	if (info->mass != 100 && info->mass > 0)
		WAD::Printf("MASS = %d;\n", info->mass);

	if (info->reactiontime != 0)
		WAD::Printf("REACTION_TIME = %dT;\n", info->reactiontime);

	if (info->painchance >= 256)
		WAD::Printf("PAINCHANCE = 100%%;\n");
	else if (info->painchance > 0)
		WAD::Printf("PAINCHANCE = %1.1f%%;\n",
			(float)info->painchance * 100.0 / 256.0);

	if (mt_num == MT_BOSSSPIT)
		WAD::Printf("SPIT_SPOT = BRAIN_SPAWNSPOT;\n");

	HandleCastOrder(info, mt_num, player);
	HandleDropItem(info, mt_num);
	HandlePlayer(info, player);
	HandleItem(info, mt_num);
	HandleSounds(info, mt_num);
	HandleFrames(info, mt_num);

	WAD::Printf("\n");

	HandleFlags(info, mt_num, player);
	HandleMBF21Flags(info, mt_num, player);
	HandleAttacks(info, mt_num);

	if (Frames::act_flags & AF_EXPLODE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = 128;\n");
	else if (Frames::act_flags & AF_DETONATE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = %d;\n", info->damage);

	if ((Frames::act_flags & AF_KEENDIE))
		Rscript::MarkKeenDie(mt_num);

	WAD::Printf("\n");
}


void Things::ConvertTHING(void)
{
	FixHeights();

	CollectTheCast();

	if (all_mode)
	{
		for (int i = 0 ; i < NUMMOBJTYPES_COMPAT ; i++)
			MarkThing(i);

		/* this is debatable...
		for (int i = MT_EXTRA00 ; i <= MT_EXTRA99 ; i++)
			MarkThing(i);
		*/
	}

	bool got_one = false;

	for (int i = 0; i < (int)new_mobjinfo.size() ; i++)
	{
	    const mobjinfo_t * info = new_mobjinfo[i];

	    if (info == NULL)
			continue;

		if (i == MT_PLAYER)
		{
			for (int p = 1 ; p <= NUMPLAYERS ; p++)
				ConvertMobj(info, i, p, false, got_one);

			continue;
		}

		ConvertMobj(info, i, 0, false, got_one);
	}

	// TODO we don't always need this, figure out WHEN WE DO
	if (true)
		ConvertMobj(&brain_explode_mobj, MT_ROCKET, 0, true, got_one);

	if (got_one)
		FinishLump();
}


void Things::ConvertATK()
{
	Attacks::got_one = false;

	for (size_t k = 0 ; k < Attacks::scratchers.size() ; k++)
	{
		Attacks::ConvertScratch(Attacks::scratchers[k]);
	}

	// Note: all_mode was handled by ConvertTHING

	for (int i = 0 ; i < (int)new_mobjinfo.size() ; i++)
	{
	    const mobjinfo_t * info = new_mobjinfo[i];

		if (info == NULL)
			continue;

		Attacks::ConvertAttack(info, i, false);

		if (i == MT_ROCKET)
			Attacks::ConvertAttack(info, i, true);
	}

	Attacks::CheckPainElemental();

	if (Attacks::got_one)
		Attacks::FinishLump();
}


//------------------------------------------------------------------------

namespace Things
{
#define FIELD_OFS(xxx)  offsetof(mobjinfo_t, xxx)

	const fieldreference_t mobj_field[] =
	{
		{ "ID #",               FIELD_OFS(doomednum),     FT_ANY },
		{ "Initial frame",      FIELD_OFS(spawnstate),    FT_FRAME },
		{ "Hit points",         FIELD_OFS(spawnhealth),   FT_GTEQ1 },
		{ "First moving frame", FIELD_OFS(seestate),      FT_FRAME },
		{ "Alert sound",        FIELD_OFS(seesound),      FT_SOUND },
		{ "Reaction time",      FIELD_OFS(reactiontime),  FT_NONEG },
		{ "Attack sound",       FIELD_OFS(attacksound),   FT_SOUND },
		{ "Injury frame",       FIELD_OFS(painstate),     FT_FRAME },
		{ "Pain chance",        FIELD_OFS(painchance),    FT_NONEG },
		{ "Pain sound",         FIELD_OFS(painsound),     FT_SOUND },
		{ "Close attack frame", FIELD_OFS(meleestate),    FT_FRAME },
		{ "Far attack frame",   FIELD_OFS(missilestate),  FT_FRAME },
		{ "Death frame",        FIELD_OFS(deathstate),    FT_FRAME },
		{ "Exploding frame",    FIELD_OFS(xdeathstate),   FT_FRAME },
		{ "Death sound",        FIELD_OFS(deathsound),    FT_SOUND },
		{ "Speed",              FIELD_OFS(speed),         FT_NONEG },
		{ "Width",              FIELD_OFS(radius),        FT_NONEG },
		{ "Height",             FIELD_OFS(height),        FT_NONEG },
		{ "Mass",               FIELD_OFS(mass),          FT_NONEG },
		{ "Missile damage",     FIELD_OFS(damage),        FT_NONEG },
		{ "Action sound",       FIELD_OFS(activesound),   FT_SOUND },
		{ "Bits",               FIELD_OFS(flags),         FT_BITS },
		{ "MBF21 Bits",         FIELD_OFS(mbf21_flags),   FT_BITS },
		{ "Infighting group",   FIELD_OFS(infight_group), FT_NONEG },
		{ "Projectile group",   FIELD_OFS(proj_group),    FT_ANY },
		{ "Splash group",       FIELD_OFS(splash_group),  FT_NONEG },
		{ "Rip sound",          FIELD_OFS(rip_sound),     FT_SOUND },
		{ "Fast speed",         FIELD_OFS(fast_speed),    FT_NONEG },
		{ "Melee range",        FIELD_OFS(melee_range),   FT_NONEG },
		{ "Respawn frame",      FIELD_OFS(raisestate),    FT_FRAME },

		{ NULL, 0, FT_ANY }   // End sentinel
	};
}


void Things::AlterThing(int new_val)
{
	int mt_num = Patch::active_obj - 1;  // NOTE WELL
	assert(mt_num >= 0);

	const char *field_name = Patch::line_buf;

	MarkThing(mt_num);

	int * raw_obj = (int *) new_mobjinfo[mt_num];
	assert(raw_obj != NULL);

	if (! Field_Alter(mobj_field, field_name, raw_obj, new_val))
	{
		PrintWarn("UNKNOWN THING FIELD: %s\n", field_name);
	}
}


void Things::AlterBexBits(char *bit_str)
{
	int mt_num = Patch::active_obj - 1;  /* NOTE WELL */
	assert(mt_num >= 0);

	MarkThing(mt_num);

	new_mobjinfo[mt_num]->flags = ParseBits(flag_list, bit_str);
}


void Things::AlterMBF21Bits(char *bit_str)
{
	int mt_num = Patch::active_obj - 1;  /* NOTE WELL */
	assert(mt_num >= 0);

	MarkThing(mt_num);

	new_mobjinfo[mt_num]->mbf21_flags = ParseBits(mbf21flag_list, bit_str);
}


}  // Deh_Edge
