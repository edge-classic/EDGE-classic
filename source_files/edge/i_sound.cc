//----------------------------------------------------------------------------
//  EDGE Sound System for SDL
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

#include "i_sound.h"

#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "miniaudio.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_tsf.h"
#include "s_sound.h"
#include "w_wad.h"

// If true, sound system is off/not working. Changed to false if sound init ok.
bool no_sound = false;

int  sound_device_frequency;
int  sound_device_bytes_per_sample;
int  sound_device_samples_per_buffer;
bool sound_device_stereo;

std::vector<std::string> available_soundfonts;
extern std::string       game_directory;
extern std::string       home_directory;
extern ConsoleVariable   midi_soundfont;

static ma_result sound_result;
static ma_engine_config sound_engine_config;
ma_engine sound_engine;

static bool TryOpenSound(int want_freq, bool want_stereo)
{
    LogPrint("StartupSound: trying %d Hz %s\n", want_freq, want_stereo ? "Stereo" : "Mono");

    sound_engine_config = ma_engine_config_init();
    sound_engine_config.channels = want_stereo ? 2 : 1;
    sound_engine_config.sampleRate = want_freq;
    sound_engine_config.noAutoStart = MA_TRUE;

    sound_result = ma_engine_init(&sound_engine_config, &sound_engine);   

    if (sound_result == MA_SUCCESS)
        return true;

    LogPrint("  failed\n");

    return false;
}

void StartupAudio(void)
{
    if (no_sound)
        return;

    int  want_freq   = 44100;
    bool want_stereo = (var_sound_stereo >= 1);

    if (FindArgument("mono") > 0)
        want_stereo = false;
    if (FindArgument("stereo") > 0)
        want_stereo = true;

    bool success = false;

    if (TryOpenSound(want_freq, want_stereo))
        success = true;

    if (!success)
    {
        LogPrint("StartupSound: Unable to find a working sound mode!\n");
        no_sound = true;
        return;
    }

    sound_device_frequency = want_freq;
    sound_device_stereo    = want_stereo ? true : false;

    // update Sound Options menu
    if (sound_device_stereo != (var_sound_stereo >= 1))
        var_sound_stereo = sound_device_stereo ? 1 : 0;

    // display some useful stuff
    LogPrint("StartupSound: Success @ %d Hz %s\n", sound_device_frequency, sound_device_stereo ? "Stereo" : "Mono");

    return;
}

void AudioShutdown(void)
{
    if (no_sound)
        return;

    ShutdownSound();

    no_sound = true;
}

void StartupMusic(void)
{
    // Check for soundfonts and instrument banks
    std::vector<epi::DirectoryEntry> sfd;
    std::string                      soundfont_dir = epi::PathAppend(game_directory, "soundfont");

    // Set default SF2 location in CVAR if needed
    if (midi_soundfont.s_.empty())
        midi_soundfont = epi::SanitizePath(epi::PathAppend(soundfont_dir, "Default.sf2"));

    if (!ReadDirectory(sfd, soundfont_dir, "*.sf2"))
    {
        LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < sfd.size(); i++)
        {
            if (!sfd[i].is_dir)
                available_soundfonts.push_back(epi::SanitizePath(sfd[i].name));
        }
    }

    if (home_directory != game_directory)
    {
        // Check home_directory soundfont folder as well; create it if it
        // doesn't exist (home_directory only)
        sfd.clear();
        soundfont_dir = epi::PathAppend(home_directory, "soundfont");
        if (!epi::IsDirectory(soundfont_dir))
            epi::MakeDirectory(soundfont_dir);

        if (!ReadDirectory(sfd, soundfont_dir, "*.sf2"))
        {
            LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < sfd.size(); i++)
            {
                if (!sfd[i].is_dir)
                    available_soundfonts.push_back(epi::SanitizePath(sfd[i].name));
            }
        }
    }

    if (!StartupTSF())
        tsf_disabled = true;

    return;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
