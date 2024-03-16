/*
 * MUS2MIDI: MUS to MIDI Library
 *
 * Copyright (C) 2014  Bret Curtis
 * Copyright (C) WildMIDI Developers  2015-2016
 * ADLMIDI Library API: Copyright (c) 2015-2023 Vitaly Novichkov
 * <admin@wohlnet.ru>
 * Copyright (c) 2024 The EDGE Team.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static constexpr uint8_t kMusFrequency = 140;
static constexpr int     kMusTempo =
    0x00068A1B; /* MPQN: 60000000 / 140BPM (140Hz) = 428571 */
                /*  0x000D1436 -> MPQN: 60000000 /  70BPM  (70Hz) = 857142 */
static constexpr uint16_t kMusDivision =
    0x0101; /* 257 for 140Hz files with a 140MPQN */
            /*  0x0088 -> 136 for  70Hz files with a 140MPQN */
            /*  0x010B -> 267 for  70hz files with a 70MPQN  */
            /*  0x01F9 -> 505 for 140hz files with a 70MPQN  */

/* New
 * QLS: MPQN/1000000 = 0.428571
 * TDPS: QLS/PPQN = 0.428571/136 = 0.003151257
 * PPQN: 136
 *
 * QLS: MPQN/1000000 = 0.428571
 * TDPS: QLS/PPQN = 0.428571/257 = 0.001667591
 * PPQN: 257
 *
 * QLS: MPQN/1000000 = 0.857142
 * TDPS: QLS/PPQN = 0.857142/267 = 0.00321027
 * PPQN: 267
 *
 * QLS: MPQN/1000000 = 0.857142
 * TDPS: QLS/PPQN = 0.857142/505 = 0.001697311
 * PPQN: 505
 *
 * Old
 * QLS: MPQN/1000000 = 1.745673
 * TDPS: QLS/PPQN = 1.745673 / 89 = 0.019614303 (seconds per tick)
 * PPQN: (TDPS = QLS/PPQN) (0.019614303 = 1.745673/PPQN) (0.019614303*PPQN
 * = 1.745673) (PPQN = 89.000001682)
 *
 */

enum MusEvent
{
    kMusEventKeyOff           = 0,
    kMusEventKeyOn            = 1,
    kMusEventPitchWheel       = 2,
    kMusEventChannelMode      = 3,
    kMusEventControllerChange = 4,
    kMusEventEnd              = 6,
};

static constexpr uint8_t kMusMidiMaxChannels = 16;

static constexpr char kMusHeader[] = { 'M', 'U', 'S', 0x1A };

static constexpr uint8_t kMusToMidiMap[] = {
    /* MIDI  Number  Description */
    0,    /* 0    program change */
    0,    /* 1    bank selection */
    0x01, /* 2    Modulation pot (frequency vibrato depth) */
    0x07, /* 3    Volume: 0-silent, ~100-normal, 127-loud */
    0x0A, /* 4    Pan (balance) pot: 0-left, 64-center (default), 127-right */
    0x0B, /* 5    Expression pot */
    0x5B, /* 6    Reverb depth */
    0x5D, /* 7    Chorus depth */
    0x40, /* 8    Sustain pedal */
    0x43, /* 9    Soft pedal */
    0x78, /* 10   All sounds off */
    0x7B, /* 11   All notes off */
    0x7E, /* 12   Mono (use numchannels + 1) */
    0x7F, /* 13   Poly */
    0x79, /* 14   reset all controllers */
};

struct MusHeader
{
    char     ID[4]; /* identifier: "MUS" 0x1A */
    uint16_t scoreLen;
    uint16_t scoreStart;
    uint16_t channels;     /* count of primary channels */
    uint16_t sec_channels; /* count of secondary channels */
    uint16_t instrCnt;
};

struct MidiHeaderChunk
{
    char    name[4];
    int32_t length;
    int16_t format;   /* make 0 */
    int16_t ntracks;  /* make 1 */
    int16_t division; /* 0xe250 ?? */
};

struct MidiTrackChunk
{
    char    name[4];
    int32_t length;
};

struct MusConversionContext
{
    uint8_t *src, *src_ptr;
    uint32_t srcsize;
    uint32_t datastart;
    uint8_t *dst, *dst_ptr;
    uint32_t dstsize, dstrem;
};

static constexpr uint16_t kMusDestinationChunkSize = 8192;
static void MusToMidiResizeDestination(struct MusConversionContext *ctx)
{
    uint32_t pos = (uint32_t)(ctx->dst_ptr - ctx->dst);
    ctx->dst =
        (uint8_t *)realloc(ctx->dst, ctx->dstsize + kMusDestinationChunkSize);
    ctx->dstsize += kMusDestinationChunkSize;
    ctx->dstrem += kMusDestinationChunkSize;
    ctx->dst_ptr = ctx->dst + pos;
}

static void MusToMidiWrite1(struct MusConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 1) MusToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void MusToMidiWrite2(struct MusConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 2) MusToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void MusToMidiWrite4(struct MusConversionContext *ctx, uint32_t val)
{
    if (ctx->dstrem < 4) MusToMidiResizeDestination(ctx);
    *ctx->dst_ptr++ = (uint8_t)((val >> 24) & 0xff);
    *ctx->dst_ptr++ = (uint8_t)((val >> 16) & 0xff);
    *ctx->dst_ptr++ = (uint8_t)((val >> 8) & 0xff);
    *ctx->dst_ptr++ = (uint8_t)((val & 0xff));
    ctx->dstrem -= 4;
}

static void MusToMidiSeekDestination(struct MusConversionContext *ctx,
                                     uint32_t                     pos)
{
    ctx->dst_ptr = ctx->dst + pos;
    while (ctx->dstsize < pos) MusToMidiResizeDestination(ctx);
    ctx->dstrem = ctx->dstsize - pos;
}

static void MusToMidiSkipDestination(struct MusConversionContext *ctx,
                                     int32_t                      pos)
{
    size_t newpos;
    ctx->dst_ptr += pos;
    newpos = ctx->dst_ptr - ctx->dst;
    while (ctx->dstsize < newpos) MusToMidiResizeDestination(ctx);
    ctx->dstrem = (uint32_t)(ctx->dstsize - newpos);
}

static uint32_t MusToMidiGetDestinationPosition(
    struct MusConversionContext *ctx)
{
    return (uint32_t)(ctx->dst_ptr - ctx->dst);
}

/* writes a variable length integer to a buffer, and returns bytes written */
static int32_t MusToMidiWriteVariableLength(int32_t value, uint8_t *out)
{
    int32_t buffer, count = 0;

    buffer = value & 0x7f;
    while ((value >>= 7) > 0)
    {
        buffer <<= 8;
        buffer += 0x80;
        buffer += (value & 0x7f);
    }

    while (1)
    {
        ++count;
        *out = (uint8_t)buffer;
        ++out;
        if (buffer & 0x80)
            buffer >>= 8;
        else
            break;
    }
    return (count);
}

#define EDGE_MUS_READ_SHORT(b) ((b)[0] | ((b)[1] << 8))
#define EDGE_MUS_READ_INT(b)                                                   \
    ((b)[0] | ((b)[1] << 8) | ((b)[2] << 16) | ((b)[3] << 24))

static int ConvertMusToMidi(uint8_t *in, uint32_t insize, uint8_t **out,
                            uint32_t *outsize, uint16_t frequency)
{
    struct MusConversionContext ctx;
    MusHeader                   header;
    uint8_t                    *cur, *end;
    uint32_t                    track_size_pos, begin_track_pos, current_pos;
    int32_t                     delta_time; /* Delta time for midi event */
    int                         temp, ret = -1;
    int                         channel_volume[kMusMidiMaxChannels];
    int                         channelMap[kMusMidiMaxChannels], currentChannel;

    if (insize < sizeof(MusHeader))
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)",
         * 0);*/
        return (-1);
    }

    if (!frequency) frequency = kMusFrequency;

    /* read the MUS header and set our location */
    memcpy(header.ID, in, 4);
    header.scoreLen     = EDGE_MUS_READ_SHORT(&in[4]);
    header.scoreStart   = EDGE_MUS_READ_SHORT(&in[6]);
    header.channels     = EDGE_MUS_READ_SHORT(&in[8]);
    header.sec_channels = EDGE_MUS_READ_SHORT(&in[10]);
    header.instrCnt     = EDGE_MUS_READ_SHORT(&in[12]);

    if (memcmp(header.ID, kMusHeader, 4))
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_MUS, nullptr,
         * 0);*/
        return (-1);
    }
    if (insize < (uint32_t)header.scoreLen + (uint32_t)header.scoreStart)
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)",
         * 0);*/
        return (-1);
    }
    /* channel #15 should be excluded in the numchannels field: */
    if (header.channels > kMusMidiMaxChannels - 1)
    {
        /*_WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, nullptr,
         * 0);*/
        return (-1);
    }

    memset(&ctx, 0, sizeof(struct MusConversionContext));
    ctx.src = ctx.src_ptr = in;
    ctx.srcsize           = insize;

    ctx.dst     = (uint8_t *)calloc(kMusDestinationChunkSize, sizeof(uint8_t));
    ctx.dst_ptr = ctx.dst;
    ctx.dstsize = kMusDestinationChunkSize;
    ctx.dstrem  = kMusDestinationChunkSize;

    /* Map channel 15 to 9 (percussions) */
    for (temp = 0; temp < kMusMidiMaxChannels; ++temp)
    {
        channelMap[temp]     = -1;
        channel_volume[temp] = 0x40;
    }
    channelMap[15] = 9;

    /* Header is 14 bytes long and add the rest as well */
    MusToMidiWrite1(&ctx, 'M');
    MusToMidiWrite1(&ctx, 'T');
    MusToMidiWrite1(&ctx, 'h');
    MusToMidiWrite1(&ctx, 'd');
    MusToMidiWrite4(&ctx, 6);            /* length of header */
    MusToMidiWrite2(&ctx, 0);            /* MIDI type (always 0) */
    MusToMidiWrite2(&ctx, 1);            /* MUS files only have 1 track */
    MusToMidiWrite2(&ctx, kMusDivision); /* division */

    /* Write out track header and track length position for later */
    begin_track_pos = MusToMidiGetDestinationPosition(&ctx);
    MusToMidiWrite1(&ctx, 'M');
    MusToMidiWrite1(&ctx, 'T');
    MusToMidiWrite1(&ctx, 'r');
    MusToMidiWrite1(&ctx, 'k');
    track_size_pos = MusToMidiGetDestinationPosition(&ctx);
    MusToMidiSkipDestination(&ctx, 4);

    /* write tempo: microseconds per quarter note */
    MusToMidiWrite1(&ctx, 0x00);   /* delta time */
    MusToMidiWrite1(&ctx, 0xff);   /* sys command */
    MusToMidiWrite2(&ctx, 0x5103); /* command - set tempo */
    MusToMidiWrite1(&ctx, kMusTempo & 0x000000ff);
    MusToMidiWrite1(&ctx, (kMusTempo & 0x0000ff00) >> 8);
    MusToMidiWrite1(&ctx, (kMusTempo & 0x00ff0000) >> 16);

    /* Percussions channel starts out at volume 100 */
    MusToMidiWrite1(&ctx, 0x00);
    MusToMidiWrite1(&ctx, 0xB9);
    MusToMidiWrite1(&ctx, 0x07);
    MusToMidiWrite1(&ctx, 100);

    /* get current position in source, and end of position */
    cur = in + header.scoreStart;
    end = cur + header.scoreLen;

    currentChannel = 0;
    delta_time     = 0;

    /* main loop */
    while (cur < end)
    {
        /*printf("LOOP DEBUG: %d\r\n",iterator++);*/
        uint8_t  channel;
        uint8_t  event;
        uint8_t  temp_buffer[32]; /* temp buffer for current iterator */
        uint8_t *out_local = temp_buffer;
        uint8_t  status, bit1, bit2, bitc = 2;

        /* read in current bit */
        event   = *cur++;
        channel = (event & 15); /* current channel */

        /* write variable length delta time */
        out_local += MusToMidiWriteVariableLength(delta_time, out_local);

        /* set all channels to 127 (max) volume */
        if (channelMap[channel] < 0)
        {
            *out_local++        = 0xB0 + currentChannel;
            *out_local++        = 0x07;
            *out_local++        = 100;
            *out_local++        = 0x00;
            channelMap[channel] = currentChannel++;
            if (currentChannel == 9) ++currentChannel;
        }
        status = channelMap[channel];

        /* handle events */
        switch ((event & 122) >> 4)
        {
            case kMusEventKeyOff:
                status |= 0x80;
                bit1 = *cur++;
                bit2 = 0x40;
                break;
            case kMusEventKeyOn:
                status |= 0x90;
                bit1 = *cur & 127;
                if (*cur++ & 128) /* volume bit? */
                    channel_volume[channelMap[channel]] = *cur++;
                bit2 = channel_volume[channelMap[channel]];
                break;
            case kMusEventPitchWheel:
                status |= 0xE0;
                bit1 = (*cur & 1) >> 6;
                bit2 = (*cur++ >> 1) & 127;
                break;
            case kMusEventChannelMode:
                status |= 0xB0;
                if (*cur >= sizeof(kMusToMidiMap) / sizeof(kMusToMidiMap[0]))
                {
                    /*_WM_ERROR_NEW("%s:%i: can't map %u to midi",
                                  __FUNCTION__, __LINE__, *cur);*/
                    goto _end;
                }
                bit1 = kMusToMidiMap[*cur++];
                bit2 = (*cur++ == 12) ? header.channels + 1 : 0x00;
                break;
            case kMusEventControllerChange:
                if (*cur == 0)
                {
                    cur++;
                    status |= 0xC0;
                    bit1 = *cur++;
                    bit2 = 0; /* silence bogus warnings */
                    bitc = 1;
                }
                else
                {
                    status |= 0xB0;
                    if (*cur >=
                        sizeof(kMusToMidiMap) / sizeof(kMusToMidiMap[0]))
                    {
                        /*_WM_ERROR_NEW("%s:%i: can't map %u to midi",
                                      __FUNCTION__, __LINE__, *cur);*/
                        goto _end;
                    }
                    bit1 = kMusToMidiMap[*cur++];
                    bit2 = *cur++;
                }
                break;
            case kMusEventEnd: /* End */
                status = 0xff;
                bit1   = 0x2f;
                bit2   = 0x00;
                if (cur != end)
                { /* should we error here or report-only? */
                    /*_WM_DEBUG_MSG("%s:%i: MUS buffer off by %ld bytes",
                                  __FUNCTION__, __LINE__, (long)(cur - end));*/
                }
                break;
            case 5:  /* Unknown */
            case 7:  /* Unknown */
            default: /* shouldn't happen */
                /*_WM_ERROR_NEW("%s:%i: unrecognized event (%u)",
                              __FUNCTION__, __LINE__, event);*/
                goto _end;
        }

        /* write it out */
        *out_local++ = status;
        *out_local++ = bit1;
        if (bitc == 2) *out_local++ = bit2;

        /* write out our temp buffer */
        if (out_local != temp_buffer)
        {
            if (ctx.dstrem < sizeof(temp_buffer))
                MusToMidiResizeDestination(&ctx);

            memcpy(ctx.dst_ptr, temp_buffer, out_local - temp_buffer);
            ctx.dst_ptr += out_local - temp_buffer;
            ctx.dstrem -= (uint32_t)(out_local - temp_buffer);
        }

        if (event & 128)
        {
            delta_time = 0;
            do {
                delta_time = (int32_t)((delta_time * 128 + (*cur & 127)) *
                                       (140.0 / (double)frequency));
            } while ((*cur++ & 128));
        }
        else { delta_time = 0; }
    }

    /* write out track length */
    current_pos = MusToMidiGetDestinationPosition(&ctx);
    MusToMidiSeekDestination(&ctx, track_size_pos);
    MusToMidiWrite4(&ctx,
                    current_pos - begin_track_pos - sizeof(MidiTrackChunk));
    MusToMidiSeekDestination(&ctx, current_pos); /* reseek to end position */

    *out     = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    ret      = 0;

_end: /* cleanup */
    if (ret < 0)
    {
        free(ctx.dst);
        *out     = nullptr;
        *outsize = 0;
    }

    return (ret);
}
