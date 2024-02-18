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

#include "i_defs.h"

#include <vector>

#include "file.h"
#include "filesystem.h"
#include "sound_data.h"
#include "sound_types.h"
#include "str_util.h"

#include "main.h"
#include "sfx.h"

#include "s_sound.h"
#include "s_cache.h"
#include "s_ogg.h"
#include "s_mp3.h"
#include "s_wav.h"

#include "dm_state.h" // game_dir
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "w_files.h"
#include "w_wad.h"

extern int  dev_freq;
extern bool var_pc_speaker_mode;

static std::vector<sound_data_c *> fx_cache;

static void Load_Silence(sound_data_c *buf)
{
    int length = 256;

    buf->freq = dev_freq;
    buf->Allocate(length, SBUF_Mono);

    memset(buf->data_L, 0, length * sizeof(int16_t));
}

static bool Load_DOOM(sound_data_c *buf, const uint8_t *lump, int length)
{
    buf->freq = lump[2] + (lump[3] << 8);

    if (buf->freq < 8000 || buf->freq > 48000)
        I_Warning("Sound Load: weird frequency: %d Hz\n", buf->freq);

    if (buf->freq < 4000)
        buf->freq = 4000;

    length -= 8;

    if (length <= 0)
        return false;

    buf->Allocate(length, SBUF_Mono);

    // convert to signed 16-bit format
    const uint8_t *src   = lump + 8;
    const uint8_t *s_end = src + length;

    int16_t *dest = buf->data_L;

    for (; src < s_end; src++)
        *dest++ = (*src ^ 0x80) << 8;

    return true;
}

static bool Load_WAV(sound_data_c *buf, uint8_t *lump, int length, bool pc_speaker)
{
    return S_LoadWAVSound(buf, lump, length, pc_speaker);
}

static bool Load_OGG(sound_data_c *buf, const uint8_t *lump, int length)
{
    return S_LoadOGGSound(buf, lump, length);
}

static bool Load_MP3(sound_data_c *buf, const uint8_t *lump, int length)
{
    return S_LoadMP3Sound(buf, lump, length);
}

//----------------------------------------------------------------------------

void S_CacheInit(void)
{
    // nothing to do
}

void S_FlushData(sound_data_c *fx)
{
    SYS_ASSERT(fx->ref_count == 0);

    fx->Free();
}

void S_CacheClearAll(void)
{
    for (int i = 0; i < (int)fx_cache.size(); i++)
        delete fx_cache[i];

    fx_cache.erase(fx_cache.begin(), fx_cache.end());
}

static bool DoCacheLoad(SoundEffectDefinition *def, sound_data_c *buf)
{
    // open the file or lump, and read it into memory
    epi::File        *F;
    sound_format_e fmt = kUnknownImage;

    if (var_pc_speaker_mode)
    {
        if (epi::GetExtension(def->pc_speaker_sound_).empty())
        {
            F = W_OpenPackFile(def->pc_speaker_sound_);
            if (!F)
            {
                std::string open_name = M_ComposeFileName(game_dir, def->pc_speaker_sound_);
                F = epi::FileOpen(open_name, epi::kFileAccessRead | epi::kFileAccessBinary);
            }
            if (!F)
            {
                M_DebugError("SFX Loader: Missing sound: '%s'\n", def->pc_speaker_sound_.c_str());
                return false;
            }
            fmt = Sound_FilenameToFormat(def->pc_speaker_sound_);
        }
        else // Assume bare name is a lump reference
        {
            int lump = -1;
            lump     = W_CheckNumForName(def->pc_speaker_sound_.c_str());
            if (lump < 0)
            {
                // Just write a debug message for SFX lumps; this prevents spam amongst the various IWADs
                M_DebugError("SFX Loader: Missing sound lump: %s\n", def->pc_speaker_sound_.c_str());
                return false;
            }
            F = W_OpenLump(lump);
            SYS_ASSERT(F);
        }
    }
    else
    {
        if (def->pack_name_ != "")
        {
            F = W_OpenPackFile(def->pack_name_);
            if (!F)
            {
                M_DebugError("SFX Loader: Missing sound in EPK: '%s'\n", def->pack_name_.c_str());
                return false;
            }
            fmt = Sound_FilenameToFormat(def->pack_name_);
        }
        else if (def->file_name_ != "")
        {
            // Why is this composed with the app dir? - Dasho
            std::string fn = M_ComposeFileName(game_dir, def->file_name_);
            F  = epi::FileOpen(fn, epi::kFileAccessRead | epi::kFileAccessBinary);
            if (!F)
            {
                M_DebugError("SFX Loader: Can't Find File '%s'\n", fn.c_str());
                return false;
            }
            fmt = Sound_FilenameToFormat(def->file_name_);
        }
        else
        {
            int lump = -1;
            lump     = W_CheckNumForName(def->lump_name_.c_str());
            if (lump < 0)
            {
                // Just write a debug message for SFX lumps; this prevents spam amongst the various IWADs
                M_DebugError("SFX Loader: Missing sound lump: %s\n", def->lump_name_.c_str());
                return false;
            }
            F = W_OpenLump(lump);
            SYS_ASSERT(F);
        }
    }

    // Load the data into the buffer
    int   length = F->GetLength();
    uint8_t *data   = F->LoadIntoMemory();

    // no longer need the epi::File
    delete F;
    F = nullptr;

    if (!data)
    {
        M_WarnError("SFX Loader: Error loading data.\n");
        return false;
    }
    if (length < 4)
    {
        delete[] data;
        M_WarnError("SFX Loader: Ignored short data (%d bytes).\n", length);
        return false;
    }

    if ((var_pc_speaker_mode && epi::GetExtension(def->pc_speaker_sound_).empty()) ||
        (def->pack_name_ == "" && def->file_name_ == ""))
    {
        // for lumps, we must detect the format from the lump contents
        fmt = Sound_DetectFormat(data, length);
    }

    bool OK = false;

    switch (fmt)
    {
    case FMT_WAV:
        OK = Load_WAV(buf, data, length, false);
        break;

    case FMT_OGG:
        OK = Load_OGG(buf, data, length);
        break;

    case FMT_MP3:
        OK = Load_MP3(buf, data, length);
        break;

    // Double-check first byte here because pack filename detection could
    // return FMT_SPK for either
    case FMT_SPK:
        if (data[0] == 0x3)
            OK = Load_DOOM(buf, data, length);
        else
            OK = Load_WAV(buf, data, length, true);
        break;

    case FMT_DOOM:
        OK = Load_DOOM(buf, data, length);
        break;

    default:
        OK = false;
        break;
    }

    // Tag sound as SFX for environmental effects - Dasho
    if (OK)
        buf->is_sfx = true;

    return OK;
}

sound_data_c *S_CacheLoad(SoundEffectDefinition *def)
{
    bool pc_speaker_skip = false;

    if (var_pc_speaker_mode)
    {
        if (def->pc_speaker_sound_.empty())
            pc_speaker_skip = true;
    }

    for (int i = 0; i < (int)fx_cache.size(); i++)
    {
        if (fx_cache[i]->priv_data == (void *)def)
        {
            fx_cache[i]->ref_count++;
            return fx_cache[i];
        }
    }

    // create data structure
    sound_data_c *buf = new sound_data_c();

    fx_cache.push_back(buf);

    buf->priv_data = def;
    buf->ref_count = 1;

    if (pc_speaker_skip)
        Load_Silence(buf);
    else if (!DoCacheLoad(def, buf))
        Load_Silence(buf);

    return buf;
}

void S_CacheRelease(sound_data_c *data)
{
    SYS_ASSERT(data->ref_count >= 1);

    data->ref_count--;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
