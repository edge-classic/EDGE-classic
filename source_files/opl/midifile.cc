//
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
//    Reading of MIDI files.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "midifile.h"

#define HEADER_CHUNK_ID "MThd"
#define TRACK_CHUNK_ID  "MTrk"
#define MAX_BUFFER_SIZE 0x10000

// haleyjd 09/09/10: packing required
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

#ifdef __GNUC__
#define PACKEDATTR __attribute__((packed))
#else
#define PACKEDATTR
#endif

typedef struct
{
    uint8_t  chunk_id[4];
    uint32_t chunk_size;
} PACKEDATTR chunk_header_t;

typedef struct
{
    chunk_header_t chunk_header;
    uint16_t format_type;
    uint16_t num_tracks;
    uint16_t time_division;
} PACKEDATTR midi_header_t;

// haleyjd 09/09/10: packing off.
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct
{
    // Length in bytes:

    unsigned int data_len;

    // Events in this track:

    midi_event_t *head;
    midi_event_t *tail;
    int num_events;
} midi_track_t;

struct midi_file_s
{
    midi_header_t header;

    // All tracks in this file:
    midi_track_t *tracks;
    unsigned int num_tracks;

    // Data buffer used to store data read for SysEx or meta events:
    uint8_t *buffer;
    unsigned int buffer_size;
};

uint16_t BE_SHORT(uint16_t n)
{
    uint16_t a;
    a  = (n & 0xFF) << 8;
    a |= (n >> 8) & 0xFF;
    return a;
}

uint32_t BE_LONG(uint32_t n)
{
    uint32_t a;
    a  = (n & 0xFFU)   << 24;
    a |= (n & 0xFF00U) << 8;
    a |= (n >>  8) & 0xFF00U;
    a |= (n >> 24) & 0xFFU;
    return a;
}

// Check the header of a chunk:

static bool CheckChunkHeader(chunk_header_t *chunk,
                                char *expected_id)
{
    bool result;

    result = (memcmp((char *) chunk->chunk_id, expected_id, 4) == 0);

    if (!result)
    {
        fprintf(stderr, "CheckChunkHeader: Expected '%s' chunk header, "
                        "got '%c%c%c%c'\n",
                        expected_id,
                        chunk->chunk_id[0], chunk->chunk_id[1],
                        chunk->chunk_id[2], chunk->chunk_id[3]);
    }

    return result;
}

// Read a single byte.  Returns false on error.

static bool ReadByte(uint8_t *result, MEMFILE *stream)
{
    size_t count = mem_fread(result, 1, 1, stream);

    if (count < 1)
    {
        fprintf(stderr, "ReadByte: Unexpected end of file\n");
        return false;
    }

    return true;
}

// Read a variable-length value.

static bool ReadVariableLength(unsigned int *result, MEMFILE *stream)
{
    int i;
    uint8_t b = 0;

    *result = 0;

    for (i=0; i<4; ++i)
    {
        if (!ReadByte(&b, stream))
        {
            fprintf(stderr, "ReadVariableLength: Error while reading "
                            "variable-length value\n");
            return false;
        }

        // Insert the bottom seven bits from this byte.

        *result <<= 7;
        *result |= b & 0x7f;

        // If the top bit is not set, this is the end.

        if ((b & 0x80) == 0)
        {
            return true;
        }
    }

    fprintf(stderr, "ReadVariableLength: Variable-length value too "
                    "long: maximum of four bytes\n");
    return false;
}

// Read a byte sequence into the data buffer.

static void *ReadByteSequence(unsigned int num_bytes, MEMFILE *stream)
{
    unsigned int i;
    uint8_t *result;

    // Allocate a buffer. Allocate one extra byte, as malloc(0) is
    // non-portable.

    result = malloc(num_bytes + 1);

    if (result == NULL)
    {
        fprintf(stderr, "ReadByteSequence: Failed to allocate buffer\n");
        return NULL;
    }

    // Read the data:

    for (i=0; i < num_bytes; i++)
    {
        if (!ReadByte(&result[i], stream))
        {
            fprintf(stderr, "ReadByteSequence: Error while reading byte %u\n", i);
            free(result);
            return NULL;
        }
    }

    return result;
}

// Read a MIDI channel event.
// two_param indicates that the event type takes two parameters
// (three byte) otherwise it is single parameter (two byte)

static bool ReadChannelEvent(midi_event_t *event,
                             uint8_t event_type, bool two_param,
                             MEMFILE *stream)
{
    uint8_t b = 0;

    // Set basics:

    event->event_type = event_type & 0xf0;
    event->data.channel.channel = event_type & 0x0f;

    // Read parameters:

    if (!ReadByte(&b, stream))
    {
        fprintf(stderr, "ReadChannelEvent: Error while reading channel "
                        "event parameters\n");
        return false;
    }

    event->data.channel.param1 = b;

    // Second parameter:

    if (two_param)
    {
        if (!ReadByte(&b, stream))
        {
            fprintf(stderr, "ReadChannelEvent: Error while reading channel "
                            "event parameters\n");
            return false;
        }

        event->data.channel.param2 = b;
    }

    return true;
}

// Read sysex event:

static bool ReadSysExEvent(midi_event_t *event, int event_type,
                           MEMFILE *stream)
{
    event->event_type = event_type;

    if (!ReadVariableLength(&event->data.sysex.length, stream))
    {
        fprintf(stderr, "ReadSysExEvent: Failed to read length of "
                                        "SysEx block\n");
        return false;
    }

    // Read the byte sequence:

    event->data.sysex.data = ReadByteSequence(event->data.sysex.length, stream);

    if (event->data.sysex.data == NULL)
    {
        fprintf(stderr, "ReadSysExEvent: Failed while reading SysEx event\n");
        return false;
    }

    return true;
}

// Read meta event:

static bool ReadMetaEvent(midi_event_t *event, MEMFILE *stream)
{
    uint8_t b = 0;

    event->event_type = MIDI_EVENT_META;

    // Read meta event type:

    if (!ReadByte(&b, stream))
    {
        fprintf(stderr, "ReadMetaEvent: Failed to read meta event type\n");
        return false;
    }

    event->data.meta.type = b;

    // Read length of meta event data:

    if (!ReadVariableLength(&event->data.meta.length, stream))
    {
        fprintf(stderr, "ReadSysExEvent: Failed to read length of "
                                        "SysEx block\n");
        return false;
    }

    // Read the byte sequence:

    event->data.meta.data = ReadByteSequence(event->data.meta.length, stream);

    if (event->data.meta.data == NULL)
    {
        fprintf(stderr, "ReadSysExEvent: Failed while reading SysEx event\n");
        return false;
    }

    return true;
}

static bool ReadEvent(midi_event_t *event, unsigned int *last_event_type,
                      MEMFILE *stream)
{
    uint8_t event_type = 0;

    if (!ReadVariableLength(&event->delta_time, stream))
    {
        fprintf(stderr, "ReadEvent: Failed to read event timestamp\n");
        return false;
    }

    if (!ReadByte(&event_type, stream))
    {
        fprintf(stderr, "ReadEvent: Failed to read event type\n");
        return false;
    }

    // All event types have their top bit set.  Therefore, if
    // the top bit is not set, it is because we are using the "same
    // as previous event type" shortcut to save a byte.  Skip back
    // a byte so that we read this byte again.

    if ((event_type & 0x80) == 0)
    {
        event_type = *last_event_type;

        if (mem_fseek(stream, -1, SEEK_CUR) < 0)
        {
            fprintf(stderr, "ReadEvent: Unable to seek in stream\n");
            return false;
        }
    }
    else
    {
        *last_event_type = event_type;
    }

    // Check event type:

    switch (event_type & 0xf0)
    {
        // Two parameter channel events:

        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_CONTROLLER:
        case MIDI_EVENT_PITCH_BEND:
            return ReadChannelEvent(event, event_type, true, stream);

        // Single parameter channel events:

        case MIDI_EVENT_PROGRAM_CHANGE:
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            return ReadChannelEvent(event, event_type, false, stream);

        default:
            break;
    }

    // Specific value?

    switch (event_type)
    {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            return ReadSysExEvent(event, event_type, stream);

        case MIDI_EVENT_META:
            return ReadMetaEvent(event, stream);

        default:
            break;
    }

    fprintf(stderr, "ReadEvent: Unknown MIDI event type: 0x%x\n", event_type);
    return false;
}

// Free an event:

static void FreeEvent(midi_event_t *event)
{
    // Some event types have dynamically allocated buffers assigned
    // to them that must be freed.

    switch (event->event_type)
    {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            free(event->data.sysex.data);
            break;

        case MIDI_EVENT_META:
            free(event->data.meta.data);
            break;

        default:
            // Nothing to do.
            break;
    }
}

// Read and check the track chunk header

static bool ReadTrackHeader(midi_track_t *track, MEMFILE *stream)
{
    size_t records_read;
    chunk_header_t chunk_header;

    records_read = mem_fread(&chunk_header, sizeof(chunk_header_t), 1, stream);

    if (records_read < 1)
    {
        return false;
    }

    if (!CheckChunkHeader(&chunk_header, TRACK_CHUNK_ID))
    {
        return false;
    }

    track->data_len = BE_LONG(chunk_header.chunk_size);

    return true;
}

static midi_event_t *AllocEvent(void)
{
    midi_event_t *event = (midi_event_t *)malloc(sizeof(midi_event_t));

    if (event == NULL)
    {
        fprintf(stderr, "AllocEvent: Failed to allocate event\n");
    }
    return event;
}

static void AddEvent(midi_track_t *track, midi_event_t *event)
{
    event->next = track->tail;

    if (track->tail != NULL)
    {
        track->tail->next = event;
    }
    else
    {
        track->head = event;
    }

    track->tail = event;
    track->num_events += 1;
}

static bool ReadTrack(midi_track_t *track, MEMFILE *stream)
{
    midi_event_t *new_events;
    midi_event_t *event;
    unsigned int last_event_type;

    track->num_events = 0;
    track->events = NULL;

    // Read the header:

    if (!ReadTrackHeader(track, stream))
    {
        return false;
    }

    // Then the events:

    last_event_type = 0;

    for (;;)
    {
        midi_event *event = AllocEvent();
        if (event == NULL)
        {
            return false;
        }

        // Read the next event:
        event = &track->events[track->num_events];
        if (! ReadEvent(event, &last_event_type, stream))
        {
            return false;
        }

        AddEvent(track, event);

        // End of track?

        if (event->event_type == MIDI_EVENT_META &&
            event->data.meta.type == MIDI_META_END_OF_TRACK)
        {
            break;
        }
    }

    return true;
}

// Free a track:

static void FreeTrack(midi_track_t *track)
{
    unsigned int i;

    for (i=0; i<track->num_events; ++i)
    {
        FreeEvent(&track->events[i]);
    }

    free(track->events);
}

static bool ReadAllTracks(midi_file_t *file, MEMFILE *stream)
{
    unsigned int i;

    // Allocate list of tracks and read each track:

    file->tracks = malloc(sizeof(midi_track_t) * file->num_tracks);

    if (file->tracks == NULL)
    {
        return false;
    }

    memset(file->tracks, 0, sizeof(midi_track_t) * file->num_tracks);

    // Read each track:

    for (i=0; i<file->num_tracks; ++i)
    {
        if (!ReadTrack(&file->tracks[i], stream))
        {
            return false;
        }
    }

    return true;
}

// Read and check the header chunk.

static bool ReadFileHeader(midi_file_t *file, MEMFILE *stream)
{
    size_t records_read;
    unsigned int format_type;

    records_read = mem_fread(&file->header, sizeof(midi_header_t), 1, stream);

    if (records_read < 1)
    {
        return false;
    }

    if (!CheckChunkHeader(&file->header.chunk_header, HEADER_CHUNK_ID)
     || BE_LONG(file->header.chunk_header.chunk_size) != 6)
    {
        fprintf(stderr, "ReadFileHeader: Invalid MIDI chunk header! "
                        "chunk_size=%i\n",
                        BE_LONG(file->header.chunk_header.chunk_size));
        return false;
    }

    format_type = BE_SHORT(file->header.format_type);
    file->num_tracks = BE_SHORT(file->header.num_tracks);

    if ((format_type != 0 && format_type != 1) || file->num_tracks < 1)
    {
        fprintf(stderr, "ReadFileHeader: Only type 0/1 "
                                         "MIDI files supported!\n");
        return false;
    }

    return true;
}

void MIDI_FreeFile(midi_file_t *file)
{
    int i;

    if (file->tracks != NULL)
    {
        for (i=0; i<file->num_tracks; ++i)
        {
            FreeTrack(&file->tracks[i]);
        }

        free(file->tracks);
    }

    free(file);
}

midi_file_t *MIDI_LoadFile(MEMFILE *stream)
{
    midi_file_t *file;

    file = malloc(sizeof(midi_file_t));

    if (file == NULL)
    {
        return NULL;
    }

    file->tracks = NULL;
    file->num_tracks = 0;
    file->buffer = NULL;
    file->buffer_size = 0;

    // Open file

    /*
    stream = fopen(filename, "rb");

    if (stream == NULL)
    {
        fprintf(stderr, "MIDI_LoadFile: Failed to open '%s'\n", filename);
        MIDI_FreeFile(file);
        return NULL;
    }
    */

    // Read MIDI file header

    if (!ReadFileHeader(file, stream))
    {
        mem_fclose(stream);
        MIDI_FreeFile(file);
        return NULL;
    }

    // Read all tracks:

    if (!ReadAllTracks(file, stream))
    {
        mem_fclose(stream);
        MIDI_FreeFile(file);
        return NULL;
    }

    mem_fclose(stream);

    return file;
}

// Get the number of tracks in a MIDI file.

unsigned int MIDI_NumTracks(midi_file_t *file)
{
    return file->num_tracks;
}

// Start iterating over the events in a track.

midi_event_t *MIDI_IterateTrack(midi_file_t *file, unsigned int track)
{
    assert(track < file->num_tracks);

    return file->tracks[track]->events;
}

// Get the time until the next MIDI event in a track.

unsigned int MIDI_GetDeltaTime(midi_track_iter_t *iter)
{
    if (iter->position < iter->track->num_events)
    {
        midi_event_t *next_event;

        next_event = &iter->track->events[iter->position];

        return next_event->delta_time;
    }
    else
    {
        return 0;
    }
}

unsigned int MIDI_GetFileTimeDivision(midi_file_t *file)
{
    short result = BE_SHORT(file->header.time_division);

    // Negative time division indicates SMPTE time and must be handled
    // differently.
    if (result < 0)
    {
        return (signed int)(-(result/256))
             * (signed int)(result & 0xFF);
    }
    else
    {
        return result;
    }
}
