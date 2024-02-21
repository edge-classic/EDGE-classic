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

#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#ifdef _MSC_VER
#include <time.h>
#else
#include <sys/time.h>
#endif

#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "i_system.h"
#include "str_util.h"
#include "str_compare.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"
#include "s_opl.h"
#include "s_fluid.h"
#include "w_wad.h"

// If true, sound system is off/not working. Changed to false if sound init ok.
bool nosound = false;

static SDL_AudioSpec mydev;
SDL_AudioDeviceID    mydev_id;

int  dev_freq;
int  dev_bytes_per_sample;
int  dev_frag_pairs;
bool dev_stereo;

static bool audio_is_locked = false;

std::vector<std::string> available_soundfonts;
std::vector<std::string> available_genmidis;
extern std::string       game_dir;
extern std::string       home_dir;
extern ConsoleVariable                      s_soundfont;

void SoundFill_Callback(void *udata, Uint8 *stream, int len)
{
    (void)udata;
    SDL_memset(stream, 0, len);
    S_MixAllChannels(stream, len);
}

static bool I_TryOpenSound(int want_freq, bool want_stereo)
{
    SDL_AudioSpec trydev;
    SDL_zero(trydev);

    I_Printf("I_StartupSound: trying %d Hz %s\n", want_freq, want_stereo ? "Stereo" : "Mono");

    trydev.freq     = want_freq;
    trydev.format   = AUDIO_S16SYS;
    trydev.channels = want_stereo ? 2 : 1;
    trydev.samples  = 1024;
    trydev.callback = SoundFill_Callback;

    mydev_id = SDL_OpenAudioDevice(nullptr, 0, &trydev, &mydev, 0);

    if (mydev_id > 0)
        return true;

    I_Printf("  failed: %s\n", SDL_GetError());

    return false;
}

void I_StartupSound(void)
{
    if (nosound)
        return;

    std::string driver = argv::Value("audiodriver");

    if (driver.empty())
    {
        const char *check = SDL_getenv("SDL_AUDIODRIVER");
        if (check)
            driver = check;
    }

    if (driver.empty())
        driver = "default";

    if (epi::StringCaseCompareASCII(driver, "default") != 0)
    {
        SDL_setenv("SDL_AUDIODRIVER", driver.c_str(), 1);
    }

    I_Printf("SDL_Audio_Driver: %s\n", driver.c_str());

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        I_Printf("I_StartupSound: Couldn't init SDL AUDIO! %s\n", SDL_GetError());
        nosound = true;
        return;
    }

    int  want_freq   = 44100;
    bool want_stereo = (var_sound_stereo >= 1);

    if (argv::Find("mono") > 0)
        want_stereo = false;
    if (argv::Find("stereo") > 0)
        want_stereo = true;

    bool success = false;

    if (I_TryOpenSound(want_freq, want_stereo))
        success = true;

    if (!success)
    {
        I_Printf("I_StartupSound: Unable to find a working sound mode!\n");
        nosound = true;
        return;
    }

    // These checks shouldn't really fail, as SDL2 allows us to force our desired format and convert silently if needed,
    // but they might end up being a good safety net - Dasho

    if (mydev.format != AUDIO_S16SYS)
    {
        I_Printf("I_StartupSound: unsupported format: %d\n", mydev.format);
        SDL_CloseAudioDevice(mydev_id);
        nosound = true;
        return;
    }

    if (mydev.channels >= 3)
    {
        I_Printf("I_StartupSound: unsupported channel num: %d\n", mydev.channels);
        SDL_CloseAudioDevice(mydev_id);

        nosound = true;
        return;
    }

    if (want_stereo && mydev.channels != 2)
        I_Printf("I_StartupSound: stereo sound not available.\n");
    else if (!want_stereo && mydev.channels != 1)
        I_Printf("I_StartupSound: mono sound not available.\n");

    if (mydev.freq < (want_freq - want_freq / 100) || mydev.freq > (want_freq + want_freq / 100))
    {
        I_Printf("I_StartupSound: %d Hz sound not available.\n", want_freq);
    }

    dev_bytes_per_sample = (mydev.channels) * 2;
    dev_frag_pairs       = mydev.size / dev_bytes_per_sample;

    SYS_ASSERT(dev_bytes_per_sample > 0);
    SYS_ASSERT(dev_frag_pairs > 0);

    dev_freq   = mydev.freq;
    dev_stereo = (mydev.channels == 2);

    // update Sound Options menu
    if (dev_stereo != (var_sound_stereo >= 1))
        var_sound_stereo = dev_stereo ? 1 : 0;

    // display some useful stuff
    I_Printf("I_StartupSound: Success @ %d Hz %s\n", dev_freq, dev_stereo ? "Stereo" : "Mono");

    return;
}

void I_ShutdownSound(void)
{
    if (nosound)
        return;

    S_Shutdown();

    nosound = true;

    SDL_CloseAudioDevice(mydev_id);
}

void I_LockAudio(void)
{
    if (audio_is_locked)
    {
        I_UnlockAudio();
        I_Error("I_LockAudio: called twice without unlock!\n");
    }

    SDL_LockAudioDevice(mydev_id);
    audio_is_locked = true;
}

void I_UnlockAudio(void)
{
    if (audio_is_locked)
    {
        SDL_UnlockAudioDevice(mydev_id);
        audio_is_locked = false;
    }
}

void I_StartupMusic(void)
{
    // Check for soundfonts and instrument banks
    std::vector<epi::DirectoryEntry> sfd;
    std::string         soundfont_dir = epi::PathAppend(game_dir, "soundfont");

    // Always add the default/internal GENMIDI lump choice
    available_genmidis.push_back("GENMIDI");
    // Set default SF2 location in CVAR if needed
    if (s_soundfont.s_.empty())
        s_soundfont = epi::SanitizePath(epi::PathAppend(soundfont_dir, "Default.sf2"));

    if (!ReadDirectory(sfd, soundfont_dir, "*.*"))
    {
        I_Warning("I_StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
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
                    available_soundfonts.push_back(epi::SanitizePath(sfd[i].name));
                }
                else if (ext == ".op2" || ext == ".ad" || ext == ".opl" || ext == ".tmb")
                    available_genmidis.push_back(epi::SanitizePath(sfd[i].name));
            }
        }
    }

    if (home_dir != game_dir)
    {
        // Check home_dir soundfont folder as well; create it if it doesn't exist (home_dir only)
        sfd.clear();
        soundfont_dir = epi::PathAppend(home_dir, "soundfont");
        if (!epi::IsDirectory(soundfont_dir))
            epi::MakeDirectory(soundfont_dir);

        if (!ReadDirectory(sfd, soundfont_dir, "*.*"))
        {
            I_Warning("I_StartupMusic: Failed to read '%s' directory!\n", soundfont_dir.c_str());
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
                        available_soundfonts.push_back(epi::SanitizePath(sfd[i].name));
                    else if (ext == ".op2" || ext == ".ad" || ext == ".opl" || ext == ".tmb")
                        available_genmidis.push_back(epi::SanitizePath(sfd[i].name));
                }
            }
        }
    }

    if (!S_StartupFluid())
        fluid_disabled = true;

    if (!S_StartupOPL())
        opl_disabled = true;

    return;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
