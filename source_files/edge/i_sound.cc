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

#include <set>

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
#include "s_midi.h"
#include "s_sound.h"
#include "w_wad.h"

// If true, sound system is off/not working. Changed to false if sound init ok.
bool no_sound = false;

int sound_device_frequency;

std::set<std::string>       available_soundfonts;
extern std::string          game_directory;
extern std::string          home_directory;
extern ConsoleVariable      midi_soundfont;

ma_engine sound_engine;
ma_engine music_engine;
// Airless/Vacuum SFX sector sounds
ma_lpf_node vacuum_node;
// Underwater sector sounds; these two chain into each other
ma_lpf_node lowpass_node;
ma_delay_node underwater_node;
// Dynamic reverb
ma_delay_node reverb_node;


void StartupAudio(void)
{
    if (no_sound)
        return;

    if (ma_engine_init(NULL, &sound_engine) != MA_SUCCESS)
    {
        LogPrint("StartupSound: Unable to initialize sound engine!\n");
        no_sound = true;
        return;
    }
    else
    {
        sound_device_frequency = ma_engine_get_sample_rate(&sound_engine);
        ma_uint32 channels = ma_engine_get_channels(&sound_engine);
        ma_engine_set_volume(&sound_engine, sound_effect_volume.f_ * 0.25f);
        // configure FX nodes
        ma_delay_node_config delay_node_config = ma_delay_node_config_init(channels, sound_device_frequency, (ma_uint32)(sound_device_frequency * 0.15f), 0.15f);
        ma_delay_node_init(ma_engine_get_node_graph(&sound_engine), &delay_node_config, NULL, &underwater_node);
        ma_lpf_node_config lpf_config = ma_lpf_node_config_init(channels, sound_device_frequency, 400.0f, 2);
        ma_lpf_node_init(ma_engine_get_node_graph(&sound_engine), &lpf_config, NULL, &lowpass_node);
        ma_node_attach_output_bus(&lowpass_node, 0, ma_engine_get_endpoint(&sound_engine), 0);
        ma_node_attach_output_bus(&underwater_node, 0, &lowpass_node, 0);
        lpf_config = ma_lpf_node_config_init(channels, sound_device_frequency, 200.0f, 2);
        ma_lpf_node_init(ma_engine_get_node_graph(&sound_engine), &lpf_config, NULL, &vacuum_node);
        ma_node_attach_output_bus(&vacuum_node, 0, ma_engine_get_endpoint(&sound_engine), 0);
        delay_node_config = ma_delay_node_config_init(channels, sound_device_frequency, (ma_uint32)(sound_device_frequency * 0.15f), 0.25f);
        ma_delay_node_init(ma_engine_get_node_graph(&sound_engine), &delay_node_config, NULL, &reverb_node);
        ma_node_attach_output_bus(&reverb_node, 0, ma_engine_get_endpoint(&sound_engine), 0);
    }

    if (!no_music)
    {
        if (ma_engine_init(NULL, &music_engine) != MA_SUCCESS)
        {
            LogPrint("StartupAudio: Unable to initialize music engine!\n");
            no_music = true;
        }
        else
            ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);
    }

    // display some useful stuff
    LogPrint("StartupSound: Success @ %d Hz, %d channels\n", sound_device_frequency,
             ma_engine_get_channels(&sound_engine));

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
    std::string                      soundfont_dir = epi::PathAppend(home_directory, "soundfont");

    // Add our built-in options first so they take precedence over a soundfont that might 
    // somehow have the same file stem
    available_soundfonts.emplace("Default");
#ifdef EDGE_CLASSIC
    available_soundfonts.emplace("OPL Emulation");
#endif

    // Create home directory soundfont folder if it doesn't aleady exist
    if (!epi::IsDirectory(soundfont_dir))
        epi::MakeDirectory(soundfont_dir);

    sfd.clear();

    if (!ReadDirectory(sfd, soundfont_dir, "*.sf2"))
    {
        LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < sfd.size(); i++)
        {
            if (!sfd[i].is_dir)
            {
                std::string filename = epi::GetStem(sfd[i].name);
                if (!available_soundfonts.count(filename))
                    available_soundfonts.emplace(filename);
            }
        }
    }
    if (!ReadDirectory(sfd, soundfont_dir, "*.sf3"))
    {
        LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < sfd.size(); i++)
        {
            if (!sfd[i].is_dir)
            {
                std::string filename = epi::GetStem(sfd[i].name);
                if (!available_soundfonts.count(filename))
                    available_soundfonts.emplace(filename);
            }
        }
    }

    if (home_directory != game_directory)
    {
        // Read the program directory, but only add names we haven't encountered yet
        sfd.clear();
        soundfont_dir = epi::PathAppend(game_directory, "soundfont");

        if (!ReadDirectory(sfd, soundfont_dir, "*.sf2"))
        {
            LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < sfd.size(); i++)
            {
                if (!sfd[i].is_dir)
                {
                    std::string filename = epi::GetStem(sfd[i].name);
                    if (!available_soundfonts.count(filename))
                        available_soundfonts.emplace(filename);
                }
            }
        }
        if (!ReadDirectory(sfd, soundfont_dir, "*.sf3"))
        {
            LogWarning("StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < sfd.size(); i++)
            {
                if (!sfd[i].is_dir)
                {
                    std::string filename = epi::GetStem(sfd[i].name);
                    if (!available_soundfonts.count(filename))
                        available_soundfonts.emplace(filename);
                }
            }
        }
    }

    if (!StartupMIDI())
        midi_disabled = true;

    return;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
