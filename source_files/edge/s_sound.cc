//----------------------------------------------------------------------------
//  EDGE Sound System
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

#ifdef EDGE_WEB
#include <emscripten.h>
#endif


#include "epi_sdl.h"
#include "i_sound.h"
#include "i_system.h"

#include "dm_state.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "w_wad.h"

#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"

#include "p_local.h" // P_ApproxDistance
#include "p_user.h"  // room_area

extern void E_ProgressMessage(const char *message);

static bool allow_hogs = true;

extern float listen_x;
extern float listen_y;
extern float listen_z;

bool var_cache_sfx = true;

/* See m_option.cc for corresponding menu items */
const int channel_counts[8] = {32, 64, 96, 128, 160, 192, 224, 256};

const int category_limit_table[3][8][3] = {
    /* 8 channel (TEST) */
    {
        {1, 1, 1}, // UI
        {1, 1, 1}, // Player
        {1, 1, 1}, // Weapon

        {1, 1, 1}, // Opponent
        {1, 1, 1}, // Monster
        {1, 1, 1}, // Object
        {1, 1, 1}, // Level
    },

    /* 16 channel */
    {
        {1, 1, 1}, // UI
        {1, 1, 1}, // Player
        {2, 2, 2}, // Weapon

        {0, 2, 7}, // Opponent
        {7, 5, 0}, // Monster
        {3, 3, 3}, // Object
        {2, 2, 2}, // Level
    },

    /* 32 channel */
    {
        {2, 2, 2}, // UI
        {2, 2, 2}, // Player
        {3, 3, 3}, // Weapon

        {0, 5, 12},  // Opponent
        {14, 10, 2}, // Monster
        {7, 6, 7},   // Object
        {4, 4, 4},   // Level
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
    if (COOP_MATCH())
        mode = 1;
    if (DEATHMATCH())
        mode = 2;

    int idx = 0;
    if (num_chan >= 16)
        idx = 1;
    if (num_chan >= 32)
        idx = 2;

    int multiply = 1;
    if (num_chan >= 64)
        multiply = num_chan / 32;

    for (int t = 0; t < SNCAT_NUMTYPES; t++)
    {
        cat_limits[t] = category_limit_table[idx][t][mode] * multiply;
        cat_counts[t] = 0;
    }
}

static int FindFreeChannel(void)
{
    for (int i = 0; i < num_chan; i++)
    {
        mix_channel_c *chan = mix_chan[i];

        if (chan->state == CHAN_Finished)
            S_KillChannel(i);

        if (chan->state == CHAN_Empty)
            return i;
    }

    return -1; // not found
}

static int FindPlayingFX(SoundEffectDefinition *def, int cat, position_c *pos)
{
    for (int i = 0; i < num_chan; i++)
    {
        mix_channel_c *chan = mix_chan[i];

        if (chan->state == CHAN_Playing && chan->category == cat && chan->pos == pos)
        {
            if (chan->def == def)
                return i;

            if (chan->def->singularity_ > 0 && chan->def->singularity_ == def->singularity_)
                return i;
        }
    }

    return -1; // not found
}

static int FindBiggestHog(int real_cat)
{
    int biggest_hog   = -1;
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
            biggest_hog   = hog;
            biggest_extra = extra;
        }
    }

    SYS_ASSERT(biggest_hog >= 0);

    return biggest_hog;
}

static void CountPlayingCats(void)
{
    for (int c = 0; c < SNCAT_NUMTYPES; c++)
        cat_counts[c] = 0;

    for (int i = 0; i < num_chan; i++)
    {
        mix_channel_c *chan = mix_chan[i];

        if (chan->state == CHAN_Playing)
            cat_counts[chan->category] += 1;
    }
}

static int ChannelScore(SoundEffectDefinition *def, int category, position_c *pos, bool boss)
{
    // for full-volume sounds, use the priority from DDF
    if (category <= SNCAT_Weapon)
    {
        return 200 - def->priority_;
    }

    // for stuff in the level, use the distance
    SYS_ASSERT(pos);

    float dist = boss ? 0 : P_ApproxDistance(listen_x - pos->x, listen_y - pos->y, listen_z - pos->z);

    int base_score = 999 - (int)(dist / 10.0);

    return base_score * 100 - def->priority_;
}

static int FindChannelToKill(int kill_cat, int real_cat, int new_score)
{
    int kill_idx   = -1;
    int kill_score = (1 << 30);

    // LogPrint("FindChannelToKill: cat:%d new_score:%d\n", kill_cat, new_score);
    for (int j = 0; j < num_chan; j++)
    {
        mix_channel_c *chan = mix_chan[j];

        if (chan->state != CHAN_Playing)
            continue;

        if (chan->category != kill_cat)
            continue;

        int score = ChannelScore(chan->def, chan->category, chan->pos, chan->boss);
        // LogPrint("> [%d] '%s' = %d\n", j, chan->def->name.c_str(), score);
        //  find one with LOWEST score
        if (score < kill_score)
        {
            kill_idx   = j;
            kill_score = score;
        }
    }
    // LogPrint("kill_idx = %d\n", kill_idx);
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
    if (no_sound)
        return;

    E_ProgressMessage("Initializing sound device...");

    int want_chan = channel_counts[var_mix_channels];

    LogPrint("StartupSound: Init %d mixing channels\n", want_chan);

    // setup channels
    S_InitChannels(want_chan);

    SetupCategoryLimits();

    S_QueueInit();

    // okidoke, start the ball rolling!
    SDL_PauseAudioDevice(current_sound_device, 0);
}

void S_Shutdown(void)
{
    if (no_sound)
        return;

    SDL_PauseAudioDevice(current_sound_device, 1);

    // make sure mixing thread is not running our code
    SDL_LockAudioDevice(current_sound_device);
    SDL_UnlockAudioDevice(current_sound_device);

    S_QueueShutdown();

    S_FreeChannels();
}

// Not-rejigged-yet stuff..
SoundEffectDefinition *LookupEffectDef(const SoundEffect *s)
{
    SYS_ASSERT(s->num >= 1);

    int num;

    if (s->num > 1)
        num = s->sounds[RandomByte() % s->num];
    else
        num = s->sounds[0];

    SYS_ASSERT(0 <= num && num < sfxdefs.size());

    return sfxdefs[num];
}

static void S_PlaySound(int idx, SoundEffectDefinition *def, int category, position_c *pos, int flags, sound_data_c *buf)
{
    // LogPrint("S_PlaySound on idx #%d DEF:%p\n", idx, def);

    // LogPrint("Looked up def: %p, caching...\n", def);

    mix_channel_c *chan = mix_chan[idx];

    chan->state = CHAN_Playing;
    chan->data  = buf;

    // LogPrint("chan=%p data=%p\n", chan, chan->data);

    chan->def      = def;
    chan->pos      = pos;
    chan->category = category; //?? store use_cat and orig_cat

    // volume computed during mixing (?)
    chan->volume_L = 0;
    chan->volume_R = 0;

    chan->offset = 0;
    chan->length = chan->data->length << 10;

    chan->loop = false;
    chan->boss = (flags & FX_Boss) ? true : false;

    chan->ComputeDelta();

    // LogPrint("FINISHED: delta=0x%lx\n", chan->delta);
}

static void DoStartFX(SoundEffectDefinition *def, int category, position_c *pos, int flags, sound_data_c *buf)
{
    CountPlayingCats();

    int k = FindPlayingFX(def, category, pos);

    if (k >= 0)
    {
        // LogPrint("@ already playing on #%d\n", k);
        mix_channel_c *chan = mix_chan[k];

        if (def->looping_ && def == chan->def)
        {
            // LogPrint("@@ RE-LOOPING\n");
            chan->loop = true;
            return;
        }
        else if (flags & FX_Single)
        {
            if (chan->def->precious_)
                return;

            // LogPrint("@@ Killing sound for SINGULAR\n");
            S_KillChannel(k);
            S_PlaySound(k, def, category, pos, flags, buf);
            return;
        }
    }

    k = FindFreeChannel();

    if (!allow_hogs)
    {
        if (cat_counts[category] >= cat_limits[category])
            k = -1;
    }

    // LogPrint("@ free channel = #%d\n", k);
    if (k < 0)
    {
        // all channels are in use.
        // either kill one, or drop the new sound.

        int new_score = ChannelScore(def, category, pos, (flags & FX_Boss) ? true : false);

        // decide which category to kill a sound in.
        int kill_cat = category;

        if (cat_counts[category] < cat_limits[category])
        {
            // we haven't reached our quota yet, hence kill a hog.
            kill_cat = FindBiggestHog(category);
            // LogPrint("@ biggest hog: %d\n", kill_cat);
        }

        SYS_ASSERT(cat_counts[kill_cat] >= cat_limits[kill_cat]);

        k = FindChannelToKill(kill_cat, category, new_score);

        // if (k<0) LogPrint("- new score too low\n");
        if (k < 0)
            return;

        // LogPrint("- killing channel %d (kill_cat:%d)  my_cat:%d\n", k, kill_cat, category);
        S_KillChannel(k);
    }

    S_PlaySound(k, def, category, pos, flags, buf);
}

void S_StartFX(SoundEffect *sfx, int category, position_c *pos, int flags)
{
    if (no_sound || !sfx)
        return;

    if (fast_forward_active)
        return;

    SYS_ASSERT(0 <= category && category < SNCAT_NUMTYPES);

    if (category >= SNCAT_Opponent && !pos)
        FatalError("S_StartFX: position missing for category: %d\n", category);

    SoundEffectDefinition *def = LookupEffectDef(sfx);
    SYS_ASSERT(def);

    // ignore very far away sounds
    if (category >= SNCAT_Opponent && !(flags & FX_Boss))
    {
        float dist = P_ApproxDistance(listen_x - pos->x, listen_y - pos->y, listen_z - pos->z);

        if (dist > def->max_distance_)
            return;
    }

    if (def->singularity_ > 0)
    {
        flags |= FX_Single;
        flags |= (def->precious_ ? FX_Precious : 0);
    }

    // LogPrint("StartFX: '%s' cat:%d flags:0x%04x\n", def->name.c_str(), category, flags);

    while (cat_limits[category] == 0)
        category++;

    sound_data_c *buf = S_CacheLoad(def);
    if (!buf)
        return;

    if (vacuum_sfx)
        buf->Mix_Vacuum();
    else if (submerged_sfx)
        buf->Mix_Submerged();
    else
    {
        if (ddf_reverb)
            buf->Mix_Reverb(dynamic_reverb, room_area, outdoor_reverb, ddf_reverb_type, ddf_reverb_ratio,
                            ddf_reverb_delay);
        else
            buf->Mix_Reverb(dynamic_reverb, room_area, outdoor_reverb, 0, 0, 0);
    }

    LockAudio();
    {
        DoStartFX(def, category, pos, flags, buf);
    }
    UnlockAudio();
}

void S_StopFX(position_c *pos)
{
    if (no_sound)
        return;

    LockAudio();
    {
        for (int i = 0; i < num_chan; i++)
        {
            mix_channel_c *chan = mix_chan[i];

            if (chan->state == CHAN_Playing && chan->pos == pos)
            {
                // LogPrint("S_StopFX: killing #%d\n", i);
                S_KillChannel(i);
            }
        }
    }
    UnlockAudio();
}

void S_StopLevelFX(void)
{
    if (no_sound)
        return;

    LockAudio();
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
    UnlockAudio();
}

void S_StopAllFX(void)
{
    if (no_sound)
        return;

    LockAudio();
    {
        for (int i = 0; i < num_chan; i++)
        {
            mix_channel_c *chan = mix_chan[i];

            if (chan->state != CHAN_Empty)
            {
                S_KillChannel(i);
            }
        }
    }
    UnlockAudio();
}

void S_SoundTicker(void)
{
    if (no_sound)
        return;

    LockAudio();
    {
        if (game_state == GS_LEVEL)
        {
            SYS_ASSERT(::numplayers > 0);

            mobj_t *pmo = ::players[displayplayer]->mo;
            SYS_ASSERT(pmo);

            S_UpdateSounds(pmo, pmo->angle);
        }
        else
        {
            S_UpdateSounds(nullptr, 0);
        }
    }
    UnlockAudio();
}

void S_ChangeChannelNum(void)
{
    if (no_sound)
        return;

    LockAudio();
    {
        int want_chan = channel_counts[var_mix_channels];

        S_ReallocChannels(want_chan);

        SetupCategoryLimits();
    }
    UnlockAudio();
}

void S_PrecacheSounds(void)
{
    if (var_cache_sfx)
    {
        E_ProgressMessage("Precaching SFX...");
        for (int i = 0; i < sfxdefs.size(); i++)
        {
            S_CacheLoad(sfxdefs[i]);
        }
    }
}

void S_ResumeAudioDevice()
{
    SDL_PauseAudioDevice(current_sound_device, 0);

#ifdef EDGE_WEB
    // Yield back to main thread for audio processing
    if (emscripten_has_asyncify())
    {
        emscripten_sleep(100);
    }
#endif
}

void S_PauseAudioDevice()
{
    S_StopAllFX();
    SDL_PauseAudioDevice(current_sound_device, 1);

#ifdef EDGE_WEB
    // Yield back to main thread for audio processing
    // If complied without async support, as with debug build, will stutter
    if (emscripten_has_asyncify())
    {
        emscripten_sleep(100);
    }
#endif
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
