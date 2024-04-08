/*
 * XMIDI: Miles XMIDI to MID Library
 *
 * Copyright (C) 2001  Ryan Nunn
 * Copyright (C) 2014  Bret Curtis
 * Copyright (C) WildMIDI Developers 2015-2016
 * Copyright (c) 2015-2022 Vitaly Novichkov <admin@wohlnet.ru>
 * Copyright (c) 2024 The EDGE Team.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/* XMIDI Converter */

#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

enum XMIConversionType
{
    kXMINoConversion           = 0x00,
    kXMIConvertMt32ToGm        = 0x01,
    kXMIConvertMt32ToGs        = 0x02,
    kXMIConvertMt32ToGs127     = 0x03,
    kXMIConvertMt32ToGs127Drum = 0x04,
    kXMIConvertGs127ToGs       = 0x05
};

enum XMIStatusByte
{
    kXMIStatusNoteOff       = 0x8,
    kXMIStatusNoteOn        = 0x9,
    kXMIStatusAftertouch    = 0xA,
    kXMIStatusController    = 0xB,
    kXMIStatusProgramChange = 0xC,
    kXMIStatusPressure      = 0xD,
    kXMIStatusPitchWheel    = 0xE,
    kXMIStatusSysex         = 0xF
};

struct XMIToMidiEvent
{
    int32_t         time;
    uint8_t         status;
    uint8_t         data[2];
    uint32_t        len;
    uint8_t        *buffer;
    XMIToMidiEvent *next;
};

struct MidiDescriptor
{
    uint16_t type;
    uint16_t tracks;
};

struct XMIToMidiConversionContext
{
    uint8_t                           *src, *src_ptr, *src_end;
    uint32_t                           srcsize;
    uint32_t                           datastart;
    uint8_t                           *dst, *dst_ptr;
    uint32_t                           dstsize, dstrem;
    uint32_t                           convert_type;
    MidiDescriptor                     info;
    int                                bank127[16];
    XMIToMidiEvent                   **events;
    int16_t                           *timing;
    XMIToMidiEvent                    *list;
    XMIToMidiEvent                    *current;
    std::vector<std::vector<uint8_t>> *dyn_out;
    std::vector<uint8_t>              *dyn_out_cur;
};

struct XMIToMidiBranch
{
    unsigned count;
    uint8_t  id[128];
    uint32_t offset[128];
};

/* forward declarations of private functions */
static void    XMIToMidiDeleteEventList(XMIToMidiEvent *mlist);
static void    XMIToMidiCreateNewEvent(struct XMIToMidiConversionContext *ctx, int32_t time); /* List manipulation */
static int     XMIToMidiGetVlq(struct XMIToMidiConversionContext *ctx, uint32_t *quant);  /* Variable length quantity */
static int     XMIToMidiGetVlq2(struct XMIToMidiConversionContext *ctx, uint32_t *quant); /* Variable length quantity */
static int     XMIToMidiPutVlq(struct XMIToMidiConversionContext *ctx, uint32_t value);   /* Variable length quantity */
static int     XMIToMidiConvertEvent(struct XMIToMidiConversionContext *ctx, const int32_t time, const uint8_t status,
                                     const int size);
static int32_t XMIToMidiConvertSystemMessage(struct XMIToMidiConversionContext *ctx, const int32_t time,
                                             const uint8_t status);
static int32_t XMIToMidiConvertFiletoList(struct XMIToMidiConversionContext *ctx, const XMIToMidiBranch *rbrn);
static uint32_t XMIToMidiConvertListToMidiTrack(struct XMIToMidiConversionContext *ctx, XMIToMidiEvent *mlist);
static int      XMIToMidiParseXMI(struct XMIToMidiConversionContext *ctx);
static int      XMIToMidiExtractTracks(struct XMIToMidiConversionContext *ctx, int32_t dstTrackNumber);
static uint32_t XMIToMidiExtractTracksFromXMI(struct XMIToMidiConversionContext *ctx);

static uint32_t XMIToMidiRead1(struct XMIToMidiConversionContext *ctx)
{
    uint8_t b0;
    assert(ctx->src_ptr + 1 < ctx->src_end);
    b0 = *ctx->src_ptr++;
    return (b0);
}

static uint32_t XMIToMidiRead2(struct XMIToMidiConversionContext *ctx)
{
    uint8_t b0, b1;
    assert(ctx->src_ptr + 2 < ctx->src_end);
    b0 = *ctx->src_ptr++;
    b1 = *ctx->src_ptr++;
    return (b0 + ((uint32_t)b1 << 8));
}

static uint32_t XMIToMidiRead4(struct XMIToMidiConversionContext *ctx)
{
    uint8_t b0, b1, b2, b3;
    assert(ctx->src_ptr + 4 < ctx->src_end);
    b3 = *ctx->src_ptr++;
    b2 = *ctx->src_ptr++;
    b1 = *ctx->src_ptr++;
    b0 = *ctx->src_ptr++;
    return (b0 + ((uint32_t)b1 << 8) + ((uint32_t)b2 << 16) + ((uint32_t)b3 << 24));
}

static uint32_t XMIToMidiRead4LittleEndian(struct XMIToMidiConversionContext *ctx)
{
    uint8_t b0, b1, b2, b3;
    assert(ctx->src_ptr + 4 < ctx->src_end);
    b3 = *ctx->src_ptr++;
    b2 = *ctx->src_ptr++;
    b1 = *ctx->src_ptr++;
    b0 = *ctx->src_ptr++;
    return (b3 + ((uint32_t)b2 << 8) + ((uint32_t)b1 << 16) + ((uint32_t)b0 << 24));
}

static void XMIToMidiCopy(struct XMIToMidiConversionContext *ctx, char *b, uint32_t len)
{
    assert(ctx->src_ptr + len < ctx->src_end);
    memcpy(b, ctx->src_ptr, len);
    ctx->src_ptr += len;
}

static constexpr uint16_t kDestinationChunkSize = 8192;
static void               XMIToMidiResizeDestination(struct XMIToMidiConversionContext *ctx)
{
    uint32_t pos = (uint32_t)(ctx->dst_ptr - ctx->dst);
    if (ctx->dyn_out && ctx->dyn_out_cur)
    {
        ctx->dyn_out_cur->resize(ctx->dstsize + kDestinationChunkSize);
        ctx->dst = ctx->dyn_out_cur->data();
    }
    else
        ctx->dst = (uint8_t *)realloc(ctx->dst, ctx->dstsize + kDestinationChunkSize);
    ctx->dstsize += kDestinationChunkSize;
    ctx->dstrem += kDestinationChunkSize;
    ctx->dst_ptr = ctx->dst + pos;
}

static void XMIToMidiWrite1(struct XMIToMidiConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 1)
        XMIToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void XMIToMidiWrite2(struct XMIToMidiConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 2)
        XMIToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void XMIToMidiWrite4(struct XMIToMidiConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 4)
        XMIToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = (val >> 24) & 0xff;
    *ctx->dst_ptr++ = (val >> 16) & 0xff;
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 4;
}

static void XMIToMidiSeekSource(struct XMIToMidiConversionContext *ctx, uint32_t pos)
{
    ctx->src_ptr = ctx->src + pos;
}

static void XMIToMidiSeekDestination(struct XMIToMidiConversionContext *ctx, uint32_t pos)
{
    ctx->dst_ptr = ctx->dst + pos;
    while (ctx->dstsize < pos)
        XMIToMidiResizeDestination(ctx);
    ctx->dstrem = ctx->dstsize - pos;
}

static void XMIToMidiSkipSource(struct XMIToMidiConversionContext *ctx, int32_t pos)
{
    ctx->src_ptr += pos;
}

static void XMIToMidiSkipDestination(struct XMIToMidiConversionContext *ctx, int32_t pos)
{
    size_t newpos;
    ctx->dst_ptr += pos;
    newpos = ctx->dst_ptr - ctx->dst;
    while (ctx->dstsize < newpos)
        XMIToMidiResizeDestination(ctx);
    ctx->dstrem = (uint32_t)(ctx->dstsize - newpos);
}

static uint32_t XMIToMidiGetSourceSize(struct XMIToMidiConversionContext *ctx)
{
    return (ctx->srcsize);
}

static uint32_t XMIToMidiGetSourcePosition(struct XMIToMidiConversionContext *ctx)
{
    return (uint32_t)(ctx->src_ptr - ctx->src);
}

static uint32_t XMIToMidiGetDestinationPosition(struct XMIToMidiConversionContext *ctx)
{
    return (uint32_t)(ctx->dst_ptr - ctx->dst);
}

/* This is a default set of patches to convert from MT32 to GM
 * The index is the MT32 Patch number and the value is the GM Patch
 * This is only suitable for music that doesn't do timbre changes
 * XMIDIs that contain Timbre changes will not convert properly.
 */
static constexpr char Mt32ToGmMap[128] = {
    0,   /* 0   Piano 1 */
    1,   /* 1   Piano 2 */
    2,   /* 2   Piano 3 (synth) */
    4,   /* 3   EPiano 1 */
    4,   /* 4   EPiano 2 */
    5,   /* 5   EPiano 3 */
    5,   /* 6   EPiano 4 */
    3,   /* 7   Honkytonk */
    16,  /* 8   Organ 1 */
    17,  /* 9   Organ 2 */
    18,  /* 10  Organ 3 */
    16,  /* 11  Organ 4 */
    19,  /* 12  Pipe Organ 1 */
    19,  /* 13  Pipe Organ 2 */
    19,  /* 14  Pipe Organ 3 */
    21,  /* 15  Accordion */
    6,   /* 16  Harpsichord 1 */
    6,   /* 17  Harpsichord 2 */
    6,   /* 18  Harpsichord 3 */
    7,   /* 19  Clavinet 1 */
    7,   /* 20  Clavinet 2 */
    7,   /* 21  Clavinet 3 */
    8,   /* 22  Celesta 1 */
    8,   /* 23  Celesta 2 */
    62,  /* 24  Synthbrass 1 (62) */
    63,  /* 25  Synthbrass 2 (63) */
    62,  /* 26  Synthbrass 3 Bank 8 */
    63,  /* 27  Synthbrass 4 Bank 8 */
    38,  /* 28  Synthbass 1 */
    39,  /* 29  Synthbass 2 */
    38,  /* 30  Synthbass 3 Bank 8 */
    39,  /* 31  Synthbass 4 Bank 8 */
    88,  /* 32  Fantasy */
    90,  /* 33  Harmonic Pan - No equiv closest is polysynth(90) :( */
    52,  /* 34  Choral ?? Currently set to SynthVox(54). Should it be
            ChoirAhhs(52)??? */
    92,  /* 35  Glass */
    97,  /* 36  Soundtrack */
    99,  /* 37  Atmosphere */
    14,  /* 38  Warmbell, sounds kind of like crystal(98) perhaps Tubular
            Bells(14) would be better. It is! */
    54,  /* 39  FunnyVox, sounds alot like Bagpipe(109) and Shania(111) */
    98,  /* 40  EchoBell, no real equiv, sounds like Crystal(98) */
    96,  /* 41  IceRain */
    68,  /* 42  Oboe 2001, no equiv, just patching it to normal oboe(68) */
    95,  /* 43  EchoPans, no equiv, setting to SweepPad */
    81,  /* 44  DoctorSolo Bank 8 */
    87,  /* 45  SchoolDaze, no real equiv */
    112, /* 46  Bell Singer */
    80,  /* 47  SquareWave */
    48,  /* 48  Strings 1 */
    48,  /* 49  Strings 2 - should be 49 */
    44,  /* 50  Strings 3 (Synth) - Experimental set to Tremollo Strings - should
            be 50 */
    45,  /* 51  Pizzicato Strings */
    40,  /* 52  Violin 1 */
    40,  /* 53  Violin 2 ? Viola */
    42,  /* 54  Cello 1 */
    42,  /* 55  Cello 2 */
    43,  /* 56  Contrabass */
    46,  /* 57  Harp 1 */
    46,  /* 58  Harp 2 */
    24,  /* 59  Guitar 1 (Nylon) */
    25,  /* 60  Guitar 2 (Steel) */
    26,  /* 61  Elec Guitar 1 */
    27,  /* 62  Elec Guitar 2 */
    104, /* 63  Sitar */
    32,  /* 64  Acou Bass 1 */
    32,  /* 65  Acou Bass 2 */
    33,  /* 66  Elec Bass 1 */
    34,  /* 67  Elec Bass 2 */
    36,  /* 68  Slap Bass 1 */
    37,  /* 69  Slap Bass 2 */
    35,  /* 70  Fretless Bass 1 */
    35,  /* 71  Fretless Bass 2 */
    73,  /* 72  Flute 1 */
    73,  /* 73  Flute 2 */
    72,  /* 74  Piccolo 1 */
    72,  /* 75  Piccolo 2 */
    74,  /* 76  Recorder */
    75,  /* 77  Pan Pipes */
    64,  /* 78  Sax 1 */
    65,  /* 79  Sax 2 */
    66,  /* 80  Sax 3 */
    67,  /* 81  Sax 4 */
    71,  /* 82  Clarinet 1 */
    71,  /* 83  Clarinet 2 */
    68,  /* 84  Oboe */
    69,  /* 85  English Horn (Cor Anglais) */
    70,  /* 86  Bassoon */
    22,  /* 87  Harmonica */
    56,  /* 88  Trumpet 1 */
    56,  /* 89  Trumpet 2 */
    57,  /* 90  Trombone 1 */
    57,  /* 91  Trombone 2 */
    60,  /* 92  French Horn 1 */
    60,  /* 93  French Horn 2 */
    58,  /* 94  Tuba */
    61,  /* 95  Brass Section 1 */
    61,  /* 96  Brass Section 2 */
    11,  /* 97  Vibes 1 */
    11,  /* 98  Vibes 2 */
    99,  /* 99  Syn Mallet Bank 1 */
    112, /* 100 WindBell no real equiv Set to TinkleBell(112) */
    9,   /* 101 Glockenspiel */
    14,  /* 102 Tubular Bells */
    13,  /* 103 Xylophone */
    12,  /* 104 Marimba */
    107, /* 105 Koto */
    111, /* 106 Sho?? set to Shanai(111) */
    77,  /* 107 Shakauhachi */
    78,  /* 108 Whistle 1 */
    78,  /* 109 Whistle 2 */
    76,  /* 110 Bottle Blow */
    76,  /* 111 Breathpipe no real equiv set to bottle blow(76) */
    47,  /* 112 Timpani */
    117, /* 113 Melodic Tom */
    116, /* 114 Deap Snare no equiv, set to Taiko(116) */
    118, /* 115 Electric Perc 1 */
    118, /* 116 Electric Perc 2 */
    116, /* 117 Taiko */
    115, /* 118 Taiko Rim, no real equiv, set to Woodblock(115) */
    119, /* 119 Cymbal, no real equiv, set to reverse cymbal(119) */
    115, /* 120 Castanets, no real equiv, in GM set to Woodblock(115) */
    112, /* 121 Triangle, no real equiv, set to TinkleBell(112) */
    55,  /* 122 Orchestral Hit */
    124, /* 123 Telephone */
    123, /* 124 BirdTweet */
    94,  /* 125 Big Notes Pad no equiv, set to halo pad (94) */
    98,  /* 126 Water Bell set to Crystal Pad(98) */
    121  /* 127 Jungle Tune set to Breath Noise */
};

/* Same as above, except include patch changes
 * so GS instruments can be used */
static constexpr char Mt32ToGsMap[256] = {
    0,
    0, /* 0   Piano 1 */
    1,
    0, /* 1   Piano 2 */
    2,
    0, /* 2   Piano 3 (synth) */
    4,
    0, /* 3   EPiano 1 */
    4,
    0, /* 4   EPiano 2 */
    5,
    0, /* 5   EPiano 3 */
    5,
    0, /* 6   EPiano 4 */
    3,
    0, /* 7   Honkytonk */
    16,
    0, /* 8   Organ 1 */
    17,
    0, /* 9   Organ 2 */
    18,
    0, /* 10  Organ 3 */
    16,
    0, /* 11  Organ 4 */
    19,
    0, /* 12  Pipe Organ 1 */
    19,
    0, /* 13  Pipe Organ 2 */
    19,
    0, /* 14  Pipe Organ 3 */
    21,
    0, /* 15  Accordion */
    6,
    0, /* 16  Harpsichord 1 */
    6,
    0, /* 17  Harpsichord 2 */
    6,
    0, /* 18  Harpsichord 3 */
    7,
    0, /* 19  Clavinet 1 */
    7,
    0, /* 20  Clavinet 2 */
    7,
    0, /* 21  Clavinet 3 */
    8,
    0, /* 22  Celesta 1 */
    8,
    0, /* 23  Celesta 2 */
    62,
    0, /* 24  Synthbrass 1 (62) */
    63,
    0, /* 25  Synthbrass 2 (63) */
    62,
    0, /* 26  Synthbrass 3 Bank 8 */
    63,
    0, /* 27  Synthbrass 4 Bank 8 */
    38,
    0, /* 28  Synthbass 1 */
    39,
    0, /* 29  Synthbass 2 */
    38,
    0, /* 30  Synthbass 3 Bank 8 */
    39,
    0, /* 31  Synthbass 4 Bank 8 */
    88,
    0, /* 32  Fantasy */
    90,
    0, /* 33  Harmonic Pan - No equiv closest is polysynth(90) :( */
    52,
    0, /* 34  Choral ?? Currently set to SynthVox(54). Should it be
          ChoirAhhs(52)??? */
    92,
    0, /* 35  Glass */
    97,
    0, /* 36  Soundtrack */
    99,
    0, /* 37  Atmosphere */
    14,
    0, /* 38  Warmbell, sounds kind of like crystal(98) perhaps Tubular
          Bells(14) would be better. It is! */
    54,
    0, /* 39  FunnyVox, sounds alot like Bagpipe(109) and Shania(111) */
    98,
    0, /* 40  EchoBell, no real equiv, sounds like Crystal(98) */
    96,
    0, /* 41  IceRain */
    68,
    0, /* 42  Oboe 2001, no equiv, just patching it to normal oboe(68) */
    95,
    0, /* 43  EchoPans, no equiv, setting to SweepPad */
    81,
    0, /* 44  DoctorSolo Bank 8 */
    87,
    0, /* 45  SchoolDaze, no real equiv */
    112,
    0, /* 46  Bell Singer */
    80,
    0, /* 47  SquareWave */
    48,
    0, /* 48  Strings 1 */
    48,
    0, /* 49  Strings 2 - should be 49 */
    44,
    0, /* 50  Strings 3 (Synth) - Experimental set to Tremollo Strings - should
          be 50 */
    45,
    0, /* 51  Pizzicato Strings */
    40,
    0, /* 52  Violin 1 */
    40,
    0, /* 53  Violin 2 ? Viola */
    42,
    0, /* 54  Cello 1 */
    42,
    0, /* 55  Cello 2 */
    43,
    0, /* 56  Contrabass */
    46,
    0, /* 57  Harp 1 */
    46,
    0, /* 58  Harp 2 */
    24,
    0, /* 59  Guitar 1 (Nylon) */
    25,
    0, /* 60  Guitar 2 (Steel) */
    26,
    0, /* 61  Elec Guitar 1 */
    27,
    0, /* 62  Elec Guitar 2 */
    104,
    0, /* 63  Sitar */
    32,
    0, /* 64  Acou Bass 1 */
    32,
    0, /* 65  Acou Bass 2 */
    33,
    0, /* 66  Elec Bass 1 */
    34,
    0, /* 67  Elec Bass 2 */
    36,
    0, /* 68  Slap Bass 1 */
    37,
    0, /* 69  Slap Bass 2 */
    35,
    0, /* 70  Fretless Bass 1 */
    35,
    0, /* 71  Fretless Bass 2 */
    73,
    0, /* 72  Flute 1 */
    73,
    0, /* 73  Flute 2 */
    72,
    0, /* 74  Piccolo 1 */
    72,
    0, /* 75  Piccolo 2 */
    74,
    0, /* 76  Recorder */
    75,
    0, /* 77  Pan Pipes */
    64,
    0, /* 78  Sax 1 */
    65,
    0, /* 79  Sax 2 */
    66,
    0, /* 80  Sax 3 */
    67,
    0, /* 81  Sax 4 */
    71,
    0, /* 82  Clarinet 1 */
    71,
    0, /* 83  Clarinet 2 */
    68,
    0, /* 84  Oboe */
    69,
    0, /* 85  English Horn (Cor Anglais) */
    70,
    0, /* 86  Bassoon */
    22,
    0, /* 87  Harmonica */
    56,
    0, /* 88  Trumpet 1 */
    56,
    0, /* 89  Trumpet 2 */
    57,
    0, /* 90  Trombone 1 */
    57,
    0, /* 91  Trombone 2 */
    60,
    0, /* 92  French Horn 1 */
    60,
    0, /* 93  French Horn 2 */
    58,
    0, /* 94  Tuba */
    61,
    0, /* 95  Brass Section 1 */
    61,
    0, /* 96  Brass Section 2 */
    11,
    0, /* 97  Vibes 1 */
    11,
    0, /* 98  Vibes 2 */
    99,
    0, /* 99  Syn Mallet Bank 1 */
    112,
    0, /* 100 WindBell no real equiv Set to TinkleBell(112) */
    9,
    0, /* 101 Glockenspiel */
    14,
    0, /* 102 Tubular Bells */
    13,
    0, /* 103 Xylophone */
    12,
    0, /* 104 Marimba */
    107,
    0, /* 105 Koto */
    111,
    0, /* 106 Sho?? set to Shanai(111) */
    77,
    0, /* 107 Shakauhachi */
    78,
    0, /* 108 Whistle 1 */
    78,
    0, /* 109 Whistle 2 */
    76,
    0, /* 110 Bottle Blow */
    76,
    0, /* 111 Breathpipe no real equiv set to bottle blow(76) */
    47,
    0, /* 112 Timpani */
    117,
    0, /* 113 Melodic Tom */
    116,
    0, /* 114 Deap Snare no equiv, set to Taiko(116) */
    118,
    0, /* 115 Electric Perc 1 */
    118,
    0, /* 116 Electric Perc 2 */
    116,
    0, /* 117 Taiko */
    115,
    0, /* 118 Taiko Rim, no real equiv, set to Woodblock(115) */
    119,
    0, /* 119 Cymbal, no real equiv, set to reverse cymbal(119) */
    115,
    0, /* 120 Castanets, no real equiv, in GM set to Woodblock(115) */
    112,
    0, /* 121 Triangle, no real equiv, set to TinkleBell(112) */
    55,
    0, /* 122 Orchestral Hit */
    124,
    0, /* 123 Telephone */
    123,
    0, /* 124 BirdTweet */
    94,
    0, /* 125 Big Notes Pad no equiv, set to halo pad (94) */
    98,
    0, /* 126 Water Bell set to Crystal Pad(98) */
    121,
    0  /* 127 Jungle Tune set to Breath Noise */
};

static int ConvertXMIToMidi(uint8_t *in, uint32_t insize, std::vector<std::vector<uint8_t>> &out, uint32_t convert_type)
{
    struct XMIToMidiConversionContext ctx;
    unsigned int                      i;
    int                               ret = -1;

    if (convert_type > kXMIConvertMt32ToGs)
    {
        /*_WM_ERROR_NEW("%s:%i:  %d is an invalid conversion type.",
         * __FUNCTION__, __LINE__, convert_type);*/
        return (ret);
    }

    memset(&ctx, 0, sizeof(struct XMIToMidiConversionContext));
    ctx.src = ctx.src_ptr = in;
    ctx.srcsize           = insize;
    ctx.src_end           = ctx.src + insize;
    ctx.convert_type      = convert_type;
    ctx.dyn_out           = &out;

    if (XMIToMidiParseXMI(&ctx) < 0)
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, nullptr,
         * 0);*/
        goto _end;
    }

    if (XMIToMidiExtractTracks(&ctx, 0) < 0)
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_MIDI, nullptr,
         * 0);*/
        goto _end;
    }

    for (i = 0; i < ctx.info.tracks; i++)
    {
        out.push_back(std::vector<uint8_t>());
        ctx.dyn_out_cur = &out.back();
        ctx.dyn_out_cur->resize(kDestinationChunkSize);
        ctx.dst     = ctx.dyn_out_cur->data();
        ctx.dst_ptr = ctx.dst;
        ctx.dstsize = kDestinationChunkSize;
        ctx.dstrem  = kDestinationChunkSize;

        /* Header is 14 bytes long and add the rest as well */
        XMIToMidiWrite1(&ctx, 'M');
        XMIToMidiWrite1(&ctx, 'T');
        XMIToMidiWrite1(&ctx, 'h');
        XMIToMidiWrite1(&ctx, 'd');

        XMIToMidiWrite4(&ctx, 6);

        XMIToMidiWrite2(&ctx, ctx.info.type);
        XMIToMidiWrite2(&ctx, 1);
        XMIToMidiWrite2(&ctx, ctx.timing[i]); /* write divisions from track0 */

        XMIToMidiConvertListToMidiTrack(&ctx, ctx.events[i]);
        ctx.dyn_out_cur->resize(ctx.dstsize - ctx.dstrem);
    }

    ret = 0;

_end: /* cleanup */
    if (ret < 0)
    {
        out.clear();
    }
    if (ctx.events)
    {
        for (i = 0; i < ctx.info.tracks; i++)
            XMIToMidiDeleteEventList(ctx.events[i]);
        free(ctx.events);
    }
    free(ctx.timing);

    return (ret);
}

static void XMIToMidiDeleteEventList(XMIToMidiEvent *mlist)
{
    XMIToMidiEvent *event;
    XMIToMidiEvent *next;

    next = mlist;

    while ((event = next) != nullptr)
    {
        next = event->next;
        free(event->buffer);
        free(event);
    }
}

/* Sets current to the new event and updates list */
static void XMIToMidiCreateNewEvent(struct XMIToMidiConversionContext *ctx, int32_t time)
{
    if (!ctx->list)
    {
        ctx->list = ctx->current = (XMIToMidiEvent *)calloc(1, sizeof(XMIToMidiEvent));
        ctx->current->time       = (time < 0) ? 0 : time;
        return;
    }

    if (time < 0)
    {
        XMIToMidiEvent *event = (XMIToMidiEvent *)calloc(1, sizeof(XMIToMidiEvent));
        event->next           = ctx->list;
        ctx->list = ctx->current = event;
        return;
    }

    if (ctx->current->time > time)
        ctx->current = ctx->list;

    while (ctx->current->next)
    {
        if (ctx->current->next->time > time)
        {
            XMIToMidiEvent *event = (XMIToMidiEvent *)calloc(1, sizeof(XMIToMidiEvent));
            event->next           = ctx->current->next;
            ctx->current->next    = event;
            ctx->current          = event;
            ctx->current->time    = time;
            return;
        }

        ctx->current = ctx->current->next;
    }

    ctx->current->next = (XMIToMidiEvent *)calloc(1, sizeof(XMIToMidiEvent));
    ctx->current       = ctx->current->next;
    ctx->current->time = time;
}

/* Conventional Variable Length Quantity */
static int XMIToMidiGetVlq(struct XMIToMidiConversionContext *ctx, uint32_t *quant)
{
    int      i;
    uint32_t data;

    *quant = 0;
    for (i = 0; i < 4; i++)
    {
        if (ctx->src_ptr + 1 >= ctx->src + ctx->srcsize)
            break;
        data = XMIToMidiRead1(ctx);
        *quant <<= 7;
        *quant |= data & 0x7F;

        if (!(data & 0x80))
        {
            i++;
            break;
        }
    }
    return (i);
}

/* XMIDI Delta Variable Length Quantity */
static int XMIToMidiGetVlq2(struct XMIToMidiConversionContext *ctx, uint32_t *quant)
{
    int     i;
    int32_t data;

    *quant = 0;
    for (i = 0; XMIToMidiGetSourcePosition(ctx) != XMIToMidiGetSourceSize(ctx); ++i)
    {
        data = XMIToMidiRead1(ctx);
        if (data & 0x80)
        {
            XMIToMidiSkipSource(ctx, -1);
            break;
        }
        *quant += data;
    }
    return (i);
}

static int XMIToMidiPutVlq(struct XMIToMidiConversionContext *ctx, uint32_t value)
{
    int32_t buffer;
    int     i = 1, j;
    buffer    = value & 0x7F;
    while (value >>= 7)
    {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
        i++;
    }
    for (j = 0; j < i; j++)
    {
        XMIToMidiWrite1(ctx, buffer & 0xFF);
        buffer >>= 8;
    }

    return (i);
}

/* Converts Events
 *
 * Source is at the first data byte
 * size 1 is single data byte
 * size 2 is dual data byte
 * size 3 is XMI Note on
 * Returns bytes converted  */
static int XMIToMidiConvertEvent(struct XMIToMidiConversionContext *ctx, const int32_t time, const uint8_t status,
                                 const int size)
{
    uint32_t        delta = 0;
    int32_t         data;
    XMIToMidiEvent *prev;
    int             i;

    data = XMIToMidiRead1(ctx);

    /*HACK!*/
    if (((status >> 4) == 0xB) && (status & 0xF) != 9 && (data == 114))
    {
        data = 32; /*Change XMI 114 controller into XG bank*/
    }

    /* Bank changes are handled here */
    if ((status >> 4) == 0xB && data == 0)
    {
        data = XMIToMidiRead1(ctx);

        ctx->bank127[status & 0xF] = 0;

        if (ctx->convert_type == kXMIConvertMt32ToGm || ctx->convert_type == kXMIConvertMt32ToGs ||
            ctx->convert_type == kXMIConvertMt32ToGs127 ||
            (ctx->convert_type == kXMIConvertMt32ToGs127Drum && (status & 0xF) == 9))
            return (2);

        XMIToMidiCreateNewEvent(ctx, time);
        ctx->current->status  = status;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = data == 127 ? 0 : data; /*HACK:*/

        if (ctx->convert_type == kXMIConvertGs127ToGs && data == 127)
            ctx->bank127[status & 0xF] = 1;

        return (2);
    }

    /* Handling for patch change mt32 conversion, probably should go elsewhere
     */
    if ((status >> 4) == 0xC && (status & 0xF) != 9 && ctx->convert_type != kXMINoConversion)
    {
        if (ctx->convert_type == kXMIConvertMt32ToGm)
        {
            data = Mt32ToGmMap[data];
        }
        else if ((ctx->convert_type == kXMIConvertGs127ToGs && ctx->bank127[status & 0xF]) ||
                 ctx->convert_type == kXMIConvertMt32ToGs || ctx->convert_type == kXMIConvertMt32ToGs127Drum)
        {
            XMIToMidiCreateNewEvent(ctx, time);
            ctx->current->status  = 0xB0 | (status & 0xF);
            ctx->current->data[0] = 0;
            ctx->current->data[1] = Mt32ToGsMap[data * 2 + 1];

            data = Mt32ToGsMap[data * 2];
        }
        else if (ctx->convert_type == kXMIConvertMt32ToGs127)
        {
            XMIToMidiCreateNewEvent(ctx, time);
            ctx->current->status  = 0xB0 | (status & 0xF);
            ctx->current->data[0] = 0;
            ctx->current->data[1] = 127;
        }
    }
    /* Drum track handling */
    else if ((status >> 4) == 0xC && (status & 0xF) == 9 &&
             (ctx->convert_type == kXMIConvertMt32ToGs127Drum || ctx->convert_type == kXMIConvertMt32ToGs127))
    {
        XMIToMidiCreateNewEvent(ctx, time);
        ctx->current->status  = 0xB9;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = 127;
    }

    XMIToMidiCreateNewEvent(ctx, time);
    ctx->current->status = status;

    ctx->current->data[0] = data;

    if (size == 1)
        return (1);

    ctx->current->data[1] = XMIToMidiRead1(ctx);

    if (size == 2)
        return (2);

    /* XMI Note On handling */
    prev = ctx->current;
    i    = XMIToMidiGetVlq(ctx, &delta);
    XMIToMidiCreateNewEvent(ctx, time + delta * 3);

    ctx->current->status  = status;
    ctx->current->data[0] = data;
    ctx->current->data[1] = 0;
    ctx->current          = prev;

    return (i + 2);
}

/* Simple routine to convert system messages */
static int32_t XMIToMidiConvertSystemMessage(struct XMIToMidiConversionContext *ctx, const int32_t time,
                                             const uint8_t status)
{
    int32_t i = 0;

    XMIToMidiCreateNewEvent(ctx, time);
    ctx->current->status = status;

    /* Handling of Meta events */
    if (status == 0xFF)
    {
        ctx->current->data[0] = XMIToMidiRead1(ctx);
        i++;
    }

    i += XMIToMidiGetVlq(ctx, &ctx->current->len);

    if (!ctx->current->len)
        return (i);

    ctx->current->buffer = (uint8_t *)malloc(sizeof(uint8_t) * ctx->current->len);
    XMIToMidiCopy(ctx, (char *)ctx->current->buffer, ctx->current->len);

    return (i + ctx->current->len);
}

/* XMIDI and Midi to List
 * Returns XMIDI PPQN   */
static int32_t XMIToMidiConvertFiletoList(struct XMIToMidiConversionContext *ctx, const XMIToMidiBranch *rbrn)
{
    int32_t  time = 0;
    uint32_t data;
    int32_t  end       = 0;
    int32_t  tempo     = 500000;
    int32_t  tempo_set = 0;
    uint32_t status    = 0;
    uint32_t file_size = XMIToMidiGetSourceSize(ctx);
    uint32_t begin     = XMIToMidiGetSourcePosition(ctx);

    /* Set Drum track to correct setting if required */
    if (ctx->convert_type == kXMIConvertMt32ToGs127)
    {
        XMIToMidiCreateNewEvent(ctx, 0);
        ctx->current->status  = 0xB9;
        ctx->current->data[0] = 0;
        ctx->current->data[1] = 127;
    }

    while (!end && XMIToMidiGetSourcePosition(ctx) < file_size)
    {
        uint32_t offset = XMIToMidiGetSourcePosition(ctx) - begin;

        /* search for branch to this offset */
        for (unsigned i = 0, n = rbrn->count; i < n; ++i)
        {
            if (offset == rbrn->offset[i])
            {
                unsigned id = rbrn->id[i];

                XMIToMidiCreateNewEvent(ctx, time);

                uint8_t *marker = (uint8_t *)malloc(sizeof(uint8_t) * 8);
                memcpy(marker, ":XBRN:", 6);
                const char hex[] = "0123456789ABCDEF";
                marker[6]        = hex[id >> 4];
                marker[7]        = hex[id & 15];

                ctx->current->status  = 0xFF;
                ctx->current->data[0] = 0x06;
                ctx->current->len     = 8;

                ctx->current->buffer = marker;
            }
        }

        XMIToMidiGetVlq2(ctx, &data);
        time += data * 3;

        status = XMIToMidiRead1(ctx);

        switch (status >> 4)
        {
        case kXMIStatusNoteOn:
            XMIToMidiConvertEvent(ctx, time, status, 3);
            break;

        /* 2 byte data */
        case kXMIStatusNoteOff:
        case kXMIStatusAftertouch:
        case kXMIStatusController:
        case kXMIStatusPitchWheel:
            XMIToMidiConvertEvent(ctx, time, status, 2);
            break;

        /* 1 byte data */
        case kXMIStatusProgramChange:
        case kXMIStatusPressure:
            XMIToMidiConvertEvent(ctx, time, status, 1);
            break;

        case kXMIStatusSysex:
            if (status == 0xFF)
            {
                int32_t  pos = XMIToMidiGetSourcePosition(ctx);
                uint32_t dat = XMIToMidiRead1(ctx);

                if (dat == 0x2F)                    /* End */
                    end = 1;
                else if (dat == 0x51 && !tempo_set) /* Tempo. Need it for PPQN */
                {
                    XMIToMidiSkipSource(ctx, 1);
                    tempo = XMIToMidiRead1(ctx) << 16;
                    tempo += XMIToMidiRead1(ctx) << 8;
                    tempo += XMIToMidiRead1(ctx);
                    tempo *= 3;
                    tempo_set = 1;
                }
                else if (dat == 0x51 && tempo_set) /* Skip any other tempo changes */
                {
                    XMIToMidiGetVlq(ctx, &dat);
                    XMIToMidiSkipSource(ctx, dat);
                    break;
                }

                XMIToMidiSeekSource(ctx, pos);
            }
            XMIToMidiConvertSystemMessage(ctx, time, status);
            break;

        default:
            break;
        }
    }
    return ((tempo * 3) / 25000);
}

/* Converts and event list to a MidiTrack
 * Returns bytes of the array
 * buf can be nullptr */
static uint32_t XMIToMidiConvertListToMidiTrack(struct XMIToMidiConversionContext *ctx, XMIToMidiEvent *mlist)
{
    int32_t         time = 0;
    XMIToMidiEvent *event;
    uint32_t        delta;
    uint8_t         last_status = 0;
    uint32_t        i           = 8;
    uint32_t        j;
    uint32_t        size_pos, cur_pos;
    int             end = 0;

    XMIToMidiWrite1(ctx, 'M');
    XMIToMidiWrite1(ctx, 'T');
    XMIToMidiWrite1(ctx, 'r');
    XMIToMidiWrite1(ctx, 'k');

    size_pos = XMIToMidiGetDestinationPosition(ctx);
    XMIToMidiSkipDestination(ctx, 4);

    for (event = mlist; event && !end; event = event->next)
    {
        delta = (event->time - time);
        time  = event->time;

        i += XMIToMidiPutVlq(ctx, delta);

        if ((event->status != last_status) || (event->status >= 0xF0))
        {
            XMIToMidiWrite1(ctx, event->status);
            i++;
        }

        last_status = event->status;

        switch (event->status >> 4)
        {
        /* 2 bytes data
         * Note off, Note on, Aftertouch, Controller and Pitch Wheel */
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xE:
            XMIToMidiWrite1(ctx, event->data[0]);
            XMIToMidiWrite1(ctx, event->data[1]);
            i += 2;
            break;

        /* 1 bytes data
         * Program Change and Channel Pressure */
        case 0xC:
        case 0xD:
            XMIToMidiWrite1(ctx, event->data[0]);
            i++;
            break;

        /* Variable length
         * SysEx */
        case 0xF:
            if (event->status == 0xFF)
            {
                if (event->data[0] == 0x2f)
                    end = 1;
                XMIToMidiWrite1(ctx, event->data[0]);
                i++;
            }
            i += XMIToMidiPutVlq(ctx, event->len);
            if (event->len)
            {
                for (j = 0; j < event->len; j++)
                {
                    XMIToMidiWrite1(ctx, event->buffer[j]);
                    i++;
                }
            }
            break;

        /* Never occur */
        default:
            /*_WM_DEBUG_MSG("%s: unrecognized event", __FUNCTION__);*/
            break;
        }
    }

    cur_pos = XMIToMidiGetDestinationPosition(ctx);
    XMIToMidiSeekDestination(ctx, size_pos);
    XMIToMidiWrite4(ctx, i - 8);
    XMIToMidiSeekDestination(ctx, cur_pos);

    return (i);
}

/* Assumes correct xmidi */
static uint32_t XMIToMidiExtractTracksFromXMI(struct XMIToMidiConversionContext *ctx)
{
    uint32_t     num = 0;
    signed short ppqn;
    uint32_t     len = 0;
    int32_t      begin;
    char         buf[32];
    uint32_t     branch[128];

    /* clear branch points */
    for (unsigned i = 0; i < 128; ++i)
        branch[i] = ~0u;

    while (XMIToMidiGetSourcePosition(ctx) < XMIToMidiGetSourceSize(ctx) && num != ctx->info.tracks)
    {
        /* Read first 4 bytes of name */
        XMIToMidiCopy(ctx, buf, 4);
        len = XMIToMidiRead4(ctx);

        /* Skip the FORM entries */
        if (!memcmp(buf, "FORM", 4))
        {
            XMIToMidiSkipSource(ctx, 4);
            XMIToMidiCopy(ctx, buf, 4);
            len = XMIToMidiRead4(ctx);
        }

        if (!memcmp(buf, "RBRN", 4))
        {
            begin = XMIToMidiGetSourcePosition(ctx);
            uint32_t count;

            if (len < 2)
            {
                /* insufficient data */
                goto rbrn_nodata;
            }

            count = XMIToMidiRead2(ctx);
            if (len - 2 < 6 * count)
            {
                /* insufficient data */
                goto rbrn_nodata;
            }

            for (uint32_t i = 0; i < count; ++i)
            {
                /* read branch point as byte offset */
                uint32_t ctlvalue  = XMIToMidiRead2(ctx);
                uint32_t evtoffset = XMIToMidiRead4LittleEndian(ctx);
                if (ctlvalue < 128)
                    branch[ctlvalue] = evtoffset;
            }

        rbrn_nodata:
            XMIToMidiSeekSource(ctx, begin + ((len + 1) & ~1));
            continue;
        }

        if (memcmp(buf, "EVNT", 4))
        {
            XMIToMidiSkipSource(ctx, (len + 1) & ~1);
            continue;
        }

        ctx->list = nullptr;
        begin     = XMIToMidiGetSourcePosition(ctx);

        /* Rearrange branches as structure */
        XMIToMidiBranch rbrn;
        rbrn.count = 0;
        for (unsigned i = 0; i < 128; ++i)
        {
            if (branch[i] != ~0u)
            {
                unsigned index     = rbrn.count;
                rbrn.id[index]     = i;
                rbrn.offset[index] = branch[i];
                rbrn.count         = index + 1;
            }
        }

        /* Convert it */
        if ((ppqn = XMIToMidiConvertFiletoList(ctx, &rbrn)) == 0)
        {
            /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, nullptr,
             * 0);*/
            break;
        }
        ctx->timing[num] = ppqn;
        ctx->events[num] = ctx->list;

        /* Increment Counter */
        num++;

        /* go to start of next track */
        XMIToMidiSeekSource(ctx, begin + ((len + 1) & ~1));

        /* clear branch points */
        for (unsigned i = 0; i < 128; ++i)
            branch[i] = ~0u;
    }

    /* Return how many were converted */
    return (num);
}

static int XMIToMidiParseXMI(struct XMIToMidiConversionContext *ctx)
{
    uint32_t i;
    uint32_t start;
    uint32_t len;
    uint32_t chunk_len;
    uint32_t file_size;
    char     buf[32];

    file_size = XMIToMidiGetSourceSize(ctx);
    if (XMIToMidiGetSourcePosition(ctx) + 8 > file_size)
    {
    badfile: /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too
                short)", 0);*/
        return (-1);
    }

    /* Read first 4 bytes of header */
    XMIToMidiCopy(ctx, buf, 4);

    /* Could be XMIDI */
    if (!memcmp(buf, "FORM", 4))
    {
        /* Read length of */
        len = XMIToMidiRead4(ctx);

        start = XMIToMidiGetSourcePosition(ctx);
        if (start + 4 > file_size)
            goto badfile;

        /* Read 4 bytes of type */
        XMIToMidiCopy(ctx, buf, 4);

        /* XDIRless XMIDI, we can handle them here. */
        if (!memcmp(buf, "XMID", 4))
        {
            /*_WM_DEBUG_MSG("Warning: XMIDI without XDIR");*/
            ctx->info.tracks = 1;
        }
        /* Not an XMIDI that we recognise */
        else if (memcmp(buf, "XDIR", 4))
        {
            goto badfile;
        }
        else
        { /* Seems Valid */
            ctx->info.tracks = 0;

            for (i = 4; i < len; i++)
            {
                /* check too short files */
                if (XMIToMidiGetSourcePosition(ctx) + 10 > file_size)
                    break;

                /* Read 4 bytes of type */
                XMIToMidiCopy(ctx, buf, 4);

                /* Read length of chunk */
                chunk_len = XMIToMidiRead4(ctx);

                /* Add eight bytes */
                i += 8;

                if (memcmp(buf, "INFO", 4))
                {
                    /* Must align */
                    XMIToMidiSkipSource(ctx, (chunk_len + 1) & ~1);
                    i += (chunk_len + 1) & ~1;
                    continue;
                }

                /* Must be at least 2 bytes long */
                if (chunk_len < 2)
                    break;

                ctx->info.tracks = XMIToMidiRead2(ctx);
                break;
            }

            /* Didn't get to fill the header */
            if (ctx->info.tracks == 0)
            {
                goto badfile;
            }

            /* Ok now to start part 2
             * Goto the right place */
            XMIToMidiSeekSource(ctx, start + ((len + 1) & ~1));
            if (XMIToMidiGetSourcePosition(ctx) + 12 > file_size)
                goto badfile;

            /* Read 4 bytes of type */
            XMIToMidiCopy(ctx, buf, 4);

            if (memcmp(buf, "CAT ", 4))
            {
                /*_WM_ERROR_NEW("XMI error: expected \"CAT \", found
                   \"%c%c%c%c\".", buf[0], buf[1], buf[2], buf[3]);*/
                return (-1);
            }

            /* Now read length of this track */
            XMIToMidiRead4(ctx);

            /* Read 4 bytes of type */
            XMIToMidiCopy(ctx, buf, 4);

            if (memcmp(buf, "XMID", 4))
            {
                /*_WM_ERROR_NEW("XMI error: expected \"XMID\", found
                   \"%c%c%c%c\".", buf[0], buf[1], buf[2], buf[3]);*/
                return (-1);
            }

            /* Valid XMID */
            ctx->datastart = XMIToMidiGetSourcePosition(ctx);
            return (0);
        }
    }

    return (-1);
}

static int XMIToMidiExtractTracks(struct XMIToMidiConversionContext *ctx, int32_t dstTrackNumber)
{
    uint32_t i;

    ctx->events = (XMIToMidiEvent **)calloc(ctx->info.tracks, sizeof(XMIToMidiEvent *));
    ctx->timing = (int16_t *)calloc(ctx->info.tracks, sizeof(int16_t));
    /* type-2 for multi-tracks, type-0 otherwise */
    ctx->info.type = (ctx->info.tracks > 1 && (dstTrackNumber < 0 || ctx->info.tracks >= dstTrackNumber)) ? 2 : 0;

    XMIToMidiSeekSource(ctx, ctx->datastart);
    i = XMIToMidiExtractTracksFromXMI(ctx);

    if (i != ctx->info.tracks)
    {
        /*_WM_ERROR_NEW("XMI error: extracted only %u out of %u tracks from
           XMIDI", ctx->info.tracks, i);*/
        return (-1);
    }

    return (0);
}
