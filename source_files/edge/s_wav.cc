//----------------------------------------------------------------------------
//  EDGE WAV Sound Loader
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#include "s_wav.h"

#include "dr_wav.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "s_blit.h"
#include "s_cache.h"
#include "snd_gather.h"
#include "w_wad.h"

bool LoadWAVSound(SoundData *buf, uint8_t *data, int length)
{
    drwav wav;

    if (!drwav_init_memory(&wav, data, length, nullptr))
    {
        LogWarning("Failed to load WAV sound (corrupt wav?)\n");
        return false;
    }

    if (wav.channels > 2)
    {
        LogWarning("WAV SFX Loader: too many channels: %d\n", wav.channels);
        drwav_uninit(&wav);
        return false;
    }

    if (wav.totalPCMFrameCount <= 0) // I think the initial loading would fail if this were the case, but
                                     // just as a sanity check - Dasho
    {
        LogWarning("WAV SFX Loader: no samples!\n");
        drwav_uninit(&wav);
        return false;
    }

    LogDebug("WAV SFX Loader: freq %d Hz, %d channels\n", wav.sampleRate, wav.channels);

    bool is_stereo = (wav.channels > 1);

    buf->frequency_ = wav.sampleRate;

    SoundGatherer gather;

    int16_t *buffer = gather.MakeChunk(wav.totalPCMFrameCount, is_stereo);

    gather.CommitChunk(drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, buffer));

    if (!gather.Finalise(buf))
        LogWarning("WAV SFX Loader: no samples!\n");

    drwav_uninit(&wav);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
