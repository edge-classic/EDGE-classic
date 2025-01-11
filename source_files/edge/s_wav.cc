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

#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "miniaudio.h"
#include "s_blit.h"
#include "s_cache.h"
#include "snd_gather.h"
#include "w_wad.h"

bool LoadWAVSound(SoundData *buf, uint8_t *data, int length)
{
    ma_decoder_config decode_config = ma_decoder_config_init_default();
    decode_config.format = ma_format_f32;
    ma_decoder decode;

    //ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);

    if (ma_decoder_init_memory(data, length, &decode_config, &decode) != MA_SUCCESS)
    {
        LogWarning("Failed to load WAV sound (corrupt wav?)\n");
        return false;
    }

    if (decode.outputChannels > 2)
    {
        LogWarning("WAV SFX Loader: too many channels: %d\n", decode.outputChannels);
        ma_decoder_uninit(&decode);
        return false;
    }

    ma_uint64 frame_count = 0;

    if (ma_decoder_get_length_in_pcm_frames(&decode, &frame_count) != MA_SUCCESS)
    {
        LogWarning("WAV SFX Loader: no samples!\n");
        ma_decoder_uninit(&decode);
        return false;
    }

    LogDebug("WAV SFX Loader: freq %d Hz, %d channels\n", decode.outputSampleRate, decode.outputChannels);

    bool is_stereo = (decode.outputChannels > 1);

    buf->frequency_ = decode.outputSampleRate;

    SoundGatherer gather;

    float *buffer = gather.MakeChunk(frame_count, is_stereo);

    ma_uint64 frames_read = 0;

    if (ma_decoder_read_pcm_frames(&decode, buffer, frame_count, &frames_read) != MA_SUCCESS)
    {
        LogWarning("WAV SFX Loader: failure loading samples!\n");
        gather.DiscardChunk();
        ma_decoder_uninit(&decode);
        return false;
    }

    gather.CommitChunk(frames_read);

    if (!gather.Finalise(buf))
        LogWarning("WAV SFX Loader: no samples!\n");

    ma_decoder_uninit(&decode);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
