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

extern bool dev_stereo; // FIXME: encapsulation

// The following structs and PC Speaker Conversion routine are adapted from the SLADE codebase,
// specifically https://github.com/sirjuddington/SLADE/blob/master/src/MainEditor/Conversions.cpp

// -----------------------------------------------------------------------------
// Converts Doom PC speaker sound data [in] to wav format, written to [out].
//
// This code is partly adapted from info found on:
// http://www.shikadi.net/moddingwiki/AudioT_Format and
// http://www.shikadi.net/moddingwiki/Inverse_Frequency_Sound_format
// -----------------------------------------------------------------------------

// Some structs for wav conversion
struct WavChunk
{
    char     id[4];
    uint32_t size;
};

struct WavFmtChunk
{
    WavChunk header;
    uint16_t tag;
    uint16_t channels;
    uint32_t samplerate;
    uint32_t datarate;
    uint16_t blocksize;
    uint16_t bps;
};

// For speaker sound conversion
struct SpkSndHeader
{
    uint16_t zero;
    uint16_t samples;
};

uint8_t *Convert_PCSpeaker(const uint8_t *data, int *length)
{
    static const double   ORIG_RATE     = 140.0;
    static const int      FACTOR        = 315; // 315*140 = 44100
    static const double   FREQ          = 1193181.0;
    static const double   RATE          = (ORIG_RATE * FACTOR);
    static const int      PC_VOLUME     = 20;
    static const uint16_t counters[128] = {
        0,    6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575, 5423, 5279, 5120, 4971, 4830, 4697, 4554,
        4435, 4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519, 3418, 3323, 3224, 3131, 3043, 2960, 2875,
        2794, 2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213, 2153, 2089, 2032, 1975, 1918, 1864, 1810,
        1757, 1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395, 1355, 1316, 1280, 1242, 1207, 1173, 1140,
        1107, 1075, 1045, 1015, 986,  959,  931,  905,  879,  854,  829,  806,  783,  760,  739,  718,
        697,  677,  658,  640,  621,  604,  586,  570,  553,  538,  522,  507,  493,  479,  465,  452,
        439,  427,  415,  403,  391,  380,  369,  359,  348,  339,  329,  319,  310,  302,  293,  285,
        276,  269,  261,  253,  246,  239,  232,  226,  219,  213,  207,  201,  195,  190,  184,  179};

    // --- Read Doom sound ---

    if (*length < 4)
    {
        I_Warning("Invalid PC Speaker Sound\n");
        return NULL;
    }

    SpkSndHeader header;
    memcpy(&header, data, 4);
    size_t numsamples;

    // Format checks
    if (header.zero != 0) // Check for magic number
    {
        I_Warning("Invalid Doom PC Speaker Sound\n");
        return NULL;
    }
    if (header.samples > (*length - 4) || header.samples < 4) // Check for sane values
    {
        I_Warning("Invalid Doom PC Speaker Sound\n");
        return NULL;
    }
    numsamples = header.samples;

    // Read samples
    std::vector<uint8_t> osamples(numsamples);
    std::vector<uint8_t> nsamples(numsamples * FACTOR);
    memcpy(osamples.data(), data + sizeof(SpkSndHeader), numsamples);

    int      sign      = -1;
    uint32_t phase_tic = 0;

    // Convert counter values to sample values
    for (size_t s = 0; s < numsamples; ++s)
    {
        if (osamples[s] > 127)
        {
            I_Warning("Invalid PC Speaker counter value: %d > 127", osamples[s]);
            return NULL;
        }
        if (osamples[s] > 0)
        {
            // First, convert counter value to frequency in Hz
            // double f = FREQ / (double)counters[osamples[s]];
            uint32_t tone         = counters[osamples[s]];
            uint32_t phase_length = (tone * RATE) / (2 * FREQ);

            // Then write a bunch of samples.
            for (int i = 0; i < FACTOR; ++i)
            {
                // Finally, convert frequency into sample value
                int pos       = (s * FACTOR) + i;
                nsamples[pos] = 128 + sign * PC_VOLUME;
                if (phase_tic++ >= phase_length)
                {
                    sign      = -sign;
                    phase_tic = 0;
                }
            }
        }
        else
        {
            memset(nsamples.data() + size_t(s * FACTOR), 128, FACTOR);
            phase_tic = 0;
        }
    }

    // --- Write WAV ---

    WavChunk    whdr, wdhdr;
    WavFmtChunk fmtchunk;

    // Setup data header
    char did[4] = {'d', 'a', 't', 'a'};
    memcpy(&wdhdr.id, &did, 4);
    wdhdr.size = numsamples * FACTOR;

    // Setup fmt chunk
    char fid[4] = {'f', 'm', 't', ' '};
    memcpy(&fmtchunk.header.id, &fid, 4);
    fmtchunk.header.size = 16;
    fmtchunk.tag         = 1;
    fmtchunk.channels    = 1;
    fmtchunk.samplerate  = RATE;
    fmtchunk.datarate    = RATE;
    fmtchunk.blocksize   = 1;
    fmtchunk.bps         = 8;

    // Setup main header
    char wid[4] = {'R', 'I', 'F', 'F'};
    memcpy(&whdr.id, &wid, 4);
    whdr.size = wdhdr.size + fmtchunk.header.size + 20;

    // Write chunks

    *length             = 20 + sizeof(WavFmtChunk) + (numsamples * FACTOR);
    int   write_counter = 0;
    uint8_t *new_data      = new uint8_t[*length];
    memcpy(new_data + write_counter, &whdr, 8);
    write_counter += 8;
    char wave[4] = {'W', 'A', 'V', 'E'};
    memcpy(new_data + write_counter, wave, 4);
    write_counter += 4;
    memcpy(new_data + write_counter, &fmtchunk, sizeof(WavFmtChunk));
    write_counter += sizeof(WavFmtChunk);
    memcpy(new_data + write_counter, &wdhdr, 8);
    write_counter += 8;
    memcpy(new_data + write_counter, nsamples.data(), numsamples * FACTOR);
    write_counter += numsamples * FACTOR;

    return new_data;
}

bool S_LoadWAVSound(sound_data_c *buf, uint8_t *data, int length, bool pc_speaker)
{
    drwav wav;

    if (pc_speaker)
        data = Convert_PCSpeaker(data, &length);

    if (!drwav_init_memory(&wav, data, length, nullptr))
    {
        I_Warning("Failed to load WAV sound (corrupt wav?)\n");
        return false;
    }

    if (wav.channels > 2)
    {
        I_Warning("WAV SFX Loader: too many channels: %d\n", wav.channels);
        drwav_uninit(&wav);
        return false;
    }

    if (wav.totalPCMFrameCount <=
        0) // I think the initial loading would fail if this were the case, but just as a sanity check - Dasho
    {
        I_Warning("WAV SFX Loader: no samples!\n");
        drwav_uninit(&wav);
        return false;
    }

    I_Debugf("WAV SFX Loader: freq %d Hz, %d channels\n", wav.sampleRate, wav.channels);

    bool is_stereo = (wav.channels > 1);

    buf->freq = wav.sampleRate;

    sound_gather_c gather;

    int16_t *buffer = gather.MakeChunk(wav.totalPCMFrameCount, is_stereo);

    gather.CommitChunk(drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, buffer));

    if (!gather.Finalise(buf, is_stereo))
        I_Warning("WAV SFX Loader: no samples!\n");

    drwav_uninit(&wav);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
