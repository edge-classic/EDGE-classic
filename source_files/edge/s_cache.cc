//----------------------------------------------------------------------------
//  Sound Caching
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

#include "s_cache.h"

#include <vector>

#include "ddf_main.h"
#include "ddf_sfx.h"
#include "dm_state.h" // game_directory
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_util.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_mobj.h"
#include "r_defs.h"
#if EDGE_DOOM_SFX_SUPPORT
#include "s_doom.h"
#endif
#if EDGE_MP3_SUPPORT
#include "s_mp3.h"
#endif
#if EDGE_OGG_SUPPORT
#include "s_ogg.h"
#endif
#include "s_sound.h"
#include "s_wav.h"
#include "snd_data.h"
#include "snd_types.h"
#include "w_files.h"
#include "w_wad.h"

extern int  sound_device_frequency;
extern bool pc_speaker_mode;

static std::vector<SoundData *> sound_effects_cache;

static void LoadSilence(SoundData *buf)
{
    int length = 256;

    buf->frequency_ = sound_device_frequency;
    buf->Allocate(length);

    memset(buf->data_, 0, length * sizeof(int16_t) * 2);
}
#if EDGE_DOOM_SFX_SUPPORT
static bool LoadDoom(SoundData *buf, uint8_t *lump, int length)
{
    return LoadDoomSound(buf, lump, length);
}
static bool LoadPCSpeaker(SoundData *buf, uint8_t *lump, int length)
{
    return LoadPCSpeakerSound(buf, lump, length);
}
#endif
#if EDGE_WAV_SUPPORT
static bool LoadWav(SoundData *buf, uint8_t *lump, int length)
{
    return LoadWAVSound(buf, lump, length);
}
#endif
#if EDGE_OGG_SUPPORT
static bool LoadOGG(SoundData *buf, const uint8_t *lump, int length)
{
    return LoadOGGSound(buf, lump, length);
}
#endif
#if EDGE_MP3_SUPPORT
static bool LoadMP3(SoundData *buf, const uint8_t *lump, int length)
{
    return LoadMP3Sound(buf, lump, length);
}
#endif

//----------------------------------------------------------------------------

void SoundCacheClearAll(void)
{
    for (int i = 0; i < (int)sound_effects_cache.size(); i++)
        delete sound_effects_cache[i];

    sound_effects_cache.erase(sound_effects_cache.begin(), sound_effects_cache.end());
}

static bool DoCacheLoad(SoundEffectDefinition *def, SoundData *buf)
{
    // open the file or lump, and read it into memory
    epi::File  *F;
    SoundFormat fmt = kSoundUnknown;

    if (pc_speaker_mode)
    {
        if (!epi::GetExtension(def->pc_speaker_sound_).empty())
        {
            F = OpenFileFromPack(def->pc_speaker_sound_);
            if (!F)
            {
                std::string open_name = epi::PathAppendIfNotAbsolute(game_directory, def->pc_speaker_sound_);
                F                     = epi::FileOpen(open_name, epi::kFileAccessRead | epi::kFileAccessBinary);
            }
            if (!F)
            {
                DebugOrError("SFX Loader: Missing sound: '%s'\n", def->pc_speaker_sound_.c_str());
                return false;
            }
            fmt = SoundFilenameToFormat(def->pc_speaker_sound_);
        }
        else // Assume bare name is a lump reference
        {
            int lump = -1;
            lump     = CheckLumpNumberForName(def->pc_speaker_sound_.c_str());
            if (lump < 0)
            {
                // Just write a debug message for SFX lumps; this prevents spam
                // amongst the various IWADs
                DebugOrError("SFX Loader: Missing sound lump: %s\n", def->pc_speaker_sound_.c_str());
                return false;
            }
            F = LoadLumpAsFile(lump);
            EPI_ASSERT(F);
        }
    }
    else
    {
        if (def->pack_name_ != "")
        {
            F = OpenFileFromPack(def->pack_name_);
            if (!F)
            {
                DebugOrError("SFX Loader: Missing sound in EPK: '%s'\n", def->pack_name_.c_str());
                return false;
            }
            fmt = SoundFilenameToFormat(def->pack_name_);
        }
        else if (def->file_name_ != "")
        {
            // Why is this composed with the app dir? - Dasho
            std::string fn = epi::PathAppendIfNotAbsolute(game_directory, def->file_name_);
            F              = epi::FileOpen(fn, epi::kFileAccessRead | epi::kFileAccessBinary);
            if (!F)
            {
                DebugOrError("SFX Loader: Can't Find File '%s'\n", fn.c_str());
                return false;
            }
            fmt = SoundFilenameToFormat(def->file_name_);
        }
        else
        {
            int lump = -1;
            lump     = CheckLumpNumberForName(def->lump_name_.c_str());
            if (lump < 0)
            {
                // Just write a debug message for SFX lumps; this prevents spam
                // amongst the various IWADs
                DebugOrError("SFX Loader: Missing sound lump: %s\n", def->lump_name_.c_str());
                return false;
            }
            F = LoadLumpAsFile(lump);
            EPI_ASSERT(F);
        }
    }

    // Load the data into the buffer
    int      length = F->GetLength();
    uint8_t *data   = F->LoadIntoMemory();

    // no longer need the epi::File
    delete F;
    F = nullptr;

    if (!data)
    {
        WarningOrError("SFX Loader: Error loading data.\n");
        return false;
    }
    if (length < 4)
    {
        delete[] data;
        WarningOrError("SFX Loader: Ignored short data (%d bytes).\n", length);
        return false;
    }

    if ((pc_speaker_mode && epi::GetExtension(def->pc_speaker_sound_).empty()) ||
        (def->pack_name_ == "" && def->file_name_ == ""))
    {
        // for lumps, we must detect the format from the lump contents
        fmt = DetectSoundFormat(data, length);
    }

    bool OK = false;

    switch (fmt)
    {
#if EDGE_WAV_SUPPORT
    case kSoundWAV:
        OK = LoadWav(buf, data, length);
        break;
#endif
#if EDGE_OGG_SUPPORT
    case kSoundOGG:
        OK = LoadOGG(buf, data, length);
        break;
#endif
#if EDGE_MP3_SUPPORT
    case kSoundMP3:
        OK = LoadMP3(buf, data, length);
        break;
#endif
#if EDGE_DOOM_SFX_SUPPORT
    // Double-check first byte here because pack filename detection could
    // return kSoundPCSpeaker for either
    case kSoundPCSpeaker:
        if (data[0] == 0x3)
            OK = LoadDoom(buf, data, length);
        else
            OK = LoadPCSpeaker(buf, data, length);
        break;

    case kSoundDoom:
        OK = LoadDoom(buf, data, length);
        break;
#endif
    default:
        OK = false;
        break;
    }

    delete[] data;

    // Tag sound as SFX for environmental effects - Dasho
    if (OK)
        buf->is_sound_effect_ = true;

    return OK;
}

SoundData *SoundCacheLoad(SoundEffectDefinition *def)
{
    bool pc_speaker_skip = false;

    if (pc_speaker_mode)
    {
        if (def->pc_speaker_sound_.empty())
            pc_speaker_skip = true;
    }

    for (int i = 0; i < (int)sound_effects_cache.size(); i++)
    {
        if (sound_effects_cache[i]->definition_data_ == (void *)def)
        {
            return sound_effects_cache[i];
        }
    }

    // create data structure
    SoundData *buf = new SoundData();

    sound_effects_cache.push_back(buf);

    buf->definition_data_ = def;

    if (pc_speaker_skip)
        LoadSilence(buf);
    else if (!DoCacheLoad(def, buf))
        LoadSilence(buf);

    return buf;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
