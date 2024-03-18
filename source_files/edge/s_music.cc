//----------------------------------------------------------------------------
//  EDGE Music handling Code
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
// -ACB- 1999/11/13 Written
//

#include "s_music.h"

#include <stdlib.h>

#include "ddf_main.h"
#include "dm_state.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_util.h"
#include "i_system.h"
#include "m_misc.h"
#include "s_flac.h"
#include "s_fluid.h"
#include "s_m4p.h"
#include "s_mp3.h"
#include "s_ogg.h"
#include "s_opl.h"
#include "s_rad.h"
#include "s_sound.h"
#include "snd_types.h"
#include "w_files.h"
#include "w_wad.h"

// music slider value
EDGE_DEFINE_CONSOLE_VARIABLE(music_volume, "0.15", kConsoleVariableFlagArchive)

bool no_music = false;

// Current music handle
static AbstractMusicPlayer *music_player;

int         entry_playing = -1;
static bool entry_looped;
bool        pc_speaker_mode = false;

void ChangeMusic(int entry_number, bool loop)
{
    if (no_music)
        return;

    // -AJA- playlist number 0 reserved to mean "no music"
    if (entry_number <= 0)
    {
        StopMusic();
        return;
    }

    // -AJA- don't restart the current song (DOOM compatibility)
    if (entry_number == entry_playing && entry_looped)
        return;

    StopMusic();

    entry_playing = entry_number;
    entry_looped  = loop;

    // when we cannot find the music entry, no music will play
    const PlaylistEntry *play = playlist.Find(entry_number);
    if (!play)
    {
        LogWarning("Could not find music entry [%d]\n", entry_number);
        return;
    }

    // open the file or lump, and read it into memory
    epi::File *F;

    switch (play->infotype_)
    {
    case kDdfMusicDataFile: {
        std::string fn = epi::PathAppendIfNotAbsolute(game_directory, play->info_);

        F = epi::FileOpen(fn, epi::kFileAccessRead | epi::kFileAccessBinary);
        if (!F)
        {
            LogWarning("ChangeMusic: Can't Find File '%s'\n", fn.c_str());
            return;
        }
        break;
    }

    case kDdfMusicDataPackage: {
        F = OpenFileFromPack(play->info_);
        if (!F)
        {
            LogWarning("ChangeMusic: PK3 entry '%s' not found.\n", play->info_.c_str());
            return;
        }
        break;
    }

    case kDdfMusicDataLump: {
        int lump = CheckLumpNumberForName(play->info_.c_str());
        if (lump < 0)
        {
            LogWarning("ChangeMusic: LUMP '%s' not found.\n", play->info_.c_str());
            return;
        }

        F = LoadLumpAsFile(lump);
        break;
    }

    default:
        LogPrint("ChangeMusic: invalid method %d for MUS/MIDI\n", play->infotype_);
        return;
    }

    int      length = F->GetLength();
    uint8_t *data   = F->LoadIntoMemory();

    if (!data)
    {
        delete F;
        LogWarning("ChangeMusic: Error loading data.\n");
        return;
    }
    if (length < 4)
    {
        delete F;
        delete data;
        LogPrint("ChangeMusic: ignored short data (%d bytes)\n", length);
        return;
    }

    SoundFormat fmt = kSoundUnknown;

    // IMF Music is the outlier in that it must be predefined in DDFPLAY with
    // the appropriate IMF frequency, as there is no way of determining this
    // from file information alone
    if (play->type_ == kDdfMusicIMF280 || play->type_ == kDdfMusicIMF560 || play->type_ == kDdfMusicIMF700)
        fmt = kSoundImf;
    else
    {
        if (play->infotype_ == kDdfMusicDataLump)
        {
            // lumps must use auto-detection based on their contents
            fmt = DetectSoundFormat(data, length);
        }
        else
        {
            // for FILE and PACK, use the file extension
            fmt = SoundFilenameToFormat(play->info_);
        }
    }

    // NOTE: players are responsible for freeing 'data'

    switch (fmt)
    {
    case kSoundOgg:
        delete F;
        music_player = PlayOggMusic(data, length, loop);
        break;

    case kSoundMp3:
        delete F;
        music_player = PlayMp3Music(data, length, loop);
        break;

    case kSoundFlac:
        delete F;
        music_player = PlayFlacMusic(data, length, loop);
        break;

    case kSoundM4p:
        delete F;
        music_player = S_PlayM4PMusic(data, length, loop);
        break;

    case kSoundRad:
        delete F;
        music_player = PlayRadMusic(data, length, loop);
        break;

    // IMF writes raw OPL registers, so must use the OPL player
    // unconditionally
    case kSoundImf:
        delete F;
        music_player = PlayOplMusic(data, length, loop, play->type_);
        break;

    case kSoundMidi:
    case kSoundMus:
    case kSoundWav: // RIFF MIDI has the same header as WAV
        delete F;
        if (var_midi_player == 0)
        {
            music_player = PlayFluidMusic(data, length, loop);
        }
        else
        {
            music_player = PlayOplMusic(data, length, loop, play->type_);
        }
        break;

    default:
        delete F;
        delete data;
        LogPrint("ChangeMusic: unknown format\n");
        break;
    }
}

void ResumeMusic(void)
{
    if (music_player)
        music_player->Resume();
}

void PauseMusic(void)
{
    if (music_player)
        music_player->Pause();
}

void StopMusic(void)
{
    // You can't stop the rock!! This does...

    if (music_player)
    {
        music_player->Stop();
        delete music_player;
        music_player = nullptr;
    }

    entry_playing = -1;
    entry_looped  = false;
}

void MusicTicker(void)
{
    if (music_player)
        music_player->Ticker();
}
//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
