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

#include "dm_state.h"
#include "epi_sdl.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"  // ApproximateDistance
#include "s_blit.h"
#include "s_cache.h"
#include "s_sound.h"
#include "w_wad.h"

extern float room_area;

extern void StartupProgressMessage(const char *message);

static bool allow_hogs = true;

extern float listen_x;
extern float listen_y;
extern float listen_z;

bool precache_sound_effects = true;

/* See m_option.cc for corresponding menu items */
const int channel_counts[8] = {32, 64, 96, 128, 160, 192, 224, 256};

const int category_limit_table[3][8][3] = {
    /* 8 channel (TEST) */
    {
        {1, 1, 1},  // UI
        {1, 1, 1},  // Player
        {1, 1, 1},  // Weapon

        {1, 1, 1},  // Opponent
        {1, 1, 1},  // Monster
        {1, 1, 1},  // Object
        {1, 1, 1},  // Level
    },

    /* 16 channel */
    {
        {1, 1, 1},  // UI
        {1, 1, 1},  // Player
        {2, 2, 2},  // Weapon

        {0, 2, 7},  // Opponent
        {7, 5, 0},  // Monster
        {3, 3, 3},  // Object
        {2, 2, 2},  // Level
    },

    /* 32 channel */
    {
        {2, 2, 2},  // UI
        {2, 2, 2},  // Player
        {3, 3, 3},  // Weapon

        {0, 5, 12},   // Opponent
        {14, 10, 2},  // Monster
        {7, 6, 7},    // Object
        {4, 4, 4},    // Level
    },

    // NOTE: never put a '0' on the WEAPON line, since the top
    // four categories should never be merged with the rest.
};

static int category_limits[kTotalCategories];
static int category_counts[kTotalCategories];

static void SetupCategoryLimits(void)
{
    // Assumes: num_chan to be already set, and the InDeathmatch()
    //          and InCooperativeMatch() macros are working.

    int mode = 0;
    if (InCooperativeMatch()) mode = 1;
    if (InDeathmatch()) mode = 2;

    int idx = 0;
    if (total_channels >= 16) idx = 1;
    if (total_channels >= 32) idx = 2;

    int multiply = 1;
    if (total_channels >= 64) multiply = total_channels / 32;

    for (int t = 0; t < kTotalCategories; t++)
    {
        category_limits[t] = category_limit_table[idx][t][mode] * multiply;
        category_counts[t] = 0;
    }
}

static int FindFreeChannel(void)
{
    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelFinished) SoundKillChannel(i);

        if (chan->state_ == kChannelEmpty) return i;
    }

    return -1;  // not found
}

static int FindPlayingFX(SoundEffectDefinition *def, int cat, Position *pos)
{
    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelPlaying && chan->category_ == cat &&
            chan->position_ == pos)
        {
            if (chan->definition_ == def) return i;

            if (chan->definition_->singularity_ > 0 &&
                chan->definition_->singularity_ == def->singularity_)
                return i;
        }
    }

    return -1;  // not found
}

static int FindBiggestHog(int real_cat)
{
    int biggest_hog   = -1;
    int biggest_extra = 0;

    for (int hog = 0; hog < kTotalCategories; hog++)
    {
        if (hog == real_cat) continue;

        int extra = category_counts[hog] - category_limits[hog];

        if (extra <= 0) continue;

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
    for (int c = 0; c < kTotalCategories; c++) category_counts[c] = 0;

    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelPlaying)
            category_counts[chan->category_] += 1;
    }
}

static int ChannelScore(SoundEffectDefinition *def, int category, Position *pos,
                        bool boss)
{
    // for full-volume sounds, use the priority from DDF
    if (category <= kCategoryWeapon) { return 200 - def->priority_; }

    // for stuff in the level, use the distance
    SYS_ASSERT(pos);

    float dist = boss
                     ? 0
                     : ApproximateDistance(listen_x - pos->x, listen_y - pos->y,
                                           listen_z - pos->z);

    int base_score = 999 - (int)(dist / 10.0);

    return base_score * 100 - def->priority_;
}

static int FindChannelToKill(int kill_cat, int real_cat, int new_score)
{
    int kill_idx   = -1;
    int kill_score = (1 << 30);

    // LogPrint("FindChannelToKill: cat:%d new_score:%d\n", kill_cat,
    // new_score);
    for (int j = 0; j < total_channels; j++)
    {
        SoundChannel *chan = mix_channels[j];

        if (chan->state_ != kChannelPlaying) continue;

        if (chan->category_ != kill_cat) continue;

        int score = ChannelScore(chan->definition_, chan->category_,
                                 chan->position_, chan->boss_);

        if (score < kill_score)
        {
            kill_idx   = j;
            kill_score = score;
        }
    }
    // LogPrint("kill_idx = %d\n", kill_idx);
    SYS_ASSERT(kill_idx >= 0);

    if (kill_cat != real_cat) return kill_idx;

    // if the score for new sound is worse than any existing
    // channel, then simply discard the new sound.
    if (new_score >= kill_score) return kill_idx;

    return -1;
}

void SoundInitialize(void)
{
    if (no_sound) return;

    StartupProgressMessage("Initializing sound device...");

    int want_chan = channel_counts[sound_mixing_channels];

    LogPrint("StartupSound: Init %d mixing channels\n", want_chan);

    // setup channels
    SoundInitializeChannels(want_chan);

    SetupCategoryLimits();

    SoundQueueInitialize();

    // okidoke, start the ball rolling!
    SDL_PauseAudioDevice(current_sound_device, 0);
}

void SoundShutdown(void)
{
    if (no_sound) return;

    SDL_PauseAudioDevice(current_sound_device, 1);

    // make sure mixing thread is not running our code
    SDL_LockAudioDevice(current_sound_device);
    SDL_UnlockAudioDevice(current_sound_device);

    SoundQueueShutdown();

    SoundFreeChannels();
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

static void S_PlaySound(int idx, SoundEffectDefinition *def, int category,
                        Position *pos, int flags, SoundData *buf)
{
    // LogPrint("S_PlaySound on idx #%d DEF:%p\n", idx, def);

    // LogPrint("Looked up def: %p, caching...\n", def);

    SoundChannel *chan = mix_channels[idx];

    chan->state_ = kChannelPlaying;
    chan->data_  = buf;

    // LogPrint("chan=%p data=%p\n", chan, chan->data);

    chan->definition_ = def;
    chan->position_   = pos;
    chan->category_   = category;  //?? store use_cat and orig_cat

    // volume computed during mixing (?)
    chan->volume_left_  = 0;
    chan->volume_right_ = 0;

    chan->offset_ = 0;
    chan->length_ = chan->data_->length_ << 10;

    chan->loop_ = false;
    chan->boss_ = (flags & kSoundEffectBoss) ? true : false;

    chan->ComputeDelta();

    // LogPrint("FINISHED: delta=0x%lx\n", chan->delta);
}

static void DoStartFX(SoundEffectDefinition *def, int category, Position *pos,
                      int flags, SoundData *buf)
{
    CountPlayingCats();

    int k = FindPlayingFX(def, category, pos);

    if (k >= 0)
    {
        SoundChannel *chan = mix_channels[k];

        if (def->looping_ && def == chan->definition_)
        {
            chan->loop_ = true;
            return;
        }
        else if (flags & kSoundEffectSingle)
        {
            if (chan->definition_->precious_) return;

            // LogPrint("@@ Killing sound for SINGULAR\n");
            SoundKillChannel(k);
            S_PlaySound(k, def, category, pos, flags, buf);
            return;
        }
    }

    k = FindFreeChannel();

    if (!allow_hogs)
    {
        if (category_counts[category] >= category_limits[category]) k = -1;
    }

    // LogPrint("@ free channel = #%d\n", k);
    if (k < 0)
    {
        // all channels are in use.
        // either kill one, or drop the new sound.

        int new_score = ChannelScore(def, category, pos,
                                     (flags & kSoundEffectBoss) ? true : false);

        // decide which category to kill a sound in.
        int kill_cat = category;

        if (category_counts[category] < category_limits[category])
        {
            // we haven't reached our quota yet, hence kill a hog.
            kill_cat = FindBiggestHog(category);
            // LogPrint("@ biggest hog: %d\n", kill_cat);
        }

        SYS_ASSERT(category_counts[kill_cat] >= category_limits[kill_cat]);

        k = FindChannelToKill(kill_cat, category, new_score);

        // if (k<0) LogPrint("- new score too low\n");
        if (k < 0) return;

        // LogPrint("- killing channel %d (kill_cat:%d)  my_cat:%d\n", k,
        // kill_cat, category);
        SoundKillChannel(k);
    }

    S_PlaySound(k, def, category, pos, flags, buf);
}

void StartSoundEffect(SoundEffect *sfx, int category, Position *pos, int flags)
{
    if (no_sound || !sfx) return;

    if (fast_forward_active) return;

    SYS_ASSERT(0 <= category && category < kTotalCategories);

    if (category >= kCategoryOpponent && !pos)
        FatalError("StartSoundEffect: position missing for category: %d\n",
                   category);

    SoundEffectDefinition *def = LookupEffectDef(sfx);
    SYS_ASSERT(def);

    // ignore very far away sounds
    if (category >= kCategoryOpponent && !(flags & kSoundEffectBoss))
    {
        float dist = ApproximateDistance(listen_x - pos->x, listen_y - pos->y,
                                         listen_z - pos->z);

        if (dist > def->max_distance_) return;
    }

    if (def->singularity_ > 0)
    {
        flags |= kSoundEffectSingle;
        flags |= (def->precious_ ? kSoundEffectPrecious : 0);
    }

    // LogPrint("StartFX: '%s' cat:%d flags:0x%04x\n", def->name.c_str(),
    // category, flags);

    while (category_limits[category] == 0) category++;

    SoundData *buf = SoundCacheLoad(def);
    if (!buf) return;

    if (vacuum_sound_effects)
        buf->MixVacuum();
    else if (submerged_sound_effects)
        buf->MixSubmerged();
    else
    {
        if (ddf_reverb)
            buf->MixReverb(dynamic_reverb, room_area, outdoor_reverb,
                           ddf_reverb_type, ddf_reverb_ratio, ddf_reverb_delay);
        else
            buf->MixReverb(dynamic_reverb, room_area, outdoor_reverb, 0, 0, 0);
    }

    LockAudio();
    {
        DoStartFX(def, category, pos, flags, buf);
    }
    UnlockAudio();
}

void StopSoundEffect(Position *pos)
{
    if (no_sound) return;

    LockAudio();
    {
        for (int i = 0; i < total_channels; i++)
        {
            SoundChannel *chan = mix_channels[i];

            if (chan->state_ == kChannelPlaying && chan->position_ == pos)
            {
                // LogPrint("StopSoundEffect: killing #%d\n", i);
                SoundKillChannel(i);
            }
        }
    }
    UnlockAudio();
}

void StopLevelSoundEffects(void)
{
    if (no_sound) return;

    LockAudio();
    {
        for (int i = 0; i < total_channels; i++)
        {
            SoundChannel *chan = mix_channels[i];

            if (chan->state_ != kChannelEmpty && chan->category_ != kCategoryUi)
            {
                SoundKillChannel(i);
            }
        }
    }
    UnlockAudio();
}

void StopAllSoundEffects(void)
{
    if (no_sound) return;

    LockAudio();
    {
        for (int i = 0; i < total_channels; i++)
        {
            SoundChannel *chan = mix_channels[i];

            if (chan->state_ != kChannelEmpty) { SoundKillChannel(i); }
        }
    }
    UnlockAudio();
}

void SoundTicker(void)
{
    if (no_sound) return;

    LockAudio();
    {
        if (game_state == kGameStateLevel)
        {
            SYS_ASSERT(::total_players > 0);

            MapObject *pmo = ::players[display_player]->map_object_;
            SYS_ASSERT(pmo);

            UpdateSounds(pmo, pmo->angle_);
        }
        else { UpdateSounds(nullptr, 0); }
    }
    UnlockAudio();
}

void UpdateSoundCategoryLimits(void)
{
    if (no_sound) return;

    LockAudio();
    {
        int want_chan = channel_counts[sound_mixing_channels];

        SoundReallocateChannels(want_chan);

        SetupCategoryLimits();
    }
    UnlockAudio();
}

void PrecacheSounds(void)
{
    if (precache_sound_effects)
    {
        StartupProgressMessage("Precaching SFX...");
        for (int i = 0; i < sfxdefs.size(); i++) { SoundCacheLoad(sfxdefs[i]); }
    }
}

void ResumeAudioDevice()
{
    SDL_PauseAudioDevice(current_sound_device, 0);

#ifdef EDGE_WEB
    // Yield back to main thread for audio processing
    if (emscripten_has_asyncify()) { emscripten_sleep(100); }
#endif
}

void PauseAudioDevice()
{
    StopAllSoundEffects();
    SDL_PauseAudioDevice(current_sound_device, 1);

#ifdef EDGE_WEB
    // Yield back to main thread for audio processing
    // If complied without async support, as with debug build, will stutter
    if (emscripten_has_asyncify()) { emscripten_sleep(100); }
#endif
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
