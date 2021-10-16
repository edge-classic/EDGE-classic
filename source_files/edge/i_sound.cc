//----------------------------------------------------------------------------
//  EDGE Sound System for SDL
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

#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/time.h>

#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_cache.h"
#include "s_blit.h"
#include "w_wad.h"


// If true, sound system is off/not working. Changed to false if sound init ok.
bool nosound = false;

/* See m_option.cc for corresponding menu items */
static const int sample_rates[5] = { 11025, 16000, 22050, 32000, 44100 };
static const int sample_bits[2]  = { 8, 16 };


static SDL_AudioSpec mydev;

int dev_freq;
int dev_bits;
int dev_bytes_per_sample;
int dev_frag_pairs;
bool dev_signed;  // S8 vs U8, S16 vs U16
bool dev_stereo;


typedef struct
{
	int freq;
	int bits;
	bool stereo;
}
sound_mode_t;

static sound_mode_t mode_try_list[2*6*2];


#define PRI_NOSOUND   -1
#define PRI_FINISHED  -2

// Error Description
static char errordesc[256] = "FOO";
static char scratcherror[256];

static bool audio_is_locked = false;


void SoundFill_Callback(void *udata, Uint8 *stream, int len)
{
	S_MixAllChannels(stream, len);
}

static bool I_TryOpenSound(const sound_mode_t *mode)
{
	SDL_AudioSpec firstdev;

	int samples = 512;

	if (mode->freq < 18000)
		samples = 256;
	else if (mode->freq >= 40000)
		samples = 1024;

	I_Printf("I_StartupSound: trying %d Hz, %d bit %s\n",
			 mode->freq, mode->bits, mode->stereo ? "Stereo" : "Mono");
	
	firstdev.freq     = mode->freq;
	firstdev.format   = (mode->bits < 12) ? AUDIO_U8 : AUDIO_S16SYS;
	firstdev.channels = mode->stereo ? 2 : 1;
	firstdev.samples  = samples;
	firstdev.callback = SoundFill_Callback;

	if (SDL_OpenAudio(&firstdev, &mydev) >= 0)
		return true;

	I_Printf("  failed: %s\n", SDL_GetError());

	// --- try again, but with the less common formats ---

	firstdev.freq     = mode->freq;
	firstdev.format   = (mode->bits < 12) ? AUDIO_S8 : AUDIO_U16SYS;
	firstdev.channels = mode->stereo ? 2 : 1;
	firstdev.samples  = samples;
	firstdev.callback = SoundFill_Callback;

	if (SDL_OpenAudio(&firstdev, &mydev) >= 0)
		return true;

	return false;
}

static int BuildSoundModeTryList(int want_freq, int want_bits, int want_stereo)
{
	// Ordering:
	//   stereoness has highest priority
	//   frequency has middle priority
	//   bits has lowest priority
	//
	// Returns: number of modes to try.

	int index = 0;

	for (int S = 0; S < 2; S++)
	for (int F = 0; F < 6; F++)
	for (int B = 0; B < 2; B++)
	{
		int cur_freq = want_freq;

		if (F > 0)
		{
			if (want_freq >= 27000)
				cur_freq = sample_rates[5-F];
			else
				cur_freq = sample_rates[(8-F) % 5];

			if (cur_freq == want_freq)
				continue;
		}

		sound_mode_t *mode = &mode_try_list[index];
		index++;

		mode->freq   = cur_freq;
		mode->bits   = (B == 0) ? want_bits : (want_bits < 12) ? 16 : 8;
		mode->stereo = (S == 0) ? want_stereo : !want_stereo;
	}

	return index;
}


void I_StartupSound(void)
{
	if (nosound) return;

	if (M_CheckParm("-waveout"))
		force_waveout = true;

	if (M_CheckParm("-dsound") || M_CheckParm("-nowaveout"))
		force_waveout = false;

	const char *driver = M_GetParm("-audiodriver");

	if (! driver)
		driver = SDL_getenv("SDL_AUDIODRIVER");

	if (! driver)
	{
		driver = "default";

#ifdef WIN32
		if (force_waveout)
			driver = "waveout";
#endif
	}

	if (stricmp(driver, "default") != 0)
	{
		char buffer[200];
		snprintf(buffer, sizeof(buffer), "SDL_AUDIODRIVER=%s", driver);
		SDL_putenv(buffer);
	}

	I_Printf("SDL_Audio_Driver: %s\n", driver);

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		I_Printf("I_StartupSound: Couldn't init SDL AUDIO! %s\n",
				 SDL_GetError());
		nosound = true;
		return;
	}

	const char *p;

	int want_freq = sample_rates[var_sample_rate];
	int want_bits = sample_bits [var_sound_bits];
	bool want_stereo = (var_sound_stereo >= 1);

	p = M_GetParm("-freq");
	if (p)
		want_freq = atoi(p);

	if (M_CheckParm("-sound8")  > 0) want_bits = 8;
	if (M_CheckParm("-sound16") > 0) want_bits = 16;

	if (M_CheckParm("-mono")   > 0) want_stereo = false;
	if (M_CheckParm("-stereo") > 0) want_stereo = true;

	int count = BuildSoundModeTryList(want_freq, want_bits, want_stereo);

	bool success = false;
	
	for (int i=0; i < count; i++)
	{
		if (I_TryOpenSound(mode_try_list + i))
		{
			success = true;
			break;
		}
	}
	
	if (! success)
	{
		I_Printf("I_StartupSound: Unable to find a working sound mode!\n");
		nosound = true;
		return;
	}
#if 0
	// get round SDL's signal handlers
	signal(SIGFPE,  SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
#endif

	// check stuff...

	switch (mydev.format)
	{
		case AUDIO_S16SYS: dev_bits=16; dev_signed=true;  break;
		case AUDIO_U16SYS: dev_bits=16; dev_signed=false; break;

		case AUDIO_S8: dev_bits=8; dev_signed=true;  break;
		case AUDIO_U8: dev_bits=8; dev_signed=false; break;

	    default:				   
			I_Printf("I_StartupSound: unsupported format: %d\n", mydev.format);
			SDL_CloseAudio();

			nosound = true;
			return;
	}

	if (mydev.channels >= 3)
	{
		I_Printf("I_StartupSound: unsupported channel num: %d\n", mydev.channels);
		SDL_CloseAudio();

		nosound = true;
		return;
	}

	if (dev_bits != want_bits)
		I_Printf("I_StartupSound: %d bits not available.\n", want_bits);

	if (want_stereo && mydev.channels != 2)
		I_Printf("I_StartupSound: stereo sound not available.\n");
	else if (!want_stereo && mydev.channels != 1)
		I_Printf("I_StartupSound: mono sound not available.\n");

	if (mydev.freq < (want_freq - want_freq/100) || 
		mydev.freq > (want_freq + want_freq/100))
	{
		I_Printf("I_StartupSound: %d Hz sound not available.\n", want_freq);
	}

	dev_bytes_per_sample = (mydev.channels) * (dev_bits / 8);
	dev_frag_pairs = mydev.size / dev_bytes_per_sample;

	SYS_ASSERT(dev_bytes_per_sample > 0);
	SYS_ASSERT(dev_frag_pairs > 0);

	dev_freq   = mydev.freq;
	dev_stereo = (mydev.channels == 2);

	// update Sound Options menu
	if (dev_bits != sample_bits[var_sound_bits])
		var_sound_bits = (dev_bits >= 16) ? 1 : 0;

	if (dev_stereo != (var_sound_stereo >= 1))
		var_sound_stereo = dev_stereo ? 1 : 0;

	if (dev_freq != sample_rates[var_sample_rate])
	{
		if (dev_freq >= 38000)
			var_sample_rate = 4; // 44 Khz
		else if (dev_freq >= 27000)
			var_sample_rate = 3; // 32 Khz
		else if (dev_freq >= 19000)
			var_sample_rate = 2; // 22 Khz
		else if (dev_freq >= 13500)
			var_sample_rate = 1; // 16 Khz
		else
			var_sample_rate = 0; // 11 Khz
	}

	// display some useful stuff
	I_Printf("I_StartupSound: Success @ %d Hz, %d bit %s\n",
			dev_freq, dev_bits, dev_stereo ? "Stereo" : "Mono");

	return;
}


void I_ShutdownSound(void)
{
	if (nosound)
		return;

	S_Shutdown();

	nosound = true;

	SDL_CloseAudio();
}


const char *I_SoundReturnError(void)
{
	memcpy(scratcherror, errordesc, sizeof(scratcherror));
	memset(errordesc, '\0', sizeof(errordesc));

	return scratcherror;
}


void I_LockAudio(void)
{
	if (audio_is_locked)
	{
		I_UnlockAudio();
		I_Error("I_LockAudio: called twice without unlock!\n");
	}

	SDL_LockAudio();
	audio_is_locked = true;
}

void I_UnlockAudio(void)
{
	if (audio_is_locked)
	{
		SDL_UnlockAudio();
		audio_is_locked = false;
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
