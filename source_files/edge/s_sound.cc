//----------------------------------------------------------------------------
//  EDGE Sound System
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

#include "i_defs.h"
#include "i_sdlinc.h"

#include "dm_state.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "w_wad.h"

#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"

#include "p_local.h" // P_ApproxDistance


static bool allow_hogs = true;

extern float listen_x;
extern float listen_y;
extern float listen_z;


/* See m_option.cc for corresponding menu items */
const int channel_counts[5] = { 8, 16, 32, 64, 96 };


const int category_limit_table[3][8][3] =
{
	/* 8 channel (TEST) */
	{
		{ 1, 1, 1 }, // UI
		{ 1, 1, 1 }, // Player
		{ 1, 1, 1 }, // Weapon

		{ 1, 1, 1 }, // Opponent
		{ 1, 1, 1 }, // Monster
		{ 1, 1, 1 }, // Object
		{ 1, 1, 1 }, // Level
	},

	/* 16 channel */
	{
		{ 1, 1, 1 }, // UI
		{ 1, 1, 1 }, // Player
		{ 2, 2, 2 }, // Weapon

		{ 0, 2, 7 }, // Opponent
		{ 7, 5, 0 }, // Monster
		{ 3, 3, 3 }, // Object
		{ 2, 2, 2 }, // Level
	},

	/* 32 channel */
	{
		{ 2, 2, 2 }, // UI
		{ 2, 2, 2 }, // Player
		{ 3, 3, 3 }, // Weapon

		{ 0, 5,12 }, // Opponent
		{14,10, 2 }, // Monster
		{ 7, 6, 7 }, // Object
		{ 4, 4, 4 }, // Level
	},

	// NOTE: never put a '0' on the WEAPON line, since the top
	// four categories should never be merged with the rest.
};


static int cat_limits[SNCAT_NUMTYPES];
static int cat_counts[SNCAT_NUMTYPES];


static void SetupCategoryLimits(void)
{
	// Assumes: num_chan to be already set, and the DEATHMATCH()
	//          and COOP_MATCH() macros are working.

	int mode = 0;
	if (COOP_MATCH()) mode = 1;
	if (DEATHMATCH()) mode = 2;

	int idx = 0;
	if (num_chan >= 16) idx=1;
	if (num_chan >= 32) idx=2;

	int multiply = 1;
	if (num_chan >= 64) multiply = num_chan / 32;

	for (int t = 0; t < SNCAT_NUMTYPES; t++)
	{
		cat_limits[t] = category_limit_table[idx][t][mode] * multiply;
		cat_counts[t] = 0;
	}
}

static int FindFreeChannel(void)
{
	for (int i=0; i < num_chan; i++)
	{
		mix_channel_c *chan = mix_chan[i];

		if (chan->state == CHAN_Finished)
			S_KillChannel(i);

		if (chan->state == CHAN_Empty)
			return i;
	}

	return -1; // not found
}

static int FindPlayingFX(sfxdef_c *def, int cat, position_c *pos)
{
	for (int i=0; i < num_chan; i++)
	{
		mix_channel_c *chan = mix_chan[i];

		if (chan->state == CHAN_Playing && chan->category == cat &&
			chan->pos == pos)
		{
			if (chan->def == def)
				return i;

			if (chan->def->singularity > 0 && chan->def->singularity == def->singularity)
				return i;
		}
	}

	return -1; // not found
}

static int FindBiggestHog(int real_cat)
{
	int biggest_hog = -1;
	int biggest_extra = 0;

	for (int hog = 0; hog < SNCAT_NUMTYPES; hog++)
	{
		if (hog == real_cat)
			continue;

		int extra = cat_counts[hog] - cat_limits[hog];

		if (extra <= 0)
			continue;

		// found a hog!
		if (biggest_hog < 0 || extra > biggest_extra)
		{
			biggest_hog = hog;
			biggest_extra = extra;
		}
	}

	SYS_ASSERT(biggest_hog >= 0);

	return biggest_hog;
}

static void CountPlayingCats(void)
{
	for (int c=0; c < SNCAT_NUMTYPES; c++)
		cat_counts[c] = 0;

	for (int i=0; i < num_chan; i++)
	{
		mix_channel_c *chan = mix_chan[i];

		if (chan->state == CHAN_Playing)
			cat_counts[chan->category] += 1;
	}
}

static int ChannelScore(sfxdef_c *def, int category, position_c *pos, bool boss)
{
	// for full-volume sounds, use the priority from DDF
	if (category <= SNCAT_Weapon)
	{
		return 200 - def->priority;
	}

	// for stuff in the level, use the distance
	SYS_ASSERT(pos);

	float dist = boss ? 0 :
		P_ApproxDistance(listen_x - pos->x, listen_y - pos->y, listen_z - pos->z);

	int base_score = 999 - (int)(dist / 10.0);

	return base_score * 100 - def->priority;
}

static int FindChannelToKill(int kill_cat, int real_cat, int new_score)
{
	int kill_idx = -1;
	int kill_score = (1<<30);

//I_Printf("FindChannelToKill: cat:%d new_score:%d\n", kill_cat, new_score);
	for (int j=0; j < num_chan; j++)
	{
		mix_channel_c *chan = mix_chan[j];

		if (chan->state != CHAN_Playing)
			continue;

		if (chan->category != kill_cat)
			continue;
		
		int score = ChannelScore(chan->def, chan->category,
								 chan->pos, chan->boss);
//I_Printf("> [%d] '%s' = %d\n", j, chan->def->name.c_str(), score);
		// find one with LOWEST score
		if (score < kill_score)
		{
			kill_idx = j;
			kill_score = score;
		}
	}
//I_Printf("kill_idx = %d\n", kill_idx);
	SYS_ASSERT(kill_idx >= 0);

	if (kill_cat != real_cat)
		return kill_idx;

	// if the score for new sound is worse than any existing
	// channel, then simply discard the new sound.
	if (new_score >= kill_score)
		return kill_idx;

	return -1;
}


void S_Init(void)
{
	if (nosound) return;

	int want_chan = channel_counts[var_mix_channels];

	I_Printf("I_StartupSound: Init %d mixing channels\n", want_chan);

	// setup channels
	S_InitChannels(want_chan);

	SetupCategoryLimits();

	S_QueueInit();

	// okidoke, start the ball rolling!
	SDL_PauseAudio(0);
}

void S_Shutdown(void)
{
	if (nosound) return;

	SDL_PauseAudio(1);

	// make sure mixing thread is not running our code
	SDL_LockAudio();
	SDL_UnlockAudio();

	S_QueueShutdown();

	S_FreeChannels();
}

// Not-rejigged-yet stuff..
sfxdef_c * LookupEffectDef(const sfx_t *s) 
{ 
	SYS_ASSERT(s->num >= 1);

	// need to use M_Random here to prevent demos and net games 
	// getting out of sync.

	int num;

	if (s->num > 1)
		num = s->sounds[M_Random() % s->num];
	else
		num = s->sounds[0];

	SYS_ASSERT(0 <= num && num < sfxdefs.GetSize());

	return sfxdefs[num];
}

static void S_PlaySound(int idx, sfxdef_c *def, int category, position_c *pos, int flags)
{
//I_Printf("S_PlaySound on idx #%d DEF:%p\n", idx, def);

//I_Printf("Looked up def: %p, caching...\n", def);
	epi::sound_data_c *buf = S_CacheLoad(def);
	if (! buf)
		return;

	mix_channel_c *chan = mix_chan[idx];

	chan->state = CHAN_Playing;
	chan->data  = buf;

//I_Printf("chan=%p data=%p\n", chan, chan->data);

	chan->def = def;
	chan->pos = pos;
	chan->category = category; //?? store use_cat and orig_cat

	// volume computed during mixing (?)
    chan->volume_L = 0;
    chan->volume_R = 0;

	chan->offset = 0;
	chan->length = chan->data->length << 10;

	chan->loop = false;
	chan->boss = (flags & FX_Boss) ? true : false;

	chan->ComputeDelta();

//I_Printf("FINISHED: delta=0x%lx\n", chan->delta);
}

static void DoStartFX(sfxdef_c *def, int category, position_c *pos, int flags)
{
	CountPlayingCats();

	int k = FindPlayingFX(def, category, pos);

	if (k >= 0)
	{
//I_Printf("@ already playing on #%d\n", k);
		mix_channel_c *chan = mix_chan[k];

		if (def->looping && def == chan->def)
		{
//I_Printf("@@ RE-LOOPING\n");
			chan->loop = true;
			return;
		}
		else if (flags & FX_Single)
		{
			if (flags & FX_Precious)
				return;

//I_Printf("@@ Killing sound for SINGULAR\n");
			S_KillChannel(k);
			S_PlaySound(k, def, category, pos, flags);
			return;
		}
	}

	k = FindFreeChannel();

	if (! allow_hogs)
	{
		if (cat_counts[category] >= cat_limits[category])
			k = -1;
	}

//I_Printf("@ free channel = #%d\n", k);
	if (k < 0)
	{
		// all channels are in use.
		// either kill one, or drop the new sound.

		int new_score = ChannelScore(def, category, pos,
							(flags & FX_Boss) ? true : false);

		// decide which category to kill a sound in.
		int kill_cat = category;
			
		if (cat_counts[category] < cat_limits[category])
		{
			// we haven't reached our quota yet, hence kill a hog.
			kill_cat = FindBiggestHog(category);
//I_Printf("@ biggest hog: %d\n", kill_cat);
		}

		SYS_ASSERT(cat_counts[kill_cat] >= cat_limits[kill_cat]);

		k = FindChannelToKill(kill_cat, category, new_score);

//if (k<0) I_Printf("- new score too low\n");
		if (k < 0)
			return;

//I_Printf("- killing channel %d (kill_cat:%d)  my_cat:%d\n", k, kill_cat, category);
		S_KillChannel(k);
	}

	S_PlaySound(k, def, category, pos, flags);
}


void S_StartFX(sfx_t *sfx, int category, position_c *pos, int flags)
{
	if (nosound || !sfx) return;

	if (fast_forward_active) return;

	SYS_ASSERT(0 <= category && category < SNCAT_NUMTYPES);

	if (category >= SNCAT_Opponent && ! pos)
		I_Error("S_StartFX: position missing for category: %d\n", category);

	sfxdef_c *def = LookupEffectDef(sfx);
	SYS_ASSERT(def);

	// ignore very far away sounds
	if (category >= SNCAT_Opponent && !(flags & FX_Boss))
	{
		float dist = P_ApproxDistance(listen_x - pos->x, listen_y - pos->y, listen_z - pos->z);

		if (dist > def->max_distance)
			return;
	}

	if (def->singularity > 0)
	{
		flags |= FX_Single;
		flags |= (def->precious ? FX_Precious : 0);
	}

//I_Printf("StartFX: '%s' cat:%d flags:0x%04x\n", def->name.c_str(), category, flags);

	while (cat_limits[category] == 0)
		category++;

	I_LockAudio();
	{
		DoStartFX(def, category, pos, flags);
	}
	I_UnlockAudio();
}


void S_StopFX(position_c *pos)
{
	if (nosound) return;

	I_LockAudio();
	{
		for (int i = 0; i < num_chan; i++)
		{
			mix_channel_c *chan = mix_chan[i];

			if (chan->state == CHAN_Playing && chan->pos == pos)
			{
//I_Printf("S_StopFX: killing #%d\n", i);
				S_KillChannel(i);
			}
		}
	}
	I_UnlockAudio();
}

void S_StopLevelFX(void)
{
	if (nosound) return;

	I_LockAudio();
	{
		for (int i = 0; i < num_chan; i++)
		{
			mix_channel_c *chan = mix_chan[i];

			if (chan->state != CHAN_Empty && chan->category != SNCAT_UI)
			{
				S_KillChannel(i);
			}
		}
	}
	I_UnlockAudio();
}


void S_SoundTicker(void)
{
	if (nosound) return;

	I_LockAudio();
	{
		if (gamestate == GS_LEVEL)
		{
			SYS_ASSERT(::numplayers > 0);

			mobj_t *pmo = ::players[displayplayer]->mo;
			SYS_ASSERT(pmo);

			S_UpdateSounds(pmo, pmo->angle);
		}
		else
		{
			S_UpdateSounds(NULL, 0);
		}
	}
	I_UnlockAudio();
}

void S_ChangeChannelNum(void)
{
	if (nosound) return;

	I_LockAudio();
	{
		int want_chan = channel_counts[var_mix_channels];

		S_ReallocChannels(want_chan);

		SetupCategoryLimits();
	}
	I_UnlockAudio();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
