//----------------------------------------------------------------------------
//  EDGE-Classic WAV Sound Loader
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022  The EDGE-Classic Community
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

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "sound_gather.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_wav.h"
#include "w_wad.h"

#define DR_WAV_NO_STDIO
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

extern bool dev_stereo;  // FIXME: encapsulation

bool S_LoadWAVSound(epi::sound_data_c *buf, const byte *data, int length)
{
	drwav wav;

    if (!drwav_init_memory(&wav, data, length, NULL))
    {
		I_Warning("Failed to load WAV sound (corrupt wav?)\n");
 
		return false;
    }

	I_Debugf("WAV SFX Loader: freq %d Hz, %d channels\n",
			 wav.sampleRate, wav.channels);

	if (wav.channels > 2)
	{
		I_Warning("WAV SFX Loader: too many channels: %d\n", wav.channels);

		drwav_uninit(&wav);

		return false;
	}

	if (wav.totalPCMFrameCount <= 0) // I think the initial loading would fail if this were the case, but just as a sanity check - Dasho
	{
		I_Error("WAV SFX Loader: no samples!\n");

		drwav_uninit(&wav);

		return false;
	}

	bool is_stereo = (wav.channels > 1);

	buf->freq = wav.sampleRate;

	epi::sound_gather_c gather;

	s16_t *buffer = gather.MakeChunk(wav.totalPCMFrameCount, is_stereo);

	gather.CommitChunk(drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, buffer));

	if (! gather.Finalise(buf, is_stereo))
		I_Error("WAV SFX Loader: no samples!\n");

	drwav_uninit(&wav);

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
