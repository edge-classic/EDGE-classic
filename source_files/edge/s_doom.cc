//----------------------------------------------------------------------------
//  EDGE Doom/PC Speaker Sound Loader
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024 The EDGE Team.
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

#include "s_doom.h"

#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "s_blit.h"
#include "s_cache.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int  sound_device_frequency;

bool LoadDoomSound(SoundData *buf, uint8_t *data, int length)
{
    buf->frequency_ = data[2] + (data[3] << 8);

    if (buf->frequency_ < 8000 || buf->frequency_ > 48000)
        LogWarning("Sound Load: weird frequency: %d Hz\n", buf->frequency_);

    if (buf->frequency_ < 4000)
        buf->frequency_ = 4000;

    length -= 8;

    if (length <= 0)
        return false;

    buf->Allocate(length);

    // convert to signed 16-bit format
    const uint8_t *src   = data + 8;
    const uint8_t *s_end = src + length;

    float *dest = buf->data_;
    float out = 0;
    
    for (; src < s_end; src++)
    {
        ma_pcm_u8_to_f32(&out, src, 1, ma_dither_mode_none);
        *dest++ = out;
        *dest++ = out;
    }

    return true;
}

static constexpr int kPCInterruptTimer = 1193181;
static constexpr uint8_t kPCVolume = 20;
static constexpr uint8_t kPCRate = 140;
static constexpr uint16_t kFrequencyTable[128] = {
        0,    6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575, 5423, 5279, 5120, 4971, 4830, 4697, 4554,
        4435, 4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519, 3418, 3323, 3224, 3131, 3043, 2960, 2875,
        2794, 2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213, 2153, 2089, 2032, 1975, 1918, 1864, 1810,
        1757, 1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395, 1355, 1316, 1280, 1242, 1207, 1173, 1140,
        1107, 1075, 1045, 1015, 986,  959,  931,  905,  879,  854,  829,  806,  783,  760,  739,  718,
        697,  677,  658,  640,  621,  604,  586,  570,  553,  538,  522,  507,  493,  479,  465,  452,
        439,  427,  415,  403,  391,  380,  369,  359,  348,  339,  329,  319,  310,  302,  293,  285,
        276,  269,  261,  253,  246,  239,  232,  226,  219,  213,  207,  201,  195,  190,  184,  179};

bool LoadPCSpeakerSound(SoundData *buf, uint8_t *data, int length)
{
    if (length < 4)
    {
        LogWarning("Invalid PC Speaker Sound (too short)\n");
        return false;
    }
	int	sign = -1;
	uint32_t tone, i, phase_length,	phase_tic = 0;
	uint32_t samples_per_byte = sound_device_frequency/kPCRate;
    uint16_t zeroed, total_samples;
    memcpy(&zeroed, data, 2);
    memcpy(&total_samples, data+2, 2);
    if (zeroed != 0)
    {
        LogWarning("Invalid PC Speaker Sound (bad magic number)\n");
        return false;
    }
    if (total_samples < 4 || total_samples > length - 4)
    {
        LogWarning("Invalid PC Speaker Sound (bad sample count)\n");
        return false;
    }
    data += 4;
    length -= 4;
    buf->Allocate(length * samples_per_byte);
    float *dst = buf->data_;
	while (length--) 
	{
        if (*data > 128)
        {
            LogWarning("Invalid PC Speaker Sound (bad tone value %d)\n", *data);
            return false;
        }
		tone = kFrequencyTable[*data++];
		phase_length = (sound_device_frequency*tone)/(2*kPCInterruptTimer);
        uint8_t value = 0;
        float float_value = 0;
		for (i=0; i<samples_per_byte; i++)
		{
			if (tone)
			{
                value = (128 + sign * kPCVolume);
                ma_pcm_u8_to_f32(&float_value, &value, 1, ma_dither_mode_none);
				*dst++ = float_value;
                *dst++ = float_value;
				if (phase_tic++ >= phase_length)
				{
					sign = -sign;
					phase_tic = 0;
				}
			} else {
				phase_tic = 0;
				*dst++ = 0;
                *dst++ = 0;
			}
		}
	}
    buf->frequency_ = sound_device_frequency;
    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
