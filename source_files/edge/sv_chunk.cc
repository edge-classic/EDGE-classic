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

#include "i_defs.h"

#include "miniz.h"

#include "math_crc.h"

#include "sv_chunk.h"

#include "filesystem.h"

#define DEBUG_GETBYTE  0
#define DEBUG_PUTBYTE  0
#define DEBUG_COMPRESS 0

#define XOR_STRING "EDGE!"
#define XOR_LEN    5

#define STRING_MARKER  0xAA
#define NULLSTR_MARKER 0xDE

#define EDGESAVE_MAGIC  "EdgeSave"
#define FIRST_CHUNK_OFS 16L

static int last_error = 0;

// maximum size that compressing could give (worst-case scenario)
#define MAX_COMP_SIZE(orig) (compressBound(orig) + 4)

// The chunk stack will never get any deeper than this
#define MAX_CHUNK_DEPTH 16

typedef struct chunk_s
{
    char s_mark[6];
    char e_mark[6];

    // read/write data.  When reading, this is only allocated/freed for
    // top level chunks (depth 0), lower chunks just point inside their
    // parent's data.  When writing, all chunks are allocated (and grow
    // bigger as needed).  Note: `end' is the byte _after_ the last one.

    unsigned char *start;
    unsigned char *end;
    unsigned char *pos;
} chunk_t;

static chunk_t chunk_stack[MAX_CHUNK_DEPTH];
static int     chunk_stack_size = 0;

static FILE        *current_fp = NULL;
static epi::CRC32 current_crc;

static bool CheckMagic(void)
{
    int i;
    int len = strlen(EDGESAVE_MAGIC);

    for (i = 0; i < len; i++)
        if (SV_GetByte() != EDGESAVE_MAGIC[i])
            return false;

    return true;
}

static void PutMagic(void)
{
    int i;
    int len = strlen(EDGESAVE_MAGIC);

    for (i = 0; i < len; i++)
        SV_PutByte(EDGESAVE_MAGIC[i]);
}

static void PutPadding(void)
{
    SV_PutByte(0x1A);
    SV_PutByte(0x0D);
    SV_PutByte(0x0A);
    SV_PutByte(0x00);
}

static inline bool VerifyMarker(const char *id)
{
    return isalnum(id[0]) && isalnum(id[1]) && isalnum(id[2]) && isalnum(id[3]);
}

void SV_ChunkInit(void)
{
    /* ZLib doesn't need to be initialised */
}

void SV_ChunkShutdown(void)
{
    // nothing to do
}

int SV_GetError(void)
{
    int result = last_error;
    last_error = 0;

    return result;
}

//----------------------------------------------------------------------------
//  READING PRIMITIVES
//----------------------------------------------------------------------------

bool SV_OpenReadFile(std::string filename)
{
    L_WriteDebug("Opening savegame file (R): %s\n", filename.c_str());

    chunk_stack_size = 0;
    last_error       = 0;

    current_crc.Reset();

    current_fp = epi::FS_OpenRawFile(filename, epi::kFileAccessRead | epi::kFileAccessBinary);

    if (!current_fp)
        return false;

    return true;
}

bool SV_CloseReadFile(void)
{
    SYS_ASSERT(current_fp);

    if (chunk_stack_size > 0)
        I_Error("SV_CloseReadFile: Too many Pushes (missing Pop somewhere).\n");

    fclose(current_fp);

    if (last_error)
        I_Warning("LOADGAME: Error(s) occurred during reading.\n");

    return true;
}

//
// Sets the version field, which is BCD, with the patch level in the
// two least significant digits.
//
bool SV_VerifyHeader(int *version)
{
    // check header

    if (!CheckMagic())
    {
        I_Warning("LOADGAME: Bad magic in savegame file\n");
        return false;
    }

    // skip padding
    SV_GetByte();
    SV_GetByte();
    SV_GetByte();
    SV_GetByte();

    // We don't do anything with version anymore, but still consume it
    (*version) = SV_GetInt();

    if (last_error)
    {
        I_Warning("LOADGAME: Bad header in savegame file\n");
        return false;
    }

    return true;
}

bool SV_VerifyContents(void)
{
    SYS_ASSERT(current_fp);
    SYS_ASSERT(chunk_stack_size == 0);

    // skip top-level chunks until end...
    for (;;)
    {
        unsigned int orig_len;
        unsigned int file_len;

        char start_marker[6];

        SV_GetMarker(start_marker);

        if (!VerifyMarker(start_marker))
        {
            I_Warning("LOADGAME: Verify failed: Invalid start marker: "
                      "%02X %02X %02X %02X\n",
                      start_marker[0], start_marker[1], start_marker[2], start_marker[3]);
            return false;
        }

        if (strcmp(start_marker, DATA_END_MARKER) == 0)
            break;

        // read chunk length
        file_len = SV_GetInt();

        // read original, uncompressed size
        orig_len = SV_GetInt();

        if ((orig_len & 3) != 0 || file_len > MAX_COMP_SIZE(orig_len))
        {
            I_Warning("LOADGAME: Verify failed: Chunk has bad size: "
                      "(file=%d orig=%d)\n",
                      file_len, orig_len);
            return false;
        }

        // skip data bytes (merely compute the CRC)
        for (; (file_len > 0) && !last_error; file_len--)
            SV_GetByte();

        // run out of data ?
        if (last_error)
        {
            I_Warning("LOADGAME: Verify failed: Chunk corrupt or "
                      "File truncated.\n");
            return false;
        }
    }

    // check trailer
    if (!CheckMagic())
    {
        I_Warning("LOADGAME: Verify failed: Bad trailer.\n");
        return false;
    }

    // CRC is now computed

    epi::CRC32 final_crc(current_crc);

    uint32_t read_crc = SV_GetInt();

    if (read_crc != final_crc.GetCRC())
    {
        I_Warning("LOADGAME: Verify failed: Bad CRC: %08X != %08X\n", current_crc.GetCRC(), read_crc);
        return false;
    }

    // Move file pointer back to beginning
    fseek(current_fp, FIRST_CHUNK_OFS, SEEK_SET);
    clearerr(current_fp);

    return true;
}

unsigned char SV_GetByte(void)
{
    chunk_t      *cur;
    unsigned char result;

    if (last_error)
        return 0;

    // read directly from file when no chunks are on the stack
    if (chunk_stack_size == 0)
    {
        int c = fgetc(current_fp);

        if (c == EOF)
        {
            I_Error("LOADGAME: Corrupt Savegame (reached EOF).\n");
            last_error = 1;
            return 0;
        }

        current_crc += (uint8_t)c;

#if (DEBUG_GETBYTE)
        {
            static int pos = 0;
            pos++;
            L_WriteDebug("%08X: %02X \n", ftell(current_fp), c);
            //			L_WriteDebug("0.%02X%s", result, ((pos % 10)==0) ? "\n" : " ");
        }
#endif

        return (unsigned char)c;
    }

    cur = &chunk_stack[chunk_stack_size - 1];

    SYS_ASSERT(cur->start);
    SYS_ASSERT(cur->pos >= cur->start);
    SYS_ASSERT(cur->pos <= cur->end);

    if (cur->pos == cur->end)
    {
        I_Error("LOADGAME: Corrupt Savegame (reached end of [%s] chunk).\n", cur->s_mark);
        last_error = 2;
        return 0;
    }

    result = cur->pos[0];
    cur->pos++;

#if (DEBUG_GETBYTE)
    {
        static int pos = 0;
        pos++;
        L_WriteDebug("%d.%02X%s", chunk_stack_size, result, ((pos % 10) == 0) ? "\n" : " ");
    }
#endif

    return result;
}

bool SV_PushReadChunk(const char *id)
{
    chunk_t     *cur;
    unsigned int file_len;

    if (chunk_stack_size >= MAX_CHUNK_DEPTH)
        I_Error("SV_PushReadChunk: Too many Pushes (missing Pop somewhere).\n");

    // read chunk length
    file_len = SV_GetInt();

    // create new chunk_t
    cur = &chunk_stack[chunk_stack_size];

    strcpy(cur->s_mark, id);
    strcpy(cur->e_mark, id);
    for (size_t i = 0; i < strlen(cur->e_mark); i++)
    {
        cur->e_mark[i] = toupper(cur->e_mark[i]);
    }

    // top level chunk ?
    if (chunk_stack_size == 0)
    {
        unsigned int i;

        unsigned int orig_len;
        unsigned int decomp_len;

        // read uncompressed size
        orig_len = SV_GetInt();

        SYS_ASSERT(file_len <= MAX_COMP_SIZE(orig_len));

        uint8_t *file_data = new uint8_t[file_len + 1];

        for (i = 0; (i < file_len) && !last_error; i++)
            file_data[i] = SV_GetByte();

        SYS_ASSERT(!last_error);

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
        else // use ZLIB
        {
            SYS_ASSERT(file_len > 0);
            SYS_ASSERT(file_len < orig_len);

            uLongf out_len = orig_len;

            int res = uncompress(cur->start, &out_len, file_data, file_len);

            if (res != Z_OK)
                I_Error("LOADGAME: ReadChunk [%s] failed: ZLIB uncompress error.\n", id);

            decomp_len = (unsigned int)out_len;
        }

        SYS_ASSERT(decomp_len == orig_len);

        delete[] file_data;
    }
    else
    {
        chunk_t *parent = &chunk_stack[chunk_stack_size - 1];

        cur->start = parent->pos;
        cur->end   = cur->start + file_len;

        // skip data in parent
        parent->pos += file_len;

        SYS_ASSERT(parent->pos >= parent->start);
        SYS_ASSERT(parent->pos <= parent->end);
    }

    cur->pos = cur->start;

    // let the SV_GetByte routine (etc) see the new chunk
    chunk_stack_size++;
    return true;
}

bool SV_PopReadChunk(void)
{
    chunk_t *cur;

    if (chunk_stack_size == 0)
        I_Error("SV_PopReadChunk: Too many Pops (missing Push somewhere).\n");

    cur = &chunk_stack[chunk_stack_size - 1];

    if (chunk_stack_size == 1)
    {
        // free the data
        delete[] cur->start;
    }

    cur->start = cur->pos = cur->end = NULL;
    chunk_stack_size--;

    return true;
}

int SV_RemainingChunkSize(void)
{
    chunk_t *cur;

    SYS_ASSERT(chunk_stack_size > 0);

    cur = &chunk_stack[chunk_stack_size - 1];

    SYS_ASSERT(cur->pos >= cur->start);
    SYS_ASSERT(cur->pos <= cur->end);

    return (cur->end - cur->pos);
}

bool SV_SkipReadChunk(const char *id)
{
    if (!SV_PushReadChunk(id))
        return false;

    return SV_PopReadChunk();
}

//----------------------------------------------------------------------------
//  WRITING PRIMITIVES
//----------------------------------------------------------------------------

bool SV_OpenWriteFile(std::string filename, int version)
{
    L_WriteDebug("Opening savegame file (W): %s\n", filename.c_str());

    chunk_stack_size = 0;
    last_error       = 0;

    current_crc.Reset();

    current_fp = epi::FS_OpenRawFile(filename, epi::kFileAccessWrite | epi::kFileAccessBinary);

    if (!current_fp)
    {
        I_Warning("SAVEGAME: Couldn't open file: %s\n", filename.c_str());
        return false;
    }

    // write header

    PutMagic();
    PutPadding();
    SV_PutInt(version);

    return true;
}

bool SV_CloseWriteFile(void)
{
    SYS_ASSERT(current_fp);

    if (chunk_stack_size != 0)
        I_Error("SV_CloseWriteFile: Too many Pushes (missing Pop somewhere).\n");

    // write trailer

    SV_PutMarker(DATA_END_MARKER);
    PutMagic();

    epi::CRC32 final_crc(current_crc);

    SV_PutInt(final_crc.GetCRC());

    if (last_error)
        I_Warning("SAVEGAME: Error(s) occurred during writing.\n");

    fclose(current_fp);

    return true;
}

bool SV_PushWriteChunk(const char *id)
{
    chunk_t *cur;

    if (chunk_stack_size >= MAX_CHUNK_DEPTH)
        I_Error("SV_PushWriteChunk: Too many Pushes (missing Pop somewhere).\n");

    // create new chunk_t
    cur = &chunk_stack[chunk_stack_size];
    chunk_stack_size++;

    strcpy(cur->s_mark, id);
    strcpy(cur->e_mark, id);
    for (size_t i = 0; i < strlen(cur->e_mark); i++)
    {
        cur->e_mark[i] = toupper(cur->e_mark[i]);
    }

    // create initial buffer
    cur->start = new uint8_t[1024];
    cur->pos   = cur->start;
    cur->end   = cur->start + 1024;

    return true;
}

bool SV_PopWriteChunk(void)
{
    int      i;
    chunk_t *cur;
    int      len;

    if (chunk_stack_size == 0)
        I_Error("SV_PopWriteChunk: Too many Pops (missing Push somewhere).\n");

    cur = &chunk_stack[chunk_stack_size - 1];

    SYS_ASSERT(cur->start);
    SYS_ASSERT(cur->pos >= cur->start);
    SYS_ASSERT(cur->pos <= cur->end);

    len = cur->pos - cur->start;

    // pad chunk to multiple of 4 characters
    for (; len & 3; len++)
        SV_PutByte(0);

    // decrement stack size, so future PutBytes go where they should
    chunk_stack_size--;

    // firstly, write out marker
    SV_PutMarker(cur->s_mark);

    // write out data.  For top-level chunks, compress it.

    if (chunk_stack_size == 0)
    {
        uLongf out_len = MAX_COMP_SIZE(len);

        uint8_t *out_buf = new uint8_t[out_len + 1];

        int res = compress2(out_buf, &out_len, cur->start, len, Z_BEST_SPEED);

        if (res != Z_OK || (int)out_len >= len)
        {
#if (DEBUG_COMPRESS)
            L_WriteDebug("WriteChunk UNCOMPRESSED (res %d != %d, out_len %d >= %d)\n", res, Z_OK, (int)out_len, len);
#endif
            // compression failed, so write uncompressed
            memcpy(out_buf, cur->start, len);
            out_len = len;
        }
#if (DEBUG_COMPRESS)
        else
        {
            L_WriteDebug("WriteChunk compress (res %d == %d, out_len %d < %d)\n", res, Z_OK, (int)out_len, len);
        }
#endif

        SYS_ASSERT((int)out_len <= (int)MAX_COMP_SIZE(len));

        // write compressed length
        SV_PutInt((int)out_len);

        // write original length to parent
        SV_PutInt(len);

        for (i = 0; i < (int)out_len && !last_error; i++)
            SV_PutByte(out_buf[i]);

        SYS_ASSERT(!last_error);

        delete[] out_buf;
    }
    else
    {
        // write chunk length to parent
        SV_PutInt(len);

        // FIXME: optimise this (transfer data directly into parent)
        for (i = 0; i < len; i++)
            SV_PutByte(cur->start[i]);
    }

    // all done, free stuff
    delete[] cur->start;

    cur->start = cur->pos = cur->end = NULL;
    return true;
}

void SV_PutByte(unsigned char value)
{
    chunk_t *cur;

#if (DEBUG_PUTBYTE)
    {
        static int pos = 0;
        pos++;
        L_WriteDebug("%d.%02x%s", chunk_stack_size, value, ((pos % 10) == 0) ? "\n" : " ");
    }
#endif

    if (last_error)
        return;

    // write directly to the file when chunk stack is empty
    if (chunk_stack_size == 0)
    {
        fputc(value, current_fp);

        if (ferror(current_fp))
        {
            I_Warning("SAVEGAME: Write error occurred !\n");
            last_error = 3;
            return;
        }

        current_crc += (uint8_t)value;
        return;
    }

    cur = &chunk_stack[chunk_stack_size - 1];

    SYS_ASSERT(cur->start);
    SYS_ASSERT(cur->pos >= cur->start);
    SYS_ASSERT(cur->pos <= cur->end);

    // space left in chunk ?  If not, resize it.
    if (cur->pos == cur->end)
    {
        int old_len = (cur->end - cur->start);
        int new_len = old_len * 2;
        int pos_idx = (cur->pos - cur->start);

        uint8_t *new_start = new uint8_t[new_len];
        memcpy(new_start, cur->start, old_len);

        delete[] cur->start;
        cur->start = new_start;

        cur->end = cur->start + new_len;
        cur->pos = cur->start + pos_idx;
    }

    *(cur->pos++) = value;
}

//----------------------------------------------------------------------------

//
//  BASIC DATATYPES
//

void SV_PutShort(unsigned short value)
{
    SV_PutByte(value & 0xff);
    SV_PutByte(value >> 8);
}

void SV_PutInt(unsigned int value)
{
    SV_PutShort(value & 0xffff);
    SV_PutShort(value >> 16);
}

unsigned short SV_GetShort(void)
{
    // -ACB- 2004/02/08 Force the order of execution; otherwise
    // compilr optimisations may reverse the order of execution
    uint8_t b1 = SV_GetByte();
    uint8_t b2 = SV_GetByte();
    return b1 | (b2 << 8);
}

unsigned int SV_GetInt(void)
{
    // -ACB- 2004/02/08 Force the order of execution; otherwise
    // compiler optimisations may reverse the order of execution
    unsigned short s1 = SV_GetShort();
    unsigned short s2 = SV_GetShort();
    return s1 | (s2 << 16);
}

//----------------------------------------------------------------------------

//
//  ANGLES
//

void SV_PutAngle(BAMAngle value)
{
    SV_PutInt((unsigned int)value);
}

BAMAngle SV_GetAngle(void)
{
    return (BAMAngle)SV_GetInt();
}

//----------------------------------------------------------------------------

//
//  FLOATING POINT
//

void SV_PutFloat(float value)
{
    int  exp;
    int  mant;
    bool neg;

    neg = (value < 0.0f);
    if (neg)
        value = -value;

    mant = (int)ldexp(frexp(value, &exp), 30);

    SV_PutShort(256 + exp);
    SV_PutInt((unsigned int)(neg ? -mant : mant));
}

float SV_GetFloat(void)
{
    int exp;
    int mant;

    exp  = SV_GetShort() - 256;
    mant = (int)SV_GetInt();

    return (float)ldexp((float)mant, -30 + exp);
}

//----------------------------------------------------------------------------

//
//  STRINGS & MARKERS
//

void SV_PutString(const char *str)
{
    if (str == NULL)
    {
        SV_PutByte(NULLSTR_MARKER);
        return;
    }

    SV_PutByte(STRING_MARKER);
    SV_PutShort(strlen(str));

    for (; *str; str++)
        SV_PutByte(*str);
}

void SV_PutMarker(const char *id)
{
    int i;

    // I_Printf("ID: %s\n", id);

    SYS_ASSERT(id);
    SYS_ASSERT(strlen(id) == 4);

    for (i = 0; i < 4; i++)
        SV_PutByte((unsigned char)id[i]);
}

const char *SV_GetString(void)
{
    int type = SV_GetByte();

    if (type == NULLSTR_MARKER)
        return NULL;

    if (type != STRING_MARKER)
        I_Error("Corrupt savegame (invalid string).\n");

    int len = SV_GetShort();

    char *result = new char[len + 1];
    result[len]  = 0;

    for (int i = 0; i < len; i++)
        result[i] = (char)SV_GetByte();

    return result;
}

const char *SV_DupString(const char *old)
{
    if (!old)
        return NULL;

    char *result = new char[strlen(old) + 1];

    strcpy(result, old);

    return result;
}

void SV_FreeString(const char *str)
{
    if (str)
        delete[] str;
}

bool SV_GetMarker(char id[5])
{
    int i;

    for (i = 0; i < 4; i++)
        id[i] = (char)SV_GetByte();

    id[4] = 0;

    return VerifyMarker(id);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
