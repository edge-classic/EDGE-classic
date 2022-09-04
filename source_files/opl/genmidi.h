//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C)      2022 Andrew Apted
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     MIDI file parsing.
//

#ifndef GENMIDI_INFO_H
#define GENMIDI_INFO_H

#include <inttypes.h>

#define GENMIDI_NUM_INSTRS      128
#define GENMIDI_NUM_PERCUSSION   47
#define GENMIDI_TOTAL_INSTRS    (128 + 47)

#define GENMIDI_HEADER          "#OPL_II#"
#define GENMIDI_FLAG_FIXED      0x0001         /* fixed pitch */
#define GENMIDI_FLAG_2VOICE     0x0004         /* double voice (OPL3) */

#ifdef __GNUC__
#define PACKEDATTR __attribute__((packed))
#else
#define PACKEDATTR
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

// 6 bytes each
typedef struct
{
    uint8_t tremolo;
    uint8_t attack;
    uint8_t sustain;
    uint8_t waveform;
    uint8_t scale;
    uint8_t level;
} PACKEDATTR genmidi_op_t;

// 16 bytes each
typedef struct
{
    genmidi_op_t modulator;
    uint8_t      feedback;
    genmidi_op_t carrier;
    uint8_t      __pad;
    int16_t      base_note_offset;
} PACKEDATTR genmidi_voice_t;

// 36 bytes each
typedef struct
{
    uint16_t flags;
    uint8_t  fine_tuning;
    uint8_t  fixed_note;

    genmidi_voice_t voices[2];
} PACKEDATTR genmidi_instr_t;

typedef struct
{
    uint8_t         magic[8];
    genmidi_instr_t instrs[GENMIDI_TOTAL_INSTRS];
} PACKEDATTR genmidi_lump_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

bool GM_LoadInstruments(const uint8_t *data, size_t length);

genmidi_instr_t * GM_GetInstrument(int key);
genmidi_instr_t * GM_GetPercussion(int key);

#endif /* #ifndef GENMIDI_INFO_H */
