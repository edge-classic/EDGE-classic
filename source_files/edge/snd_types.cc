//------------------------------------------------------------------------
//  Sound Format Detection
//------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023 - The EDGE Team.
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
//------------------------------------------------------------------------

#include "snd_types.h"

#include "epi.h"
#include "filesystem.h"
#include "m4p.h"
#include "str_util.h"

SoundFormat DetectSoundFormat(uint8_t *data, int song_len)
{
    // Start by trying the simple reliable header checks

    if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F')
    {
        return kSoundWav;
    }

    if (data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C')
    {
        return kSoundFlac;
    }

    if (data[0] == 'O' && data[1] == 'g' && data[2] == 'g')
    {
        return kSoundOgg;
    }

    if (data[0] == 'M' && data[1] == 'U' && data[2] == 'S')
    {
        return kSoundMus;
    }

    if (data[0] == 'M' && data[1] == 'T' && data[2] == 'h' && data[3] == 'd')
    {
        return kSoundMidi;
    }

    // XMI MIDI
    if (song_len > 12 && data[0] == 'F' && data[1] == 'O' && data[2] == 'R' &&
        data[3] == 'M' && data[8] == 'X' && data[9] == 'D' && data[10] == 'I' &&
        data[11] == 'R')
    {
        return kSoundMidi;
    }

    // GMF MIDI
    if (data[0] == 'G' && data[1] == 'M' && data[2] == 'F' && data[3] == '\x1')
    {
        return kSoundMidi;
    }

    // Electronic Arts MIDI
    if (song_len > data[0] && data[0] >= 0x5D)
    {
        int offset = data[0] - 0x10;
        if (data[offset] == 'r' && data[offset + 1] == 's' &&
            data[offset + 2] == 'x' && data[offset + 3] == 'x' &&
            data[offset + 4] == '}' && data[offset + 5] == 'u')
            return kSoundMidi;
    }

    // Reality Adlib Tracker 2
    if (song_len > 16)
    {
        bool        is_rad = true;
        const char *hdrtxt = "RAD by REALiTY!!";
        for (int i = 0; i < 16; i++)
        {
            if (data[i] != *hdrtxt++)
            {
                is_rad = false;
                break;
            }
        }
        if (is_rad) return kSoundRad;
    }

    // Moving on to more specialized or less reliable detections

    if (m4p_TestFromData(data, song_len)) { return kSoundM4p; }

    if ((data[0] == 'I' && data[1] == 'D' && data[2] == '3') ||
        (data[0] == 0xFF && ((data[1] >> 4 & 0xF) == 0xF)))
    {
        return kSoundMp3;
    }

    if (data[0] == 0x3) { return kSoundDoom; }

    if (data[0] == 0x0) { return kSoundPcSpeaker; }

    return kSoundUnknown;
}

SoundFormat SoundFilenameToFormat(std::string_view filename)
{
    std::string ext = epi::GetExtension(filename);

    epi::StringLowerASCII(ext);

    if (ext == ".wav" || ext == ".wave") return kSoundWav;

    if (ext == ".flac") return kSoundFlac;

    if (ext == ".ogg") return kSoundOgg;

    if (ext == ".mp3") return kSoundMp3;

    // Test MUS vs EA-MIDI MUS ?
    if (ext == ".mus") return kSoundMus;

    if (ext == ".mid" || ext == ".midi" || ext == ".xmi" || ext == ".rmi" ||
        ext == ".rmid")
        return kSoundMidi;

    if (ext == ".mod" || ext == ".s3m" || ext == ".xm" || ext == ".it")
        return kSoundM4p;

    if (ext == ".rad") return kSoundRad;

    // Not sure if these will ever be encountered in the wild, but according to
    // the VGMPF Wiki they are valid DMX file extensions
    if (ext == ".dsp" || ext == ".pcs" || ext == ".gsp" || ext == ".gsw")
        return kSoundDoom;

    // Will actually result in checking the first byte to further determine if
    // it's Doom or PC Speaker format; the above kSoundDoom stuff is
    // unconditional which is why I didn't throw it up there
    if (ext == ".lmp") return kSoundPcSpeaker;

    return kSoundUnknown;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
