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
#include "file.h"
#include "filesystem.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_fluid.h"
#include "s_opl.h"
#include "s_sound.h"
#include "str_compare.h"
#include "str_util.h"
#include "w_wad.h"

// If true, sound system is off/not working. Changed to false if sound init ok.
bool no_sound = false;

static SDL_AudioSpec sound_device_check;
SDL_AudioDeviceID    current_sound_device;

int  sound_device_frequency;
int  sound_device_bytes_per_sample;
int  sound_device_samples_per_buffer;
bool sound_device_stereo;

static bool audio_is_locked = false;

std::vector<std::string> available_soundfonts;
std::vector<std::string> available_opl_banks;
extern std::string       game_directory;
extern std::string       home_directory;
extern ConsoleVariable   midi_soundfont;

void SoundFillCallback(void *udata, Uint8 *stream, int len)
{
    (void)udata;
    SDL_memset(stream, 0, len);
    S_MixAllChannels(stream, len);
}

static bool TryOpenSound(int want_freq, bool want_stereo)
{
    SDL_AudioSpec trydev;
    SDL_zero(trydev);

    LogPrint("StartupSound: trying %d Hz %s\n", want_freq,
             want_stereo ? "Stereo" : "Mono");

    trydev.freq     = want_freq;
    trydev.format   = AUDIO_S16SYS;
    trydev.channels = want_stereo ? 2 : 1;
    trydev.samples  = 1024;
    trydev.callback = SoundFillCallback;

    current_sound_device =
        SDL_OpenAudioDevice(nullptr, 0, &trydev, &sound_device_check, 0);

    if (current_sound_device > 0) return true;

    LogPrint("  failed: %s\n", SDL_GetError());

    return false;
}

void StartupSound(void)
{
    if (no_sound) return;

    std::string driver = ArgumentValue("audiodriver");

    if (driver.empty())
    {
        const char *check = SDL_getenv("SDL_AUDIODRIVER");
        if (check) driver = check;
    }

    if (driver.empty()) driver = "default";

    if (epi::StringCaseCompareASCII(driver, "default") != 0)
    {
        SDL_setenv("SDL_AUDIODRIVER", driver.c_str(), 1);
    }

    LogPrint("SDL_Audio_Driver: %s\n", driver.c_str());

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        LogPrint("StartupSound: Couldn't init SDL AUDIO! %s\n",
                 SDL_GetError());
        no_sound = true;
        return;
    }

    int  want_freq   = 44100;
    bool want_stereo = (var_sound_stereo >= 1);

    if (ArgumentFind("mono") > 0) want_stereo = false;
    if (ArgumentFind("stereo") > 0) want_stereo = true;

    bool success = false;

    if (TryOpenSound(want_freq, want_stereo)) success = true;

    if (!success)
    {
        LogPrint("StartupSound: Unable to find a working sound mode!\n");
        no_sound = true;
        return;
    }

    // These checks shouldn't really fail, as SDL2 allows us to force our
    // desired format and convert silently if needed, but they might end up
    // being a good safety net - Dasho

    if (sound_device_check.format != AUDIO_S16SYS)
    {
        LogPrint("StartupSound: unsupported format: %d\n",
                 sound_device_check.format);
        SDL_CloseAudioDevice(current_sound_device);
        no_sound = true;
        return;
    }

    if (sound_device_check.channels >= 3)
    {
        LogPrint("StartupSound: unsupported channel num: %d\n",
                 sound_device_check.channels);
        SDL_CloseAudioDevice(current_sound_device);

        no_sound = true;
        return;
    }

    if (want_stereo && sound_device_check.channels != 2)
        LogPrint("StartupSound: stereo sound not available.\n");
    else if (!want_stereo && sound_device_check.channels != 1)
        LogPrint("StartupSound: mono sound not available.\n");

    if (sound_device_check.freq < (want_freq - want_freq / 100) ||
        sound_device_check.freq > (want_freq + want_freq / 100))
    {
        LogPrint("StartupSound: %d Hz sound not available.\n", want_freq);
    }

    sound_device_bytes_per_sample = (sound_device_check.channels) * 2;
    sound_device_samples_per_buffer =
        sound_device_check.size / sound_device_bytes_per_sample;

    SYS_ASSERT(sound_device_bytes_per_sample > 0);
    SYS_ASSERT(sound_device_samples_per_buffer > 0);

    sound_device_frequency = sound_device_check.freq;
    sound_device_stereo    = (sound_device_check.channels == 2);

    // update Sound Options menu
    if (sound_device_stereo != (var_sound_stereo >= 1))
        var_sound_stereo = sound_device_stereo ? 1 : 0;

    // display some useful stuff
    LogPrint("StartupSound: Success @ %d Hz %s\n", sound_device_frequency,
             sound_device_stereo ? "Stereo" : "Mono");

    return;
}

void ShutdownSound(void)
{
    if (no_sound) return;

    S_Shutdown();

    no_sound = true;

    SDL_CloseAudioDevice(current_sound_device);
}

void LockAudio(void)
{
    if (audio_is_locked)
    {
        UnlockAudio();
        FatalError("LockAudio: called twice without unlock!\n");
    }

    SDL_LockAudioDevice(current_sound_device);
    audio_is_locked = true;
}

void UnlockAudio(void)
{
    if (audio_is_locked)
    {
        SDL_UnlockAudioDevice(current_sound_device);
        audio_is_locked = false;
    }
}

void StartupMusic(void)
{
    // Check for soundfonts and instrument banks
    std::vector<epi::DirectoryEntry> sfd;
    std::string soundfont_dir = epi::PathAppend(game_directory, "soundfont");

    // Always add the default/internal GENMIDI lump choice
    available_opl_banks.push_back("GENMIDI");
    // Set default SF2 location in CVAR if needed
    if (midi_soundfont.s_.empty())
        midi_soundfont =
            epi::SanitizePath(epi::PathAppend(soundfont_dir, "Default.sf2"));

    if (!ReadDirectory(sfd, soundfont_dir, "*.*"))
    {
        LogWarning("StartupMusic: Failed to read '%s' directory!\n",
                  soundfont_dir.c_str());
    }
    else
    {
        for (size_t i = 0; i < sfd.size(); i++)
        {
            if (!sfd[i].is_dir)
            {
                std::string ext = epi::GetExtension(sfd[i].name);
                epi::StringLowerASCII(ext);
                if (ext == ".sf2")
                {
                    available_soundfonts.push_back(
                        epi::SanitizePath(sfd[i].name));
                }
                else if (ext == ".op2" || ext == ".ad" || ext == ".opl" ||
                         ext == ".tmb")
                    available_opl_banks.push_back(
                        epi::SanitizePath(sfd[i].name));
            }
        }
    }

    if (home_directory != game_directory)
    {
        // Check home_directory soundfont folder as well; create it if it
        // doesn't exist (home_directory only)
        sfd.clear();
        soundfont_dir = epi::PathAppend(home_directory, "soundfont");
        if (!epi::IsDirectory(soundfont_dir)) epi::MakeDirectory(soundfont_dir);

        if (!ReadDirectory(sfd, soundfont_dir, "*.*"))
        {
            LogWarning("StartupMusic: Failed to read '%s' directory!\n",
                      soundfont_dir.c_str());
        }
        else
        {
            for (size_t i = 0; i < sfd.size(); i++)
            {
                if (!sfd[i].is_dir)
                {
                    std::string ext = epi::GetExtension(sfd[i].name);
                    epi::StringLowerASCII(ext);
                    if (ext == ".sf2")
                        available_soundfonts.push_back(
                            epi::SanitizePath(sfd[i].name));
                    else if (ext == ".op2" || ext == ".ad" || ext == ".opl" ||
                             ext == ".tmb")
                        available_opl_banks.push_back(
                            epi::SanitizePath(sfd[i].name));
                }
            }
        }
    }

    if (!S_StartupFluid()) fluid_disabled = true;

    if (!S_StartupOPL()) opl_disabled = true;

    return;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
