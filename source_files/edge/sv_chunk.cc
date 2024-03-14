//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Chunks)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
//
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// -AJA- 2000/07/13: Wrote this file.
//

#include "sv_chunk.h"

#include "epi.h"
#include "filesystem.h"
#include "i_system.h"
#include "math_crc.h"
#include "miniz.h"

#define EDGE_DEBUG_SAVE_GET_BYTE       0
#define EDGE_DEBUG_SAVE_PUT_BYTE       0
#define EDGE_DEBUG_SAVE_CHUNK_COMPRESS 0

static constexpr uint8_t kStringMarker     = 0xAA;
static constexpr uint8_t kNullStringMarker = 0xDE;
static constexpr const char *kEdgeSaveMagic    = "EdgeSave";
static constexpr uint8_t kFirstChunkOffset = 16;
// The chunk stack will never get any deeper than this
static constexpr uint8_t kMaximumChunkDepth = 16;

static int last_error = 0;

struct SaveChunk
{
    char start_marker[6];
    char end_marker[6];

    // read/write data.  When reading, this is only allocated/freed for
    // top level chunks (depth 0), lower chunks just point inside their
    // parent's data.  When writing, all chunks are allocated (and grow
    // bigger as needed).  Note: `end' is the byte _after_ the last one.

    uint8_t *start;
    uint8_t *end;
    uint8_t *position;
};

static SaveChunk chunk_stack[kMaximumChunkDepth];
static int       chunk_stack_size = 0;

static FILE      *current_file_pointer = nullptr;
static epi::CRC32 current_crc;

static bool CheckMagic(void)
{
    int i;
    int len = strlen(kEdgeSaveMagic);

    for (i = 0; i < len; i++)
        if (SaveChunkGetByte() != kEdgeSaveMagic[i]) return false;

    return true;
}

static void PutMagic(void)
{
    int i;
    int len = strlen(kEdgeSaveMagic);

    for (i = 0; i < len; i++) SaveChunkPutByte(kEdgeSaveMagic[i]);
}

static void PutPadding(void)
{
    SaveChunkPutByte(0x1A);
    SaveChunkPutByte(0x0D);
    SaveChunkPutByte(0x0A);
    SaveChunkPutByte(0x00);
}

static inline bool VerifyMarker(const char *id)
{
    return epi::IsAlphanumericASCII(id[0]) && epi::IsAlphanumericASCII(id[1]) &&
           epi::IsAlphanumericASCII(id[2]) && epi::IsAlphanumericASCII(id[3]);
}

int SaveGetError(void)
{
    int result = last_error;
    last_error = 0;

    return result;
}

//----------------------------------------------------------------------------
//  READING PRIMITIVES
//----------------------------------------------------------------------------

bool SaveFileOpenRead(std::string filename)
{
    LogDebug("Opening savegame file (R): %s\n", filename.c_str());

    chunk_stack_size = 0;
    last_error       = 0;

    current_crc.Reset();

    current_file_pointer = epi::FileOpenRaw(
        filename, epi::kFileAccessRead | epi::kFileAccessBinary);

    if (!current_file_pointer) return false;

    return true;
}

bool SaveFileCloseRead(void)
{
    EPI_ASSERT(current_file_pointer);

    if (chunk_stack_size > 0)
        FatalError(
            "SV_CloseReadFile: Too many Pushes (missing Pop somewhere).\n");

    fclose(current_file_pointer);

    if (last_error) LogWarning("LOADGAME: Error(s) occurred during reading.\n");

    return true;
}

//
// Sets the version field, which is BCD, with the patch level in the
// two least significant digits.
//
bool SaveFileVerifyHeader(int *version)
{
    // check header

    if (!CheckMagic())
    {
        LogWarning("LOADGAME: Bad magic in savegame file\n");
        return false;
    }

    // skip padding
    SaveChunkGetByte();
    SaveChunkGetByte();
    SaveChunkGetByte();
    SaveChunkGetByte();

    // We don't do anything with version anymore, but still consume it
    (*version) = SaveChunkGetInteger();

    if (last_error)
    {
        LogWarning("LOADGAME: Bad header in savegame file\n");
        return false;
    }

    return true;
}

bool SaveFileVerifyContents(void)
{
    EPI_ASSERT(current_file_pointer);
    EPI_ASSERT(chunk_stack_size == 0);

    // skip top-level chunks until end...
    for (;;)
    {
        uint32_t orig_len;
        uint32_t file_len;

        char start_marker[6];

        SaveChunkGetMarker(start_marker);

        if (!VerifyMarker(start_marker))
        {
            LogWarning(
                "LOADGAME: Verify failed: Invalid start marker: "
                "%02X %02X %02X %02X\n",
                start_marker[0], start_marker[1], start_marker[2],
                start_marker[3]);
            return false;
        }

        if (strcmp(start_marker, kDataEndMarker) == 0) break;

        // read chunk length
        file_len = SaveChunkGetInteger();

        // read original, uncompressed size
        orig_len = SaveChunkGetInteger();

        if ((orig_len & 3) != 0 || file_len > (compressBound(orig_len) + 4))
        {
            LogWarning(
                "LOADGAME: Verify failed: Chunk has bad size: "
                "(file=%d orig=%d)\n",
                file_len, orig_len);
            return false;
        }

        // skip data bytes (merely compute the CRC)
        for (; (file_len > 0) && !last_error; file_len--) SaveChunkGetByte();

        // run out of data ?
        if (last_error)
        {
            LogWarning(
                "LOADGAME: Verify failed: Chunk corrupt or "
                "File truncated.\n");
            return false;
        }
    }

    // check trailer
    if (!CheckMagic())
    {
        LogWarning("LOADGAME: Verify failed: Bad trailer.\n");
        return false;
    }

    // CRC is now computed

    epi::CRC32 final_crc(current_crc);

    uint32_t read_crc = SaveChunkGetInteger();

    if (read_crc != final_crc.GetCRC())
    {
        LogWarning("LOADGAME: Verify failed: Bad CRC: %08X != %08X\n",
                   current_crc.GetCRC(), read_crc);
        return false;
    }

    // Move file pointer back to beginning
    fseek(current_file_pointer, kFirstChunkOffset, SEEK_SET);
    clearerr(current_file_pointer);

    return true;
}

uint8_t SaveChunkGetByte(void)
{
    SaveChunk *cur;
    uint8_t    result;

    if (last_error) return 0;

    // read directly from file when no chunks are on the stack
    if (chunk_stack_size == 0)
    {
        int c = fgetc(current_file_pointer);

        if (c == EOF)
        {
            FatalError("LOADGAME: Corrupt Savegame (reached EOF).\n");
            last_error = 1;
            return 0;
        }

        current_crc += (uint8_t)c;

#if (EDGE_DEBUG_SAVE_GET_BYTE)
        {
            static int position = 0;
            position++;
            LogDebug("%08X: %02X \n", ftell(current_file_pointer), c);
            //			LogDebug("0.%02X%s", result, ((position %
            // 10)==0) ?
            //"\n" : " ");
        }
#endif

        return (uint8_t)c;
    }

    cur = &chunk_stack[chunk_stack_size - 1];

    EPI_ASSERT(cur->start);
    EPI_ASSERT(cur->position >= cur->start);
    EPI_ASSERT(cur->position <= cur->end);

    if (cur->position == cur->end)
    {
        FatalError("LOADGAME: Corrupt Savegame (reached end of [%s] chunk).\n",
                   cur->start_marker);
        last_error = 2;
        return 0;
    }

    result = cur->position[0];
    cur->position++;

#if (EDGE_DEBUG_SAVE_GET_BYTE)
    {
        static int position = 0;
        position++;
        LogDebug("%d.%02X%s", chunk_stack_size, result,
                 ((position % 10) == 0) ? "\n" : " ");
    }
#endif

    return result;
}

bool SavePushReadChunk(const char *id)
{
    SaveChunk *cur;
    uint32_t   file_len;

    if (chunk_stack_size >= kMaximumChunkDepth)
        FatalError(
            "SV_PushReadChunk: Too many Pushes (missing Pop somewhere).\n");

    // read chunk length
    file_len = SaveChunkGetInteger();

    // create new chunk_t
    cur = &chunk_stack[chunk_stack_size];

    strcpy(cur->start_marker, id);
    strcpy(cur->end_marker, id);
    for (size_t i = 0; i < strlen(cur->end_marker); i++)
    {
        cur->end_marker[i] = epi::ToUpperASCII(cur->end_marker[i]);
    }

    // top level chunk ?
    if (chunk_stack_size == 0)
    {
        uint32_t i;

        uint32_t orig_len;
        uint32_t decomp_len;

        // read uncompressed size
        orig_len = SaveChunkGetInteger();

        EPI_ASSERT(file_len <= (compressBound(orig_len) + 4));

        uint8_t *file_data = new uint8_t[file_len + 1];

        for (i = 0; (i < file_len) && !last_error; i++)
            file_data[i] = SaveChunkGetByte();

        EPI_ASSERT(!last_error);

        cur->start = new uint8_t[orig_len + 1];
        cur->end   = cur->start + orig_len;

        // decompress data
        decomp_len = orig_len;

        if (orig_len == file_len)
        {
            // no compression
            memcpy(cur->start, file_data, file_len);
            decomp_len = file_len;
        }
        else  // use ZLIB
        {
            EPI_ASSERT(file_len > 0);
            EPI_ASSERT(file_len < orig_len);

            uLongf out_len = orig_len;

            int res = uncompress(cur->start, &out_len, file_data, file_len);

            if (res != Z_OK)
                FatalError(
                    "LOADGAME: ReadChunk [%s] failed: ZLIB uncompress error.\n",
                    id);

            decomp_len = (uint32_t)out_len;
        }

        EPI_ASSERT(decomp_len == orig_len);

        delete[] file_data;
    }
    else
    {
        SaveChunk *parent = &chunk_stack[chunk_stack_size - 1];

        cur->start = parent->position;
        cur->end   = cur->start + file_len;

        // skip data in parent
        parent->position += file_len;

        EPI_ASSERT(parent->position >= parent->start);
        EPI_ASSERT(parent->position <= parent->end);
    }

    cur->position = cur->start;

    // let the SV_GetByte routine (etc) see the new chunk
    chunk_stack_size++;
    return true;
}

bool SavePopReadChunk(void)
{
    SaveChunk *cur;

    if (chunk_stack_size == 0)
        FatalError(
            "SV_PopReadChunk: Too many Pops (missing Push somewhere).\n");

    cur = &chunk_stack[chunk_stack_size - 1];

    if (chunk_stack_size == 1)
    {
        // free the data
        delete[] cur->start;
    }

    cur->start = cur->position = cur->end = nullptr;
    chunk_stack_size--;

    return true;
}

int SaveRemainingChunkSize(void)
{
    SaveChunk *cur;

    EPI_ASSERT(chunk_stack_size > 0);

    cur = &chunk_stack[chunk_stack_size - 1];

    EPI_ASSERT(cur->position >= cur->start);
    EPI_ASSERT(cur->position <= cur->end);

    return (cur->end - cur->position);
}

bool SaveSkipReadChunk(const char *id)
{
    if (!SavePushReadChunk(id)) return false;

    return SavePopReadChunk();
}

//----------------------------------------------------------------------------
//  WRITING PRIMITIVES
//----------------------------------------------------------------------------

bool SaveFileOpenWrite(std::string filename, int version)
{
    LogDebug("Opening savegame file (W): %s\n", filename.c_str());

    chunk_stack_size = 0;
    last_error       = 0;

    current_crc.Reset();

    current_file_pointer = epi::FileOpenRaw(
        filename, epi::kFileAccessWrite | epi::kFileAccessBinary);

    if (!current_file_pointer)
    {
        LogWarning("SAVEGAME: Couldn't open file: %s\n", filename.c_str());
        return false;
    }

    // write header

    PutMagic();
    PutPadding();
    SaveChunkPutInteger(version);

    return true;
}

bool SaveFileCloseWrite(void)
{
    EPI_ASSERT(current_file_pointer);

    if (chunk_stack_size != 0)
        FatalError(
            "SV_CloseWriteFile: Too many Pushes (missing Pop somewhere).\n");

    // write trailer

    SaveChunkPutMarker(kDataEndMarker);
    PutMagic();

    epi::CRC32 final_crc(current_crc);

    SaveChunkPutInteger(final_crc.GetCRC());

    if (last_error) LogWarning("SAVEGAME: Error(s) occurred during writing.\n");

    fclose(current_file_pointer);

    return true;
}

bool SavePushWriteChunk(const char *id)
{
    SaveChunk *cur;

    if (chunk_stack_size >= kMaximumChunkDepth)
        FatalError(
            "SV_PushWriteChunk: Too many Pushes (missing Pop somewhere).\n");

    // create new chunk_t
    cur = &chunk_stack[chunk_stack_size];
    chunk_stack_size++;

    strcpy(cur->start_marker, id);
    strcpy(cur->end_marker, id);
    for (size_t i = 0; i < strlen(cur->end_marker); i++)
    {
        cur->end_marker[i] = epi::ToUpperASCII(cur->end_marker[i]);
    }

    // create initial buffer
    cur->start    = new uint8_t[1024];
    cur->position = cur->start;
    cur->end      = cur->start + 1024;

    return true;
}

bool SavePopWriteChunk(void)
{
    int        i;
    SaveChunk *cur;
    int        len;

    if (chunk_stack_size == 0)
        FatalError(
            "SV_PopWriteChunk: Too many Pops (missing Push somewhere).\n");

    cur = &chunk_stack[chunk_stack_size - 1];

    EPI_ASSERT(cur->start);
    EPI_ASSERT(cur->position >= cur->start);
    EPI_ASSERT(cur->position <= cur->end);

    len = cur->position - cur->start;

    // pad chunk to multiple of 4 characters
    for (; len & 3; len++) SaveChunkPutByte(0);

    // decrement stack size, so future PutBytes go where they should
    chunk_stack_size--;

    // firstly, write out marker
    SaveChunkPutMarker(cur->start_marker);

    // write out data.  For top-level chunks, compress it.

    if (chunk_stack_size == 0)
    {
        uLongf out_len = (compressBound(len) + 4);

        uint8_t *out_buf = new uint8_t[out_len + 1];

        int res = compress2(out_buf, &out_len, cur->start, len, Z_BEST_SPEED);

        if (res != Z_OK || (int)out_len >= len)
        {
#if (EDGE_DEBUG_SAVE_CHUNK_COMPRESS)
            LogDebug(
                "WriteChunk UNCOMPRESSED (res %d != %d, out_len %d >= %d)\n",
                res, Z_OK, (int)out_len, len);
#endif
            // compression failed, so write uncompressed
            memcpy(out_buf, cur->start, len);
            out_len = len;
        }
#if (EDGE_DEBUG_SAVE_CHUNK_COMPRESS)
        else
        {
            LogDebug("WriteChunk compress (res %d == %d, out_len %d < %d)\n",
                     res, Z_OK, (int)out_len, len);
        }
#endif

        EPI_ASSERT((int)out_len <= (int)(compressBound(len) + 4));

        // write compressed length
        SaveChunkPutInteger((int)out_len);

        // write original length to parent
        SaveChunkPutInteger(len);

        for (i = 0; i < (int)out_len && !last_error; i++)
            SaveChunkPutByte(out_buf[i]);

        EPI_ASSERT(!last_error);

        delete[] out_buf;
    }
    else
    {
        // write chunk length to parent
        SaveChunkPutInteger(len);

        // FIXME: optimise this (transfer data directly into parent)
        for (i = 0; i < len; i++) SaveChunkPutByte(cur->start[i]);
    }

    // all done, free stuff
    delete[] cur->start;

    cur->start = cur->position = cur->end = nullptr;
    return true;
}

void SaveChunkPutByte(uint8_t value)
{
    SaveChunk *cur;

#if (EDGE_DEBUG_SAVE_PUT_BYTE)
    {
        static int position = 0;
        position++;
        LogDebug("%d.%02x%s", chunk_stack_size, value,
                 ((position % 10) == 0) ? "\n" : " ");
    }
#endif

    if (last_error) return;

    // write directly to the file when chunk stack is empty
    if (chunk_stack_size == 0)
    {
        fputc(value, current_file_pointer);

        if (ferror(current_file_pointer))
        {
            LogWarning("SAVEGAME: Write error occurred !\n");
            last_error = 3;
            return;
        }

        current_crc += (uint8_t)value;
        return;
    }

    cur = &chunk_stack[chunk_stack_size - 1];

    EPI_ASSERT(cur->start);
    EPI_ASSERT(cur->position >= cur->start);
    EPI_ASSERT(cur->position <= cur->end);

    // space left in chunk ?  If not, resize it.
    if (cur->position == cur->end)
    {
        int old_len      = (cur->end - cur->start);
        int new_len      = old_len * 2;
        int position_idx = (cur->position - cur->start);

        uint8_t *new_start = new uint8_t[new_len];
        memcpy(new_start, cur->start, old_len);

        delete[] cur->start;
        cur->start = new_start;

        cur->end      = cur->start + new_len;
        cur->position = cur->start + position_idx;
    }

    *(cur->position++) = value;
}

//----------------------------------------------------------------------------

//
//  BASIC DATATYPES
//

void SaveChunkPutShort(uint16_t value)
{
    SaveChunkPutByte(value & 0xff);
    SaveChunkPutByte(value >> 8);
}

void SaveChunkPutInteger(uint32_t value)
{
    SaveChunkPutShort(value & 0xffff);
    SaveChunkPutShort(value >> 16);
}

uint16_t SaveChunkGetShort(void)
{
    // -ACB- 2004/02/08 Force the order of execution; otherwise
    // compilr optimisations may reverse the order of execution
    uint8_t b1 = SaveChunkGetByte();
    uint8_t b2 = SaveChunkGetByte();
    return b1 | (b2 << 8);
}

uint32_t SaveChunkGetInteger(void)
{
    // -ACB- 2004/02/08 Force the order of execution; otherwise
    // compiler optimisations may reverse the order of execution
    uint16_t s1 = SaveChunkGetShort();
    uint16_t s2 = SaveChunkGetShort();
    return s1 | (s2 << 16);
}

//----------------------------------------------------------------------------

//
//  ANGLES
//

void SaveChunkPutAngle(BAMAngle value) { SaveChunkPutInteger((uint32_t)value); }

BAMAngle SaveChunkGetAngle(void) { return (BAMAngle)SaveChunkGetInteger(); }

//----------------------------------------------------------------------------

//
//  FLOATING POINT
//

void SaveChunkPutFloat(float value)
{
    int  exp;
    int  mant;
    bool neg;

    neg = (value < 0.0f);
    if (neg) value = -value;

    mant = (int)ldexp(frexp(value, &exp), 30);

    SaveChunkPutShort(256 + exp);
    SaveChunkPutInteger((uint32_t)(neg ? -mant : mant));
}

float SaveChunkGetFloat(void)
{
    int exp;
    int mant;

    exp  = SaveChunkGetShort() - 256;
    mant = (int)SaveChunkGetInteger();

    return (float)ldexp((float)mant, -30 + exp);
}

//----------------------------------------------------------------------------

//
//  STRINGS & MARKERS
//

void SaveChunkPutString(const char *str)
{
    if (str == nullptr)
    {
        SaveChunkPutByte(kNullStringMarker);
        return;
    }

    SaveChunkPutByte(kStringMarker);
    SaveChunkPutShort(strlen(str));

    for (; *str; str++) SaveChunkPutByte(*str);
}

void SaveChunkPutMarker(const char *id)
{
    int i;

    // LogPrint("ID: %s\n", id);

    EPI_ASSERT(id);
    EPI_ASSERT(strlen(id) == 4);

    for (i = 0; i < 4; i++) SaveChunkPutByte((uint8_t)id[i]);
}

const char *SaveChunkGetString(void)
{
    int type = SaveChunkGetByte();

    if (type == kNullStringMarker) return nullptr;

    if (type != kStringMarker)
        FatalError("Corrupt savegame (invalid string).\n");

    int len = SaveChunkGetShort();

    char *result = new char[len + 1];
    result[len]  = 0;

    for (int i = 0; i < len; i++) result[i] = (char)SaveChunkGetByte();

    return result;
}

const char *SaveChunkCopyString(const char *old)
{
    if (!old) return nullptr;

    char *result = new char[strlen(old) + 1];

    strcpy(result, old);

    return result;
}

void SaveChunkFreeString(const char *str)
{
    if (str) delete[] str;
}

bool SaveChunkGetMarker(char id[5])
{
    int i;

    for (i = 0; i < 4; i++) id[i] = (char)SaveChunkGetByte();

    id[4] = 0;

    return VerifyMarker(id);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
