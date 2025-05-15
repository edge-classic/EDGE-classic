//----------------------------------------------------------------------------------
//  Mod4Play IT/S3M/XM/MOD/FT Module Replayer
//----------------------------------------------------------------------------------
//
// BSD 3-Clause License
//
// Copyright (c) 2023-2025, dashodanger
// Copyright (c) 2020-2021, Olav Sørensen
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------
//
// This is a consolidation of 8bitbubsy's efforts in porting Impulse Tracker 2 and
// Fast Tracker 2 to C, in order to make it easier to embed in game engines, etc.
//
// The code appears in this order:
// - MEMFILE: A FILE* like interface to an in-memory buffer
// - MMCMP: MMCMP Unpacker
// - Fast Tracker 2: XM/MOD/FT Playback
// - Impulse Tracker 2: IT/S3M Playback
// - Mod4Play: Public Unified Interface
//
// Each portion is laid out with the following sections (if applicable):
// - Defines
// - Typedefs
// - Declarations: Both forward declarations and certain variables
// - Implementation: The actual function bodies
// - Tables: Lookup/ramp tables, etc
//
// Everything outside of the Mod4Play implementation functions has been declared
// static.
//
// Define `M4P_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation. You can additionally define M4P_MALLOC,
// M4P_CALLOC and M4P_FREE in order to override usage of regular malloc/calloc/free.
//----------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

// See if song in memory is a supported type (IT/S3M/XM/MOD)
int m4p_TestFromData(uint8_t *Data, uint32_t DataLen);

// Load song from memory and initialize appropriate replayer
bool m4p_LoadFromData(uint8_t *Data, uint32_t DataLen, int32_t mixingFrequency, int32_t mixingBufferSize);

// Set replayer status to Play (does not generate output)
void m4p_PlaySong(void);

// Generate samples and fill buffer
void m4p_GenerateSamples(int16_t *buffer, int32_t numSamples);

// Generate samples and fill buffer
void m4p_GenerateFloatSamples(float *buffer, int32_t numSamples);

// Set replayer status to Stop
void m4p_Stop(void);

// De-initialize replayer
void m4p_Close(void);

// Free song memfile
void m4p_FreeSong(void);

#ifdef __cplusplus
}
#endif

#ifdef M4P_IMPLEMENTATION

#ifdef __cplusplus
extern "C"
{
#endif

#include <assert.h>
#include <math.h>
#ifndef _WIN32
#include <limits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define these to use custom memory management functions
#ifndef M4P_MALLOC
#define M4P_MALLOC malloc
#endif
#ifndef M4P_CALLOC
#define M4P_CALLOC calloc
#endif
#ifndef M4P_FREE
#define M4P_FREE free
#endif

//-----------------------------------------------------------------------------------
// 							Typedefs - MEMFILE
//-----------------------------------------------------------------------------------
typedef struct
{
    uint8_t *_ptr, *_base;
    bool     _eof;
    size_t   _cnt, _bufsiz;
} MEMFILE;

//-----------------------------------------------------------------------------------
// 							Implementation - MEMFILE
//-----------------------------------------------------------------------------------

static MEMFILE *mopen(const uint8_t *src, uint32_t length)
{
    if (src == NULL || length == 0)
        return NULL;

    MEMFILE *b = (MEMFILE *)M4P_MALLOC(sizeof(MEMFILE));
    if (b == NULL)
        return NULL;

    b->_base   = (uint8_t *)src;
    b->_ptr    = (uint8_t *)src;
    b->_cnt    = length;
    b->_bufsiz = length;
    b->_eof    = false;

    return b;
}

static void mclose(MEMFILE **buf)
{
    if (*buf != NULL)
    {
        M4P_FREE(*buf);
        *buf = NULL;
    }
}

static size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf)
{
    if (buf == NULL || buf->_ptr == NULL)
        return 0;

    size_t wrcnt = size * count;
    if (size == 0 || buf->_eof)
        return 0;

    int32_t pcnt = (buf->_cnt > wrcnt) ? (int32_t)wrcnt : (int32_t)buf->_cnt;
    memcpy(buffer, buf->_ptr, pcnt);

    buf->_cnt -= pcnt;
    buf->_ptr += pcnt;

    if (buf->_cnt <= 0)
    {
        buf->_ptr = buf->_base + buf->_bufsiz;
        buf->_cnt = 0;
        buf->_eof = true;
    }

    return pcnt / size;
}

static bool meof(MEMFILE *buf)
{
    if (buf == NULL)
        return true;

    return buf->_eof;
}

static void mseek(MEMFILE *buf, int32_t offset, int32_t whence)
{
    if (buf == NULL)
        return;

    if (buf->_base)
    {
        switch (whence)
        {
        case SEEK_SET:
            buf->_ptr = buf->_base + offset;
            break;
        case SEEK_CUR:
            buf->_ptr += offset;
            break;
        case SEEK_END:
            buf->_ptr = buf->_base + buf->_bufsiz + offset;
            break;
        default:
            break;
        }

        buf->_eof = false;
        if (buf->_ptr >= buf->_base + buf->_bufsiz)
        {
            buf->_ptr = buf->_base + buf->_bufsiz;
            buf->_eof = true;
        }

        buf->_cnt = (buf->_base + buf->_bufsiz) - buf->_ptr;
    }
}

static void mrewind(MEMFILE *buf)
{
    mseek(buf, 0, SEEK_SET);
}

static size_t mtell(MEMFILE *buf)
{
    return (buf->_ptr - buf->_base);
}

static bool mreadexact(MEMFILE *m, void *dst, uint32_t num)
{
    if ((m == NULL) || meof(m))
        return false;

    if (mread(dst, 1, num, m) != num)
        return false;

    return true;
}

//-----------------------------------------------------------------------------------
// 							Defines - MMCMP
//-----------------------------------------------------------------------------------

#define MMCMP_COMP  0x0001
#define MMCMP_DELTA 0x0002
#define MMCMP_16BIT 0x0004
#define MMCMP_ABS16 0x0200

//-----------------------------------------------------------------------------------
// 							Typedefs - MMCMP
//-----------------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct MMCMPFILEHEADER
{
    uint32_t id_ziRC; // "ziRC"
    uint32_t id_ONia; // "ONia"
    uint16_t hdrsize;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
MMCMPFILEHEADER,
    *LPMMCMPFILEHEADER;

typedef struct MMCMPHEADER
{
    uint16_t version;
    uint16_t nblocks;
    uint32_t filesize;
    uint32_t blktable;
    uint8_t  glb_comp;
    uint8_t  fmt_comp;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
MMCMPHEADER,
    *LPMMCMPHEADER;

typedef struct MMCMPBLOCK
{
    uint32_t unpk_size;
    uint32_t pk_size;
    uint32_t xor_chk;
    uint16_t sub_blk;
    uint16_t flags;
    uint16_t tt_entries;
    uint16_t num_bits;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
MMCMPBLOCK,
    *LPMMCMPBLOCK;

typedef struct MMCMPSUBBLOCK
{
    uint32_t unpk_pos;
    uint32_t unpk_size;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
MMCMPSUBBLOCK,
    *LPMMCMPSUBBLOCK;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct MMCMPBITBUFFER
{
    uint32_t       bitcount;
    uint32_t       bitbuffer;
    const uint8_t *pSrc;
    const uint8_t *pEnd;
} MMCMPBITBUFFER;

//-----------------------------------------------------------------------------------
// 							Declarations - MMCMP
//-----------------------------------------------------------------------------------

static const uint8_t  MMCMP8BitCommands[8]   = {0x01, 0x03, 0x07, 0x0F, 0x1E, 0x3C, 0x78, 0xF8};
static const uint8_t  MMCMP16BitFetch[16]    = {4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t  MMCMP8BitFetch[8]      = {3, 3, 3, 3, 2, 1, 0, 0};
static const uint16_t MMCMP16BitCommands[16] = {0x01,  0x03,  0x07,  0x0F,  0x1E,   0x3C,   0x78,   0xF0,
                                                0x1F0, 0x3F0, 0x7F0, 0xFF0, 0x1FF0, 0x3FF0, 0x7FF0, 0xFFF0};

//-----------------------------------------------------------------------------------
// 							Implementation - MMCMP
//-----------------------------------------------------------------------------------

static uint32_t GetBits(MMCMPBITBUFFER *bb, uint32_t nBits)
{
    if (!nBits)
        return 0;

    while (bb->bitcount < 24)
    {
        bb->bitbuffer |= ((bb->pSrc < bb->pEnd) ? *bb->pSrc++ : 0) << bb->bitcount;
        bb->bitcount += 8;
    }

    uint32_t d = bb->bitbuffer & ((1 << nBits) - 1);

    bb->bitbuffer >>= nBits;
    bb->bitcount -= nBits;

    return d;
}

static bool unpackMMCMP(uint8_t **ppMemFile, uint32_t *pdwMemLength)
{
    uint32_t          dwMemLength = *pdwMemLength;
    const uint8_t    *lpMemFile   = *ppMemFile;
    uint8_t          *pBuffer;
    LPMMCMPFILEHEADER pmfh = (LPMMCMPFILEHEADER)lpMemFile;
    LPMMCMPHEADER     pmmh = (LPMMCMPHEADER)(lpMemFile + 10);
    uint32_t         *pblk_table;
    uint32_t          dwFileSize;

    if ((dwMemLength < 256) || !pmfh || pmfh->id_ziRC != 0x4352697A || pmfh->id_ONia != 0x61694e4f ||
        pmfh->hdrsize < 14 || !pmmh->nblocks || pmmh->filesize < 16 || pmmh->filesize > 0x8000000 ||
        pmmh->blktable >= dwMemLength || pmmh->blktable + 4 * pmmh->nblocks > dwMemLength)
    {
        return false;
    }

    dwFileSize = pmmh->filesize;

    pBuffer = (uint8_t *)M4P_MALLOC((dwFileSize + 31) & ~15);
    if (pBuffer == NULL)
        return false;

    pblk_table = (uint32_t *)(lpMemFile + pmmh->blktable);
    for (uint32_t nBlock = 0; nBlock < pmmh->nblocks; nBlock++)
    {
        uint32_t        dwMemPos = pblk_table[nBlock];
        LPMMCMPBLOCK    pblk     = (LPMMCMPBLOCK)(lpMemFile + dwMemPos);
        LPMMCMPSUBBLOCK psubblk  = (LPMMCMPSUBBLOCK)(lpMemFile + dwMemPos + 20);

        if (dwMemPos + 20 >= dwMemLength || dwMemPos + 20 + pblk->sub_blk * 8 >= dwMemLength)
            break;

        dwMemPos += 20 + pblk->sub_blk * 8;

        // Data is not packed
        if (!(pblk->flags & MMCMP_COMP))
        {
            for (uint32_t i = 0; i < pblk->sub_blk; i++)
            {
                if (psubblk->unpk_pos > dwFileSize || psubblk->unpk_pos + psubblk->unpk_size > dwFileSize)
                    break;

                memcpy(pBuffer + psubblk->unpk_pos, lpMemFile + dwMemPos, psubblk->unpk_size);
                dwMemPos += psubblk->unpk_size;
                psubblk++;
            }
        }
        else if (pblk->flags & MMCMP_16BIT) // Data is 16-bit packed
        {
            MMCMPBITBUFFER bb;
            uint16_t      *pDest   = (uint16_t *)(pBuffer + psubblk->unpk_pos);
            uint32_t       dwSize  = psubblk->unpk_size >> 1;
            uint32_t       dwPos   = 0;
            uint32_t       numbits = pblk->num_bits;
            uint32_t       subblk = 0, oldval = 0;

            bb.bitcount  = 0;
            bb.bitbuffer = 0;
            bb.pSrc      = lpMemFile + dwMemPos + pblk->tt_entries;
            bb.pEnd      = lpMemFile + dwMemPos + pblk->pk_size;

            while (subblk < pblk->sub_blk)
            {
                uint32_t newval = 0x10000;
                uint32_t d      = GetBits(&bb, numbits + 1);

                if (d >= MMCMP16BitCommands[numbits])
                {
                    uint32_t nFetch  = MMCMP16BitFetch[numbits];
                    uint32_t newbits = GetBits(&bb, nFetch) + ((d - MMCMP16BitCommands[numbits]) << nFetch);

                    if (newbits != numbits)
                    {
                        numbits = newbits & 0x0F;
                    }
                    else
                    {
                        d = GetBits(&bb, 4);
                        if (d == 0x0F)
                        {
                            if (GetBits(&bb, 1))
                                break;

                            newval = 0xFFFF;
                        }
                        else
                        {
                            newval = 0xFFF0 + d;
                        }
                    }
                }
                else
                {
                    newval = d;
                }

                if (newval < 0x10000)
                {
                    newval = (newval & 1) ? (uint32_t)(-(int32_t)((newval + 1) >> 1)) : (uint32_t)(newval >> 1);
                    if (pblk->flags & MMCMP_DELTA)
                    {
                        newval += oldval;
                        oldval = newval;
                    }
                    else if (!(pblk->flags & MMCMP_ABS16))
                    {
                        newval ^= 0x8000;
                    }

                    pDest[dwPos++] = (uint16_t)newval;
                }

                if (dwPos >= dwSize)
                {
                    subblk++;
                    dwPos  = 0;
                    dwSize = psubblk[subblk].unpk_size >> 1;
                    pDest  = (uint16_t *)(pBuffer + psubblk[subblk].unpk_pos);
                }
            }
        }
        else // Data is 8-bit packed
        {
            MMCMPBITBUFFER bb;
            uint8_t       *pDest   = pBuffer + psubblk->unpk_pos;
            uint32_t       dwSize  = psubblk->unpk_size;
            uint32_t       dwPos   = 0;
            uint32_t       numbits = pblk->num_bits;
            uint32_t       subblk = 0, oldval = 0;
            const uint8_t *ptable = lpMemFile + dwMemPos;

            bb.bitcount  = 0;
            bb.bitbuffer = 0;
            bb.pSrc      = lpMemFile + dwMemPos + pblk->tt_entries;
            bb.pEnd      = lpMemFile + dwMemPos + pblk->pk_size;
            while (subblk < pblk->sub_blk)
            {
                uint32_t newval = 0x100;
                uint32_t d      = GetBits(&bb, numbits + 1);

                if (d >= MMCMP8BitCommands[numbits])
                {
                    uint32_t nFetch  = MMCMP8BitFetch[numbits];
                    uint32_t newbits = GetBits(&bb, nFetch) + ((d - MMCMP8BitCommands[numbits]) << nFetch);
                    if (newbits != numbits)
                    {
                        numbits = newbits & 0x07;
                    }
                    else
                    {
                        d = GetBits(&bb, 3);
                        if (d == 7)
                        {
                            if (GetBits(&bb, 1))
                                break;

                            newval = 0xFF;
                        }
                        else
                        {
                            newval = 0xF8 + d;
                        }
                    }
                }
                else
                {
                    newval = d;
                }

                if (newval < 0x100)
                {
                    int32_t n = ptable[newval];
                    if (pblk->flags & MMCMP_DELTA)
                    {
                        n += oldval;
                        oldval = n;
                    }

                    pDest[dwPos++] = (uint8_t)n;
                }

                if (dwPos >= dwSize)
                {
                    subblk++;
                    dwPos  = 0;
                    dwSize = psubblk[subblk].unpk_size;
                    pDest  = pBuffer + psubblk[subblk].unpk_pos;
                }
            }
        }
    }

    *ppMemFile    = pBuffer;
    *pdwMemLength = dwFileSize;

    return true;
}

//-----------------------------------------------------------------------------------
// 							Defines - FastTracker 2
//-----------------------------------------------------------------------------------

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLAMP16(i)                                                                                                     \
    if ((int16_t)(i) != i)                                                                                             \
    i = 0x7FFF ^ (i >> 31)
#define DEFAULT_AMP        4
#define DEFAULT_MASTER_VOL 256
#define INSTR_HEADER_SIZE  263
#define MAX_FRQ            32000
#define MAX_NOTES          (10 * 12 * 16 + 16)

#define SWAP16(value) ((((uint16_t)((value) & 0x00FF)) << 8) | (((uint16_t)((value) & 0xFF00)) >> 8))

#define GET_VOL                                                                                                        \
    const int32_t CDA_LVol = v->SLVol1;                                                                                \
    const int32_t CDA_RVol = v->SRVol1;

#define GET_VOL_CENTER const int32_t CDA_LVol = v->SLVol1;

#define GET_VOL_RAMP                                                                                                   \
    int32_t CDA_LVol = v->SLVol2;                                                                                      \
    int32_t CDA_RVol = v->SRVol2;

#define SET_VOL_BACK                                                                                                   \
    v->SLVol2 = CDA_LVol;                                                                                              \
    v->SRVol2 = CDA_RVol;

#define GET_MIXER_VARS                                                                                                 \
    int32_t *audioMix = CDA_MixBuffer + (bufferPos << 1);                                                              \
    int32_t  realPos  = v->SPos;                                                                                       \
    uint32_t pos      = v->SPosDec;                                                                                    \
    uint16_t CDA_MixBuffPos =                                                                                          \
        (32768 + 96) - 8; /* address of FT2 mix buffer minus mix sample size (used for quirky LERP) */

#define GET_RAMP_VARS                                                                                                  \
    int32_t CDA_LVolIP = v->SLVolIP;                                                                                   \
    int32_t CDA_RVolIP = v->SRVolIP;

#define SET_BASE8                                                                                                      \
    const int8_t *CDA_LinearAdr = (int8_t *)v->SBase;                                                                  \
    const int8_t *CDA_LinAdrRev = (int8_t *)v->SRevBase;                                                               \
    const int8_t *smpPtr        = CDA_LinearAdr + realPos;

#define SET_BASE16                                                                                                     \
    const int16_t *CDA_LinearAdr = (int16_t *)v->SBase;                                                                \
    const int16_t *CDA_LinAdrRev = (int16_t *)v->SRevBase;                                                             \
    const int16_t *smpPtr        = CDA_LinearAdr + realPos;

#define INC_POS                                                                                                        \
    smpPtr += CDA_IPValH;                                                                                              \
    smpPtr +=                                                                                                          \
        (CDA_IPValL >                                                                                                  \
         (uint32_t)~pos); /* if pos would 32-bit overflow after CDA_IPValL add, add one to smpPtr (branchless) */      \
    pos += CDA_IPValL;

#define SET_BACK_MIXER_POS                                                                                             \
    v->SPosDec = pos & 0xFFFF0000;                                                                                     \
    v->SPos    = realPos;

#define VOL_RAMP                                                                                                       \
    CDA_LVol += CDA_LVolIP;                                                                                            \
    CDA_RVol += CDA_RVolIP;

// stereo mixing without interpolation

#define MIX_8BIT                                                                                                       \
    sample = (*smpPtr) << (28 - 8);                                                                                    \
    *audioMix++ += ((int64_t)sample * (int32_t)CDA_LVol) >> 32;                                                        \
    *audioMix++ += ((int64_t)sample * (int32_t)CDA_RVol) >> 32;                                                        \
    INC_POS

#define MIX_16BIT                                                                                                      \
    sample = (*smpPtr) << (28 - 16);                                                                                   \
    *audioMix++ += ((int64_t)sample * (int32_t)CDA_LVol) >> 32;                                                        \
    *audioMix++ += ((int64_t)sample * (int32_t)CDA_RVol) >> 32;                                                        \
    INC_POS

// center mixing without interpolation

#define MIX_8BIT_M                                                                                                     \
    sample = (*smpPtr) << (28 - 8);                                                                                    \
    sample = ((int64_t)sample * (int32_t)CDA_LVol) >> 32;                                                              \
    *audioMix++ += sample;                                                                                             \
    *audioMix++ += sample;                                                                                             \
    INC_POS

#define MIX_16BIT_M                                                                                                    \
    sample = (*smpPtr) << (28 - 16);                                                                                   \
    sample = ((int64_t)sample * (int32_t)CDA_LVol) >> 32;                                                              \
    *audioMix++ += sample;                                                                                             \
    *audioMix++ += sample;                                                                                             \
    INC_POS

// linear interpolation with bit-accurate results to FT2.08/FT2.09
#define LERP(s1, s2, f)                                                                                                \
    {                                                                                                                  \
        s2 -= s1;                                                                                                      \
        f >>= 1;                                                                                                       \
        s2 = ((int64_t)s2 * (int32_t)f) >> 32;                                                                         \
        f += f;                                                                                                        \
        s2 += s2;                                                                                                      \
        s2 += s1;                                                                                                      \
    }

// stereo mixing w/ linear interpolation

#define MIX_8BIT_INTRP                                                                                                 \
    sample  = smpPtr[0] << 8;                                                                                          \
    sample2 = smpPtr[1] << 8;                                                                                          \
    LERP(sample, sample2, pos)                                                                                         \
    sample2 <<= (28 - 16);                                                                                             \
    *audioMix++ += ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32;                                                       \
    *audioMix++ += ((int64_t)sample2 * (int32_t)CDA_RVol) >> 32;                                                       \
    INC_POS

#define MIX_16BIT_INTRP                                                                                                \
    sample  = smpPtr[0];                                                                                               \
    sample2 = smpPtr[1];                                                                                               \
    LERP(sample, sample2, pos)                                                                                         \
    sample2 <<= (28 - 16);                                                                                             \
    *audioMix++ += ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32;                                                       \
    *audioMix++ += ((int64_t)sample2 * (int32_t)CDA_RVol) >> 32;                                                       \
    INC_POS

// center mixing w/ linear interpolation

#define MIX_8BIT_INTRP_M                                                                                               \
    sample  = smpPtr[0] << 8;                                                                                          \
    sample2 = smpPtr[1] << 8;                                                                                          \
    LERP(sample, sample2, pos)                                                                                         \
    sample2 <<= (28 - 16);                                                                                             \
    sample = ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32;                                                             \
    *audioMix++ += sample;                                                                                             \
    *audioMix++ += sample;                                                                                             \
    INC_POS

#define MIX_16BIT_INTRP_M                                                                                              \
    sample  = smpPtr[0];                                                                                               \
    sample2 = smpPtr[1];                                                                                               \
    LERP(sample, sample2, pos)                                                                                         \
    sample2 <<= (28 - 16);                                                                                             \
    sample = ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32;                                                             \
    *audioMix++ += sample;                                                                                             \
    *audioMix++ += sample;                                                                                             \
    INC_POS

#define LIMIT_MIX_NUM                                                                                                  \
    int32_t samplesToMix;                                                                                              \
    int32_t SFrq = v->SFrq;                                                                                            \
    int32_t i    = (v->SLen - 1) - realPos;                                                                            \
    if (i > UINT16_MAX)                                                                                                \
        i = UINT16_MAX; /* 8bb: added this to prevent 64-bit div (still bit-accurate mixing results) */                \
    if (SFrq != 0)                                                                                                     \
    {                                                                                                                  \
        const uint32_t tmp32 = (i << 16) | ((0xFFFF0000 - pos) >> 16);                                                 \
        samplesToMix         = (tmp32 / (uint32_t)SFrq) + 1;                                                           \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        samplesToMix = 65535;                                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    if (samplesToMix > CDA_BytesLeft)                                                                                  \
        samplesToMix = CDA_BytesLeft;

#define LIMIT_MIX_NUM_RAMP                                                                                             \
    if (v->SVolIPLen == 0)                                                                                             \
    {                                                                                                                  \
        CDA_LVolIP = 0;                                                                                                \
        CDA_RVolIP = 0;                                                                                                \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        if (samplesToMix > v->SVolIPLen)                                                                               \
            samplesToMix = v->SVolIPLen;                                                                               \
                                                                                                                       \
        v->SVolIPLen -= samplesToMix;                                                                                  \
    }

#define HANDLE_POS_START                                                                                               \
    const bool backwards = (v->SType & (SType_Rev + SType_RevDir)) == SType_Rev + SType_RevDir;                        \
    if (backwards)                                                                                                     \
    {                                                                                                                  \
        SFrq    = 0 - SFrq;                                                                                            \
        realPos = ~realPos;                                                                                            \
        smpPtr  = CDA_LinAdrRev + realPos;                                                                             \
        pos ^= 0xFFFF0000;                                                                                             \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        smpPtr = CDA_LinearAdr + realPos;                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    pos += CDA_MixBuffPos;                                                                                             \
    const int32_t  CDA_IPValH = (int32_t)SFrq >> 16;                                                                   \
    const uint32_t CDA_IPValL =                                                                                        \
        ((uint32_t)(SFrq & 0xFFFF) << 16) + 8; /* 8 = mixer buffer increase (for LERP to be bit-accurate to FT2) */

#define HANDLE_POS_END                                                                                                 \
    if (backwards)                                                                                                     \
    {                                                                                                                  \
        pos ^= 0xFFFF0000;                                                                                             \
        realPos = ~(int32_t)(smpPtr - CDA_LinAdrRev);                                                                  \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        realPos = (int32_t)(smpPtr - CDA_LinearAdr);                                                                   \
    }                                                                                                                  \
    CDA_MixBuffPos = pos & 0xFFFF;                                                                                     \
    pos &= 0xFFFF0000;                                                                                                 \
                                                                                                                       \
    if (realPos >= v->SLen)                                                                                            \
    {                                                                                                                  \
        uint8_t SType = v->SType;                                                                                      \
        if (SType & (SType_Fwd + SType_Rev))                                                                           \
        {                                                                                                              \
            do                                                                                                         \
            {                                                                                                          \
                realPos -= v->SRepL;                                                                                   \
                SType ^= SType_RevDir;                                                                                 \
            } while (realPos >= v->SLen);                                                                              \
            v->SType = SType;                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            v->SType = SType_Off;                                                                                      \
            return;                                                                                                    \
        }                                                                                                              \
    }

//-----------------------------------------------------------------------------------
// 							Enumerations - FastTracker 2
//-----------------------------------------------------------------------------------

enum // voice flags
{
    IS_Vol      = 1,
    IS_Period   = 2,
    IS_NyTon    = 4,
    IS_Pan      = 8,
    IS_QuickVol = 16
};

enum // note
{
    NOTE_KEYOFF = 97
};

enum // header flags
{
    LINEAR_FREQUENCIES = 1
};

enum // sample flags
{
    LOOP_OFF      = 0,
    LOOP_FORWARD  = 1,
    LOOP_PINGPONG = 2,
    SAMPLE_16BIT  = 16
};

enum // envelope flags
{
    ENV_ENABLED = 1,
    ENV_SUSTAIN = 2,
    ENV_LOOP    = 4
};

enum
{
    Status_SetVol    = 1,
    Status_SetPan    = 2,
    Status_SetFrq    = 4,
    Status_StartTone = 8,
    Status_StopTone  = 16,
    Status_QuickVol  = 32,

    SType_Fwd     = 1,
    SType_Rev     = 2,
    SType_RevDir  = 4,
    SType_Off     = 8,
    SType_16      = 16,
    SType_Fadeout = 32
};

//-----------------------------------------------------------------------------------
// 							Typedefs - FastTracker 2
//-----------------------------------------------------------------------------------
typedef struct songTyp_t
{
    char     name[20 + 1];
    uint8_t  antChn, pattDelTime, pattDelTime2, pBreakPos, songTab[256];
    bool     pBreakFlag, posJumpFlag;
    int16_t  songPos, pattNr, pattPos, pattLen;
    uint16_t len, repS, speed, tempo, globVol, timer, ver;

    uint16_t antInstrs; // 8bb: added this
} songTyp;

typedef struct sampleTyp_t
{
    char    name[22 + 1];
    int32_t len, repS, repL;
    uint8_t vol;
    int8_t  fine;
    uint8_t typ, pan;
    int8_t  relTon;
    int8_t *pek;
} sampleTyp;

typedef struct instrTyp_t
{
    char      name[22 + 1];
    uint8_t   ta[96];
    int16_t   envVP[12][2], envPP[12][2];
    uint8_t   envVPAnt, envPPAnt;
    uint8_t   envVSust, envVRepS, envVRepE;
    uint8_t   envPSust, envPRepS, envPRepE;
    uint8_t   envVTyp, envPTyp;
    uint8_t   vibTyp, vibSweep, vibDepth, vibRate;
    uint16_t  fadeOut;
    uint8_t   mute;
    int16_t   antSamp;
    sampleTyp samp[16];
} instrTyp;

typedef struct stmTyp_t
{
    uint8_t   status;
    int8_t    relTonNr, fineTune;
    uint8_t   sampleNr, instrNr, effTyp, eff, smpOffset, tremorSave, tremorPos;
    uint8_t   globVolSlideSpeed, panningSlideSpeed, mute, waveCtrl, portaDir;
    uint8_t   glissFunk, vibPos, tremPos, vibSpeed, vibDepth, tremSpeed, tremDepth;
    uint8_t   pattPos, loopCnt, volSlideSpeed, fVolSlideUpSpeed, fVolSlideDownSpeed;
    uint8_t   fPortaUpSpeed, fPortaDownSpeed, ePortaUpSpeed, ePortaDownSpeed;
    uint8_t   portaUpSpeed, portaDownSpeed, retrigSpeed, retrigCnt, retrigVol;
    uint8_t   volKolVol, tonNr, envPPos, eVibPos, envVPos, realVol, oldVol, outVol;
    uint8_t   oldPan, outPan, finalPan;
    bool      envSustainActive;
    int16_t   envVIPValue, envPIPValue;
    uint16_t  outPeriod, realPeriod, finalPeriod, finalVol, tonTyp, wantPeriod, portaSpeed;
    uint16_t  envVCnt, envVAmp, envPCnt, envPAmp, eVibAmp, eVibSweep;
    uint16_t  fadeOutAmp, fadeOutSpeed;
    int32_t   smpStartPos;
    instrTyp *instrSeg;
} stmTyp;

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct tonTyp_t // this one must be packed on some systems
{
    uint8_t ton, instr, vol, effTyp, eff;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
tonTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct
{
    const void *SBase, *SRevBase;
    uint8_t     SType, SPan, SVol;
    int32_t     SLVol1, SRVol1, SLVol2, SRVol2, SLVolIP, SRVolIP, SVolIPLen;
    int32_t     SLen, SRepS, SRepL, SPos, SMixType;
    uint32_t    SPosDec, SFrq;
} CIType;

typedef struct
{
    const void *SBase;
    uint8_t     Status, SType;
    int16_t     SVol, SPan;
    int32_t     SFrq, SLen, SRepS, SRepL, SStartPos;
} WaveChannelInfoType;

typedef void (*mixRoutine)(void *, int32_t, int32_t);

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct songHeaderTyp_t
{
    char     sig[17], name[21], progName[20];
    uint16_t ver;
    int32_t  headerSize;
    uint16_t len, repS, antChn, antPtn, antInstrs, flags, defTempo, defSpeed;
    uint8_t  songTab[256];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
songHeaderTyp;

typedef struct modSampleTyp
{
    char     name[22];
    uint16_t len;
    uint8_t  fine, vol;
    uint16_t repS, repL;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
modSampleTyp;

typedef struct songMOD31HeaderTyp
{
    char         name[20];
    modSampleTyp sample[31];
    uint8_t      len, repS, songTab[128];
    char         Sig[4];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
songMOD31HeaderTyp;

typedef struct songMOD15HeaderTyp
{
    char         name[20];
    modSampleTyp sample[15];
    uint8_t      len, repS, songTab[128];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
songMOD15HeaderTyp;

typedef struct sampleHeaderTyp_t
{
    int32_t len, repS, repL;
    uint8_t vol;
    int8_t  fine;
    uint8_t typ, pan;
    int8_t  relTon;
    uint8_t skrap;
    char    name[22];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
sampleHeaderTyp;

typedef struct instrHeaderTyp_t
{
    int32_t         instrSize;
    char            name[22];
    uint8_t         typ;
    uint16_t        antSamp;
    int32_t         sampleSize;
    uint8_t         ta[96];
    int16_t         envVP[12][2], envPP[12][2];
    uint8_t         envVPAnt, envPPAnt, envVSust, envVRepS, envVRepE, envPSust, envPRepS;
    uint8_t         envPRepE, envVTyp, envPTyp, vibTyp, vibSweep, vibDepth, vibRate;
    uint16_t        fadeOut;
    uint8_t         midiOn, midiChannel;
    int16_t         midiProgram, midiBend;
    int8_t          mute;
    uint8_t         reserved[15];
    sampleHeaderTyp samp[32];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
instrHeaderTyp;

typedef struct patternHeaderTyp_t
{
    int32_t  patternHeaderSize;
    uint8_t  typ;
    uint16_t pattLen, dataLen;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
patternHeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef void (*volKolEfxRoutine)(stmTyp *ch);
typedef void (*volKolEfxRoutine2)(stmTyp *ch, uint8_t *volKol);
typedef void (*efxRoutine)(stmTyp *ch, uint8_t param);

//-----------------------------------------------------------------------------------
// 						Declarations - FastTracker 2
//-----------------------------------------------------------------------------------

static const uint32_t  panningTab[257];
static const uint16_t  amigaPeriods[1936];
static const uint16_t  linearPeriods[1936];
static const int32_t   logTab[768];
static const uint16_t  amigaPeriod[96];
static const uint8_t   vibTab[32];
static const int8_t    vibSineTab[256];
static const uint8_t   arpTab[256];
static tonTyp          nilPatternLine[32]; // 8bb: used for non-allocated (empty) patterns
static int16_t         chnReloc[32];
static int32_t        *CDA_MixBuffer = NULL;
static CIType          CI[32 * 2];
static bool            interpolationFlag, volumeRampingFlag, moduleLoaded, musicPaused;
static bool            linearFrqTab;
static const uint16_t *note2Period;
static uint16_t        pattLens[256];
static int16_t         PMPTmpActiveChannel;
static int32_t         masterVol = DEFAULT_MASTER_VOL, PMPLeft = 0;
static int32_t         realReplayRate, quickVolSizeVal, speedVal;
static uint32_t        frequenceDivFactor, frequenceMulFactor, CDA_Amp = 8 * DEFAULT_AMP;
static tonTyp         *patt[256];
static instrTyp       *instr[1 + 128];
static songTyp         song;
static stmTyp          stm[32];
static const char     *MODSig[16] = {"2CHN", "M.K.", "6CHN", "8CHN", "10CH", "12CH", "14CH", "16CH",
                                     "18CH", "20CH", "22CH", "24CH", "26CH", "28CH", "30CH", "32CH"};

static void     updateReplayRate(void);
static void     resumeMusic(void);
static void     P_SetSpeed(uint16_t bpm);
static void     P_StartTone(sampleTyp *s, int32_t smpStartPos);
static void     stopMusic(void);
static int32_t  soundBufferSize;
static MEMFILE *mopen(const uint8_t *src, uint32_t length);
static void     mclose(MEMFILE **buf);
static size_t   mread(void *buffer, size_t size, size_t count, MEMFILE *buf);
static bool     meof(MEMFILE *buf);
static void     mseek(MEMFILE *buf, int32_t offset, int32_t whence);
static void     mrewind(MEMFILE *buf);
static void     resetMusic(void);
static void     freeAllPatterns(void);
static void     setFrqTab(bool linear);
static void     volume(stmTyp *ch, uint8_t param); // 8bb: volume slide
static void     vibrato2(stmTyp *ch);
static void     tonePorta(stmTyp *ch, uint8_t param);
static void     mix8b(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16b(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix8bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     mix16bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos);
static void     dummy(stmTyp *ch, uint8_t param);
static void     finePortaUp(stmTyp *ch, uint8_t param);
static void     finePortaDown(stmTyp *ch, uint8_t param);
static void     setGlissCtrl(stmTyp *ch, uint8_t param);
static void     setVibratoCtrl(stmTyp *ch, uint8_t param);
static void     jumpLoop(stmTyp *ch, uint8_t param);
static void     setTremoloCtrl(stmTyp *ch, uint8_t param);
static void     volFineUp(stmTyp *ch, uint8_t param);
static void     volFineDown(stmTyp *ch, uint8_t param);
static void     noteCut0(stmTyp *ch, uint8_t param);
static void     pattDelay(stmTyp *ch, uint8_t param);
static void     E_Effects_TickZero(stmTyp *ch, uint8_t param);
static void     posJump(stmTyp *ch, uint8_t param);
static void     pattBreak(stmTyp *ch, uint8_t param);
static void     setSpeed(stmTyp *ch, uint8_t param);
static void     setGlobaVol(stmTyp *ch, uint8_t param);
static void     setEnvelopePos(stmTyp *ch, uint8_t param);
static void     v_SetVibSpeed(stmTyp *ch, uint8_t *volKol);
static void     v_Volume(stmTyp *ch, uint8_t *volKol);
static void     v_FineSlideDown(stmTyp *ch, uint8_t *volKol);
static void     v_FineSlideUp(stmTyp *ch, uint8_t *volKol);
static void     v_SetPan(stmTyp *ch, uint8_t *volKol);
static void     v_SlideDown(stmTyp *ch);
static void     v_SlideUp(stmTyp *ch);
static void     v_Vibrato(stmTyp *ch);
static void     v_PanSlideLeft(stmTyp *ch);
static void     v_PanSlideRight(stmTyp *ch);
static void     v_TonePorta(stmTyp *ch);
static void     v_dummy(stmTyp *ch);
static void     v_dummy2(stmTyp *ch, uint8_t *volKol);
static void     setPan(stmTyp *ch, uint8_t param);
static void     setVol(stmTyp *ch, uint8_t param);
static void     xFinePorta(stmTyp *ch, uint8_t param);
static void     arp(stmTyp *ch, uint8_t param);
static void     portaUp(stmTyp *ch, uint8_t param);
static void     portaDown(stmTyp *ch, uint8_t param);
static void     vibrato(stmTyp *ch, uint8_t param);
static void     tonePlusVol(stmTyp *ch, uint8_t param);
static void     vibratoPlusVol(stmTyp *ch, uint8_t param);
static void     tremolo(stmTyp *ch, uint8_t param);
static void     globalVolSlide(stmTyp *ch, uint8_t param);
static void     keyOffCmd(stmTyp *ch, uint8_t param);
static void     panningSlide(stmTyp *ch, uint8_t param);
static void     tremor(stmTyp *ch, uint8_t param);
static void     retrigNote(stmTyp *ch, uint8_t param);
static void     noteCut(stmTyp *ch, uint8_t param);
static void     noteDelay(stmTyp *ch, uint8_t param);
static void     E_Effects_TickNonZero(stmTyp *ch, uint8_t param);
static void     doMultiRetrig(stmTyp *ch, uint8_t param);

static mixRoutine mixRoutineTable[16] = {(mixRoutine)mix8b,
                                         (mixRoutine)mix8bIntrp,
                                         (mixRoutine)mix8bRamp,
                                         (mixRoutine)mix8bRampIntrp,
                                         (mixRoutine)mix16b,
                                         (mixRoutine)mix16bIntrp,
                                         (mixRoutine)mix16bRamp,
                                         (mixRoutine)mix16bRampIntrp,
                                         (mixRoutine)mix8bCenter,
                                         (mixRoutine)mix8bIntrpCenter,
                                         (mixRoutine)mix8bRampCenter,
                                         (mixRoutine)mix8bRampIntrpCenter,
                                         (mixRoutine)mix16bCenter,
                                         (mixRoutine)mix16bIntrpCenter,
                                         (mixRoutine)mix16bRampCenter,
                                         (mixRoutine)mix16bRampIntrpCenter};

static const efxRoutine EJumpTab_TickZero[16] = {
    dummy,          // 0
    finePortaUp,    // 1
    finePortaDown,  // 2
    setGlissCtrl,   // 3
    setVibratoCtrl, // 4
    dummy,          // 5
    jumpLoop,       // 6
    setTremoloCtrl, // 7
    dummy,          // 8
    dummy,          // 9
    volFineUp,      // A
    volFineDown,    // B
    noteCut0,       // C
    dummy,          // D
    pattDelay,      // E
    dummy           // F
};

static const volKolEfxRoutine VJumpTab_TickNonZero[16] = {
    v_dummy, v_dummy, v_dummy, v_dummy,   v_dummy, v_dummy,        v_SlideDown,     v_SlideUp,
    v_dummy, v_dummy, v_dummy, v_Vibrato, v_dummy, v_PanSlideLeft, v_PanSlideRight, v_TonePorta};

static const volKolEfxRoutine2 VJumpTab_TickZero[16] = {
    v_dummy2,        v_Volume,      v_Volume,      v_Volume, v_Volume, v_Volume, v_dummy2, v_dummy2,
    v_FineSlideDown, v_FineSlideUp, v_SetVibSpeed, v_dummy2, v_SetPan, v_dummy2, v_dummy2, v_dummy2};

static const efxRoutine JumpTab_TickZero[36] = {
    dummy,              // 0
    dummy,              // 1
    dummy,              // 2
    dummy,              // 3
    dummy,              // 4
    dummy,              // 5
    dummy,              // 6
    dummy,              // 7
    setPan,             // 8
    dummy,              // 9
    dummy,              // A
    posJump,            // B
    setVol,             // C
    pattBreak,          // D
    E_Effects_TickZero, // E
    setSpeed,           // F
    setGlobaVol,        // G
    dummy,              // H
    dummy,              // I
    dummy,              // J
    dummy,              // K
    setEnvelopePos,     // L
    dummy,              // M
    dummy,              // N
    dummy,              // O
    dummy,              // P
    dummy,              // Q
    dummy,              // R
    dummy,              // S
    dummy,              // T
    dummy,              // U
    dummy,              // V
    dummy,              // W
    xFinePorta,         // X
    dummy,              // Y
    dummy               // Z
};

static const efxRoutine EJumpTab_TickNonZero[16] = {
    dummy,      // 0
    dummy,      // 1
    dummy,      // 2
    dummy,      // 3
    dummy,      // 4
    dummy,      // 5
    dummy,      // 6
    dummy,      // 7
    dummy,      // 8
    retrigNote, // 9
    dummy,      // A
    dummy,      // B
    noteCut,    // C
    noteDelay,  // D
    dummy,      // E
    dummy       // F
};

static const efxRoutine JumpTab_TickNonZero[36] = {
    arp,                   // 0
    portaUp,               // 1
    portaDown,             // 2
    tonePorta,             // 3
    vibrato,               // 4
    tonePlusVol,           // 5
    vibratoPlusVol,        // 6
    tremolo,               // 7
    dummy,                 // 8
    dummy,                 // 9
    volume,                // A
    dummy,                 // B
    dummy,                 // C
    dummy,                 // D
    E_Effects_TickNonZero, // E
    dummy,                 // F
    dummy,                 // G
    globalVolSlide,        // H
    dummy,                 // I
    dummy,                 // J
    keyOffCmd,             // K
    dummy,                 // L
    dummy,                 // M
    dummy,                 // N
    dummy,                 // O
    panningSlide,          // P
    dummy,                 // Q
    doMultiRetrig,         // R
    dummy,                 // S
    tremor,                // T
    dummy,                 // U
    dummy,                 // V
    dummy,                 // W
    dummy,                 // X
    dummy,                 // Y
    dummy                  // Z
};

//-----------------------------------------------------------------------------------
// 							Implementation - FastTracker 2
//-----------------------------------------------------------------------------------

static void mix8b(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_VOL
    GET_MIXER_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT
            MIX_8BIT
            MIX_8BIT
            MIX_8BIT
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix8bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_VOL
    GET_MIXER_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_INTRP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_INTRP
            MIX_8BIT_INTRP
            MIX_8BIT_INTRP
            MIX_8BIT_INTRP
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix8bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT
            VOL_RAMP
            MIX_8BIT
            VOL_RAMP
            MIX_8BIT
            VOL_RAMP
            MIX_8BIT
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix8bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_INTRP
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_INTRP
            VOL_RAMP
            MIX_8BIT_INTRP
            VOL_RAMP
            MIX_8BIT_INTRP
            VOL_RAMP
            MIX_8BIT_INTRP
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix16b(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_VOL
    GET_MIXER_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT
            MIX_16BIT
            MIX_16BIT
            MIX_16BIT
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix16bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_VOL
    GET_MIXER_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_INTRP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_INTRP
            MIX_16BIT_INTRP
            MIX_16BIT_INTRP
            MIX_16BIT_INTRP
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix16bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT
            VOL_RAMP
            MIX_16BIT
            VOL_RAMP
            MIX_16BIT
            VOL_RAMP
            MIX_16BIT
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix16bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_INTRP
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_INTRP
            VOL_RAMP
            MIX_16BIT_INTRP
            VOL_RAMP
            MIX_16BIT_INTRP
            VOL_RAMP
            MIX_16BIT_INTRP
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix8bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_VOL_CENTER
    GET_MIXER_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_M
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_M
            MIX_8BIT_M
            MIX_8BIT_M
            MIX_8BIT_M
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix8bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_VOL_CENTER
    GET_MIXER_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_INTRP_M
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_INTRP_M
            MIX_8BIT_INTRP_M
            MIX_8BIT_INTRP_M
            MIX_8BIT_INTRP_M
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix8bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_M
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_M
            VOL_RAMP
            MIX_8BIT_M
            VOL_RAMP
            MIX_8BIT_M
            VOL_RAMP
            MIX_8BIT_M
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix8bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE8

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_8BIT_INTRP_M
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_8BIT_INTRP_M
            VOL_RAMP
            MIX_8BIT_INTRP_M
            VOL_RAMP
            MIX_8BIT_INTRP_M
            VOL_RAMP
            MIX_8BIT_INTRP_M
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix16bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_VOL_CENTER
    GET_MIXER_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_M
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_M
            MIX_16BIT_M
            MIX_16BIT_M
            MIX_16BIT_M
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix16bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_VOL_CENTER
    GET_MIXER_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        CDA_BytesLeft -= samplesToMix;

        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_INTRP_M
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_INTRP_M
            MIX_16BIT_INTRP_M
            MIX_16BIT_INTRP_M
            MIX_16BIT_INTRP_M
        }
        HANDLE_POS_END
    }

    SET_BACK_MIXER_POS
}

static void mix16bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_M
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_M
            VOL_RAMP
            MIX_16BIT_M
            VOL_RAMP
            MIX_16BIT_M
            VOL_RAMP
            MIX_16BIT_M
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

static void mix16bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
    int32_t sample, sample2;

    GET_MIXER_VARS
    GET_RAMP_VARS
    SET_BASE16

    int32_t CDA_BytesLeft = numSamples;
    while (CDA_BytesLeft > 0)
    {
        LIMIT_MIX_NUM
        LIMIT_MIX_NUM_RAMP
        CDA_BytesLeft -= samplesToMix;

        GET_VOL_RAMP
        HANDLE_POS_START
        for (i = 0; i < (samplesToMix & 3); i++)
        {
            MIX_16BIT_INTRP_M
            VOL_RAMP
        }
        samplesToMix >>= 2;
        for (i = 0; i < samplesToMix; i++)
        {
            MIX_16BIT_INTRP_M
            VOL_RAMP
            MIX_16BIT_INTRP_M
            VOL_RAMP
            MIX_16BIT_INTRP_M
            VOL_RAMP
            MIX_16BIT_INTRP_M
            VOL_RAMP
        }
        HANDLE_POS_END
        SET_VOL_BACK
    }

    SET_BACK_MIXER_POS
}

/* 8bb: This is done in a slightly different way, but the result
** is the same (bit-accurate to FT2.08/FT2.09 w/ SB16, and WAV-writer).
**
** Mixer macros are stored in snd_masm.h
*/

static void PMPMix32Proc(CIType *v, int32_t numSamples, int32_t bufferPos)
{
    if (v->SType & SType_Off)
        return; // voice is not active

    uint32_t volStatus = v->SLVol1 | v->SRVol1;
    if (volumeRampingFlag)
        volStatus |= v->SLVol2 | v->SRVol2;

    if (volStatus == 0)                                                         // silence mix
    {
        const uint64_t samplesToMix = (uint64_t)v->SFrq * (uint32_t)numSamples; // 16.16fp

        const int32_t samples     = (int32_t)(samplesToMix >> 16);
        const int32_t samplesFrac = (samplesToMix & 0xFFFF) + (v->SPosDec >> 16);

        int32_t realPos = v->SPos + samples + (samplesFrac >> 16);
        int32_t posFrac = samplesFrac & 0xFFFF;

        if (realPos >= v->SLen)
        {
            uint8_t SType = v->SType;
            if (SType & (SType_Fwd + SType_Rev))
            {
                do
                {
                    SType ^= SType_RevDir;
                    realPos -= v->SRepL;
                } while (realPos >= v->SLen);
                v->SType = SType;
            }
            else
            {
                v->SType = SType_Off;
                return;
            }
        }

        v->SPosDec = posFrac << 16;
        v->SPos    = realPos;
    }
    else // normal mixing
    {
        bool mixInCenter;
        if (volumeRampingFlag)
            mixInCenter = (v->SLVol2 == v->SRVol2) && (v->SLVolIP == v->SRVolIP);
        else
            mixInCenter = v->SLVol1 == v->SRVol1;

        mixRoutineTable[(mixInCenter * 8) + v->SMixType](v, numSamples, bufferPos);
    }
}

static void retrigVolume(stmTyp *ch)
{
    ch->realVol = ch->oldVol;
    ch->outVol  = ch->oldVol;
    ch->outPan  = ch->oldPan;
    ch->status |= IS_Vol + IS_Pan + IS_QuickVol;
}

static void retrigEnvelopeVibrato(stmTyp *ch)
{
    // 8bb: reset vibrato position
    if (!(ch->waveCtrl & 0x04))
        ch->vibPos = 0;

    /*
    ** 8bb:
    ** In FT2.00 .. FT2.09, if the sixth bit of "ch->waveCtrl" is set
    ** (from effect E7x where x is $4..$7 or $C..$F) and you trigger a note,
    ** the replayer interrupt will freeze / lock up. This is because of a
    ** label bug in the original code, causing it to jump back to itself
    ** indefinitely.
    */

    // 8bb: safely reset tremolo position
    if (!(ch->waveCtrl & 0x40))
        ch->tremPos = 0;

    ch->retrigCnt = 0;
    ch->tremorPos = 0;

    ch->envSustainActive = true;

    instrTyp *ins = ch->instrSeg;

    if (ins->envVTyp & ENV_ENABLED)
    {
        ch->envVCnt = 65535;
        ch->envVPos = 0;
    }

    if (ins->envPTyp & ENV_ENABLED)
    {
        ch->envPCnt = 65535;
        ch->envPPos = 0;
    }

    ch->fadeOutSpeed = ins->fadeOut; // 8bb: ranges 0..4095 (FT2 doesn't check if it's higher than 4095!)

    // 8bb: final fadeout range is in fact 0..32768, and not 0..65536 like the XM format doc says
    ch->fadeOutAmp = 32768;

    if (ins->vibDepth > 0)
    {
        ch->eVibPos = 0;

        if (ins->vibSweep > 0)
        {
            ch->eVibAmp   = 0;
            ch->eVibSweep = (ins->vibDepth << 8) / ins->vibSweep;
        }
        else
        {
            ch->eVibAmp   = ins->vibDepth << 8;
            ch->eVibSweep = 0;
        }
    }
}

static void keyOff(stmTyp *ch)
{
    instrTyp *ins = ch->instrSeg;

    if (!(ins->envPTyp & ENV_ENABLED)) // 8bb: probably an FT2 bug
    {
        if (ch->envPCnt >= (uint16_t)ins->envPP[ch->envPPos][0])
            ch->envPCnt = ins->envPP[ch->envPPos][0] - 1;
    }

    if (ins->envVTyp & ENV_ENABLED)
    {
        if (ch->envVCnt >= (uint16_t)ins->envVP[ch->envVPos][0])
            ch->envVCnt = ins->envVP[ch->envVPos][0] - 1;
    }
    else
    {
        ch->realVol = 0;
        ch->outVol  = 0;
        ch->status |= IS_Vol + IS_QuickVol;
    }

    ch->envSustainActive = false;
}

static uint32_t getFrequenceValue(uint16_t period) // 8bb: converts period to 16.16fp resampling delta
{
    uint32_t delta;

    if (period == 0)
        return 0;

    if (linearFrqTab)
    {
        const uint16_t invPeriod =
            (12 * 192 * 4) - period; // 8bb: this intentionally underflows uint16_t to be accurate to FT2

        const uint32_t quotient  = invPeriod / 768;
        const uint32_t remainder = invPeriod % 768;

        const int32_t octShift = 14 - quotient;

        delta = (uint32_t)(((int64_t)logTab[remainder] * (int32_t)frequenceMulFactor) >> 24);
        delta >>= (octShift & 31); // 8bb: added needed 32-bit bitshift mask
    }
    else
    {
        delta = frequenceDivFactor / (uint32_t)period;
    }

    return delta;
}

static void startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch)
{
    if (ton == NOTE_KEYOFF)
    {
        keyOff(ch);
        return;
    }

    // 8bb: if we came from Rxy (retrig), we didn't check note (Ton) yet
    if (ton == 0)
    {
        ton = ch->tonNr;
        if (ton == 0)
            return; // 8bb: if still no note, return
    }

    ch->tonNr = ton;

    instrTyp *ins = instr[ch->instrNr];
    if (ins == NULL)
        ins = instr[0];

    ch->instrSeg = ins;
    ch->mute     = ins->mute;

    uint8_t smp  = ins->ta[ton - 1] & 0xF; // 8bb: added for safety
    ch->sampleNr = smp;

    sampleTyp *s = &ins->samp[smp];
    ch->relTonNr = s->relTon;

    ton += ch->relTonNr;
    if (ton >= 10 * 12)
        return;

    ch->oldVol = s->vol;
    ch->oldPan = s->pan;

    if (effTyp == 0x0E && (eff & 0xF0) == 0x50) // 8bb: EFx - Set Finetune
        ch->fineTune = ((eff & 0x0F) << 4) - 128;
    else
        ch->fineTune = s->fine;

    if (ton != 0)
    {
        const uint16_t tmpTon = ((ton - 1) << 4) + (((int8_t)ch->fineTune >> 3) + 16); // 8bb: 0..1935
        if (tmpTon < MAX_NOTES) // 8bb: tmpTon is *always* below MAX_NOTES here, this check is not needed
            ch->outPeriod = ch->realPeriod = note2Period[tmpTon];
    }

    ch->status |= IS_Period + IS_Vol + IS_Pan + IS_NyTon + IS_QuickVol;

    if (effTyp == 9) // 8bb: 9xx - Set Sample Offset
    {
        if (eff)
            ch->smpOffset = ch->eff;

        ch->smpStartPos = ch->smpOffset << 8;
    }
    else
    {
        ch->smpStartPos = 0;
    }

    P_StartTone(s, ch->smpStartPos);
}

static void dummy(stmTyp *ch, uint8_t param)
{
    return;

    (void)ch;
    (void)param;
}

static void finePortaUp(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->fPortaUpSpeed;

    ch->fPortaUpSpeed = param;

    ch->realPeriod -= param << 2;
    if ((int16_t)ch->realPeriod < 1)
        ch->realPeriod = 1;

    ch->outPeriod = ch->realPeriod;
    ch->status |= IS_Period;
}

static void finePortaDown(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->fPortaDownSpeed;

    ch->fPortaDownSpeed = param;

    ch->realPeriod += param << 2;
    if ((int16_t)ch->realPeriod > MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
        ch->realPeriod = MAX_FRQ - 1;

    ch->outPeriod = ch->realPeriod;
    ch->status |= IS_Period;
}

static void setGlissCtrl(stmTyp *ch, uint8_t param)
{
    ch->glissFunk = param;
}

static void setVibratoCtrl(stmTyp *ch, uint8_t param)
{
    ch->waveCtrl = (ch->waveCtrl & 0xF0) | param;
}

static void jumpLoop(stmTyp *ch, uint8_t param)
{
    if (param == 0)
    {
        ch->pattPos = song.pattPos & 0xFF;
    }
    else if (ch->loopCnt == 0)
    {
        ch->loopCnt = param;

        song.pBreakPos  = ch->pattPos;
        song.pBreakFlag = true;
    }
    else if (--ch->loopCnt > 0)
    {
        song.pBreakPos  = ch->pattPos;
        song.pBreakFlag = true;
    }
}

static void setTremoloCtrl(stmTyp *ch, uint8_t param)
{
    ch->waveCtrl = (param << 4) | (ch->waveCtrl & 0x0F);
}

static void volFineUp(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->fVolSlideUpSpeed;

    ch->fVolSlideUpSpeed = param;

    ch->realVol += param;
    if (ch->realVol > 64)
        ch->realVol = 64;

    ch->outVol = ch->realVol;
    ch->status |= IS_Vol;
}

static void volFineDown(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->fVolSlideDownSpeed;

    ch->fVolSlideDownSpeed = param;

    ch->realVol -= param;
    if ((int8_t)ch->realVol < 0)
        ch->realVol = 0;

    ch->outVol = ch->realVol;
    ch->status |= IS_Vol;
}

static void noteCut0(stmTyp *ch, uint8_t param)
{
    if (param == 0) // 8bb: only a parameter of zero is handled here
    {
        ch->realVol = 0;
        ch->outVol  = 0;
        ch->status |= IS_Vol + IS_QuickVol;
    }
}

static void pattDelay(stmTyp *ch, uint8_t param)
{
    if (song.pattDelTime2 == 0)
        song.pattDelTime = param + 1;

    (void)ch;
}

static void E_Effects_TickZero(stmTyp *ch, uint8_t param)
{
    EJumpTab_TickZero[param >> 4](ch, param & 0x0F);
}

static void posJump(stmTyp *ch, uint8_t param)
{
    song.songPos     = (int16_t)param - 1;
    song.pBreakPos   = 0;
    song.posJumpFlag = true;

    (void)ch;
}

static void pattBreak(stmTyp *ch, uint8_t param)
{
    song.posJumpFlag = true;

    param = ((param >> 4) * 10) + (param & 0x0F);
    if (param <= 63)
        song.pBreakPos = param;
    else
        song.pBreakPos = 0;

    (void)ch;
}

static void setSpeed(stmTyp *ch, uint8_t param)
{
    if (param >= 32)
    {
        song.speed = param;
        P_SetSpeed(song.speed);
    }
    else
    {
        song.timer = song.tempo = param;
    }

    (void)ch;
}

static void setGlobaVol(stmTyp *ch, uint8_t param)
{
    if (param > 64)
        param = 64;

    song.globVol = param;

    stmTyp *c = stm;
    for (int32_t i = 0; i < song.antChn; i++, c++) // 8bb: this updates the volume for all voices
        c->status |= IS_Vol;

    (void)ch;
}

static void setEnvelopePos(stmTyp *ch, uint8_t param)
{
    int8_t  envPos;
    bool    envUpdate;
    int16_t newEnvPos;

    instrTyp *ins = ch->instrSeg;

    // *** VOLUME ENVELOPE ***
    if (ins->envVTyp & ENV_ENABLED)
    {
        ch->envVCnt = param - 1;

        envPos    = 0;
        envUpdate = true;
        newEnvPos = param;

        if (ins->envVPAnt > 1)
        {
            envPos++;
            for (int32_t i = 0; i < ins->envVPAnt - 1; i++)
            {
                if (newEnvPos < ins->envVP[envPos][0])
                {
                    envPos--;

                    newEnvPos -= ins->envVP[envPos][0];
                    if (newEnvPos == 0)
                    {
                        envUpdate = false;
                        break;
                    }

                    if (ins->envVP[envPos + 1][0] <= ins->envVP[envPos][0])
                    {
                        envUpdate = true;
                        break;
                    }

                    ch->envVIPValue = ((ins->envVP[envPos + 1][1] - ins->envVP[envPos][1]) & 0xFF) << 8;
                    ch->envVIPValue /= (ins->envVP[envPos + 1][0] - ins->envVP[envPos][0]);

                    ch->envVAmp = (ch->envVIPValue * (newEnvPos - 1)) + ((ins->envVP[envPos][1] & 0xFF) << 8);

                    envPos++;

                    envUpdate = false;
                    break;
                }

                envPos++;
            }

            if (envUpdate)
                envPos--;
        }

        if (envUpdate)
        {
            ch->envVIPValue = 0;
            ch->envVAmp     = (ins->envVP[envPos][1] & 0xFF) << 8;
        }

        if (envPos >= ins->envVPAnt)
        {
            envPos = ins->envVPAnt - 1;
            if (envPos < 0)
                envPos = 0;
        }

        ch->envVPos = envPos;
    }

    // *** PANNING ENVELOPE ***
    if (ins->envVTyp & ENV_SUSTAIN) // 8bb: FT2 bug? (should probably have been "ins->envPTyp & ENV_ENABLED")
    {
        ch->envPCnt = param - 1;

        envPos    = 0;
        envUpdate = true;
        newEnvPos = param;

        if (ins->envPPAnt > 1)
        {
            envPos++;
            for (int32_t i = 0; i < ins->envPPAnt - 1; i++)
            {
                if (newEnvPos < ins->envPP[envPos][0])
                {
                    envPos--;

                    newEnvPos -= ins->envPP[envPos][0];
                    if (newEnvPos == 0)
                    {
                        envUpdate = false;
                        break;
                    }

                    if (ins->envPP[envPos + 1][0] <= ins->envPP[envPos][0])
                    {
                        envUpdate = true;
                        break;
                    }

                    ch->envPIPValue = ((ins->envPP[envPos + 1][1] - ins->envPP[envPos][1]) & 0xFF) << 8;
                    ch->envPIPValue /= (ins->envPP[envPos + 1][0] - ins->envPP[envPos][0]);

                    ch->envPAmp = (ch->envPIPValue * (newEnvPos - 1)) + ((ins->envPP[envPos][1] & 0xFF) << 8);

                    envPos++;

                    envUpdate = false;
                    break;
                }

                envPos++;
            }

            if (envUpdate)
                envPos--;
        }

        if (envUpdate)
        {
            ch->envPIPValue = 0;
            ch->envPAmp     = (ins->envPP[envPos][1] & 0xFF) << 8;
        }

        if (envPos >= ins->envPPAnt)
        {
            envPos = ins->envPPAnt - 1;
            if (envPos < 0)
                envPos = 0;
        }

        ch->envPPos = envPos;
    }
}

/* -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy command (multiretrig)
*/

static void v_SetVibSpeed(stmTyp *ch, uint8_t *volKol)
{
    *volKol = (ch->volKolVol & 0x0F) << 2;
    if (*volKol != 0)
        ch->vibSpeed = *volKol;
}

static void v_Volume(stmTyp *ch, uint8_t *volKol)
{
    *volKol -= 16;
    if (*volKol > 64) // 8bb: no idea why FT2 has this check, this can't happen...
        *volKol = 64;

    ch->outVol = ch->realVol = *volKol;
    ch->status |= IS_Vol + IS_QuickVol;
}

static void v_FineSlideDown(stmTyp *ch, uint8_t *volKol)
{
    *volKol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
    if ((int8_t)*volKol < 0)
        *volKol = 0;

    ch->outVol = ch->realVol = *volKol;
    ch->status |= IS_Vol;
}

static void v_FineSlideUp(stmTyp *ch, uint8_t *volKol)
{
    *volKol = (ch->volKolVol & 0x0F) + ch->realVol;
    if (*volKol > 64)
        *volKol = 64;

    ch->outVol = ch->realVol = *volKol;
    ch->status |= IS_Vol;
}

static void v_SetPan(stmTyp *ch, uint8_t *volKol)
{
    *volKol <<= 4;

    ch->outPan = *volKol;
    ch->status |= IS_Pan;
}

// -- non-tick-zero volume column effects --

static void v_SlideDown(stmTyp *ch)
{
    uint8_t newVol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
    if ((int8_t)newVol < 0)
        newVol = 0;

    ch->outVol = ch->realVol = newVol;
    ch->status |= IS_Vol;
}

static void v_SlideUp(stmTyp *ch)
{
    uint8_t newVol = (ch->volKolVol & 0x0F) + ch->realVol;
    if (newVol > 64)
        newVol = 64;

    ch->outVol = ch->realVol = newVol;
    ch->status |= IS_Vol;
}

static void v_Vibrato(stmTyp *ch)
{
    const uint8_t param = ch->volKolVol & 0xF;
    if (param > 0)
        ch->vibDepth = param;

    vibrato2(ch);
}

static void v_PanSlideLeft(stmTyp *ch)
{
    uint16_t tmp16 = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->outPan;
    if (tmp16 < 256) // 8bb: includes an FT2 bug: pan-slide-left of 0 = set pan to 0
        tmp16 = 0;

    ch->outPan = (uint8_t)tmp16;
    ch->status |= IS_Pan;
}

static void v_PanSlideRight(stmTyp *ch)
{
    uint16_t tmp16 = (ch->volKolVol & 0x0F) + ch->outPan;
    if (tmp16 > 255)
        tmp16 = 255;

    ch->outPan = (uint8_t)tmp16;
    ch->status |= IS_Pan;
}

static void v_TonePorta(stmTyp *ch)
{
    tonePorta(ch, 0); // 8bb: the last parameter is actually not used in tonePorta()
}

static void v_dummy(stmTyp *ch)
{
    (void)ch;
    return;
}

static void v_dummy2(stmTyp *ch, uint8_t *volKol)
{
    (void)ch;
    (void)volKol;
    return;
}

static void setPan(stmTyp *ch, uint8_t param)
{
    ch->outPan = param;
    ch->status |= IS_Pan;
}

static void setVol(stmTyp *ch, uint8_t param)
{
    if (param > 64)
        param = 64;

    ch->outVol = ch->realVol = param;
    ch->status |= IS_Vol + IS_QuickVol;
}

static void xFinePorta(stmTyp *ch, uint8_t param)
{
    const uint8_t type = param >> 4;
    param &= 0x0F;

    if (type == 0x1) // extra fine porta up
    {
        if (param == 0)
            param = ch->ePortaUpSpeed;

        ch->ePortaUpSpeed = param;

        uint16_t newPeriod = ch->realPeriod;

        newPeriod -= param;
        if ((int16_t)newPeriod < 1)
            newPeriod = 1;

        ch->outPeriod = ch->realPeriod = newPeriod;
        ch->status |= IS_Period;
    }
    else if (type == 0x2) // extra fine porta down
    {
        if (param == 0)
            param = ch->ePortaDownSpeed;

        ch->ePortaDownSpeed = param;

        uint16_t newPeriod = ch->realPeriod;

        newPeriod += param;
        if ((int16_t)newPeriod > MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
            newPeriod = MAX_FRQ - 1;

        ch->outPeriod = ch->realPeriod = newPeriod;
        ch->status |= IS_Period;
    }
}

static void doMultiRetrig(stmTyp *ch, uint8_t param) // 8bb: "param" is never used (needed for efx jumptable structure)
{
    uint8_t cnt = ch->retrigCnt + 1;
    if (cnt < ch->retrigSpeed)
    {
        ch->retrigCnt = cnt;
        return;
    }

    ch->retrigCnt = 0;

    int16_t vol = ch->realVol;
    switch (ch->retrigVol)
    {
    case 0x1:
        vol -= 1;
        break;
    case 0x2:
        vol -= 2;
        break;
    case 0x3:
        vol -= 4;
        break;
    case 0x4:
        vol -= 8;
        break;
    case 0x5:
        vol -= 16;
        break;
    case 0x6:
        vol = (vol >> 1) + (vol >> 3) + (vol >> 4);
        break;
    case 0x7:
        vol >>= 1;
        break;
    case 0x8:
        break; // 8bb: does not change the volume
    case 0x9:
        vol += 1;
        break;
    case 0xA:
        vol += 2;
        break;
    case 0xB:
        vol += 4;
        break;
    case 0xC:
        vol += 8;
        break;
    case 0xD:
        vol += 16;
        break;
    case 0xE:
        vol = (vol >> 1) + vol;
        break;
    case 0xF:
        vol += vol;
        break;
    default:
        break;
    }
    vol = CLAMP(vol, 0, 64);

    ch->realVol = (uint8_t)vol;
    ch->outVol  = ch->realVol;

    if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
    {
        ch->outVol  = ch->volKolVol - 0x10;
        ch->realVol = ch->outVol;
    }
    else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
    {
        ch->outPan = (ch->volKolVol & 0x0F) << 4;
    }

    startTone(0, 0, 0, ch);

    (void)param;
}

static void multiRetrig(stmTyp *ch, uint8_t param, uint8_t volumeColumnData)
{
    uint8_t tmpParam;

    tmpParam = param & 0x0F;
    if (tmpParam == 0)
        tmpParam = ch->retrigSpeed;

    ch->retrigSpeed = tmpParam;

    tmpParam = param >> 4;
    if (tmpParam == 0)
        tmpParam = ch->retrigVol;

    ch->retrigVol = tmpParam;

    if (volumeColumnData == 0)
        doMultiRetrig(ch, 0);        // 8bb: the second parameter is never used (needed for efx jumptable structure)
}

static void checkEffects(stmTyp *ch) // tick0 effect handling
{
    // volume column effects
    uint8_t newVolKol =
        ch->volKolVol; // 8bb: manipulated by vol. column effects, then used for multiretrig check (FT2 quirk)
    VJumpTab_TickZero[ch->volKolVol >> 4](ch, &newVolKol);

    // normal effects
    const uint8_t param = ch->eff;

    if ((ch->effTyp == 0 && param == 0) || ch->effTyp > 35)
        return;

    // 8bb: this one has to be done here instead of in the jumptable, as it needs the "newVolKol" parameter (FT2 quirk)
    if (ch->effTyp == 27) // 8bb: Rxy - Multi Retrig
    {
        multiRetrig(ch, param, newVolKol);
        return;
    }

    JumpTab_TickZero[ch->effTyp](ch, ch->eff);
}

static void fixTonePorta(stmTyp *ch, const tonTyp *p, uint8_t inst)
{
    if (p->ton > 0)
    {
        if (p->ton == NOTE_KEYOFF)
        {
            keyOff(ch);
        }
        else
        {
            const uint16_t portaTmp = (((p->ton - 1) + ch->relTonNr) << 4) + (((int8_t)ch->fineTune >> 3) + 16);
            if (portaTmp < MAX_NOTES)
            {
                ch->wantPeriod = note2Period[portaTmp];

                if (ch->wantPeriod == ch->realPeriod)
                    ch->portaDir = 0;
                else if (ch->wantPeriod > ch->realPeriod)
                    ch->portaDir = 1;
                else
                    ch->portaDir = 2;
            }
        }
    }

    if (inst > 0)
    {
        retrigVolume(ch);

        if (p->ton != NOTE_KEYOFF)
            retrigEnvelopeVibrato(ch);
    }
}

static void getNewNote(stmTyp *ch, const tonTyp *p)
{
    ch->volKolVol = p->vol;

    if (ch->effTyp == 0)
    {
        if (ch->eff != 0) // 8bb: we have an arpeggio (0xy) running, set period back
        {
            ch->outPeriod = ch->realPeriod;
            ch->status |= IS_Period;
        }
    }
    else
    {
        // 8bb: if we have a vibrato (4xy/6xy) on previous row (ch) that ends at current row (p), set period back
        if ((ch->effTyp == 4 || ch->effTyp == 6) && (p->effTyp != 4 && p->effTyp != 6))
        {
            ch->outPeriod = ch->realPeriod;
            ch->status |= IS_Period;
        }
    }

    ch->effTyp = p->effTyp;
    ch->eff    = p->eff;
    ch->tonTyp = (p->instr << 8) | p->ton;

    // 8bb: 'inst' var is used for later if-checks
    uint8_t inst = p->instr;
    if (inst > 0)
    {
        if (inst <= 128)
            ch->instrNr = inst;
        else
            inst = 0;
    }

    bool checkEfx = true;
    if (p->effTyp == 0x0E)                    // 8bb: check for EDx (Note Delay) and E90 (Retrigger Note)
    {
        if (p->eff >= 0xD1 && p->eff <= 0xDF) // 8bb: ED1..EDF (Note Delay)
            return;
        else if (p->eff == 0x90)              // 8bb: E90 (Retrigger Note)
            checkEfx = false;
    }

    if (checkEfx)
    {
        if ((ch->volKolVol & 0xF0) == 0xF0) // 8bb: Portamento (volume column)
        {
            const uint8_t volKolParam = ch->volKolVol & 0x0F;
            if (volKolParam > 0)
                ch->portaSpeed = volKolParam << 6;

            fixTonePorta(ch, p, inst);
            checkEffects(ch);
            return;
        }

        if (p->effTyp == 3 || p->effTyp == 5) // 8bb: Portamento (3xx/5xx)
        {
            if (p->effTyp != 5 && p->eff != 0)
                ch->portaSpeed = p->eff << 2;

            fixTonePorta(ch, p, inst);
            checkEffects(ch);
            return;
        }

        if (p->effTyp == 0x14 && p->eff == 0) // 8bb: K00 (Key Off - only handle tick 0 here)
        {
            keyOff(ch);

            if (inst)
                retrigVolume(ch);

            checkEffects(ch);
            return;
        }

        if (p->ton == 0)
        {
            if (inst > 0)
            {
                retrigVolume(ch);
                retrigEnvelopeVibrato(ch);
            }

            checkEffects(ch);
            return;
        }
    }

    if (p->ton == NOTE_KEYOFF)
        keyOff(ch);
    else
        startTone(p->ton, p->effTyp, p->eff, ch);

    if (inst > 0)
    {
        retrigVolume(ch);
        if (p->ton != NOTE_KEYOFF)
            retrigEnvelopeVibrato(ch);
    }

    checkEffects(ch);
}

static void fixaEnvelopeVibrato(stmTyp *ch)
{
    bool     envInterpolateFlag, envDidInterpolate;
    uint8_t  envPos;
    int16_t  autoVibVal;
    uint16_t autoVibAmp, envVal;
    uint32_t vol;

    instrTyp *ins = ch->instrSeg;

    // *** FADEOUT ***
    if (!ch->envSustainActive)
    {
        ch->status |= IS_Vol;

        if (ch->fadeOutAmp >= ch->fadeOutSpeed)
        {
            ch->fadeOutAmp -= ch->fadeOutSpeed;
        }
        else
        {
            ch->fadeOutAmp   = 0;
            ch->fadeOutSpeed = 0;
        }
    }

    if (!ch->mute)
    {
        // *** VOLUME ENVELOPE ***
        envVal = 0;
        if (ins->envVTyp & ENV_ENABLED)
        {
            envDidInterpolate = false;
            envPos            = ch->envVPos;

            if (++ch->envVCnt == ins->envVP[envPos][0])
            {
                ch->envVAmp = ins->envVP[envPos][1] << 8;

                envPos++;
                if (ins->envVTyp & ENV_LOOP)
                {
                    envPos--;

                    if (envPos == ins->envVRepE)
                    {
                        if (!(ins->envVTyp & ENV_SUSTAIN) || envPos != ins->envVSust || ch->envSustainActive)
                        {
                            envPos = ins->envVRepS;

                            ch->envVCnt = ins->envVP[envPos][0];
                            ch->envVAmp = ins->envVP[envPos][1] << 8;
                        }
                    }

                    envPos++;
                }

                if (envPos < ins->envVPAnt)
                {
                    envInterpolateFlag = true;
                    if ((ins->envVTyp & ENV_SUSTAIN) && ch->envSustainActive)
                    {
                        if (envPos - 1 == ins->envVSust)
                        {
                            envPos--;
                            ch->envVIPValue    = 0;
                            envInterpolateFlag = false;
                        }
                    }

                    if (envInterpolateFlag)
                    {
                        ch->envVPos = envPos;

                        ch->envVIPValue = 0;
                        if (ins->envVP[envPos][0] > ins->envVP[envPos - 1][0])
                        {
                            ch->envVIPValue = (ins->envVP[envPos][1] - ins->envVP[envPos - 1][1]) << 8;
                            ch->envVIPValue /= (ins->envVP[envPos][0] - ins->envVP[envPos - 1][0]);

                            envVal            = ch->envVAmp;
                            envDidInterpolate = true;
                        }
                    }
                }
                else
                {
                    ch->envVIPValue = 0;
                }
            }

            if (!envDidInterpolate)
            {
                ch->envVAmp += ch->envVIPValue;

                envVal = ch->envVAmp;
                if (envVal > 64 * 256)
                {
                    if (envVal > 128 * 256)
                        envVal = 64 * 256;
                    else
                        envVal = 0;

                    ch->envVIPValue = 0;
                }
            }

            envVal >>= 8;

            vol = (envVal * ch->outVol * ch->fadeOutAmp) >> (16 + 2);
            vol = (vol * song.globVol) >> 7;

            ch->status |= IS_Vol; // 8bb: this updates vol on every tick (because vol envelope is enabled)
        }
        else
        {
            vol = ((ch->outVol << 4) * ch->fadeOutAmp) >> 16;
            vol = (vol * song.globVol) >> 7;
        }

        ch->finalVol = (uint16_t)vol; // 0..256
    }
    else
    {
        ch->finalVol = 0;
    }

    // *** PANNING ENVELOPE ***

    envVal = 0;
    if (ins->envPTyp & ENV_ENABLED)
    {
        envDidInterpolate = false;
        envPos            = ch->envPPos;

        if (++ch->envPCnt == ins->envPP[envPos][0])
        {
            ch->envPAmp = ins->envPP[envPos][1] << 8;

            envPos++;
            if (ins->envPTyp & ENV_LOOP)
            {
                envPos--;

                if (envPos == ins->envPRepE)
                {
                    if (!(ins->envPTyp & ENV_SUSTAIN) || envPos != ins->envPSust || ch->envSustainActive)
                    {
                        envPos = ins->envPRepS;

                        ch->envPCnt = ins->envPP[envPos][0];
                        ch->envPAmp = ins->envPP[envPos][1] << 8;
                    }
                }

                envPos++;
            }

            if (envPos < ins->envPPAnt)
            {
                envInterpolateFlag = true;
                if ((ins->envPTyp & ENV_SUSTAIN) && ch->envSustainActive)
                {
                    if (envPos - 1 == ins->envPSust)
                    {
                        envPos--;
                        ch->envPIPValue    = 0;
                        envInterpolateFlag = false;
                    }
                }

                if (envInterpolateFlag)
                {
                    ch->envPPos = envPos;

                    ch->envPIPValue = 0;
                    if (ins->envPP[envPos][0] > ins->envPP[envPos - 1][0])
                    {
                        ch->envPIPValue = (ins->envPP[envPos][1] - ins->envPP[envPos - 1][1]) << 8;
                        ch->envPIPValue /= (ins->envPP[envPos][0] - ins->envPP[envPos - 1][0]);

                        envVal            = ch->envPAmp;
                        envDidInterpolate = true;
                    }
                }
            }
            else
            {
                ch->envPIPValue = 0;
            }
        }

        if (!envDidInterpolate)
        {
            ch->envPAmp += ch->envPIPValue;

            envVal = ch->envPAmp;
            if (envVal > 64 * 256)
            {
                if (envVal > 128 * 256)
                    envVal = 64 * 256;
                else
                    envVal = 0;

                ch->envPIPValue = 0;
            }
        }

        int16_t panTmp = ch->outPan - 128;
        if (panTmp > 0)
            panTmp = 0 - panTmp;
        panTmp += 128;

        panTmp <<= 3;
        envVal -= 32 * 256;

        ch->finalPan = ch->outPan + (uint8_t)(((int16_t)envVal * panTmp) >> 16);
        ch->status |= IS_Pan;
    }
    else
    {
        ch->finalPan = ch->outPan;
    }

    // *** AUTO VIBRATO ***
    if (ins->vibDepth > 0)
    {
        if (ch->eVibSweep > 0)
        {
            autoVibAmp = ch->eVibSweep;
            if (ch->envSustainActive)
            {
                autoVibAmp += ch->eVibAmp;
                if ((autoVibAmp >> 8) > ins->vibDepth)
                {
                    autoVibAmp    = ins->vibDepth << 8;
                    ch->eVibSweep = 0;
                }

                ch->eVibAmp = autoVibAmp;
            }
        }
        else
        {
            autoVibAmp = ch->eVibAmp;
        }

        ch->eVibPos += ins->vibRate;

        if (ins->vibTyp == 1)
            autoVibVal = (ch->eVibPos > 127) ? 64 : -64;          // square
        else if (ins->vibTyp == 2)
            autoVibVal = (((ch->eVibPos >> 1) + 64) & 127) - 64;  // ramp up
        else if (ins->vibTyp == 3)
            autoVibVal = ((-(ch->eVibPos >> 1) + 64) & 127) - 64; // ramp down
        else
            autoVibVal = vibSineTab[ch->eVibPos];                 // sine

        autoVibVal <<= 2;
        uint16_t tmpPeriod = (autoVibVal * (int16_t)autoVibAmp) >> 16;

        tmpPeriod += ch->outPeriod;
        if (tmpPeriod >= MAX_FRQ)
            tmpPeriod = 0; // 8bb: yes, FT2 does this (!)

        ch->finalPeriod = tmpPeriod;
        ch->status |= IS_Period;
    }
    else
    {
        ch->finalPeriod = ch->outPeriod;
    }
}

// 8bb: converts period to note number, for arpeggio and portamento (in semitone-slide mode)
static uint16_t relocateTon(uint16_t period, uint8_t arpNote, stmTyp *ch)
{
    int32_t tmpPeriod;

    const int32_t fineTune = ((int8_t)ch->fineTune >> 3) + 16;

    /* 8bb: FT2 bug, should've been 10*12*16. Notes above B-7 (95) will have issues.
    ** You can only achieve such high notes by having a high relative note value
    ** in the sample.
    */
    int32_t hiPeriod = 8 * 12 * 16;

    int32_t loPeriod = 0;

    for (int32_t i = 0; i < 8; i++)
    {
        tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + fineTune;

        int32_t lookUp = tmpPeriod - 8;
        if (lookUp < 0)
            lookUp =
                0; // 8bb: safety fix (C-0 w/ ftune <= -65). This buggy read seems to return 0 in FT2 (TODO: verify)

        if (period >= note2Period[lookUp])
            hiPeriod = (tmpPeriod - fineTune) & ~15;
        else
            loPeriod = (tmpPeriod - fineTune) & ~15;
    }

    tmpPeriod = loPeriod + fineTune + (arpNote << 4);
    if (tmpPeriod >= (8 * 12 * 16 + 15) - 1) // 8bb: FT2 bug, should've been 10*12*16+16 (also notice the +2 difference)
        tmpPeriod = (8 * 12 * 16 + 16) - 1;

    return note2Period[tmpPeriod];
}

static void vibrato2(stmTyp *ch)
{
    uint8_t tmpVib = (ch->vibPos >> 2) & 0x1F;

    switch (ch->waveCtrl & 3)
    {
    // 0: sine
    case 0:
        tmpVib = vibTab[tmpVib];
        break;

    // 1: ramp
    case 1: {
        tmpVib <<= 3;
        if ((int8_t)ch->vibPos < 0)
            tmpVib = ~tmpVib;
    }
    break;

    // 2/3: square
    default:
        tmpVib = 255;
        break;
    }

    tmpVib = (tmpVib * ch->vibDepth) >> 5;

    if ((int8_t)ch->vibPos < 0)
        ch->outPeriod = ch->realPeriod - tmpVib;
    else
        ch->outPeriod = ch->realPeriod + tmpVib;

    ch->status |= IS_Period;
    ch->vibPos += ch->vibSpeed;
}

static void arp(stmTyp *ch, uint8_t param)
{
    /* 8bb: The original arpTab table only supports 16 ticks, so it can and will overflow.
    ** I have added overflown values to the table so that we can handle up to 256 ticks.
    ** The added overflow entries are accurate to the overflow-read in FT2.08/FT2.09.
    */
    const uint8_t tick = arpTab[song.timer & 0xFF];

    if (tick == 0)
    {
        ch->outPeriod = ch->realPeriod;
    }
    else
    {
        const uint8_t note = (tick == 1) ? (param >> 4) : (param & 0x0F);
        ch->outPeriod      = relocateTon(ch->realPeriod, note, ch);
    }

    ch->status |= IS_Period;
}

static void portaUp(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->portaUpSpeed;

    ch->portaUpSpeed = param;

    ch->realPeriod -= param << 2;
    if ((int16_t)ch->realPeriod < 1)
        ch->realPeriod = 1;

    ch->outPeriod = ch->realPeriod;
    ch->status |= IS_Period;
}

static void portaDown(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->portaDownSpeed;

    ch->portaDownSpeed = param;

    ch->realPeriod += param << 2;
    if ((int16_t)ch->realPeriod > MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
        ch->realPeriod = MAX_FRQ - 1;

    ch->outPeriod = ch->realPeriod;
    ch->status |= IS_Period;
}

static void tonePorta(stmTyp *ch, uint8_t param) // 8bb: param is a placeholder (not used)
{
    if (ch->portaDir == 0)
        return;

    if (ch->portaDir > 1)
    {
        ch->realPeriod -= ch->portaSpeed;
        if ((int16_t)ch->realPeriod <= (int16_t)ch->wantPeriod)
        {
            ch->portaDir   = 1;
            ch->realPeriod = ch->wantPeriod;
        }
    }
    else
    {
        ch->realPeriod += ch->portaSpeed;
        if (ch->realPeriod >= ch->wantPeriod)
        {
            ch->portaDir   = 1;
            ch->realPeriod = ch->wantPeriod;
        }
    }

    if (ch->glissFunk) // 8bb: semitone-slide flag
        ch->outPeriod = relocateTon(ch->realPeriod, 0, ch);
    else
        ch->outPeriod = ch->realPeriod;

    ch->status |= IS_Period;

    (void)param;
}

static void vibrato(stmTyp *ch, uint8_t param)
{
    uint8_t tmp8;

    if (ch->eff > 0)
    {
        tmp8 = param & 0x0F;
        if (tmp8 > 0)
            ch->vibDepth = tmp8;

        tmp8 = (param & 0xF0) >> 2;
        if (tmp8 > 0)
            ch->vibSpeed = tmp8;
    }

    vibrato2(ch);
}

static void tonePlusVol(stmTyp *ch, uint8_t param)
{
    tonePorta(ch, 0); // 8bb: the last parameter is not used in tonePorta()
    volume(ch, param);

    (void)param;
}

static void vibratoPlusVol(stmTyp *ch, uint8_t param)
{
    vibrato2(ch);
    volume(ch, param);

    (void)param;
}

static void tremolo(stmTyp *ch, uint8_t param)
{
    uint8_t tmp8;
    int16_t tremVol;

    const uint8_t tmpEff = param;
    if (tmpEff > 0)
    {
        tmp8 = tmpEff & 0x0F;
        if (tmp8 > 0)
            ch->tremDepth = tmp8;

        tmp8 = (tmpEff & 0xF0) >> 2;
        if (tmp8 > 0)
            ch->tremSpeed = tmp8;
    }

    uint8_t tmpTrem = (ch->tremPos >> 2) & 0x1F;
    switch ((ch->waveCtrl >> 4) & 3)
    {
    // 0: sine
    case 0:
        tmpTrem = vibTab[tmpTrem];
        break;

    // 1: ramp
    case 1: {
        tmpTrem <<= 3;
        if ((int8_t)ch->vibPos < 0) // 8bb: FT2 bug, should've been ch->tremPos
            tmpTrem = ~tmpTrem;
    }
    break;

    // 2/3: square
    default:
        tmpTrem = 255;
        break;
    }
    tmpTrem = (tmpTrem * ch->tremDepth) >> 6;

    if ((int8_t)ch->tremPos < 0)
    {
        tremVol = ch->realVol - tmpTrem;
        if (tremVol < 0)
            tremVol = 0;
    }
    else
    {
        tremVol = ch->realVol + tmpTrem;
        if (tremVol > 64)
            tremVol = 64;
    }

    ch->outVol = (uint8_t)tremVol;
    ch->status |= IS_Vol;
    ch->tremPos += ch->tremSpeed;
}

static void volume(stmTyp *ch, uint8_t param) // 8bb: volume slide
{
    if (param == 0)
        param = ch->volSlideSpeed;

    ch->volSlideSpeed = param;

    uint8_t newVol = ch->realVol;
    if ((param & 0xF0) == 0)
    {
        newVol -= param;
        if ((int8_t)newVol < 0)
            newVol = 0;
    }
    else
    {
        param >>= 4;

        newVol += param;
        if (newVol > 64)
            newVol = 64;
    }

    ch->outVol = ch->realVol = newVol;
    ch->status |= IS_Vol;
}

static void globalVolSlide(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->globVolSlideSpeed;

    ch->globVolSlideSpeed = param;

    uint8_t newVol = (uint8_t)song.globVol;
    if ((param & 0xF0) == 0)
    {
        newVol -= param;
        if ((int8_t)newVol < 0)
            newVol = 0;
    }
    else
    {
        param >>= 4;

        newVol += param;
        if (newVol > 64)
            newVol = 64;
    }

    song.globVol = newVol;

    stmTyp *c = stm;
    for (int32_t i = 0; i < song.antChn; i++, c++) // 8bb: this updates the volume for all voices
        c->status |= IS_Vol;
}

static void keyOffCmd(stmTyp *ch, uint8_t param)
{
    if ((uint8_t)(song.tempo - song.timer) == (param & 31))
        keyOff(ch);
}

static void panningSlide(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->panningSlideSpeed;

    ch->panningSlideSpeed = param;

    int16_t newPan = (int16_t)ch->outPan;
    if ((param & 0xF0) == 0)
    {
        newPan -= param;
        if (newPan < 0)
            newPan = 0;
    }
    else
    {
        param >>= 4;

        newPan += param;
        if (newPan > 255)
            newPan = 255;
    }

    ch->outPan = (uint8_t)newPan;
    ch->status |= IS_Pan;
}

static void tremor(stmTyp *ch, uint8_t param)
{
    if (param == 0)
        param = ch->tremorSave;

    ch->tremorSave = param;

    uint8_t tremorSign = ch->tremorPos & 0x80;
    uint8_t tremorData = ch->tremorPos & 0x7F;

    tremorData--;
    if ((int8_t)tremorData < 0)
    {
        if (tremorSign == 0x80)
        {
            tremorSign = 0x00;
            tremorData = param & 0x0F;
        }
        else
        {
            tremorSign = 0x80;
            tremorData = param >> 4;
        }
    }

    ch->tremorPos = tremorSign | tremorData;
    ch->outVol    = (tremorSign == 0x80) ? ch->realVol : 0;
    ch->status |= IS_Vol + IS_QuickVol;
}

static void retrigNote(stmTyp *ch, uint8_t param)
{
    if (param == 0) // 8bb: E9x with a param of zero is handled in getNewNote()
        return;

    if ((song.tempo - song.timer) % param == 0)
    {
        startTone(0, 0, 0, ch);
        retrigEnvelopeVibrato(ch);
    }
}

static void noteCut(stmTyp *ch, uint8_t param)
{
    if ((uint8_t)(song.tempo - song.timer) == param)
    {
        ch->outVol = ch->realVol = 0;
        ch->status |= IS_Vol + IS_QuickVol;
    }
}

static void noteDelay(stmTyp *ch, uint8_t param)
{
    if ((uint8_t)(song.tempo - song.timer) == param)
    {
        startTone(ch->tonTyp & 0xFF, 0, 0, ch);

        if ((ch->tonTyp & 0xFF00) > 0) // 8bb: do we have an instrument number?
            retrigVolume(ch);

        retrigEnvelopeVibrato(ch);

        if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
        {
            ch->outVol  = ch->volKolVol - 16;
            ch->realVol = ch->outVol;
        }
        else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
        {
            ch->outPan = (ch->volKolVol & 0x0F) << 4;
        }
    }
}

static void E_Effects_TickNonZero(stmTyp *ch, uint8_t param)
{
    EJumpTab_TickNonZero[param >> 4](ch, param & 0xF);
}

static void doEffects(stmTyp *ch) // tick>0 effect handling
{
    const uint8_t volKolEfx = ch->volKolVol >> 4;
    if (volKolEfx > 0)
        VJumpTab_TickNonZero[volKolEfx](ch);

    if ((ch->eff == 0 && ch->effTyp == 0) || ch->effTyp > 35)
        return;

    JumpTab_TickNonZero[ch->effTyp](ch, ch->eff);
}

static void getNextPos(void)
{
    song.pattPos++;

    if (song.pattDelTime > 0)
    {
        song.pattDelTime2 = song.pattDelTime;
        song.pattDelTime  = 0;
    }

    if (song.pattDelTime2 > 0)
    {
        song.pattDelTime2--;
        if (song.pattDelTime2 > 0)
            song.pattPos--;
    }

    if (song.pBreakFlag)
    {
        song.pBreakFlag = false;
        song.pattPos    = song.pBreakPos;
    }

    if (song.pattPos >= song.pattLen || song.posJumpFlag)
    {
        song.pattPos     = song.pBreakPos;
        song.pBreakPos   = 0;
        song.posJumpFlag = false;

        song.songPos++;
        if (song.songPos >= song.len)
            song.songPos = song.repS;

        song.pattNr  = song.songTab[(uint8_t)song.songPos];
        song.pattLen = pattLens[(uint8_t)song.pattNr];
    }
}

static void mainPlayer(void)
{
    if (musicPaused)
        return;

    bool tickZero = false;

    song.timer--;
    if (song.timer == 0)
    {
        song.timer = song.tempo;
        tickZero   = true;
    }

    const bool readNewNote = tickZero && (song.pattDelTime2 == 0);
    if (readNewNote)
    {
        const tonTyp *pattPtr = nilPatternLine;
        if (patt[song.pattNr] != NULL)
            pattPtr = &patt[song.pattNr][song.pattPos * song.antChn];

        stmTyp *c = stm;
        for (uint8_t i = 0; i < song.antChn; i++, c++, pattPtr++)
        {
            PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
            getNewNote(c, pattPtr);
            fixaEnvelopeVibrato(c);
        }
    }
    else
    {
        stmTyp *c = stm;
        for (uint8_t i = 0; i < song.antChn; i++, c++)
        {
            PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
            doEffects(c);
            fixaEnvelopeVibrato(c);
        }
    }

    if (song.timer == 1)
        getNextPos();
}

static void mix_UpdateChannel(int32_t nr, WaveChannelInfoType *WCI);

static void P_SetSpeed(uint16_t bpm)
{
    // 8bb: added this
    if (bpm == 0)
        bpm = 125;

    speedVal = ((realReplayRate + realReplayRate) + (realReplayRate >> 1)) /
               bpm; // 8bb: same as doing "((realReplayRate * 5) / 2) / bpm"
}

static void P_StartTone(sampleTyp *s, int32_t smpStartPos)
{
    WaveChannelInfoType WCI;

    WCI.SStartPos = smpStartPos;
    WCI.SBase     = s->pek;
    WCI.SLen      = s->len;
    WCI.SRepS     = s->repS;
    WCI.SRepL     = s->repL;
    WCI.SType     = s->typ;
    WCI.Status    = Status_StartTone + Status_StopTone;

    mix_UpdateChannel(PMPTmpActiveChannel, &WCI);
}

// 8bb: added these two
static bool mix_Init(int32_t audioBufferSize)
{
    CDA_MixBuffer = (int32_t *)M4P_MALLOC(audioBufferSize * 2 * sizeof(int32_t));
    if (CDA_MixBuffer == NULL)
        return false;

    PMPLeft = 0;
    return true;
}

static void mix_Free(void)
{
    if (CDA_MixBuffer != NULL)
    {
        M4P_FREE(CDA_MixBuffer);
        CDA_MixBuffer = NULL;
    }
}
// --------------------

static void updateVolume(CIType *v, int32_t volIPLen)
{
    const uint32_t vol = v->SVol * CDA_Amp;

    v->SLVol1 = (vol * panningTab[256 - v->SPan]) >> (32 - 28);
    v->SRVol1 = (vol * panningTab[v->SPan]) >> (32 - 28);

    if (volumeRampingFlag)
    {
        v->SLVolIP   = (v->SLVol1 - v->SLVol2) / volIPLen;
        v->SRVolIP   = (v->SRVol1 - v->SRVol2) / volIPLen;
        v->SVolIPLen = volIPLen;
    }
}

static void mix_UpdateChannel(int32_t nr, WaveChannelInfoType *WCI)
{
    CIType       *v      = &CI[chnReloc[nr]];
    const uint8_t status = WCI->Status;

    if (status & Status_StopTone)
    {
        if (volumeRampingFlag)
        {
            // 8bb: fade out current voice
            v->SType |= SType_Fadeout;
            v->SVol = 0;
            updateVolume(v, quickVolSizeVal);

            // 8bb: swap current voice with neighbor
            chnReloc[nr] ^= 1;
            v = &CI[chnReloc[nr]];
        }

        v->SType = SType_Off;
    }

    if (status & Status_SetPan)
        v->SPan = (uint8_t)WCI->SPan;

    if (status & Status_SetVol)
    {
        uint16_t vol = WCI->SVol;
        if (vol > 0)
            vol--; // 8bb: 0..256 -> 0..255 ( FT2 does this to prevent mul overflow in updateVolume() )
        v->SVol = (uint8_t)vol;
    }

    if (status & (Status_SetVol + Status_SetPan))
        updateVolume(v, (status & Status_QuickVol) ? quickVolSizeVal : speedVal);

    if (status & Status_SetFrq)
        v->SFrq = WCI->SFrq;

    if (status & Status_StartTone)
    {
        int32_t len;

        uint8_t    type        = WCI->SType;
        const bool sample16Bit = (type >> 4) & 1;

        if (type & (SType_Fwd + SType_Rev))
        {
            int32_t repL = WCI->SRepL;
            int32_t repS = WCI->SRepS;

            if (sample16Bit)
            {
                repL >>= 1;
                repS >>= 1;

                v->SRevBase = (int16_t *)WCI->SBase + (repS + repS + repL);
            }
            else
            {
                v->SRevBase = (int8_t *)WCI->SBase + (repS + repS + repL);
            }

            v->SRepL = repL;
            v->SRepS = repS;

            len = repS + repL;
        }
        else
        {
            type &= ~(SType_Fwd + SType_Rev); // 8bb: keep loop flags only

            len = WCI->SLen;
            if (sample16Bit)
                len >>= 1;

            if (len == 0)
                return;
        }

        // 8bb: overflown 9xx (set sample offset), cut voice (voice got ended earlier in "if (status &
        // Status_StopTone)")
        if (WCI->SStartPos >= len)
            return;

        v->SLen     = len;
        v->SPos     = WCI->SStartPos;
        v->SPosDec  = 0;
        v->SBase    = WCI->SBase;
        v->SMixType = (sample16Bit * 4) + (volumeRampingFlag * 2) + interpolationFlag;
        v->SType    = type;
    }
}

static void mix_UpdateChannelVolPanFrq(void)
{
    WaveChannelInfoType WCI;
    memset(&WCI, 0, sizeof(WaveChannelInfoType));

    stmTyp *ch = stm;
    for (int32_t i = 0; i < song.antChn; i++, ch++)
    {
        uint8_t newStatus = 0;

        const uint8_t status = ch->status;
        ch->status           = 0;

        if (status == 0)
            continue;

        if (status & IS_Vol)
        {
            WCI.SVol = ch->finalVol;
            newStatus |= Status_SetVol;
        }

        if (status & IS_QuickVol)
            newStatus |= Status_QuickVol;

        if (status & IS_Pan)
        {
            WCI.SPan = ch->finalPan;
            newStatus |= Status_SetPan;
        }

        if (status & IS_Period)
        {
            WCI.SFrq = getFrequenceValue(ch->finalPeriod);
            newStatus |= Status_SetFrq;
        }

        WCI.Status = newStatus;
        mix_UpdateChannel(i, &WCI);
    }
}

static void mix_ClearChannels(void) // 8bb: rewritten to handle all voices instead of song.antChn
{
    memset(CI, 0, sizeof(CI));

    CIType *v = CI;
    for (int16_t i = 0; i < 32 * 2; i++, v++)
    {
        v->SPan  = 128;
        v->SType = SType_Off;
    }

    for (int16_t i = 0; i < 32; i++)
        chnReloc[i] = i + i;
}

static void mix_SaveIPVolumes(void)
{
    CIType *v = CI;
    for (int32_t i = 0; i < song.antChn * 2; i++, v++)
    {
        // 8bb: this cuts any active fade-out voices (volume ramping)
        if (v->SType & SType_Fadeout)
            v->SType = SType_Off;

        v->SLVol2    = v->SLVol1;
        v->SRVol2    = v->SRVol1;
        v->SVolIPLen = 0;
    }
}

static void mix_UpdateBuffer(int16_t *buffer, int32_t numSamples)
{
    if (numSamples <= 0)
        return;

    if (musicPaused) // silence output
    {
        memset(buffer, 0, numSamples * (2 * sizeof(int16_t)));
        return;
    }

    memset(CDA_MixBuffer, 0, numSamples * (2 * sizeof(int32_t)));

    int32_t c = 0;
    int32_t a = numSamples;

    while (a > 0)
    {
        if (PMPLeft == 0)
        {
            mix_SaveIPVolumes();
            mainPlayer();
            mix_UpdateChannelVolPanFrq();
            PMPLeft = speedVal;
        }

        int32_t b = a;
        if (b > PMPLeft)
            b = PMPLeft;

        CIType *v = CI;
        for (int32_t i = 0; i < song.antChn * 2; i++, v++)
            PMPMix32Proc(v, b, c);

        c += b;
        a -= b;
        PMPLeft -= b;
    }

    numSamples *= 2; // 8bb: stereo

    /* 8bb: Done a bit differently since we don't use a
    ** Sound Blaster with its master volume setting.
    ** Instead we change the amplitude here.
    */

    if (masterVol == 256) // 8bb: max master volume, no need to change amp
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            buffer[i] = (int16_t)out32;
        }
    }
    else
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            out32     = (out32 * masterVol) >> 8;
            buffer[i] = (int16_t)out32;
        }
    }
}

static void mix_UpdateBufferFloat(float *buffer, int32_t numSamples)
{
    if (numSamples <= 0)
        return;

    if (musicPaused) // silence output
    {
        memset(buffer, 0, numSamples * (2 * sizeof(int16_t)));
        return;
    }

    memset(CDA_MixBuffer, 0, numSamples * (2 * sizeof(int32_t)));

    int32_t c = 0;
    int32_t a = numSamples;

    while (a > 0)
    {
        if (PMPLeft == 0)
        {
            mix_SaveIPVolumes();
            mainPlayer();
            mix_UpdateChannelVolPanFrq();
            PMPLeft = speedVal;
        }

        int32_t b = a;
        if (b > PMPLeft)
            b = PMPLeft;

        CIType *v = CI;
        for (int32_t i = 0; i < song.antChn * 2; i++, v++)
            PMPMix32Proc(v, b, c);

        c += b;
        a -= b;
        PMPLeft -= b;
    }

    numSamples *= 2; // 8bb: stereo

                     /* 8bb: Done a bit differently since we don't use a
                     ** Sound Blaster with its master volume setting.
                     ** Instead we change the amplitude here.
                     */

#if defined _MSC_VER || (defined __SIZEOF_FLOAT__ && __SIZEOF_FLOAT__ == 4)
    if (masterVol == 256) // 8bb: max master volume, no need to change amp
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            *(uint32_t *)buffer = 0x43818000 ^ ((uint16_t)out32);
            *buffer++ -= 259.0f;
        }
    }
    else
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            out32               = (out32 * masterVol) >> 8;
            *(uint32_t *)buffer = 0x43818000 ^ ((uint16_t)out32);
            *buffer++ -= 259.0f;
        }
    }
#else
    if (masterVol == 256) // 8bb: max master volume, no need to change amp
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            *buffer++ = (float)out32 * 0.000030517578125f;
        }
    }
    else
    {
        for (int32_t i = 0; i < numSamples; i++)
        {
            int32_t out32 = CDA_MixBuffer[i] >> 8;
            CLAMP16(out32);
            out32     = (out32 * masterVol) >> 8;
            *buffer++ = (float)out32 * 0.000030517578125f;
        }
    }
#endif
}

/***************************************************************************
 *        ROUTINES FOR SAMPLE HANDLING ETC.                                *
 ***************************************************************************/

// 8bb: modifies wrapped sample after loop/end (for branchless mixer interpolation)
static void fixSample(sampleTyp *s)
{
    if (s->pek == NULL)
        return; // empty sample

    const bool sample16Bit = !!(s->typ & SAMPLE_16BIT);
    uint8_t    loopType    = s->typ & 3;
    int16_t   *ptr16       = (int16_t *)s->pek;
    int32_t    len         = s->len;
    int32_t    loopStart   = s->repS;
    int32_t    loopEnd     = s->repS + s->repL;

    if (sample16Bit)
    {
        len >>= 1;
        loopStart >>= 1;
        loopEnd >>= 1;
    }

    if (len < 1)
        return;

    /* 8bb:
    ** This is the exact bit test order of which FT2 handles
    ** the sample tap fix.
    **
    ** This order is important for rare cases where both the
    ** "forward" and "pingpong" loop bits are set at once.
    **
    ** This means that if both flags are set, the mixer will
    ** play the sample with pingpong looping, but the sample fix
    ** is handled as if it was a forward loop. This results in
    ** the wrong interpolation tap sample being written after the
    ** loop end point.
    */

    if (loopType & LOOP_FORWARD)
    {
        if (sample16Bit)
            ptr16[loopEnd] = ptr16[loopStart];
        else
            s->pek[loopEnd] = s->pek[loopStart];

        return;
    }
    else if (loopType & LOOP_PINGPONG)
    {
        if (sample16Bit)
            ptr16[loopEnd] = ptr16[loopEnd - 1];
        else
            s->pek[loopEnd] = s->pek[loopEnd - 1];
    }
    else // no loop
    {
        if (sample16Bit)
            ptr16[len] = 0;
        else
            s->pek[len] = 0;
    }
}

static void checkSampleRepeat(int32_t nr, int32_t nr2)
{
    instrTyp *i = instr[nr];
    if (i == NULL)
        return;

    sampleTyp *s = &i->samp[nr2];

    if (s->repS < 0)
        s->repS = 0;
    if (s->repL < 0)
        s->repL = 0;
    if (s->repS > s->len)
        s->repS = s->len;
    if (s->repS + s->repL > s->len)
        s->repL = s->len - s->repS;
}

static void upDateInstrs(void)
{
    for (int32_t i = 0; i <= 128; i++)
    {
        instrTyp *ins = instr[i];
        if (ins == NULL)
            continue;

        sampleTyp *s = ins->samp;
        for (int32_t j = 0; j < 16; j++, s++)
        {
            checkSampleRepeat(i, j);
            fixSample(s);

            if (s->pek == NULL)
            {
                s->len  = 0;
                s->repS = 0;
                s->repL = 0;
            }
        }
    }
}

static bool patternEmpty(uint16_t nr)
{
    if (patt[nr] == NULL)
        return true;

    const uint8_t *scanPtr = (const uint8_t *)patt[nr];
    const int32_t  scanLen = pattLens[nr] * song.antChn * sizeof(tonTyp);

    for (int32_t i = 0; i < scanLen; i++)
    {
        if (scanPtr[i] != 0)
            return false;
    }

    return true;
}

static bool allocateInstr(uint16_t i)
{
    if (instr[i] != NULL)
        return true;

    instrTyp *p = (instrTyp *)M4P_CALLOC(1, sizeof(instrTyp));
    if (p == NULL)
        return false;

    sampleTyp *s = p->samp;
    for (int32_t j = 0; j < 16; j++, s++)
    {
        s->pan = 128;
        s->vol = 64;
    }

    instr[i] = p;
    return true;
}

static void freeInstr(uint16_t nr)
{
    if (nr > 128)
        return;

    instrTyp *ins = instr[nr];
    if (ins == NULL)
        return;

    sampleTyp *s = ins->samp;
    for (uint8_t i = 0; i < 16; i++, s++)
    {
        if (s->pek != NULL)
            M4P_FREE(s->pek);
    }

    M4P_FREE(ins);
    instr[nr] = NULL;
}

static void freeAllInstr(void)
{
    for (uint16_t i = 0; i <= 128; i++)
        freeInstr(i);
}

static void freeAllPatterns(void) // 8bb: added this one, since it's handy
{
    for (int32_t i = 0; i < 256; i++)
    {
        if (patt[i] != NULL)
        {
            M4P_FREE(patt[i]);
            patt[i] = NULL;
        }

        pattLens[i] = 64;
    }
}

static void delta2Samp(int8_t *p, uint32_t len, bool sample16Bit)
{
    if (sample16Bit)
    {
        len >>= 1;

        int16_t *p16 = (int16_t *)p;

        int16_t olds16 = 0;
        for (uint32_t i = 0; i < len; i++)
        {
            const int16_t news16 = p16[i] + olds16;
            p16[i]               = news16;
            olds16               = news16;
        }
    }
    else
    {
        int8_t *p8 = (int8_t *)p;

        int8_t olds8 = 0;
        for (uint32_t i = 0; i < len; i++)
        {
            const int8_t news8 = p8[i] + olds8;
            p8[i]              = news8;
            olds8              = news8;
        }
    }
}

static void unpackPatt(uint8_t *dst, uint16_t inn, uint16_t len, uint8_t antChn)
{
    if (dst == NULL)
        return;

    const uint8_t *src    = dst + inn;
    const int32_t  srcEnd = len * (sizeof(tonTyp) * antChn);

    int32_t srcIdx = 0;
    for (int32_t i = 0; i < len; i++)
    {
        for (int32_t j = 0; j < antChn; j++)
        {
            if (srcIdx >= srcEnd)
                return; // error!

            const uint8_t note = *src++;
            if (note & 0x80)
            {
                *dst++ = (note & 0x01) ? *src++ : 0;
                *dst++ = (note & 0x02) ? *src++ : 0;
                *dst++ = (note & 0x04) ? *src++ : 0;
                *dst++ = (note & 0x08) ? *src++ : 0;
                *dst++ = (note & 0x10) ? *src++ : 0;
            }
            else
            {
                *dst++ = note;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
            }

            // 8bb: added this. If note >97, remove it (prevents out-of-range read in note->sample LUT)
            if (*(dst - 5) > 97)
                *(dst - 5) = 0;

            srcIdx += sizeof(tonTyp);
        }
    }
}

static void stopVoices(void)
{
    stmTyp *ch = stm;
    for (uint8_t i = 0; i < 32; i++, ch++)
    {
        ch->tonTyp   = 0;
        ch->relTonNr = 0;
        ch->instrNr  = 0;
        ch->instrSeg = instr[0]; // 8bb: placeholder instrument
        ch->status   = IS_Vol;

        ch->realVol  = 0;
        ch->outVol   = 0;
        ch->oldVol   = 0;
        ch->finalVol = 0;
        ch->oldPan   = 128;
        ch->outPan   = 128;
        ch->finalPan = 128;
        ch->vibDepth = 0;
    }
}

static void setPos(int32_t pos, int32_t row) // -1 = don't change
{
    if (pos != -1)
    {
        song.songPos = (int16_t)pos;
        if (song.len > 0 && song.songPos >= song.len)
            song.songPos = song.len - 1;

        song.pattNr  = song.songTab[song.songPos];
        song.pattLen = pattLens[song.pattNr];
    }

    if (row != -1)
    {
        song.pattPos = (int16_t)row;
        if (song.pattPos >= song.pattLen)
            song.pattPos = song.pattLen - 1;
    }

    song.timer = 1;
}

static void resetMusic(void)
{
    song.timer = 1;
    stopVoices();
    setPos(0, 0);
}

static void freeMusic(void)
{
    stopMusic();
    freeAllInstr();
    freeAllPatterns();

    song.tempo = 6;
    song.speed = 125;
    song.timer = 1;

    setFrqTab(true);
    resetMusic();
}

/***************************************************************************
 *        MODULE LOADING ROUTINES                                          *
 ***************************************************************************/

static bool loadInstrHeader(MEMFILE *f, uint16_t i)
{
    instrHeaderTyp ih;

    memset(&ih, 0, INSTR_HEADER_SIZE);
    mread(&ih.instrSize, 4, 1, f);
    if (ih.instrSize > INSTR_HEADER_SIZE)
        ih.instrSize = INSTR_HEADER_SIZE;

    if (ih.instrSize < 4) // 8bb: added protection
        return false;

    mread(ih.name, ih.instrSize - 4, 1, f);

    if (ih.antSamp > 16)
        return false;

    if (ih.antSamp > 0)
    {
        if (!allocateInstr(i))
            return false;

        instrTyp *ins = instr[i];

        memcpy(ins->name, ih.name, 22);
        ins->name[22] = '\0';

        // 8bb: copy instrument header elements to our instrument struct
        memcpy(ins->ta, ih.ta, 96);
        memcpy(ins->envVP, ih.envVP, 12 * 2 * sizeof(int16_t));
        memcpy(ins->envPP, ih.envPP, 12 * 2 * sizeof(int16_t));
        ins->envVPAnt = ih.envVPAnt;
        ins->envPPAnt = ih.envPPAnt;
        ins->envVSust = ih.envVSust;
        ins->envVRepS = ih.envVRepS;
        ins->envVRepE = ih.envVRepE;
        ins->envPSust = ih.envPSust;
        ins->envPRepS = ih.envPRepS;
        ins->envPRepE = ih.envPRepE;
        ins->envVTyp  = ih.envVTyp;
        ins->envPTyp  = ih.envPTyp;
        ins->vibTyp   = ih.vibTyp;
        ins->vibSweep = ih.vibSweep;
        ins->vibDepth = ih.vibDepth;
        ins->vibRate  = ih.vibRate;
        ins->fadeOut  = ih.fadeOut;
        ins->mute     = (ih.mute == 1) ? true : false; // 8bb: correct logic!
        ins->antSamp  = ih.antSamp;

        if (mread(ih.samp, ih.antSamp * sizeof(sampleHeaderTyp), 1, f) != 1)
            return false;

        sampleTyp       *s   = instr[i]->samp;
        sampleHeaderTyp *src = ih.samp;
        for (int32_t j = 0; j < ih.antSamp; j++, s++, src++)
        {
            memcpy(s->name, src->name, 22);
            s->name[22] = '\0';

            s->len    = src->len;
            s->repS   = src->repS;
            s->repL   = src->repL;
            s->vol    = src->vol;
            s->fine   = src->fine;
            s->typ    = src->typ;
            s->pan    = src->pan;
            s->relTon = src->relTon;
        }
    }

    return true;
}

static bool loadInstrSample(MEMFILE *f, uint16_t i)
{
    if (instr[i] == NULL)
        return true; // empty instrument

    sampleTyp *s = instr[i]->samp;
    for (uint16_t j = 0; j < instr[i]->antSamp; j++, s++)
    {
        if (s->len > 0)
        {
            bool sample16Bit = !!(s->typ & SAMPLE_16BIT);

            s->pek = (int8_t *)M4P_MALLOC(s->len + 2); // 8bb: +2 for fixed interpolation tap sample
            if (s->pek == NULL)
                return false;

            mread(s->pek, 1, s->len, f);
            delta2Samp(s->pek, s->len, sample16Bit);
        }

        checkSampleRepeat(i, j);
    }

    return true;
}

static bool loadPatterns(MEMFILE *f, uint16_t antPtn)
{
    uint8_t          tmpLen;
    patternHeaderTyp ph;

    for (uint16_t i = 0; i < antPtn; i++)
    {
        mread(&ph.patternHeaderSize, 4, 1, f);
        mread(&ph.typ, 1, 1, f);

        ph.pattLen = 0;
        if (song.ver == 0x0102)
        {
            mread(&tmpLen, 1, 1, f);
            mread(&ph.dataLen, 2, 1, f);
            ph.pattLen = (uint16_t)tmpLen + 1; // 8bb: +1 in v1.02

            if (ph.patternHeaderSize > 8)
                mseek(f, ph.patternHeaderSize - 8, SEEK_CUR);
        }
        else
        {
            mread(&ph.pattLen, 2, 1, f);
            mread(&ph.dataLen, 2, 1, f);

            if (ph.patternHeaderSize > 9)
                mseek(f, ph.patternHeaderSize - 9, SEEK_CUR);
        }

        if (meof(f))
        {
            mclose(&f);
            return false;
        }

        pattLens[i] = ph.pattLen;
        if (ph.dataLen)
        {
            const uint16_t a = ph.pattLen * song.antChn * sizeof(tonTyp);

            patt[i] = (tonTyp *)M4P_MALLOC(a);
            if (patt[i] == NULL)
                return false;

            uint8_t *pattPtr = (uint8_t *)patt[i];

            memset(pattPtr, 0, a);
            mread(&pattPtr[a - ph.dataLen], 1, ph.dataLen, f);
            unpackPatt(pattPtr, a - ph.dataLen, ph.pattLen, song.antChn);
        }

        if (patternEmpty(i))
        {
            if (patt[i] != NULL)
            {
                M4P_FREE(patt[i]);
                patt[i] = NULL;
            }

            pattLens[i] = 64;
        }
    }

    return true;
}

static bool loadMusicMOD(MEMFILE *f)
{
    uint8_t             ha[sizeof(songMOD31HeaderTyp)];
    songMOD31HeaderTyp *h_MOD31 = (songMOD31HeaderTyp *)ha;
    songMOD15HeaderTyp *h_MOD15 = (songMOD15HeaderTyp *)ha;

    mread(ha, sizeof(ha), 1, f);
    if (meof(f))
        goto loadError2;

    memcpy(song.name, h_MOD31->name, 20);
    song.name[20] = '\0';

    uint8_t j = 0;
    for (uint8_t i = 1; i <= 16; i++)
    {
        if (memcmp(h_MOD31->Sig, MODSig[i - 1], 4) == 0)
            j = i + i;
    }

    if (memcmp(h_MOD31->Sig, "M!K!", 4) == 0 || memcmp(h_MOD31->Sig, "FLT4", 4) == 0)
        j = 4;

    if (memcmp(h_MOD31->Sig, "OCTA", 4) == 0)
        j = 8;

    uint8_t typ;
    if (j > 0)
    {
        typ         = 1;
        song.antChn = j;
    }
    else
    {
        typ         = 2;
        song.antChn = 4;
    }

    int16_t ai;
    if (typ == 1)
    {
        mseek(f, sizeof(songMOD31HeaderTyp), SEEK_SET);
        song.len  = h_MOD31->len;
        song.repS = h_MOD31->repS;
        memcpy(song.songTab, h_MOD31->songTab, 128);
        ai = 31;
    }
    else
    {
        mseek(f, sizeof(songMOD15HeaderTyp), SEEK_SET);
        song.len  = h_MOD15->len;
        song.repS = h_MOD15->repS;
        memcpy(song.songTab, h_MOD15->songTab, 128);
        ai = 15;
    }

    song.antInstrs = ai; // 8bb: added this

    if (meof(f))
        goto loadError2;

    int32_t b = 0;
    for (int32_t a = 0; a < 128; a++)
    {
        if (song.songTab[a] > b)
            b = song.songTab[a];
    }

    uint8_t pattBuf[32 * 4 * 64]; // 8bb: max pattern size (32 channels, 64 rows)
    for (uint16_t a = 0; a <= b; a++)
    {
        patt[a] = (tonTyp *)M4P_CALLOC(song.antChn * 64, sizeof(tonTyp));
        if (patt[a] == NULL)
            goto loadError;

        pattLens[a] = 64;

        mread(pattBuf, 1, song.antChn * 4 * 64, f);
        if (meof(f))
            goto loadError;

        // convert pattern
        uint8_t *bytes = pattBuf;
        tonTyp  *ton   = patt[a];
        for (int32_t i = 0; i < 64 * song.antChn; i++, bytes += 4, ton++)
        {
            const uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
            for (uint8_t k = 0; k < 96; k++)
            {
                if (period >= amigaPeriod[k])
                {
                    ton->ton = k + 1;
                    break;
                }
            }

            ton->instr  = (bytes[0] & 0xF0) | (bytes[2] >> 4);
            ton->effTyp = bytes[2] & 0x0F;
            ton->eff    = bytes[3];

            switch (ton->effTyp)
            {
            case 0xC: {
                if (ton->eff > 64)
                    ton->eff = 64;
            }
            break;

            case 0x1:
            case 0x2: {
                if (ton->eff == 0)
                    ton->effTyp = 0;
            }
            break;

            case 0x5: {
                if (ton->eff == 0)
                    ton->effTyp = 3;
            }
            break;

            case 0x6: {
                if (ton->eff == 0)
                    ton->effTyp = 4;
            }
            break;

            case 0xA: {
                if (ton->eff == 0)
                    ton->effTyp = 0;
            }
            break;

            case 0xE: {
                const uint8_t effTyp = ton->effTyp >> 4;
                const uint8_t eff    = ton->effTyp & 15;

                if (eff == 0 && (effTyp == 0x1 || effTyp == 0x2 || effTyp == 0xA || effTyp == 0xB))
                {
                    ton->eff    = 0;
                    ton->effTyp = 0;
                }
            }
            break;

            default:
                break;
            }
        }

        if (patternEmpty(a))
        {
            M4P_FREE(patt[a]);
            patt[a]     = NULL;
            pattLens[a] = 64;
        }
    }

    for (uint16_t a = 1; a <= ai; a++)
    {
        modSampleTyp *modSmp = &h_MOD31->sample[a - 1];

        uint32_t len = 2 * SWAP16(modSmp->len);
        if (len == 0)
            continue;

        if (!allocateInstr(a))
            goto loadError;

        sampleTyp *xmSmp = &instr[a]->samp[0];

        memcpy(xmSmp->name, modSmp->name, 22);
        xmSmp->name[22] = '\0';

        uint32_t repS = 2 * SWAP16(modSmp->repS);
        uint32_t repL = 2 * SWAP16(modSmp->repL);

        if (repL <= 2)
        {
            repS = 0;
            repL = 0;
        }

        if (repS + repL > len)
        {
            if (repS >= len)
            {
                repS = 0;
                repL = 0;
            }
            else
            {
                repL = len - repS;
            }
        }

        xmSmp->typ  = (repL > 2) ? 1 : 0;
        xmSmp->len  = len;
        xmSmp->vol  = (modSmp->vol <= 64) ? modSmp->vol : 64;
        xmSmp->fine = 8 * ((2 * ((modSmp->fine & 15) ^ 8)) - 16);
        xmSmp->repL = repL;
        xmSmp->repS = repS;

        xmSmp->pek = (int8_t *)M4P_MALLOC(len + 2);
        if (xmSmp->pek == NULL)
            goto loadError;

        mread(xmSmp->pek, 1, len, f);
    }

    mclose(&f);

    if (song.repS > song.len)
        song.repS = 0;

    resetMusic();
    upDateInstrs();

    moduleLoaded = true;
    return true;
loadError:
    freeAllInstr();
    freeAllPatterns();
loadError2:
    mclose(&f);
    return false;
}

static bool loadMusicFromData(const uint8_t *data, uint32_t dataLength) // .XM/.MOD/.FT
{
    uint16_t      i;
    songHeaderTyp h;

    freeMusic();
    setFrqTab(false);

    moduleLoaded = false;

    MEMFILE *f = mopen(data, dataLength);
    if (f == NULL)
        return false;

    // 8bb: instr 0 is a placeholder for empty instruments
    allocateInstr(0);
    instr[0]->samp[0].vol = 0;

    mread(&h, sizeof(h), 1, f);
    if (meof(f))
        goto loadError2;

    if (memcmp(h.sig, "Extended Module: ", 17) != 0)
    {
        mrewind(f);
        return loadMusicMOD(f);
    }

    if (h.ver < 0x0102 || h.ver > 0x104 || h.antChn < 2 || h.antChn > 32 || (h.antChn & 1) != 0 || h.antPtn > 256 ||
        h.antInstrs > 128)
    {
        goto loadError2;
    }

    mseek(f, 60 + h.headerSize, SEEK_SET);
    if (meof(f))
        goto loadError2;

    memcpy(song.name, h.name, 20);
    song.name[20] = '\0';

    song.len               = h.len;
    song.repS              = h.repS;
    song.antChn            = (uint8_t)h.antChn;
    bool linearFrequencies = !!(h.flags & LINEAR_FREQUENCIES);
    setFrqTab(linearFrequencies);
    memcpy(song.songTab, h.songTab, 256);

    song.antInstrs = h.antInstrs; // 8bb: added this
    if (h.defSpeed == 0)
        h.defSpeed = 125;         // 8bb: (BPM) FT2 doesn't do this, but we do it for safety
    song.speed = h.defSpeed;
    song.tempo = h.defTempo;
    song.ver   = h.ver;

    // 8bb: bugfixes...
    if (song.speed < 1)
        song.speed = 1;
    if (song.tempo < 1)
        song.tempo = 1;
    // ----------------

    if (song.ver < 0x0104) // old FT2 XM format
    {
        for (i = 1; i <= h.antInstrs; i++)
        {
            if (!loadInstrHeader(f, i))
                goto loadError;
        }

        if (!loadPatterns(f, h.antPtn))
            goto loadError;

        for (i = 1; i <= h.antInstrs; i++)
        {
            if (!loadInstrSample(f, i))
                goto loadError;
        }
    }
    else // latest FT2 XM format
    {
        if (!loadPatterns(f, h.antPtn))
            goto loadError;

        for (i = 1; i <= h.antInstrs; i++)
        {
            if (!loadInstrHeader(f, i))
                goto loadError;

            if (!loadInstrSample(f, i))
                goto loadError;
        }
    }

    mclose(&f);

    if (song.repS > song.len)
        song.repS = 0;

    resetMusic();
    upDateInstrs();

    moduleLoaded = true;
    return true;

loadError:
    freeAllInstr();
    freeAllPatterns();
loadError2:
    mclose(&f);
    return false;
}

/***************************************************************************
 *        PROCESS HANDLING                                                 *
 ***************************************************************************/

static bool startMusic(void)
{
    if (!moduleLoaded || song.speed == 0)
        return false;

    mix_ClearChannels();
    stopVoices();
    song.globVol = 64;

    speedVal        = ((realReplayRate * 5) / 2) / song.speed;
    quickVolSizeVal = realReplayRate / 200;

    if (!mix_Init(soundBufferSize))
        return false;

    musicPaused = false;
    return true;
}

static void pauseMusic(void)
{
    musicPaused = true;
}

static void resumeMusic(void)
{
    musicPaused = false;
}

static void stopMusic(void)
{
    pauseMusic();

    mix_Free();
    song.globVol = 64;

    resumeMusic();
}

static void startPlaying(void)
{
    stopMusic();
    song.pattDelTime = song.pattDelTime2 = 0; // 8bb: added these
    setPos(0, 0);
    startMusic();
}

static void stopPlaying(void)
{
    stopMusic();
    stopVoices();
}

/***************************************************************************
 *        CONFIGURATION ROUTINES                                           *
 ***************************************************************************/

static void setFrqTab(bool linear)
{
    linearFrqTab = linear;
    note2Period  = linear ? linearPeriods : amigaPeriods;
}

static void updateReplayRate(void)
{
    // 8bb: bit-exact to FT2
    frequenceDivFactor = (uint32_t)round(65536.0 * 1712.0 / realReplayRate * 8363.0);
    frequenceMulFactor = (uint32_t)round(256.0 * 65536.0 / realReplayRate * 8363.0);
}

/***************************************************************************
 *        INITIALIZATION ROUTINES                                          *
 ***************************************************************************/

static bool initMusic(int32_t audioFrequency, int32_t audioBufferSize, bool interpolation, bool volumeRamping)
{
    freeMusic();
    memset(stm, 0, sizeof(stm));

    realReplayRate = CLAMP(audioFrequency, 8000, 96000);
    updateReplayRate();

    soundBufferSize   = audioBufferSize;
    interpolationFlag = interpolation;
    volumeRampingFlag = volumeRamping;

    song.tempo = 6;
    song.speed = 125;
    setFrqTab(true);
    resetMusic();

    return true;
}

//-----------------------------------------------------------------------------------
// 								Tables - FastTracker 2
//-----------------------------------------------------------------------------------

// 8bb: bit-accurate FT2 tables (FT2.08/FT2.09)

/*
** for (int32_t i = 0; i < 257; i++)
**     panningTab[i] = (int32_t)round(65536.0 * sqrt(i / 256.0));
*/
static const uint32_t panningTab[257] = {
    0,     4096,  5793,  7094,  8192,  9159,  10033, 10837, 11585, 12288, 12953, 13585, 14189, 14768, 15326, 15864,
    16384, 16888, 17378, 17854, 18318, 18770, 19212, 19644, 20066, 20480, 20886, 21283, 21674, 22058, 22435, 22806,
    23170, 23530, 23884, 24232, 24576, 24915, 25249, 25580, 25905, 26227, 26545, 26859, 27170, 27477, 27780, 28081,
    28378, 28672, 28963, 29251, 29537, 29819, 30099, 30377, 30652, 30924, 31194, 31462, 31727, 31991, 32252, 32511,
    32768, 33023, 33276, 33527, 33776, 34024, 34270, 34514, 34756, 34996, 35235, 35472, 35708, 35942, 36175, 36406,
    36636, 36864, 37091, 37316, 37540, 37763, 37985, 38205, 38424, 38642, 38858, 39073, 39287, 39500, 39712, 39923,
    40132, 40341, 40548, 40755, 40960, 41164, 41368, 41570, 41771, 41972, 42171, 42369, 42567, 42763, 42959, 43154,
    43348, 43541, 43733, 43925, 44115, 44305, 44494, 44682, 44869, 45056, 45242, 45427, 45611, 45795, 45977, 46160,
    46341, 46522, 46702, 46881, 47059, 47237, 47415, 47591, 47767, 47942, 48117, 48291, 48465, 48637, 48809, 48981,
    49152, 49322, 49492, 49661, 49830, 49998, 50166, 50332, 50499, 50665, 50830, 50995, 51159, 51323, 51486, 51649,
    51811, 51972, 52134, 52294, 52454, 52614, 52773, 52932, 53090, 53248, 53405, 53562, 53719, 53874, 54030, 54185,
    54340, 54494, 54647, 54801, 54954, 55106, 55258, 55410, 55561, 55712, 55862, 56012, 56162, 56311, 56459, 56608,
    56756, 56903, 57051, 57198, 57344, 57490, 57636, 57781, 57926, 58071, 58215, 58359, 58503, 58646, 58789, 58931,
    59073, 59215, 59357, 59498, 59639, 59779, 59919, 60059, 60199, 60338, 60477, 60615, 60753, 60891, 61029, 61166,
    61303, 61440, 61576, 61712, 61848, 61984, 62119, 62254, 62388, 62523, 62657, 62790, 62924, 63057, 63190, 63323,
    63455, 63587, 63719, 63850, 63982, 64113, 64243, 64374, 64504, 64634, 64763, 64893, 65022, 65151, 65279, 65408,
    65536};

// 8bb: the last 17 values are off (but identical to FT2.08/FT2.09) because of a bug in how it calculates this table
static const uint16_t amigaPeriods[1936] = {
    29024, 28912, 28800, 28704, 28608, 28496, 28384, 28288, 28192, 28096, 28000, 27888, 27776, 27680, 27584, 27488,
    27392, 27296, 27200, 27104, 27008, 26912, 26816, 26720, 26624, 26528, 26432, 26336, 26240, 26144, 26048, 25952,
    25856, 25760, 25664, 25568, 25472, 25392, 25312, 25216, 25120, 25024, 24928, 24848, 24768, 24672, 24576, 24480,
    24384, 24304, 24224, 24144, 24064, 23968, 23872, 23792, 23712, 23632, 23552, 23456, 23360, 23280, 23200, 23120,
    23040, 22960, 22880, 22784, 22688, 22608, 22528, 22448, 22368, 22288, 22208, 22128, 22048, 21968, 21888, 21792,
    21696, 21648, 21600, 21520, 21440, 21360, 21280, 21200, 21120, 21040, 20960, 20896, 20832, 20752, 20672, 20576,
    20480, 20416, 20352, 20288, 20224, 20160, 20096, 20016, 19936, 19872, 19808, 19728, 19648, 19584, 19520, 19424,
    19328, 19280, 19232, 19168, 19104, 19024, 18944, 18880, 18816, 18752, 18688, 18624, 18560, 18480, 18400, 18320,
    18240, 18192, 18144, 18080, 18016, 17952, 17888, 17824, 17760, 17696, 17632, 17568, 17504, 17440, 17376, 17296,
    17216, 17168, 17120, 17072, 17024, 16960, 16896, 16832, 16768, 16704, 16640, 16576, 16512, 16464, 16416, 16336,
    16256, 16208, 16160, 16112, 16064, 16000, 15936, 15872, 15808, 15760, 15712, 15648, 15584, 15536, 15488, 15424,
    15360, 15312, 15264, 15216, 15168, 15104, 15040, 14992, 14944, 14880, 14816, 14768, 14720, 14672, 14624, 14568,
    14512, 14456, 14400, 14352, 14304, 14248, 14192, 14144, 14096, 14048, 14000, 13944, 13888, 13840, 13792, 13744,
    13696, 13648, 13600, 13552, 13504, 13456, 13408, 13360, 13312, 13264, 13216, 13168, 13120, 13072, 13024, 12976,
    12928, 12880, 12832, 12784, 12736, 12696, 12656, 12608, 12560, 12512, 12464, 12424, 12384, 12336, 12288, 12240,
    12192, 12152, 12112, 12072, 12032, 11984, 11936, 11896, 11856, 11816, 11776, 11728, 11680, 11640, 11600, 11560,
    11520, 11480, 11440, 11392, 11344, 11304, 11264, 11224, 11184, 11144, 11104, 11064, 11024, 10984, 10944, 10896,
    10848, 10824, 10800, 10760, 10720, 10680, 10640, 10600, 10560, 10520, 10480, 10448, 10416, 10376, 10336, 10288,
    10240, 10208, 10176, 10144, 10112, 10080, 10048, 10008, 9968,  9936,  9904,  9864,  9824,  9792,  9760,  9712,
    9664,  9640,  9616,  9584,  9552,  9512,  9472,  9440,  9408,  9376,  9344,  9312,  9280,  9240,  9200,  9160,
    9120,  9096,  9072,  9040,  9008,  8976,  8944,  8912,  8880,  8848,  8816,  8784,  8752,  8720,  8688,  8648,
    8608,  8584,  8560,  8536,  8512,  8480,  8448,  8416,  8384,  8352,  8320,  8288,  8256,  8232,  8208,  8168,
    8128,  8104,  8080,  8056,  8032,  8000,  7968,  7936,  7904,  7880,  7856,  7824,  7792,  7768,  7744,  7712,
    7680,  7656,  7632,  7608,  7584,  7552,  7520,  7496,  7472,  7440,  7408,  7384,  7360,  7336,  7312,  7284,
    7256,  7228,  7200,  7176,  7152,  7124,  7096,  7072,  7048,  7024,  7000,  6972,  6944,  6920,  6896,  6872,
    6848,  6824,  6800,  6776,  6752,  6728,  6704,  6680,  6656,  6632,  6608,  6584,  6560,  6536,  6512,  6488,
    6464,  6440,  6416,  6392,  6368,  6348,  6328,  6304,  6280,  6256,  6232,  6212,  6192,  6168,  6144,  6120,
    6096,  6076,  6056,  6036,  6016,  5992,  5968,  5948,  5928,  5908,  5888,  5864,  5840,  5820,  5800,  5780,
    5760,  5740,  5720,  5696,  5672,  5652,  5632,  5612,  5592,  5572,  5552,  5532,  5512,  5492,  5472,  5448,
    5424,  5412,  5400,  5380,  5360,  5340,  5320,  5300,  5280,  5260,  5240,  5224,  5208,  5188,  5168,  5144,
    5120,  5104,  5088,  5072,  5056,  5040,  5024,  5004,  4984,  4968,  4952,  4932,  4912,  4896,  4880,  4856,
    4832,  4820,  4808,  4792,  4776,  4756,  4736,  4720,  4704,  4688,  4672,  4656,  4640,  4620,  4600,  4580,
    4560,  4548,  4536,  4520,  4504,  4488,  4472,  4456,  4440,  4424,  4408,  4392,  4376,  4360,  4344,  4324,
    4304,  4292,  4280,  4268,  4256,  4240,  4224,  4208,  4192,  4176,  4160,  4144,  4128,  4116,  4104,  4084,
    4064,  4052,  4040,  4028,  4016,  4000,  3984,  3968,  3952,  3940,  3928,  3912,  3896,  3884,  3872,  3856,
    3840,  3828,  3816,  3804,  3792,  3776,  3760,  3748,  3736,  3720,  3704,  3692,  3680,  3668,  3656,  3642,
    3628,  3614,  3600,  3588,  3576,  3562,  3548,  3536,  3524,  3512,  3500,  3486,  3472,  3460,  3448,  3436,
    3424,  3412,  3400,  3388,  3376,  3364,  3352,  3340,  3328,  3316,  3304,  3292,  3280,  3268,  3256,  3244,
    3232,  3220,  3208,  3196,  3184,  3174,  3164,  3152,  3140,  3128,  3116,  3106,  3096,  3084,  3072,  3060,
    3048,  3038,  3028,  3018,  3008,  2996,  2984,  2974,  2964,  2954,  2944,  2932,  2920,  2910,  2900,  2890,
    2880,  2870,  2860,  2848,  2836,  2826,  2816,  2806,  2796,  2786,  2776,  2766,  2756,  2746,  2736,  2724,
    2712,  2706,  2700,  2690,  2680,  2670,  2660,  2650,  2640,  2630,  2620,  2612,  2604,  2594,  2584,  2572,
    2560,  2552,  2544,  2536,  2528,  2520,  2512,  2502,  2492,  2484,  2476,  2466,  2456,  2448,  2440,  2428,
    2416,  2410,  2404,  2396,  2388,  2378,  2368,  2360,  2352,  2344,  2336,  2328,  2320,  2310,  2300,  2290,
    2280,  2274,  2268,  2260,  2252,  2244,  2236,  2228,  2220,  2212,  2204,  2196,  2188,  2180,  2172,  2162,
    2152,  2146,  2140,  2134,  2128,  2120,  2112,  2104,  2096,  2088,  2080,  2072,  2064,  2058,  2052,  2042,
    2032,  2026,  2020,  2014,  2008,  2000,  1992,  1984,  1976,  1970,  1964,  1956,  1948,  1942,  1936,  1928,
    1920,  1914,  1908,  1902,  1896,  1888,  1880,  1874,  1868,  1860,  1852,  1846,  1840,  1834,  1828,  1821,
    1814,  1807,  1800,  1794,  1788,  1781,  1774,  1768,  1762,  1756,  1750,  1743,  1736,  1730,  1724,  1718,
    1712,  1706,  1700,  1694,  1688,  1682,  1676,  1670,  1664,  1658,  1652,  1646,  1640,  1634,  1628,  1622,
    1616,  1610,  1604,  1598,  1592,  1587,  1582,  1576,  1570,  1564,  1558,  1553,  1548,  1542,  1536,  1530,
    1524,  1519,  1514,  1509,  1504,  1498,  1492,  1487,  1482,  1477,  1472,  1466,  1460,  1455,  1450,  1445,
    1440,  1435,  1430,  1424,  1418,  1413,  1408,  1403,  1398,  1393,  1388,  1383,  1378,  1373,  1368,  1362,
    1356,  1353,  1350,  1345,  1340,  1335,  1330,  1325,  1320,  1315,  1310,  1306,  1302,  1297,  1292,  1286,
    1280,  1276,  1272,  1268,  1264,  1260,  1256,  1251,  1246,  1242,  1238,  1233,  1228,  1224,  1220,  1214,
    1208,  1205,  1202,  1198,  1194,  1189,  1184,  1180,  1176,  1172,  1168,  1164,  1160,  1155,  1150,  1145,
    1140,  1137,  1134,  1130,  1126,  1122,  1118,  1114,  1110,  1106,  1102,  1098,  1094,  1090,  1086,  1081,
    1076,  1073,  1070,  1067,  1064,  1060,  1056,  1052,  1048,  1044,  1040,  1036,  1032,  1029,  1026,  1021,
    1016,  1013,  1010,  1007,  1004,  1000,  996,   992,   988,   985,   982,   978,   974,   971,   968,   964,
    960,   957,   954,   951,   948,   944,   940,   937,   934,   930,   926,   923,   920,   917,   914,   910,
    907,   903,   900,   897,   894,   890,   887,   884,   881,   878,   875,   871,   868,   865,   862,   859,
    856,   853,   850,   847,   844,   841,   838,   835,   832,   829,   826,   823,   820,   817,   814,   811,
    808,   805,   802,   799,   796,   793,   791,   788,   785,   782,   779,   776,   774,   771,   768,   765,
    762,   759,   757,   754,   752,   749,   746,   743,   741,   738,   736,   733,   730,   727,   725,   722,
    720,   717,   715,   712,   709,   706,   704,   701,   699,   696,   694,   691,   689,   686,   684,   681,
    678,   676,   675,   672,   670,   667,   665,   662,   660,   657,   655,   653,   651,   648,   646,   643,
    640,   638,   636,   634,   632,   630,   628,   625,   623,   621,   619,   616,   614,   612,   610,   607,
    604,   602,   601,   599,   597,   594,   592,   590,   588,   586,   584,   582,   580,   577,   575,   572,
    570,   568,   567,   565,   563,   561,   559,   557,   555,   553,   551,   549,   547,   545,   543,   540,
    538,   536,   535,   533,   532,   530,   528,   526,   524,   522,   520,   518,   516,   514,   513,   510,
    508,   506,   505,   503,   502,   500,   498,   496,   494,   492,   491,   489,   487,   485,   484,   482,
    480,   478,   477,   475,   474,   472,   470,   468,   467,   465,   463,   461,   460,   458,   457,   455,
    453,   451,   450,   448,   447,   445,   443,   441,   440,   438,   437,   435,   434,   432,   431,   429,
    428,   426,   425,   423,   422,   420,   419,   417,   416,   414,   413,   411,   410,   408,   407,   405,
    404,   402,   401,   399,   398,   396,   395,   393,   392,   390,   389,   388,   387,   385,   384,   382,
    381,   379,   378,   377,   376,   374,   373,   371,   370,   369,   368,   366,   365,   363,   362,   361,
    360,   358,   357,   355,   354,   353,   352,   350,   349,   348,   347,   345,   344,   343,   342,   340,
    339,   338,   337,   336,   335,   333,   332,   331,   330,   328,   327,   326,   325,   324,   323,   321,
    320,   319,   318,   317,   316,   315,   314,   312,   311,   310,   309,   308,   307,   306,   305,   303,
    302,   301,   300,   299,   298,   297,   296,   295,   294,   293,   292,   291,   290,   288,   287,   286,
    285,   284,   283,   282,   281,   280,   279,   278,   277,   276,   275,   274,   273,   272,   271,   270,
    269,   268,   267,   266,   266,   265,   264,   263,   262,   261,   260,   259,   258,   257,   256,   255,
    254,   253,   252,   251,   251,   250,   249,   248,   247,   246,   245,   244,   243,   242,   242,   241,
    240,   239,   238,   237,   237,   236,   235,   234,   233,   232,   231,   230,   230,   229,   228,   227,
    227,   226,   225,   224,   223,   222,   222,   221,   220,   219,   219,   218,   217,   216,   215,   214,
    214,   213,   212,   211,   211,   210,   209,   208,   208,   207,   206,   205,   205,   204,   203,   202,
    202,   201,   200,   199,   199,   198,   198,   197,   196,   195,   195,   194,   193,   192,   192,   191,
    190,   189,   189,   188,   188,   187,   186,   185,   185,   184,   184,   183,   182,   181,   181,   180,
    180,   179,   179,   178,   177,   176,   176,   175,   175,   174,   173,   172,   172,   171,   171,   170,
    169,   169,   169,   168,   167,   166,   166,   165,   165,   164,   164,   163,   163,   162,   161,   160,
    160,   159,   159,   158,   158,   157,   157,   156,   156,   155,   155,   154,   153,   152,   152,   151,
    151,   150,   150,   149,   149,   148,   148,   147,   147,   146,   146,   145,   145,   144,   144,   143,
    142,   142,   142,   141,   141,   140,   140,   139,   139,   138,   138,   137,   137,   136,   136,   135,
    134,   134,   134,   133,   133,   132,   132,   131,   131,   130,   130,   129,   129,   128,   128,   127,
    127,   126,   126,   125,   125,   124,   124,   123,   123,   123,   123,   122,   122,   121,   121,   120,
    120,   119,   119,   118,   118,   117,   117,   117,   117,   116,   116,   115,   115,   114,   114,   113,
    113,   112,   112,   112,   112,   111,   111,   110,   110,   109,   109,   108,   108,   108,   108,   107,
    107,   106,   106,   105,   105,   105,   105,   104,   104,   103,   103,   102,   102,   102,   102,   101,
    101,   100,   100,   99,    99,    99,    99,    98,    98,    97,    97,    97,    97,    96,    96,    95,
    95,    95,    95,    94,    94,    93,    93,    93,    93,    92,    92,    91,    91,    91,    91,    90,
    90,    89,    89,    89,    89,    88,    88,    87,    87,    87,    87,    86,    86,    85,    85,    85,
    85,    84,    84,    84,    84,    83,    83,    82,    82,    82,    82,    81,    81,    81,    81,    80,
    80,    79,    79,    79,    79,    78,    78,    78,    78,    77,    77,    77,    77,    76,    76,    75,
    75,    75,    75,    75,    75,    74,    74,    73,    73,    73,    73,    72,    72,    72,    72,    71,
    71,    71,    71,    70,    70,    70,    70,    69,    69,    69,    69,    68,    68,    68,    68,    67,
    67,    67,    67,    66,    66,    66,    66,    65,    65,    65,    65,    64,    64,    64,    64,    63,
    63,    63,    63,    63,    63,    62,    62,    62,    62,    61,    61,    61,    61,    60,    60,    60,
    60,    60,    60,    59,    59,    59,    59,    58,    58,    58,    58,    57,    57,    57,    57,    57,
    57,    56,    56,    56,    56,    55,    55,    55,    55,    55,    55,    54,    54,    54,    54,    53,
    53,    53,    53,    53,    53,    52,    52,    52,    52,    52,    52,    51,    51,    51,    51,    50,
    50,    50,    50,    50,    50,    49,    49,    49,    49,    49,    49,    48,    48,    48,    48,    48,
    48,    47,    47,    47,    47,    47,    47,    46,    46,    46,    46,    46,    46,    45,    45,    45,
    45,    45,    45,    44,    44,    44,    44,    44,    44,    43,    43,    43,    43,    43,    43,    42,
    42,    42,    42,    42,    42,    42,    42,    41,    41,    41,    41,    41,    41,    40,    40,    40,
    40,    40,    40,    39,    39,    39,    39,    39,    39,    39,    39,    38,    38,    38,    38,    38,
    38,    38,    38,    37,    37,    37,    37,    37,    37,    36,    36,    36,    36,    36,    36,    36,
    36,    35,    35,    35,    35,    35,    35,    35,    35,    34,    34,    34,    34,    34,    34,    34,
    34,    33,    33,    33,    33,    33,    33,    33,    33,    32,    32,    32,    32,    32,    32,    32,
    32,    32,    32,    31,    31,    31,    31,    31,    31,    31,    31,    30,    30,    30,    30,    30,
    30,    30,    30,    30,    30,    29,    29,    29,    29,    29,    29,    29,    29,    29,    29,    22,
    16,    8,     0,     16,    32,    24,    16,    8,     0,     16,    32,    24,    16,    8,     0,     0};

static const uint16_t linearPeriods[1936] = {
    7744, 7740, 7736, 7732, 7728, 7724, 7720, 7716, 7712, 7708, 7704, 7700, 7696, 7692, 7688, 7684, 7680, 7676, 7672,
    7668, 7664, 7660, 7656, 7652, 7648, 7644, 7640, 7636, 7632, 7628, 7624, 7620, 7616, 7612, 7608, 7604, 7600, 7596,
    7592, 7588, 7584, 7580, 7576, 7572, 7568, 7564, 7560, 7556, 7552, 7548, 7544, 7540, 7536, 7532, 7528, 7524, 7520,
    7516, 7512, 7508, 7504, 7500, 7496, 7492, 7488, 7484, 7480, 7476, 7472, 7468, 7464, 7460, 7456, 7452, 7448, 7444,
    7440, 7436, 7432, 7428, 7424, 7420, 7416, 7412, 7408, 7404, 7400, 7396, 7392, 7388, 7384, 7380, 7376, 7372, 7368,
    7364, 7360, 7356, 7352, 7348, 7344, 7340, 7336, 7332, 7328, 7324, 7320, 7316, 7312, 7308, 7304, 7300, 7296, 7292,
    7288, 7284, 7280, 7276, 7272, 7268, 7264, 7260, 7256, 7252, 7248, 7244, 7240, 7236, 7232, 7228, 7224, 7220, 7216,
    7212, 7208, 7204, 7200, 7196, 7192, 7188, 7184, 7180, 7176, 7172, 7168, 7164, 7160, 7156, 7152, 7148, 7144, 7140,
    7136, 7132, 7128, 7124, 7120, 7116, 7112, 7108, 7104, 7100, 7096, 7092, 7088, 7084, 7080, 7076, 7072, 7068, 7064,
    7060, 7056, 7052, 7048, 7044, 7040, 7036, 7032, 7028, 7024, 7020, 7016, 7012, 7008, 7004, 7000, 6996, 6992, 6988,
    6984, 6980, 6976, 6972, 6968, 6964, 6960, 6956, 6952, 6948, 6944, 6940, 6936, 6932, 6928, 6924, 6920, 6916, 6912,
    6908, 6904, 6900, 6896, 6892, 6888, 6884, 6880, 6876, 6872, 6868, 6864, 6860, 6856, 6852, 6848, 6844, 6840, 6836,
    6832, 6828, 6824, 6820, 6816, 6812, 6808, 6804, 6800, 6796, 6792, 6788, 6784, 6780, 6776, 6772, 6768, 6764, 6760,
    6756, 6752, 6748, 6744, 6740, 6736, 6732, 6728, 6724, 6720, 6716, 6712, 6708, 6704, 6700, 6696, 6692, 6688, 6684,
    6680, 6676, 6672, 6668, 6664, 6660, 6656, 6652, 6648, 6644, 6640, 6636, 6632, 6628, 6624, 6620, 6616, 6612, 6608,
    6604, 6600, 6596, 6592, 6588, 6584, 6580, 6576, 6572, 6568, 6564, 6560, 6556, 6552, 6548, 6544, 6540, 6536, 6532,
    6528, 6524, 6520, 6516, 6512, 6508, 6504, 6500, 6496, 6492, 6488, 6484, 6480, 6476, 6472, 6468, 6464, 6460, 6456,
    6452, 6448, 6444, 6440, 6436, 6432, 6428, 6424, 6420, 6416, 6412, 6408, 6404, 6400, 6396, 6392, 6388, 6384, 6380,
    6376, 6372, 6368, 6364, 6360, 6356, 6352, 6348, 6344, 6340, 6336, 6332, 6328, 6324, 6320, 6316, 6312, 6308, 6304,
    6300, 6296, 6292, 6288, 6284, 6280, 6276, 6272, 6268, 6264, 6260, 6256, 6252, 6248, 6244, 6240, 6236, 6232, 6228,
    6224, 6220, 6216, 6212, 6208, 6204, 6200, 6196, 6192, 6188, 6184, 6180, 6176, 6172, 6168, 6164, 6160, 6156, 6152,
    6148, 6144, 6140, 6136, 6132, 6128, 6124, 6120, 6116, 6112, 6108, 6104, 6100, 6096, 6092, 6088, 6084, 6080, 6076,
    6072, 6068, 6064, 6060, 6056, 6052, 6048, 6044, 6040, 6036, 6032, 6028, 6024, 6020, 6016, 6012, 6008, 6004, 6000,
    5996, 5992, 5988, 5984, 5980, 5976, 5972, 5968, 5964, 5960, 5956, 5952, 5948, 5944, 5940, 5936, 5932, 5928, 5924,
    5920, 5916, 5912, 5908, 5904, 5900, 5896, 5892, 5888, 5884, 5880, 5876, 5872, 5868, 5864, 5860, 5856, 5852, 5848,
    5844, 5840, 5836, 5832, 5828, 5824, 5820, 5816, 5812, 5808, 5804, 5800, 5796, 5792, 5788, 5784, 5780, 5776, 5772,
    5768, 5764, 5760, 5756, 5752, 5748, 5744, 5740, 5736, 5732, 5728, 5724, 5720, 5716, 5712, 5708, 5704, 5700, 5696,
    5692, 5688, 5684, 5680, 5676, 5672, 5668, 5664, 5660, 5656, 5652, 5648, 5644, 5640, 5636, 5632, 5628, 5624, 5620,
    5616, 5612, 5608, 5604, 5600, 5596, 5592, 5588, 5584, 5580, 5576, 5572, 5568, 5564, 5560, 5556, 5552, 5548, 5544,
    5540, 5536, 5532, 5528, 5524, 5520, 5516, 5512, 5508, 5504, 5500, 5496, 5492, 5488, 5484, 5480, 5476, 5472, 5468,
    5464, 5460, 5456, 5452, 5448, 5444, 5440, 5436, 5432, 5428, 5424, 5420, 5416, 5412, 5408, 5404, 5400, 5396, 5392,
    5388, 5384, 5380, 5376, 5372, 5368, 5364, 5360, 5356, 5352, 5348, 5344, 5340, 5336, 5332, 5328, 5324, 5320, 5316,
    5312, 5308, 5304, 5300, 5296, 5292, 5288, 5284, 5280, 5276, 5272, 5268, 5264, 5260, 5256, 5252, 5248, 5244, 5240,
    5236, 5232, 5228, 5224, 5220, 5216, 5212, 5208, 5204, 5200, 5196, 5192, 5188, 5184, 5180, 5176, 5172, 5168, 5164,
    5160, 5156, 5152, 5148, 5144, 5140, 5136, 5132, 5128, 5124, 5120, 5116, 5112, 5108, 5104, 5100, 5096, 5092, 5088,
    5084, 5080, 5076, 5072, 5068, 5064, 5060, 5056, 5052, 5048, 5044, 5040, 5036, 5032, 5028, 5024, 5020, 5016, 5012,
    5008, 5004, 5000, 4996, 4992, 4988, 4984, 4980, 4976, 4972, 4968, 4964, 4960, 4956, 4952, 4948, 4944, 4940, 4936,
    4932, 4928, 4924, 4920, 4916, 4912, 4908, 4904, 4900, 4896, 4892, 4888, 4884, 4880, 4876, 4872, 4868, 4864, 4860,
    4856, 4852, 4848, 4844, 4840, 4836, 4832, 4828, 4824, 4820, 4816, 4812, 4808, 4804, 4800, 4796, 4792, 4788, 4784,
    4780, 4776, 4772, 4768, 4764, 4760, 4756, 4752, 4748, 4744, 4740, 4736, 4732, 4728, 4724, 4720, 4716, 4712, 4708,
    4704, 4700, 4696, 4692, 4688, 4684, 4680, 4676, 4672, 4668, 4664, 4660, 4656, 4652, 4648, 4644, 4640, 4636, 4632,
    4628, 4624, 4620, 4616, 4612, 4608, 4604, 4600, 4596, 4592, 4588, 4584, 4580, 4576, 4572, 4568, 4564, 4560, 4556,
    4552, 4548, 4544, 4540, 4536, 4532, 4528, 4524, 4520, 4516, 4512, 4508, 4504, 4500, 4496, 4492, 4488, 4484, 4480,
    4476, 4472, 4468, 4464, 4460, 4456, 4452, 4448, 4444, 4440, 4436, 4432, 4428, 4424, 4420, 4416, 4412, 4408, 4404,
    4400, 4396, 4392, 4388, 4384, 4380, 4376, 4372, 4368, 4364, 4360, 4356, 4352, 4348, 4344, 4340, 4336, 4332, 4328,
    4324, 4320, 4316, 4312, 4308, 4304, 4300, 4296, 4292, 4288, 4284, 4280, 4276, 4272, 4268, 4264, 4260, 4256, 4252,
    4248, 4244, 4240, 4236, 4232, 4228, 4224, 4220, 4216, 4212, 4208, 4204, 4200, 4196, 4192, 4188, 4184, 4180, 4176,
    4172, 4168, 4164, 4160, 4156, 4152, 4148, 4144, 4140, 4136, 4132, 4128, 4124, 4120, 4116, 4112, 4108, 4104, 4100,
    4096, 4092, 4088, 4084, 4080, 4076, 4072, 4068, 4064, 4060, 4056, 4052, 4048, 4044, 4040, 4036, 4032, 4028, 4024,
    4020, 4016, 4012, 4008, 4004, 4000, 3996, 3992, 3988, 3984, 3980, 3976, 3972, 3968, 3964, 3960, 3956, 3952, 3948,
    3944, 3940, 3936, 3932, 3928, 3924, 3920, 3916, 3912, 3908, 3904, 3900, 3896, 3892, 3888, 3884, 3880, 3876, 3872,
    3868, 3864, 3860, 3856, 3852, 3848, 3844, 3840, 3836, 3832, 3828, 3824, 3820, 3816, 3812, 3808, 3804, 3800, 3796,
    3792, 3788, 3784, 3780, 3776, 3772, 3768, 3764, 3760, 3756, 3752, 3748, 3744, 3740, 3736, 3732, 3728, 3724, 3720,
    3716, 3712, 3708, 3704, 3700, 3696, 3692, 3688, 3684, 3680, 3676, 3672, 3668, 3664, 3660, 3656, 3652, 3648, 3644,
    3640, 3636, 3632, 3628, 3624, 3620, 3616, 3612, 3608, 3604, 3600, 3596, 3592, 3588, 3584, 3580, 3576, 3572, 3568,
    3564, 3560, 3556, 3552, 3548, 3544, 3540, 3536, 3532, 3528, 3524, 3520, 3516, 3512, 3508, 3504, 3500, 3496, 3492,
    3488, 3484, 3480, 3476, 3472, 3468, 3464, 3460, 3456, 3452, 3448, 3444, 3440, 3436, 3432, 3428, 3424, 3420, 3416,
    3412, 3408, 3404, 3400, 3396, 3392, 3388, 3384, 3380, 3376, 3372, 3368, 3364, 3360, 3356, 3352, 3348, 3344, 3340,
    3336, 3332, 3328, 3324, 3320, 3316, 3312, 3308, 3304, 3300, 3296, 3292, 3288, 3284, 3280, 3276, 3272, 3268, 3264,
    3260, 3256, 3252, 3248, 3244, 3240, 3236, 3232, 3228, 3224, 3220, 3216, 3212, 3208, 3204, 3200, 3196, 3192, 3188,
    3184, 3180, 3176, 3172, 3168, 3164, 3160, 3156, 3152, 3148, 3144, 3140, 3136, 3132, 3128, 3124, 3120, 3116, 3112,
    3108, 3104, 3100, 3096, 3092, 3088, 3084, 3080, 3076, 3072, 3068, 3064, 3060, 3056, 3052, 3048, 3044, 3040, 3036,
    3032, 3028, 3024, 3020, 3016, 3012, 3008, 3004, 3000, 2996, 2992, 2988, 2984, 2980, 2976, 2972, 2968, 2964, 2960,
    2956, 2952, 2948, 2944, 2940, 2936, 2932, 2928, 2924, 2920, 2916, 2912, 2908, 2904, 2900, 2896, 2892, 2888, 2884,
    2880, 2876, 2872, 2868, 2864, 2860, 2856, 2852, 2848, 2844, 2840, 2836, 2832, 2828, 2824, 2820, 2816, 2812, 2808,
    2804, 2800, 2796, 2792, 2788, 2784, 2780, 2776, 2772, 2768, 2764, 2760, 2756, 2752, 2748, 2744, 2740, 2736, 2732,
    2728, 2724, 2720, 2716, 2712, 2708, 2704, 2700, 2696, 2692, 2688, 2684, 2680, 2676, 2672, 2668, 2664, 2660, 2656,
    2652, 2648, 2644, 2640, 2636, 2632, 2628, 2624, 2620, 2616, 2612, 2608, 2604, 2600, 2596, 2592, 2588, 2584, 2580,
    2576, 2572, 2568, 2564, 2560, 2556, 2552, 2548, 2544, 2540, 2536, 2532, 2528, 2524, 2520, 2516, 2512, 2508, 2504,
    2500, 2496, 2492, 2488, 2484, 2480, 2476, 2472, 2468, 2464, 2460, 2456, 2452, 2448, 2444, 2440, 2436, 2432, 2428,
    2424, 2420, 2416, 2412, 2408, 2404, 2400, 2396, 2392, 2388, 2384, 2380, 2376, 2372, 2368, 2364, 2360, 2356, 2352,
    2348, 2344, 2340, 2336, 2332, 2328, 2324, 2320, 2316, 2312, 2308, 2304, 2300, 2296, 2292, 2288, 2284, 2280, 2276,
    2272, 2268, 2264, 2260, 2256, 2252, 2248, 2244, 2240, 2236, 2232, 2228, 2224, 2220, 2216, 2212, 2208, 2204, 2200,
    2196, 2192, 2188, 2184, 2180, 2176, 2172, 2168, 2164, 2160, 2156, 2152, 2148, 2144, 2140, 2136, 2132, 2128, 2124,
    2120, 2116, 2112, 2108, 2104, 2100, 2096, 2092, 2088, 2084, 2080, 2076, 2072, 2068, 2064, 2060, 2056, 2052, 2048,
    2044, 2040, 2036, 2032, 2028, 2024, 2020, 2016, 2012, 2008, 2004, 2000, 1996, 1992, 1988, 1984, 1980, 1976, 1972,
    1968, 1964, 1960, 1956, 1952, 1948, 1944, 1940, 1936, 1932, 1928, 1924, 1920, 1916, 1912, 1908, 1904, 1900, 1896,
    1892, 1888, 1884, 1880, 1876, 1872, 1868, 1864, 1860, 1856, 1852, 1848, 1844, 1840, 1836, 1832, 1828, 1824, 1820,
    1816, 1812, 1808, 1804, 1800, 1796, 1792, 1788, 1784, 1780, 1776, 1772, 1768, 1764, 1760, 1756, 1752, 1748, 1744,
    1740, 1736, 1732, 1728, 1724, 1720, 1716, 1712, 1708, 1704, 1700, 1696, 1692, 1688, 1684, 1680, 1676, 1672, 1668,
    1664, 1660, 1656, 1652, 1648, 1644, 1640, 1636, 1632, 1628, 1624, 1620, 1616, 1612, 1608, 1604, 1600, 1596, 1592,
    1588, 1584, 1580, 1576, 1572, 1568, 1564, 1560, 1556, 1552, 1548, 1544, 1540, 1536, 1532, 1528, 1524, 1520, 1516,
    1512, 1508, 1504, 1500, 1496, 1492, 1488, 1484, 1480, 1476, 1472, 1468, 1464, 1460, 1456, 1452, 1448, 1444, 1440,
    1436, 1432, 1428, 1424, 1420, 1416, 1412, 1408, 1404, 1400, 1396, 1392, 1388, 1384, 1380, 1376, 1372, 1368, 1364,
    1360, 1356, 1352, 1348, 1344, 1340, 1336, 1332, 1328, 1324, 1320, 1316, 1312, 1308, 1304, 1300, 1296, 1292, 1288,
    1284, 1280, 1276, 1272, 1268, 1264, 1260, 1256, 1252, 1248, 1244, 1240, 1236, 1232, 1228, 1224, 1220, 1216, 1212,
    1208, 1204, 1200, 1196, 1192, 1188, 1184, 1180, 1176, 1172, 1168, 1164, 1160, 1156, 1152, 1148, 1144, 1140, 1136,
    1132, 1128, 1124, 1120, 1116, 1112, 1108, 1104, 1100, 1096, 1092, 1088, 1084, 1080, 1076, 1072, 1068, 1064, 1060,
    1056, 1052, 1048, 1044, 1040, 1036, 1032, 1028, 1024, 1020, 1016, 1012, 1008, 1004, 1000, 996,  992,  988,  984,
    980,  976,  972,  968,  964,  960,  956,  952,  948,  944,  940,  936,  932,  928,  924,  920,  916,  912,  908,
    904,  900,  896,  892,  888,  884,  880,  876,  872,  868,  864,  860,  856,  852,  848,  844,  840,  836,  832,
    828,  824,  820,  816,  812,  808,  804,  800,  796,  792,  788,  784,  780,  776,  772,  768,  764,  760,  756,
    752,  748,  744,  740,  736,  732,  728,  724,  720,  716,  712,  708,  704,  700,  696,  692,  688,  684,  680,
    676,  672,  668,  664,  660,  656,  652,  648,  644,  640,  636,  632,  628,  624,  620,  616,  612,  608,  604,
    600,  596,  592,  588,  584,  580,  576,  572,  568,  564,  560,  556,  552,  548,  544,  540,  536,  532,  528,
    524,  520,  516,  512,  508,  504,  500,  496,  492,  488,  484,  480,  476,  472,  468,  464,  460,  456,  452,
    448,  444,  440,  436,  432,  428,  424,  420,  416,  412,  408,  404,  400,  396,  392,  388,  384,  380,  376,
    372,  368,  364,  360,  356,  352,  348,  344,  340,  336,  332,  328,  324,  320,  316,  312,  308,  304,  300,
    296,  292,  288,  284,  280,  276,  272,  268,  264,  260,  256,  252,  248,  244,  240,  236,  232,  228,  224,
    220,  216,  212,  208,  204,  200,  196,  192,  188,  184,  180,  176,  172,  168,  164,  160,  156,  152,  148,
    144,  140,  136,  132,  128,  124,  120,  116,  112,  108,  104,  100,  96,   92,   88,   84,   80,   76,   72,
    68,   64,   60,   56,   52,   48,   44,   40,   36,   32,   28,   24,   20,   16,   12,   8,    4};

/*
** for (int32_t i = 0; i < 768; i++)
**     logTab[i] = (int32_t)round(16777216.0 * exp2(i / 768.0));
*/
static const int32_t logTab[768] = {
    16777216, 16792365, 16807527, 16822704, 16837894, 16853097, 16868315, 16883546, 16898791, 16914049, 16929322,
    16944608, 16959908, 16975222, 16990549, 17005891, 17021246, 17036615, 17051999, 17067396, 17082806, 17098231,
    17113670, 17129123, 17144589, 17160070, 17175564, 17191073, 17206595, 17222132, 17237683, 17253247, 17268826,
    17284419, 17300026, 17315646, 17331282, 17346931, 17362594, 17378271, 17393963, 17409669, 17425389, 17441123,
    17456871, 17472634, 17488410, 17504202, 17520007, 17535826, 17551660, 17567508, 17583371, 17599248, 17615139,
    17631044, 17646964, 17662898, 17678847, 17694810, 17710787, 17726779, 17742785, 17758806, 17774841, 17790891,
    17806955, 17823034, 17839127, 17855235, 17871357, 17887494, 17903645, 17919811, 17935992, 17952187, 17968397,
    17984621, 18000860, 18017114, 18033382, 18049665, 18065963, 18082276, 18098603, 18114945, 18131302, 18147673,
    18164060, 18180461, 18196877, 18213307, 18229753, 18246213, 18262689, 18279179, 18295684, 18312204, 18328739,
    18345288, 18361853, 18378433, 18395028, 18411637, 18428262, 18444902, 18461556, 18478226, 18494911, 18511611,
    18528325, 18545056, 18561801, 18578561, 18595336, 18612127, 18628932, 18645753, 18662589, 18679441, 18696307,
    18713189, 18730086, 18746998, 18763925, 18780868, 18797826, 18814800, 18831788, 18848792, 18865812, 18882846,
    18899897, 18916962, 18934043, 18951139, 18968251, 18985378, 19002521, 19019679, 19036853, 19054042, 19071247,
    19088467, 19105703, 19122954, 19140221, 19157504, 19174802, 19192116, 19209445, 19226790, 19244151, 19261527,
    19278919, 19296327, 19313750, 19331190, 19348645, 19366115, 19383602, 19401104, 19418622, 19436156, 19453706,
    19471271, 19488853, 19506450, 19524063, 19541692, 19559337, 19576998, 19594675, 19612368, 19630077, 19647802,
    19665543, 19683300, 19701072, 19718861, 19736666, 19754488, 19772325, 19790178, 19808047, 19825933, 19843835,
    19861752, 19879686, 19897637, 19915603, 19933586, 19951585, 19969600, 19987631, 20005679, 20023743, 20041823,
    20059920, 20078033, 20096162, 20114308, 20132470, 20150648, 20168843, 20187054, 20205282, 20223526, 20241787,
    20260064, 20278358, 20296668, 20314995, 20333338, 20351698, 20370074, 20388467, 20406877, 20425303, 20443746,
    20462206, 20480682, 20499175, 20517684, 20536211, 20554754, 20573313, 20591890, 20610483, 20629093, 20647720,
    20666364, 20685025, 20703702, 20722396, 20741107, 20759835, 20778580, 20797342, 20816121, 20834917, 20853729,
    20872559, 20891406, 20910270, 20929150, 20948048, 20966963, 20985895, 21004844, 21023810, 21042794, 21061794,
    21080812, 21099846, 21118898, 21137968, 21157054, 21176158, 21195278, 21214417, 21233572, 21252745, 21271935,
    21291142, 21310367, 21329609, 21348868, 21368145, 21387439, 21406751, 21426080, 21445426, 21464790, 21484172,
    21503571, 21522987, 21542421, 21561873, 21581342, 21600829, 21620333, 21639855, 21659395, 21678952, 21698527,
    21718119, 21737729, 21757357, 21777003, 21796666, 21816348, 21836046, 21855763, 21875498, 21895250, 21915020,
    21934808, 21954614, 21974438, 21994279, 22014139, 22034016, 22053912, 22073825, 22093757, 22113706, 22133674,
    22153659, 22173663, 22193684, 22213724, 22233781, 22253857, 22273951, 22294063, 22314194, 22334342, 22354509,
    22374693, 22394897, 22415118, 22435357, 22455615, 22475891, 22496186, 22516499, 22536830, 22557179, 22577547,
    22597933, 22618338, 22638761, 22659202, 22679662, 22700141, 22720638, 22741153, 22761687, 22782240, 22802811,
    22823400, 22844009, 22864635, 22885281, 22905945, 22926628, 22947329, 22968049, 22988788, 23009546, 23030322,
    23051117, 23071931, 23092764, 23113615, 23134485, 23155374, 23176282, 23197209, 23218155, 23239120, 23260103,
    23281106, 23302127, 23323168, 23344227, 23365306, 23386403, 23407520, 23428656, 23449810, 23470984, 23492177,
    23513389, 23534620, 23555871, 23577140, 23598429, 23619737, 23641065, 23662411, 23683777, 23705162, 23726566,
    23747990, 23769433, 23790896, 23812377, 23833879, 23855399, 23876939, 23898499, 23920078, 23941676, 23963294,
    23984932, 24006589, 24028265, 24049962, 24071677, 24093413, 24115168, 24136942, 24158736, 24180550, 24202384,
    24224237, 24246111, 24268003, 24289916, 24311848, 24333801, 24355773, 24377765, 24399776, 24421808, 24443859,
    24465931, 24488022, 24510133, 24532265, 24554416, 24576587, 24598778, 24620990, 24643221, 24665472, 24687744,
    24710036, 24732347, 24754679, 24777031, 24799403, 24821796, 24844209, 24866641, 24889095, 24911568, 24934062,
    24956576, 24979110, 25001665, 25024240, 25046835, 25069451, 25092088, 25114744, 25137421, 25160119, 25182837,
    25205576, 25228335, 25251115, 25273915, 25296736, 25319578, 25342440, 25365322, 25388226, 25411150, 25434095,
    25457060, 25480047, 25503054, 25526081, 25549130, 25572199, 25595290, 25618401, 25641533, 25664686, 25687859,
    25711054, 25734270, 25757506, 25780764, 25804042, 25827342, 25850662, 25874004, 25897367, 25920751, 25944156,
    25967582, 25991029, 26014497, 26037987, 26061498, 26085030, 26108583, 26132158, 26155754, 26179371, 26203009,
    26226669, 26250350, 26274053, 26297777, 26321522, 26345289, 26369077, 26392887, 26416718, 26440571, 26464445,
    26488341, 26512259, 26536198, 26560158, 26584141, 26608145, 26632170, 26656218, 26680287, 26704377, 26728490,
    26752624, 26776780, 26800958, 26825158, 26849380, 26873623, 26897888, 26922176, 26946485, 26970816, 26995169,
    27019544, 27043941, 27068360, 27092802, 27117265, 27141750, 27166258, 27190787, 27215339, 27239913, 27264509,
    27289127, 27313768, 27338430, 27363116, 27387823, 27412552, 27437304, 27462079, 27486875, 27511695, 27536536,
    27561400, 27586286, 27611195, 27636126, 27661080, 27686057, 27711056, 27736077, 27761121, 27786188, 27811277,
    27836389, 27861524, 27886681, 27911861, 27937064, 27962290, 27987538, 28012809, 28038103, 28063420, 28088760,
    28114122, 28139508, 28164916, 28190347, 28215802, 28241279, 28266779, 28292302, 28317849, 28343418, 28369011,
    28394626, 28420265, 28445927, 28471612, 28497320, 28523052, 28548806, 28574584, 28600385, 28626210, 28652058,
    28677929, 28703823, 28729741, 28755683, 28781647, 28807636, 28833647, 28859682, 28885741, 28911823, 28937929,
    28964058, 28990211, 29016388, 29042588, 29068811, 29095059, 29121330, 29147625, 29173944, 29200286, 29226652,
    29253042, 29279456, 29305894, 29332355, 29358841, 29385350, 29411883, 29438441, 29465022, 29491627, 29518256,
    29544910, 29571587, 29598288, 29625014, 29651764, 29678538, 29705336, 29732158, 29759004, 29785875, 29812770,
    29839689, 29866633, 29893600, 29920593, 29947609, 29974650, 30001716, 30028805, 30055920, 30083059, 30110222,
    30137410, 30164622, 30191859, 30219120, 30246407, 30273717, 30301053, 30328413, 30355798, 30383207, 30410642,
    30438101, 30465584, 30493093, 30520627, 30548185, 30575768, 30603377, 30631010, 30658668, 30686351, 30714059,
    30741792, 30769550, 30797333, 30825141, 30852975, 30880833, 30908717, 30936625, 30964559, 30992519, 31020503,
    31048513, 31076548, 31104608, 31132694, 31160805, 31188941, 31217103, 31245290, 31273503, 31301741, 31330005,
    31358294, 31386609, 31414949, 31443315, 31471707, 31500124, 31528567, 31557035, 31585529, 31614049, 31642595,
    31671166, 31699764, 31728387, 31757036, 31785710, 31814411, 31843138, 31871890, 31900669, 31929473, 31958304,
    31987160, 32016043, 32044951, 32073886, 32102847, 32131834, 32160847, 32189887, 32218952, 32248044, 32277162,
    32306307, 32335478, 32364675, 32393898, 32423148, 32452424, 32481727, 32511056, 32540412, 32569794, 32599202,
    32628638, 32658099, 32687588, 32717103, 32746645, 32776213, 32805808, 32835430, 32865078, 32894754, 32924456,
    32954184, 32983940, 33013723, 33043532, 33073369, 33103232, 33133122, 33163040, 33192984, 33222955, 33252954,
    33282979, 33313032, 33343112, 33373219, 33403353, 33433514, 33463703, 33493919, 33524162};

static const uint16_t amigaPeriod[96] = {
    6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3624, 3424, 3232, 3048, 2880,
    2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140,
    1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
    428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,
    170,  160,  151,  143,  135,  127,  120,  113,  107,  101,  95,   90,   85,   80,   75,   71,
    67,   63,   60,   56,   53,   50,   47,   45,   42,   40,   37,   35,   33,   31,   30,   28,
};

static const uint8_t vibTab[32] = {0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                                   255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24};

static const int8_t vibSineTab[256] = {
    0,   -2,  -3,  -5,  -6,  -8,  -9,  -11, -12, -14, -16, -17, -19, -20, -22, -23, -24, -26, -27, -29, -30, -32,
    -33, -34, -36, -37, -38, -39, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -55, -56,
    -56, -57, -58, -59, -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -64, -64, -64, -64, -64, -64, -64,
    -64, -64, -64, -64, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59, -59, -58, -57, -56, -56, -55, -54,
    -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -39, -38, -37, -36, -34, -33, -32, -30, -29,
    -27, -26, -24, -23, -22, -20, -19, -17, -16, -14, -12, -11, -9,  -8,  -6,  -5,  -3,  -2,  0,   2,   3,   5,
    6,   8,   9,   11,  12,  14,  16,  17,  19,  20,  22,  23,  24,  26,  27,  29,  30,  32,  33,  34,  36,  37,
    38,  39,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  56,  57,  58,  59,
    59,  60,  60,  61,  61,  62,  62,  62,  63,  63,  63,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
    63,  63,  63,  62,  62,  62,  61,  61,  60,  60,  59,  59,  58,  57,  56,  56,  55,  54,  53,  52,  51,  50,
    49,  48,  47,  46,  45,  44,  43,  42,  41,  39,  38,  37,  36,  34,  33,  32,  30,  29,  27,  26,  24,  23,
    22,  20,  19,  17,  16,  14,  12,  11,  9,   8,   6,   5,   3,   2};

static const uint8_t arpTab[256] = {
    0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,

    /* 8bb: the following are overflown bytes from FT2's binary, read by the buggy
    ** arpeggio routine. (values confirmed to be the same on FT2.08 and FT2.09)
    */
    0x00, 0x18, 0x31, 0x4A, 0x61, 0x78, 0x8D, 0xA1, 0xB4, 0xC5, 0xD4, 0xE0, 0xEB, 0xF4, 0xFA, 0xFD, 0xFF, 0xFD, 0xFA,
    0xF4, 0xEB, 0xE0, 0xD4, 0xC5, 0xB4, 0xA1, 0x8D, 0x78, 0x61, 0x4A, 0x31, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x03, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x06, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00,
    0x02, 0x00, 0x03, 0x04, 0x05, 0x00, 0x00, 0x0B, 0x00, 0x0A, 0x02, 0x01, 0x03, 0x04, 0x07, 0x00, 0x05, 0x06, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0x02, 0x00, 0x00, 0x8F, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x46, 0x4F, 0x52, 0x4D, 0x49, 0x4C, 0x42, 0x4D, 0x42, 0x4D};

//-----------------------------------------------------------------------------------
// 							Defines - Impulse Tracker 2
//-----------------------------------------------------------------------------------

#ifdef _WIN32

#ifdef _WIN64
#define CPU_32BIT 0
#define CPU_64BIT 1
#else
#define CPU_32BIT 1
#define CPU_64BIT 0
#endif

#else

#if __WORDSIZE == 64
#define CPU_32BIT 0
#define CPU_64BIT 1
#else
#define CPU_32BIT 1
#define CPU_64BIT 0
#endif

#endif

#if CPU_64BIT
#define CPU_BITS      64
#define uintCPUWord_t uint64_t
#define intCPUWord_t  int64_t
#else
#define CPU_BITS      32
#define uintCPUWord_t uint32_t
#define intCPUWord_t  int32_t
#endif
#define CHN_DISOWNED       128
#define DIR_FORWARDS       0
#define DIR_BACKWARDS      1
#define PAN_SURROUND       100
#define LOOP_PINGPONG      24
#define LOOP_FORWARDS      8
#define MAX_PATTERNS       200
#define MAX_SAMPLES        200
#define MAX_INSTRUMENTS    200
#define MAX_ORDERS         256
#define MAX_ROWS           200
#define MAX_HOST_CHANNELS  64
#define MAX_SLAVE_CHANNELS 256
#define MAX_SONGMSG_LENGTH 8000
#define S3M_ROWS           64
// 8bb: 31 is possible through initial tempo (but 32 is general minimum)
#define LOWEST_BPM_POSSIBLE 31
#define MIX_FRAC_BITS       16
#define MIX_FRAC_MASK       ((1 << MIX_FRAC_BITS) - 1)
/* 8bb:
** Amount of extra bytes to allocate for every instrument sample,
** this is used for a hack for resampling interpolation to be
** branchless in the inner channel mixer loop.
** Warning: Do not change this!
*/
#define SMP_DAT_OFFSET    16
#define SAMPLE_PAD_LENGTH (SMP_DAT_OFFSET + 16)

#define Get32Bit8Waveform                                                                                              \
    sample  = smp[0];                                                                                                  \
    sample2 = smp[1];                                                                                                  \
    sample2 -= sample;                                                                                                 \
    sample2 *= (int32_t)sc->Frac32;                                                                                    \
    sample2 >>= MIX_FRAC_BITS - 8;                                                                                     \
    sample <<= 8;                                                                                                      \
    sample += sample2;

#define Get32Bit16Waveform                                                                                             \
    sample  = smp[0];                                                                                                  \
    sample2 = smp[1];                                                                                                  \
    sample2 -= sample;                                                                                                 \
    sample2 >>= 1;                                                                                                     \
    sample2 *= (int32_t)sc->Frac32;                                                                                    \
    sample2 >>= MIX_FRAC_BITS - 1;                                                                                     \
    sample += sample2;

#define UpdatePos                                                                                                      \
    sc->Frac32 += Driver.Delta32;                                                                                      \
    smp += (int32_t)sc->Frac32 >> MIX_FRAC_BITS;                                                                       \
    sc->Frac32 &= MIX_FRAC_MASK;

#define M32Mix8_M                                                                                                      \
    sample = *smp << 8;                                                                                                \
    (*MixBufPtr++) -= sample * sc->LeftVolume;                                                                         \
    (*MixBufPtr++) -= sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix16_M                                                                                                     \
    sample = *smp;                                                                                                     \
    (*MixBufPtr++) -= sample * sc->LeftVolume;                                                                         \
    (*MixBufPtr++) -= sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix8S_M                                                                                                     \
    sample = *smp << 8;                                                                                                \
    (*MixBufPtr++) -= sample * sc->LeftVolume;                                                                         \
    (*MixBufPtr++) += sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix16S_M                                                                                                    \
    sample = *smp;                                                                                                     \
    (*MixBufPtr++) -= sample * sc->LeftVolume;                                                                         \
    (*MixBufPtr++) += sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix8I_M                                                                                                     \
    Get32Bit8Waveform(*MixBufPtr++) -= sample * sc->LeftVolume;                                                        \
    (*MixBufPtr++) -= sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix16I_M                                                                                                    \
    Get32Bit16Waveform(*MixBufPtr++) -= sample * sc->LeftVolume;                                                       \
    (*MixBufPtr++) -= sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix8IS_M                                                                                                    \
    Get32Bit8Waveform(*MixBufPtr++) -= sample * sc->LeftVolume;                                                        \
    (*MixBufPtr++) += sample * sc->RightVolume;                                                                        \
    UpdatePos

#define M32Mix16IS_M                                                                                                   \
    Get32Bit16Waveform(*MixBufPtr++) -= sample * sc->LeftVolume;                                                       \
    (*MixBufPtr++) += sample * sc->RightVolume;                                                                        \
    UpdatePos

//-----------------------------------------------------------------------------------
// 						Enumerations - Impulse Tracker 2
//-----------------------------------------------------------------------------------

enum
{
    FORMAT_UNKNOWN = 0,
    FORMAT_IT      = 1,
    FORMAT_S3M     = 2
};

enum // 8bb: envelope flags
{
    ENVF_ENABLED     = 1,
    ENVF_LOOP        = 2,
    ENVF_SUSTAINLOOP = 4,
    ENVF_CARRY       = 8,
    ENVF_TYPE_FILTER = 128 // 8bb: for pitch envelope only
};

enum                       // 8bb: sample flags
{
    SMPF_ASSOCIATED_WITH_HEADER = 1,
    SMPF_16BIT                  = 2,
    SMPF_STEREO                 = 4,
    SMPF_COMPRESSED             = 8,
    SMPF_USE_LOOP               = 16,
    SMPF_USE_SUSTAINLOOP        = 32,
    SMPF_LOOP_PINGPONG          = 64,
    SMPF_SUSTAINLOOP_PINGPONG   = 128
};

enum // 8bb: host channel flags
{
    HF_UPDATE_EFX_IF_CHAN_ON    = 1,
    HF_ALWAYS_UPDATE_EFX        = 2,
    HF_CHAN_ON                  = 4,
    HF_CHAN_CUT                 = 8,  // No longer implemented
    HF_PITCH_SLIDE_ONGOING      = 16,
    HF_FREEPLAY_NOTE            = 32, // 8bb: Only needed for tracker. Logic removed.
    HF_ROW_UPDATED              = 64,
    HF_APPLY_RANDOM_VOL         = 128,
    HF_UPDATE_VOLEFX_IF_CHAN_ON = 256,
    HF_ALWAYS_VOLEFX            = 512
};

enum // 8bb: slave channel flags
{
    SF_CHAN_ON         = 1,
    SF_RECALC_PAN      = 2,
    SF_NOTE_OFF        = 4,
    SF_FADEOUT         = 8,
    SF_RECALC_VOL      = 16,
    SF_FREQ_CHANGE     = 32,
    SF_RECALC_FINALVOL = 64,
    SF_CENTRAL_PAN     = 128,
    SF_NEW_NOTE        = 256,
    SF_NOTE_STOP       = 512,
    SF_LOOP_CHANGED    = 1024,
    SF_CHN_MUTED       = 2048,
    SF_VOLENV_ON       = 4096,
    SF_PANENV_ON       = 8192,
    SF_PITCHENV_ON     = 16384,
    SF_PAN_CHANGED     = 32768
};

enum                              // 8bb: IT header flags
{
    ITF_STEREO               = 1,
    ITF_VOL0_OPTIMIZATION    = 2, // 8bb: not used in IT1.04 and later
    ITF_INSTR_MODE           = 4,
    ITF_LINEAR_FRQ           = 8,
    ITF_OLD_EFFECTS          = 16,
    ITF_COMPAT_GXX           = 32,
    ITF_USE_MIDI_PITCH_CNTRL = 64,
    ITF_REQ_MIDI_CFG         = 128
};

enum                             // 8bb: audio driver flags
{
    DF_SUPPORTS_MIDI        = 1,
    DF_USES_VOLRAMP         = 2, // 8bb: aka. "hiqual"
    DF_WAVEFORM             = 4, // Output waveform data available
    DF_HAS_RESONANCE_FILTER = 8  // 8bb: added this
};

enum
{
    MIDICOMMAND_START         = 0x0000,
    MIDICOMMAND_STOP          = 0x0020,
    MIDICOMMAND_TICK          = 0x0040,
    MIDICOMMAND_PLAYNOTE      = 0x0060,
    MIDICOMMAND_STOPNOTE      = 0x0080,
    MIDICOMMAND_CHANGEVOLUME  = 0x00A0,
    MIDICOMMAND_CHANGEPAN     = 0x00C0,
    MIDICOMMAND_BANKSELECT    = 0x00E0,
    MIDICOMMAND_PROGRAMSELECT = 0x0100,
    MIDICOMMAND_CHANGEPITCH   = 0xFFFF
};

enum
{
    NNA_NOTE_CUT  = 0,
    NNA_CONTINUE  = 1,
    NNA_NOTE_OFF  = 2,
    NNA_NOTE_FADE = 3,

    DCT_DISABLED   = 0,
    DCT_NOTE       = 1,
    DCT_SAMPLE     = 2,
    DCT_INSTRUMENT = 3,

    DCA_NOTE_CUT = 0
};

//-----------------------------------------------------------------------------------
// 						Typedefs - Impulse Tracker 2
//-----------------------------------------------------------------------------------

typedef struct pattern_t
{
    uint16_t Rows;
    uint8_t *PackedData;
} pattern_t;

typedef struct envNode_t
{
    int8_t   Magnitude;
    uint16_t Tick;
} envNode_t;

typedef struct env_t
{
    uint8_t   Flags, Num, LoopBegin, LoopEnd, SustainLoopBegin, SustainLoopEnd;
    envNode_t NodePoints[25];
} env_t;

typedef struct instrument_t
{
    char     DOSFilename[12 + 1];
    uint8_t  NNA, DCT, DCA;
    uint16_t FadeOut;
    uint8_t  PitchPanSep, PitchPanCenter, GlobVol, DefPan, RandVol, RandPan;
    char     InstrumentName[26];
    uint8_t  FilterCutoff, FilterResonance, MIDIChn, MIDIProg;
    uint16_t MIDIBank;
    uint16_t SmpNoteTable[120];
    env_t    VolEnv, PanEnv, PitchEnv;
} instrument_t;

typedef struct smp_t
{
    char     DOSFilename[12 + 1];
    uint8_t  GlobVol, Flags, Vol;
    char     SampleName[26];
    uint8_t  Cvt, DefPan;
    uint32_t Length, LoopBegin, LoopEnd, C5Speed, SustainLoopBegin, SustainLoopEnd, OffsetInFile;
    uint8_t  AutoVibratoSpeed, AutoVibratoDepth, AutoVibratoRate, AutoVibratoWaveform;
    void    *Data;

    // 8bb: added this for custom HQ driver
    void *OrigData, *DataR, *OrigDataR;
} sample_t;

typedef struct hostChn_t
{
    uint16_t Flags;
    uint8_t  NotePackMask, RawNote, Ins, Vol, Cmd, CmdVal, OldCmd, OldCmdVal, VolCmd, VolCmdVal;
    uint8_t  MIDIChn, MIDIProg, TranslatedNote, Smp;
    uint8_t  DKL, EFG, O00, I00, J00, M00, N00, P00, Q00, T00, S00, W00, GOE, SFx;
    uint8_t  HighSmpOffs;
    uint8_t  HostChnNum, VolSet;
    void    *SlaveChnPtr;
    uint8_t  PattLoopStartRow, PattLoopCount;
    uint8_t  PanbrelloWaveform, PanbrelloPos, PanbrelloDepth, PanbrelloSpeed, LastPanbrelloData;
    int8_t   LastVibratoData, LastTremoloData;
    uint8_t  ChnPan, ChnVol;
    int8_t   VolSlideDelta;
    uint8_t  TremorCount, TremorOnOff, RetrigCount;
    int32_t  PortaFreq;
    uint8_t  VibratoWaveform, VibratoPos, VibratoDepth, VibratoSpeed;
    uint8_t  TremoloWaveform, TremoloPos, TremoloDepth, TremoloSpeed;
    uint8_t  MiscEfxData[16];
} hostChn_t;

typedef struct envState_t
{
    int32_t Value, Delta;
    int16_t Tick, CurNode, NextTick;
} envState_t;

typedef struct slaveChn_t
{
    uint16_t      Flags;
    uint32_t      MixOffset; // 8bb: which sample mix function to use
    uint8_t       LoopMode, LoopDirection;
    int32_t       LeftVolume, RightVolume;
    int32_t       Frequency, FrequencySet;
    uint8_t       SmpBitDepth, AutoVibratoPos;
    uint16_t      AutoVibratoDepth;
    int32_t       OldLeftVolume, OldRightVolume;
    uint8_t       FinalVol7Bit, Vol, VolSet, ChnVol, SmpVol, FinalPan;
    uint16_t      FadeOut;
    uint8_t       DCT, DCA, Pan, PanSet;
    instrument_t *InsPtr;
    sample_t     *SmpPtr;
    uint8_t       Note, Ins;
    uint8_t       Smp;
    void         *HostChnPtr;
    uint8_t       HostChnNum, NNA, MIDIChn, MIDIProg;
    uint16_t      MIDIBank;
    int32_t       LoopBegin, LoopEnd;
    uint32_t      Frac32;
    uint16_t      FinalVol15Bit;
    int32_t       SamplingPosition;
    int32_t       filtera, filterb, filterc;
    envState_t    VolEnvState, PanEnvState, PitchEnvState;

    // 8bb: added these
    uint32_t Delta32;
    int32_t  OldSamples[2];
    int32_t  DestVolL, DestVolR, CurrVolL, CurrVolR; // 8bb: ramp
    float    fOldSamples[4], fFiltera, fFilterb, fFilterc;

    // 8bb: for custom HQ mixer
    float    fOldLeftVolume, fOldRightVolume, fLeftVolume, fRightVolume;
    float    fDestVolL, fDestVolR, fCurrVolL, fCurrVolR;
    uint64_t Frac64, Delta64;
} slaveChn_t;

typedef struct it_header_t
{
    char     SongName[26];
    uint16_t OrdNum, InsNum, SmpNum, PatNum, Cwtv, Cmwt, Flags, Special;
    uint8_t  GlobalVol, MixVolume, InitialSpeed, InitialTempo, PanSep;
    uint16_t MessageLength;
    uint32_t MessageOffset;
    uint8_t  ChnlPan[MAX_HOST_CHANNELS], ChnlVol[MAX_HOST_CHANNELS];
} it_header_t;

typedef struct // 8bb: custom struct
{
    uint32_t NumChannels;
    uint8_t  Flags, FilterParameters[128];
    uint32_t MixMode, MixSpeed;
    int32_t  Delta32;
    int64_t  Delta64;
    float    QualityFactorTable[128], FreqParameterMultiplier, FreqMultiplier;
} driver_t;

typedef struct song_t
{
    it_header_t  Header;
    uint8_t      Orders[MAX_ORDERS];
    instrument_t Ins[MAX_INSTRUMENTS];
    sample_t     Smp[MAX_SAMPLES];
    pattern_t    Patt[MAX_PATTERNS];
    char         Message[MAX_SONGMSG_LENGTH + 1]; // 8bb: +1 to fit protection-NUL

    bool     Playing, Loaded;
    uint8_t *PatternOffset, LastMIDIByte;
    uint16_t CurrentOrder, CurrentPattern, CurrentRow, ProcessOrder, ProcessRow;
    uint16_t BreakRow;
    uint8_t  RowDelay;
    bool     RowDelayOn, StopSong, PatternLooping;
    uint16_t NumberOfRows, CurrentTick, CurrentSpeed, ProcessTick;
    uint16_t Tempo, GlobalVolume;
    uint16_t DecodeExpectedPattern, DecodeExpectedRow;
} song_t;

typedef void (*mixFunc)(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

//-----------------------------------------------------------------------------------
// 						Declarations - Impulse Tracker 2
//-----------------------------------------------------------------------------------

static hostChn_t      hChn[MAX_HOST_CHANNELS];
static slaveChn_t     sChn[MAX_SLAVE_CHANNELS];
static const uint32_t PitchTable[120];
static const int8_t   FineSineData[3 * 256];
static const uint32_t FineLinearSlideUpTable[16];
static const uint32_t LinearSlideUpTable[257];
static const uint16_t FineLinearSlideDownTable[16];
static const uint16_t LinearSlideDownTable[257];
static song_t         Song;
static driver_t       Driver;
static uint16_t       MixVolume;
static int32_t        BytesToMix, *MixBuffer, MixTransferRemaining, MixTransferOffset;
static uint8_t       *PatternDataArea, EncodingInfo[MAX_HOST_CHANNELS * 6];
static bool           FirstTimeLoading = true;
static bool           FirstTimeInit    = true;
static uint8_t        MIDIInterpretState, MIDIInterpretType; // 8bb: for MIDISendFilter()
static uint16_t       RandSeed1 = 0x1234, RandSeed2 = 0x5678;
static char           MIDIDataArea[(9 + 16 + 128) * 32];
static uint8_t        ChannelCountTable[100], ChannelVolumeTable[100];
static slaveChn_t    *ChannelLocationTable[100];
static uint32_t       AllocateNumChannels;
static slaveChn_t    *AllocateSlaveOffset, *LastSlaveChannel;
static const uint8_t  SlideTable[9] = {1, 4, 8, 16, 32, 64, 96, 128, 255};

static void        RecalculateAllVolumes(void);
static void        Update(void);
static void        Music_InitTempo(void);
static void        M32Mix8(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix16(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix8S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix16S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix8I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix16I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix8IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        M32Mix16IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void        Decompress16BitData(int16_t *Dst, const uint8_t *Src, uint32_t BlockLen);
static void        Decompress8BitData(int8_t *Dst, const uint8_t *Src, uint32_t BlockLen);
static bool        LoadCompressed16BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded);
static bool        LoadCompressed8BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded);
static void        ClearEncodingInfo(void);
static bool        GetPatternLength(uint16_t Rows, uint16_t *LengthOut);
static void        EncodePattern(pattern_t *p, uint8_t Rows);
static bool        StorePattern(uint8_t NumRows, int32_t Pattern);
static bool        TranslateS3MPattern(uint8_t *Src, int32_t Pattern);
static void        Music_SetDefaultMIDIDataArea(void); // 8bb: added this
static char       *Music_GetMIDIDataArea(void);
static void        RecalculateAllVolumes(void);
static void        MIDITranslate(hostChn_t *hc, slaveChn_t *sc, uint16_t Input);
static void        InitPlayInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins);
static slaveChn_t *AllocateChannel(hostChn_t *hc, uint8_t *hcFlags);
static uint8_t     Random(void);
static void        GetLoopInformation(slaveChn_t *sc);
static void        ApplyRandomValues(hostChn_t *hc);
static void        PitchSlideUpLinear(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);
static void        PitchSlideUp(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);
static void        PitchSlideDown(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);
static void        Music_Close(void);
static void        NoCommand(hostChn_t *hc);
static void        InitNoCommand(hostChn_t *hc);
static void        InitCommandA(hostChn_t *hc);
static void        InitCommandB(hostChn_t *hc);
static void        InitCommandC(hostChn_t *hc);
static void        InitCommandD(hostChn_t *hc);
static void        InitCommandE(hostChn_t *hc);
static void        InitCommandF(hostChn_t *hc);
static void        InitCommandG(hostChn_t *hc);
static void        InitCommandH(hostChn_t *hc);
static void        InitCommandI(hostChn_t *hc);
static void        InitCommandJ(hostChn_t *hc);
static void        InitCommandK(hostChn_t *hc);
static void        InitCommandL(hostChn_t *hc);
static void        InitCommandM(hostChn_t *hc);
static void        InitCommandN(hostChn_t *hc);
static void        InitCommandO(hostChn_t *hc);
static void        InitCommandP(hostChn_t *hc);
static void        InitCommandQ(hostChn_t *hc);
static void        InitCommandR(hostChn_t *hc);
static void        InitCommandS(hostChn_t *hc);
static void        InitCommandT(hostChn_t *hc);
static void        InitCommandU(hostChn_t *hc);
static void        InitCommandV(hostChn_t *hc);
static void        InitCommandW(hostChn_t *hc);
static void        InitCommandX(hostChn_t *hc);
static void        InitCommandY(hostChn_t *hc);
static void        InitCommandZ(hostChn_t *hc);
static void        InitCommandG11(hostChn_t *hc);
static void        InitCommandM2(hostChn_t *hc, uint8_t vol);
static void        InitCommandX2(hostChn_t *hc, uint8_t pan); // 8bb: pan = 0..63
static void        CommandD(hostChn_t *hc);
static void        CommandE(hostChn_t *hc);
static void        CommandF(hostChn_t *hc);
static void        CommandG(hostChn_t *hc);
static void        CommandH(hostChn_t *hc);
static void        CommandI(hostChn_t *hc);
static void        CommandJ(hostChn_t *hc);
static void        CommandK(hostChn_t *hc);
static void        CommandL(hostChn_t *hc);
static void        CommandN(hostChn_t *hc);
static void        CommandP(hostChn_t *hc);
static void        CommandQ(hostChn_t *hc);
static void        CommandR(hostChn_t *hc);
static void        CommandS(hostChn_t *hc);
static void        CommandT(hostChn_t *hc);
static void        CommandW(hostChn_t *hc);
static void        CommandY(hostChn_t *hc);
static void        CommandH5(hostChn_t *hc, slaveChn_t *sc, int8_t VibratoData);
static void        CommandR2(hostChn_t *hc, slaveChn_t *sc, int8_t TremoloData);
static void        VolumeCommandC(hostChn_t *hc);
static void        VolumeCommandD(hostChn_t *hc);
static void        VolumeCommandE(hostChn_t *hc);
static void        VolumeCommandF(hostChn_t *hc);
static void        VolumeCommandG(hostChn_t *hc);

static uint8_t EmptyPattern[72] = {64, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0,  0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const mixFunc SB16_MixFunctionTables[8] = {(mixFunc)M32Mix8,   (mixFunc)M32Mix16,  (mixFunc)M32Mix8S,
                                                  (mixFunc)M32Mix16S, (mixFunc)M32Mix8I,  (mixFunc)M32Mix16I,
                                                  (mixFunc)M32Mix8IS, (mixFunc)M32Mix16IS};

static void (*InitCommandTable[])(hostChn_t *hc) = {
    InitNoCommand, InitCommandA,  InitCommandB,  InitCommandC, InitCommandD, InitCommandE, InitCommandF,
    InitCommandG,  InitCommandH,  InitCommandI,  InitCommandJ, InitCommandK, InitCommandL, InitCommandM,
    InitCommandN,  InitCommandO,  InitCommandP,  InitCommandQ, InitCommandR, InitCommandS, InitCommandT,
    InitCommandU,  InitCommandV,  InitCommandW,  InitCommandX, InitCommandY, InitCommandZ, InitNoCommand,
    InitNoCommand, InitNoCommand, InitNoCommand, InitNoCommand};

static void (*CommandTable[])(hostChn_t *hc) = {
    NoCommand, NoCommand, NoCommand, NoCommand, CommandD,  CommandE,  CommandF,  CommandG,  CommandH,  CommandI,
    CommandJ,  CommandK,  CommandL,  NoCommand, CommandN,  NoCommand, CommandP,  CommandQ,  CommandR,  CommandS,
    CommandT,  CommandH,  NoCommand, CommandW,  NoCommand, CommandY,  NoCommand, NoCommand, NoCommand, NoCommand};

static void (*VolumeEffectTable[])(hostChn_t *hc) = {NoCommand,      NoCommand,      VolumeCommandC, VolumeCommandD,
                                                     VolumeCommandE, VolumeCommandF, VolumeCommandG, CommandH};

//-----------------------------------------------------------------------------------
// 						Implementation - Impulse Tracker 2
//-----------------------------------------------------------------------------------

static void M32Mix8(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int8_t *base = (int8_t *)sc->SmpPtr->Data;
    const int8_t *smp  = base + sc->SamplingPosition;
    int32_t       sample;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix8_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix8_M M32Mix8_M M32Mix8_M M32Mix8_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix16(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int16_t *base = (int16_t *)sc->SmpPtr->Data;
    const int16_t *smp  = base + sc->SamplingPosition;
    int32_t        sample;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix16_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix16_M M32Mix16_M M32Mix16_M M32Mix16_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix8S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int8_t *base = (int8_t *)sc->SmpPtr->Data;
    const int8_t *smp  = base + sc->SamplingPosition;
    int32_t       sample;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix8S_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix8S_M M32Mix8S_M M32Mix8S_M M32Mix8S_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix16S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int16_t *base = (int16_t *)sc->SmpPtr->Data;
    const int16_t *smp  = base + sc->SamplingPosition;
    int32_t        sample;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix16S_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix16S_M M32Mix16S_M M32Mix16S_M M32Mix16S_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix8I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int8_t *base = (int8_t *)sc->SmpPtr->Data;
    const int8_t *smp  = base + sc->SamplingPosition;
    int32_t       sample, sample2;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix8I_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix8I_M M32Mix8I_M M32Mix8I_M M32Mix8I_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix16I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int16_t *base = (int16_t *)sc->SmpPtr->Data;
    const int16_t *smp  = base + sc->SamplingPosition;
    int32_t        sample, sample2;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix16I_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix16I_M M32Mix16I_M M32Mix16I_M M32Mix16I_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix8IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int8_t *base = (int8_t *)sc->SmpPtr->Data;
    const int8_t *smp  = base + sc->SamplingPosition;
    int32_t       sample, sample2;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix8IS_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix8IS_M M32Mix8IS_M M32Mix8IS_M M32Mix8IS_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void M32Mix16IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
    const int16_t *base = (int16_t *)sc->SmpPtr->Data;
    const int16_t *smp  = base + sc->SamplingPosition;
    int32_t        sample, sample2;

    for (int32_t i = 0; i < (NumSamples & 3); i++)
    {
        M32Mix16IS_M
    }
    NumSamples >>= 2;

    for (int32_t i = 0; i < NumSamples; i++)
    {
        M32Mix16IS_M M32Mix16IS_M M32Mix16IS_M M32Mix16IS_M
    }

    sc->SamplingPosition = (int32_t)(smp - base);
}

static void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples)
{
    const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;

    uint32_t SampleOffset = sc->SamplingPosition + (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
    sc->Frac32 += SamplesToMix & MIX_FRAC_MASK;
    SampleOffset += (uint32_t)sc->Frac32 >> MIX_FRAC_BITS;
    sc->Frac32 &= MIX_FRAC_MASK;

    if (SampleOffset >= (uint32_t)sc->LoopEnd)
    {
        sc->Flags = SF_NOTE_STOP;
        if (!(sc->HostChnNum & CHN_DISOWNED))
        {
            ((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
            return;
        }
    }

    sc->SamplingPosition = SampleOffset;
}

static void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples)
{
    const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;

    sc->Frac32 += SamplesToMix & MIX_FRAC_MASK;
    sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
    sc->SamplingPosition += (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
    sc->Frac32 &= MIX_FRAC_MASK;

    if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // Reset position...
    {
        const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
        if (LoopLength == 0)
            sc->SamplingPosition = 0;
        else
            sc->SamplingPosition = sc->LoopBegin + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);
    }
}

static void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples)
{
    const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

    const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;
    uint32_t       IntSamples   = (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
    uint16_t       FracSamples  = (uint16_t)(SamplesToMix & MIX_FRAC_MASK);

    if (sc->LoopDirection == DIR_BACKWARDS)
    {
        sc->Frac32 -= FracSamples;
        sc->SamplingPosition += ((int32_t)sc->Frac32 >> MIX_FRAC_BITS);
        sc->SamplingPosition -= IntSamples;
        sc->Frac32 &= MIX_FRAC_MASK;

        if (sc->SamplingPosition <= sc->LoopBegin)
        {
            uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
            if (NewLoopPos >= LoopLength)
            {
                sc->SamplingPosition = (sc->LoopEnd - 1) + (LoopLength - NewLoopPos);
            }
            else
            {
                sc->LoopDirection    = DIR_FORWARDS;
                sc->SamplingPosition = sc->LoopBegin + NewLoopPos;
                sc->Frac32           = (uint16_t)(0 - sc->Frac32);
            }
        }
    }
    else // 8bb: forwards
    {
        sc->Frac32 += FracSamples;
        sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
        sc->SamplingPosition += IntSamples;
        sc->Frac32 &= MIX_FRAC_MASK;

        if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
        {
            uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
            if (NewLoopPos >= LoopLength)
            {
                sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
            }
            else
            {
                sc->LoopDirection    = DIR_BACKWARDS;
                sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
                sc->Frac32           = (uint16_t)(0 - sc->Frac32);
            }
        }
    }
}

static void SB16_MixSamples(void)
{
    MixTransferOffset = 0;

    memset(MixBuffer, 0, BytesToMix * 2 * sizeof(int32_t));

    slaveChn_t *sc = sChn;
    for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
    {
        if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
            continue;

        if (sc->Flags & SF_NOTE_STOP)
        {
            sc->Flags &= ~SF_CHAN_ON;
            continue;
        }

        if (sc->Flags & SF_FREQ_CHANGE)
        {
            if ((uint32_t)sc->Frequency >> MIX_FRAC_BITS >= Driver.MixSpeed)
            {
                sc->Flags = SF_NOTE_STOP;
                if (!(sc->HostChnNum & CHN_DISOWNED))
                    ((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Turn off channel

                continue;
            }

            // 8bb: calculate mixer delta
            uint32_t Quotient  = (uint32_t)sc->Frequency / Driver.MixSpeed;
            uint32_t Remainder = (uint32_t)sc->Frequency % Driver.MixSpeed;
            sc->Delta32 = (Quotient << MIX_FRAC_BITS) | (uint16_t)((Remainder << MIX_FRAC_BITS) / Driver.MixSpeed);
        }

        if (sc->Flags & (SF_RECALC_FINALVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
        {
            if (!(sc->Flags & SF_CHN_MUTED))
            {
                if (!(Song.Header.Flags & ITF_STEREO))                                       // 8bb: mono?
                {
                    sc->LeftVolume = sc->RightVolume = (sc->FinalVol15Bit * MixVolume) >> 8; // 8bb: 0..16384
                }
                else if (sc->FinalPan == PAN_SURROUND)
                {
                    sc->LeftVolume = sc->RightVolume = (sc->FinalVol15Bit * MixVolume) >> 9; // 8bb: 0..8192
                }
                else                                                                         // 8bb: normal (panned)
                {
                    sc->LeftVolume  = ((64 - sc->FinalPan) * MixVolume * sc->FinalVol15Bit) >> 14; // 8bb: 0..16384
                    sc->RightVolume = (sc->FinalPan * MixVolume * sc->FinalVol15Bit) >> 14;
                }
            }
        }

        if (sc->Delta32 == 0) // 8bb: added this protection just in case (shouldn't happen)
            continue;

        uint32_t       MixBlockSize = BytesToMix;
        const uint32_t LoopLength   = sc->LoopEnd - sc->LoopBegin; // 8bb: also length for non-loopers

        if ((sc->Flags & SF_CHN_MUTED) || (sc->LeftVolume == 0 && sc->RightVolume == 0))
        {
            if ((int32_t)LoopLength > 0)
            {
                if (sc->LoopMode == LOOP_PINGPONG)
                    UpdatePingPongLoop(sc, MixBlockSize);
                else if (sc->LoopMode == LOOP_FORWARDS)
                    UpdateForwardsLoop(sc, MixBlockSize);
                else
                    UpdateNoLoop(sc, MixBlockSize);
            }

            sc->Flags &= ~(SF_RECALC_PAN | SF_RECALC_VOL | SF_FREQ_CHANGE | SF_RECALC_FINALVOL | SF_NEW_NOTE |
                           SF_NOTE_STOP | SF_LOOP_CHANGED | SF_PAN_CHANGED);

            continue;
        }

        const bool    Surround     = (sc->FinalPan == PAN_SURROUND);
        const bool    Sample16it   = !!(sc->SmpBitDepth & SMPF_16BIT);
        const mixFunc Mix          = SB16_MixFunctionTables[(Driver.MixMode << 2) + (Surround << 1) + Sample16it];
        int32_t      *MixBufferPtr = MixBuffer;

        if ((int32_t)LoopLength > 0)
        {
            if (sc->LoopMode == LOOP_PINGPONG)
            {
                while (MixBlockSize > 0)
                {
                    uint32_t NewLoopPos;
                    if (sc->LoopDirection == DIR_BACKWARDS)
                    {
                        if (sc->SamplingPosition <= sc->LoopBegin)
                        {
                            NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
                            if (NewLoopPos >= LoopLength)
                            {
                                sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);
                            }
                            else
                            {
                                sc->LoopDirection    = DIR_FORWARDS;
                                sc->SamplingPosition = sc->LoopBegin + NewLoopPos;
                                sc->Frac32           = (uint16_t)(0 - sc->Frac32);
                            }
                        }
                    }
                    else // 8bb: forwards
                    {
                        if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
                        {
                            NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
                            if (NewLoopPos >= LoopLength)
                            {
                                sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
                            }
                            else
                            {
                                sc->LoopDirection    = DIR_BACKWARDS;
                                sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
                                sc->Frac32           = (uint16_t)(0 - sc->Frac32);
                            }
                        }
                    }

                    uint32_t SamplesToMix;
                    if (sc->LoopDirection == DIR_BACKWARDS)
                    {
                        SamplesToMix = sc->SamplingPosition - (sc->LoopBegin + 1);
#if CPU_32BIT
                        if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of
                                                       // slow software 64-bit div)
                            SamplesToMix = UINT16_MAX;
#endif
                        SamplesToMix =
                            ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)sc->Frac32) / sc->Delta32) + 1;
                        Driver.Delta32 = 0 - sc->Delta32;
                    }
                    else // 8bb: forwards
                    {
                        SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
                        if (SamplesToMix > UINT16_MAX)
                            SamplesToMix = UINT16_MAX;
#endif
                        SamplesToMix =
                            ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) /
                             sc->Delta32) +
                            1;
                        Driver.Delta32 = sc->Delta32;
                    }

                    if (SamplesToMix > MixBlockSize)
                        SamplesToMix = MixBlockSize;

                    Mix(sc, MixBufferPtr, SamplesToMix);
                    MixBufferPtr += SamplesToMix << 1;

                    MixBlockSize -= SamplesToMix;
                }
            }
            else if (sc->LoopMode == LOOP_FORWARDS)
            {
                while (MixBlockSize > 0)
                {
                    if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
                        sc->SamplingPosition =
                            sc->LoopBegin + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);

                    uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
                    if (SamplesToMix > UINT16_MAX)
                        SamplesToMix = UINT16_MAX;
#endif
                    SamplesToMix =
                        ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) /
                         sc->Delta32) +
                        1;
                    if (SamplesToMix > MixBlockSize)
                        SamplesToMix = MixBlockSize;

                    Driver.Delta32 = sc->Delta32;
                    Mix(sc, MixBufferPtr, SamplesToMix);
                    MixBufferPtr += SamplesToMix << 1;

                    MixBlockSize -= SamplesToMix;
                }
            }
            else // 8bb: no loop
            {
                while (MixBlockSize > 0)
                {
                    if ((uint32_t)sc->SamplingPosition >=
                        (uint32_t)sc->LoopEnd) // 8bb: LoopEnd = sample end, even for non-loopers
                    {
                        sc->Flags = SF_NOTE_STOP;
                        if (!(sc->HostChnNum & CHN_DISOWNED))
                            ((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off

                        break;
                    }

                    uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
                    if (SamplesToMix > UINT16_MAX)
                        SamplesToMix = UINT16_MAX;
#endif
                    SamplesToMix =
                        ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) /
                         sc->Delta32) +
                        1;
                    if (SamplesToMix > MixBlockSize)
                        SamplesToMix = MixBlockSize;

                    Driver.Delta32 = sc->Delta32;
                    Mix(sc, MixBufferPtr, SamplesToMix);
                    MixBufferPtr += SamplesToMix << 1;

                    MixBlockSize -= SamplesToMix;
                }
            }
        }

        sc->Flags &= ~(SF_RECALC_PAN | SF_RECALC_VOL | SF_FREQ_CHANGE | SF_RECALC_FINALVOL | SF_NEW_NOTE |
                       SF_NOTE_STOP | SF_LOOP_CHANGED | SF_PAN_CHANGED);
    }
}

static void SB16_SetTempo(uint8_t Tempo)
{
    assert(Tempo >= LOWEST_BPM_POSSIBLE);
    BytesToMix = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) / Tempo;
}

static void SB16_SetMixVolume(uint8_t vol)
{
    // dasho: shifted to try to have more 'parity' with
    // the FT2 default output volume; it can be a bit
    // jarring otherwise
    MixVolume = vol >> 2;
    RecalculateAllVolumes();
}

static void SB16_ResetMixer(void) // 8bb: added this
{
    MixTransferRemaining = 0;
    MixTransferOffset    = 0;
}

static int32_t SB16_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput) // 8bb: added this
{
    const uint8_t SampleShiftValue = (Song.Header.Flags & ITF_STEREO) ? 13 : 14;

    int32_t SamplesTodo = (SamplesToOutput == 0) ? BytesToMix : SamplesToOutput;
    for (int32_t i = 0; i < SamplesTodo * 2; i++)
    {
        int32_t Sample = MixBuffer[MixTransferOffset++] >> SampleShiftValue;

        if (Sample < INT16_MIN)
            Sample = INT16_MIN;
        else if (Sample > INT16_MAX)
            Sample = INT16_MAX;

        *AudioOut16++ = (int16_t)Sample;
    }

    return SamplesTodo;
}

static int32_t SB16_PostMix_Float(float *AudioOut32, int32_t SamplesToOutput)
{
    const uint8_t SampleShiftValue = (Song.Header.Flags & ITF_STEREO) ? 13 : 14;

    int32_t SamplesTodo = (SamplesToOutput == 0) ? BytesToMix : SamplesToOutput;
    for (int32_t i = 0; i < SamplesTodo * 2; i++)
    {
        int32_t Sample = MixBuffer[MixTransferOffset++] >> SampleShiftValue;

        if (Sample < INT16_MIN)
            Sample = INT16_MIN;
        else if (Sample > INT16_MAX)
            Sample = INT16_MAX;
#if defined _MSC_VER || (defined __SIZEOF_FLOAT__ && __SIZEOF_FLOAT__ == 4)
        *(uint32_t *)AudioOut32 = 0x43818000 ^ ((uint16_t)Sample);
        *AudioOut32++ -= 259.0f;
#else
        *AudioOut32++ = (float)Sample * 0.000030517578125f;
#endif
    }

    return SamplesTodo;
}

static void SB16_Mix(int32_t numSamples, int16_t *audioOut) // 8bb: added this (original SB16 driver uses IRQ callback)
{
    int32_t SamplesLeft = numSamples;
    while (SamplesLeft > 0)
    {
        if (MixTransferRemaining == 0)
        {
            Update();
            SB16_MixSamples();
            MixTransferRemaining = BytesToMix;
        }

        int32_t SamplesToTransfer = SamplesLeft;
        if (SamplesToTransfer > MixTransferRemaining)
            SamplesToTransfer = MixTransferRemaining;

        SB16_PostMix(audioOut, SamplesToTransfer);
        audioOut += SamplesToTransfer * 2;

        MixTransferRemaining -= SamplesToTransfer;
        SamplesLeft -= SamplesToTransfer;
    }
}

static void SB16_Mix_Float(int32_t numSamples, float *audioOut)
{
    int32_t SamplesLeft = numSamples;
    while (SamplesLeft > 0)
    {
        if (MixTransferRemaining == 0)
        {
            Update();
            SB16_MixSamples();
            MixTransferRemaining = BytesToMix;
        }

        int32_t SamplesToTransfer = SamplesLeft;
        if (SamplesToTransfer > MixTransferRemaining)
            SamplesToTransfer = MixTransferRemaining;

        SB16_PostMix_Float(audioOut, SamplesToTransfer);
        audioOut += SamplesToTransfer * 2;

        MixTransferRemaining -= SamplesToTransfer;
        SamplesLeft -= SamplesToTransfer;
    }
}

/* 8bb:
** Fixes sample end bytes for interpolation (yes, we have room after the data).
** Sustain loops are always handled as non-looping during fix in IT2.
*/
static void SB16_FixSamples(void)
{
    sample_t *s = Song.Smp;
    for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
    {
        if (s->Data == NULL || s->Length == 0)
            continue;

        int8_t    *data8       = (int8_t *)s->Data;
        const bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
        const bool HasLoop     = !!(s->Flags & SMPF_USE_LOOP);

        int8_t *smp8Ptr = &data8[s->Length << Sample16Bit];

        // 8bb: added this protection for looped samples
        if (HasLoop && s->LoopEnd - s->LoopBegin < 2)
        {
            *smp8Ptr++ = 0;
            *smp8Ptr++ = 0;
            return;
        }

        int8_t byte1 = 0;
        int8_t byte2 = 0;

        if (HasLoop)
        {
            int32_t src;
            if (s->Flags & SMPF_LOOP_PINGPONG)
            {
                src = s->LoopEnd - 2;
                if (src < 0)
                    src = 0;
            }
            else // 8bb: forward loop
            {
                src = s->LoopBegin;
            }

            if (Sample16Bit)
                src <<= 1;

            byte1 = data8[src + 0];
            byte2 = data8[src + 1];
        }

        *smp8Ptr++ = byte1;
        *smp8Ptr++ = byte2;
    }
}

static void SB16_CloseDriver(void)
{
    if (MixBuffer != NULL)
    {
        M4P_FREE(MixBuffer);
        MixBuffer = NULL;
    }
}

static bool SB16_InitDriver(int32_t mixingFrequency)
{
    if (mixingFrequency < 8000)
        mixingFrequency = 8000;
    else if (mixingFrequency > 64000)
        mixingFrequency = 64000;

    const int32_t MaxSamplesToMix = (((mixingFrequency << 1) + (mixingFrequency >> 1)) / LOWEST_BPM_POSSIBLE) + 1;

    MixBuffer = (int32_t *)M4P_MALLOC(MaxSamplesToMix * 2 * sizeof(int32_t));
    if (MixBuffer == NULL)
        return false;

    Driver.Flags       = DF_SUPPORTS_MIDI;
    Driver.NumChannels = 64;
    Driver.MixSpeed    = mixingFrequency;

    /*
    ** MixMode 0 = "32 Bit Non-interpolated"
    ** MixMode 1 = "32 Bit Interpolated"
    */
    Driver.MixMode = 1; // 8bb: "32 Bit Interpolated"
    return true;
}

static void NoCommand(hostChn_t *hc)
{
    (void)hc;
    return;
}

static void CommandEChain(hostChn_t *hc, uint16_t SlideValue)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    PitchSlideDown(hc, sc, SlideValue);
    sc->FrequencySet = sc->Frequency;
}

static void CommandFChain(hostChn_t *hc, uint16_t SlideValue)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    PitchSlideUp(hc, sc, SlideValue);
    sc->FrequencySet = sc->Frequency;
}

static void CommandD2(hostChn_t *hc, slaveChn_t *sc, uint8_t vol)
{
    sc->Vol = sc->VolSet = hc->VolSet = vol;
    sc->Flags |= SF_RECALC_VOL;
}

static void InitVibrato(hostChn_t *hc)
{
    if (Song.Header.Flags & ITF_OLD_EFFECTS)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

        sc->Flags |= SF_FREQ_CHANGE;
        CommandH5(hc, sc, hc->LastVibratoData);
    }
    else
    {
        CommandH(hc);
    }
}

static void InitCommandD7(hostChn_t *hc, slaveChn_t *sc) // Jmp point for Lxx (8bb: and Dxx/Kxx)
{
    sc->Flags |= SF_RECALC_VOL;

    uint8_t hi = hc->DKL & 0xF0;
    uint8_t lo = hc->DKL & 0x0F;

    if (lo == 0)
    {
        // Slide up.
        hc->VolSlideDelta = hi >> 4;
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

        if (hc->VolSlideDelta == 0x0F)
            CommandD(hc);
    }
    else if (hi == 0)
    {
        // Slide down

        hc->VolSlideDelta = -lo;
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

        if (hc->VolSlideDelta == -15)
            CommandD(hc);
    }
    else if (lo == 0x0F)
    {
        // Slide up (fine)
        hc->VolSlideDelta = 0;

        uint8_t vol = sc->VolSet + (hi >> 4);
        if (vol > 64)
            vol = 64;

        sc->Vol = sc->VolSet = hc->VolSet = vol;
    }
    else if (hi == 0xF0)
    {
        // Slide down (fine)
        hc->VolSlideDelta = 0;

        uint8_t vol = sc->VolSet - lo;
        if ((int8_t)vol < 0)
            vol = 0;

        sc->Vol = sc->VolSet = hc->VolSet = vol;
    }
}

static void InitVolumeEffect(hostChn_t *hc)
{
    if (!(hc->NotePackMask & 0x44))
        return;

    int8_t volCmd = (hc->Vol & 0x7F) - 65;
    if (volCmd < 0)
        return;

    if (hc->Vol & 0x80)
        volCmd += 60;

    uint8_t cmd = (uint8_t)volCmd / 10;
    uint8_t val = (uint8_t)volCmd % 10;

    hc->VolCmd = cmd; // Store effect number

    /* Memory for effects A->D, (EFG)/H don't share.
    **
    ** Effects Ax and Bx (fine volume slide up and down) require immediate
    ** handling. No flags required. (effect 0 and 1)
    **
    ** Effects Cx, Dx, Ex, Fx (volume/pitch slides) require flag to be
    ** set   (effects 2->5)
    **
    ** Effects Gx and Hx need init (handling) code + flags.
    ** (effects 6 and 7).
    */

    if (val > 0)
    {
        if (cmd < 4)
        {
            hc->VolCmdVal = val;
        }
        else if (cmd < 6)
        {
            hc->EFG = val << 2;
        }
        else if (cmd == 6)
        {
            if (Song.Header.Flags & ITF_COMPAT_GXX)
                hc->GOE = SlideTable[val - 1];
            else
                hc->EFG = SlideTable[val - 1];
        }
    }

    if (hc->Flags & HF_CHAN_ON)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

        if (cmd > 1)
        {
            hc->Flags |= HF_UPDATE_VOLEFX_IF_CHAN_ON;

            if (cmd > 6)
            {
                if (val != 0)
                    hc->VibratoDepth = val << 2;

                if (hc->Flags & HF_CHAN_ON)
                    InitVibrato(hc);
            }
            else if (cmd == 6)
            {
                InitCommandG11(hc);
            }
        }
        else if (cmd == 1)
        {
            // Fine volume slide down

            int8_t vol = sc->VolSet - hc->VolCmdVal;
            if (vol < 0)
                vol = 0;

            CommandD2(hc, sc, vol);
        }
        else
        {
            // Fine volume slide up

            int8_t vol = sc->VolSet + hc->VolCmdVal;
            if (vol > 64)
                vol = 64;

            CommandD2(hc, sc, vol);
        }
    }
    else
    {
        // Channel not on!

        if (cmd == 7) // Vibrato?
        {
            if (val != 0)
                hc->VibratoDepth = val << 2;

            if (hc->Flags & HF_CHAN_ON)
                InitVibrato(hc);
        }
    }
}

static void VolumeCommandC(hostChn_t *hc)
{
    slaveChn_t *sc = hc->SlaveChnPtr;

    int8_t vol = sc->VolSet + hc->VolCmdVal;
    if (vol > 64)
    {
        hc->Flags &= ~HF_UPDATE_VOLEFX_IF_CHAN_ON; // Turn off effect calling
        vol = 64;
    }

    CommandD2(hc, sc, vol);
}

static void VolumeCommandD(hostChn_t *hc)
{
    slaveChn_t *sc = hc->SlaveChnPtr;

    int8_t vol = sc->VolSet - hc->VolCmdVal;
    if (vol < 0)
    {
        hc->Flags &= ~HF_UPDATE_VOLEFX_IF_CHAN_ON; // Turn off effect calling
        vol = 0;
    }

    CommandD2(hc, sc, vol);
}

static void VolumeCommandE(hostChn_t *hc)
{
    CommandEChain(hc, hc->EFG << 2);
}

static void VolumeCommandF(hostChn_t *hc)
{
    CommandFChain(hc, hc->EFG << 2);
}

static void VolumeCommandG(hostChn_t *hc)
{
    if (!(hc->Flags & HF_PITCH_SLIDE_ONGOING))
        return;

    int16_t SlideValue = hc->EFG << 2;
    if (Song.Header.Flags & ITF_COMPAT_GXX)
        SlideValue = hc->GOE << 2;

    if (SlideValue == 0)
        return;

    slaveChn_t *sc = hc->SlaveChnPtr;

    if (hc->MiscEfxData[2] == 1) // 8bb: slide up?
    {
        PitchSlideUp(hc, sc, SlideValue);
        sc->FrequencySet = sc->Frequency;

        if ((sc->Flags & SF_NOTE_STOP) || sc->Frequency >= hc->PortaFreq)
        {
            sc->Flags &= ~SF_NOTE_STOP;
            hc->Flags |= HF_CHAN_ON;                                              // Turn on

            sc->FrequencySet = sc->Frequency = hc->PortaFreq;
            hc->Flags &= ~(HF_PITCH_SLIDE_ONGOING | HF_UPDATE_VOLEFX_IF_CHAN_ON); // Turn off calling
        }
    }
    else                                                                          // 8bb: slide down
    {
        PitchSlideDown(hc, sc, SlideValue);

        if (sc->Frequency <= hc->PortaFreq)
        {
            sc->Frequency = hc->PortaFreq;
            hc->Flags &= ~(HF_PITCH_SLIDE_ONGOING | HF_UPDATE_VOLEFX_IF_CHAN_ON); // Turn off calling
        }

        sc->FrequencySet = sc->Frequency;
    }
}

static void InitNoCommand3(hostChn_t *hc, uint8_t hcFlags)
{
    // Randomise volume if required.

    bool ApplyRandomVolume = !!(hc->Flags & HF_APPLY_RANDOM_VOL);

    hc->Flags = (hc->Flags & 0xFF00) | hcFlags;

    if (ApplyRandomVolume)
        ApplyRandomValues(hc);

    InitVolumeEffect(hc);
}

static void NoOldEffect(hostChn_t *hc, uint8_t hcFlags)
{
    uint8_t vol = hc->Vol;
    if (!((hc->NotePackMask & 0x44) && vol <= 64)) // 8bb: improve this yucky logic...
    {
        if ((hc->NotePackMask & 0x44) && (vol & 0x7F) < 65)
        {
            // Panning set!
            hc->Flags = (hc->Flags & 0xFF00) | hcFlags;
            InitCommandX2(hc, vol - 128);
        }

        if (!(hc->NotePackMask & 0x22) || hc->Smp == 0) // Instrument present?
        {
            InitNoCommand3(hc, hcFlags);
            return;
        }

        vol = Song.Smp[hc->Smp - 1].Vol; // Default volume
    }

    hc->VolSet = vol;

    if (hcFlags & HF_CHAN_ON)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
        sc->Vol = sc->VolSet = vol;
        sc->Flags |= SF_RECALC_VOL;
    }

    InitNoCommand3(hc, hcFlags);
}

static void InitNoCommand11(hostChn_t *hc, slaveChn_t *sc, uint8_t hcFlags)
{
    GetLoopInformation(sc);

    if (!(hc->NotePackMask & (0x22 + 0x44)))
    {
        InitNoCommand3(hc, hcFlags);
        return;
    }

    if ((Song.Header.Flags & (ITF_INSTR_MODE | ITF_OLD_EFFECTS)) == ITF_INSTR_MODE + ITF_OLD_EFFECTS)
    {
        if ((hc->NotePackMask & 0x22) && hc->Ins != 255)
        {
            sc->FadeOut = 1024;
            InitPlayInstrument(hc, sc, &Song.Ins[hc->Ins - 1]);
        }
    }

    NoOldEffect(hc, hcFlags);
}

static void InitNoCommand(hostChn_t *hc)
{
    uint8_t hcFlags = hc->Flags & 0xFF;

    if (!(hc->NotePackMask & 0x33))
    {
        NoOldEffect(hc, hcFlags);
        return;
    }

    // Note here! Check for noteoff.
    if (hc->TranslatedNote >= 120)
    {
        if (hcFlags & HF_CHAN_ON)
        {
            slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

            if (hc->TranslatedNote == 255) // 8bb: note off
            {
                sc->Flags |= SF_NOTE_OFF;
                InitNoCommand11(hc, sc, hcFlags);
                return;
            }
            else if (hc->TranslatedNote == 254) // 8bb: note cut
            {
                hcFlags &= ~HF_CHAN_ON;

                if (sc->Smp == 100 || (Driver.Flags & DF_USES_VOLRAMP))
                    sc->Flags |= SF_NOTE_STOP;
                else
                    sc->Flags = SF_NOTE_STOP;
            }
            else // 8bb: note fade (?)
            {
                sc->Flags |= SF_FADEOUT;
            }
        }

        NoOldEffect(hc, hcFlags);
        return;
    }

    if (hcFlags & HF_CHAN_ON)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
        if (!(hc->NotePackMask & 0x11) && sc->Note == hc->RawNote && sc->Ins == hc->Ins)
        {
            NoOldEffect(hc, hcFlags);
            return;
        }
    }

    if ((hc->NotePackMask & 0x44) && hc->Vol >= 193 && hc->Vol <= 202 && (hc->Flags & HF_CHAN_ON))
    {
        InitVolumeEffect(hc);
        return;
    }

    slaveChn_t *sc = AllocateChannel(hc, &hcFlags);
    if (sc == NULL)
    {
        NoOldEffect(hc, hcFlags);
        return;
    }

    // Channel allocated.

    sample_t *s = sc->SmpPtr;

    sc->Vol = sc->VolSet = hc->VolSet;

    if (!(Song.Header.Flags & ITF_INSTR_MODE))
    {
        if (s->DefPan & 0x80)
            hc->ChnPan = sc->Pan = s->DefPan & 127;
    }

    sc->SamplingPosition = 0;
    sc->Frac32           = 0; // 8bb: clear fractional sampling position
    sc->Frac64           = 0; // 8bb: also clear frac for my high-quality driver/mixer
    sc->Frequency = sc->FrequencySet = ((uint64_t)s->C5Speed * (uint32_t)PitchTable[hc->TranslatedNote]) >> 16;

    hcFlags |= HF_CHAN_ON;
    hcFlags &= ~HF_PITCH_SLIDE_ONGOING;

    InitNoCommand11(hc, sc, hcFlags);
}

static void InitCommandA(hostChn_t *hc)
{
    if (hc->CmdVal != 0)
    {
        Song.CurrentTick  = (Song.CurrentTick - Song.CurrentSpeed) + hc->CmdVal;
        Song.CurrentSpeed = hc->CmdVal;
    }

    InitNoCommand(hc);
}

static void InitCommandB(hostChn_t *hc)
{
    /*
    if (hc->CmdVal <= Song.CurrentOrder)
        Song.StopSong = true; // 8bb: for WAV writer
    */

    Song.ProcessOrder = hc->CmdVal - 1;
    Song.ProcessRow   = 0xFFFE;

    InitNoCommand(hc);
}

static void InitCommandC(hostChn_t *hc)
{
    if (!Song.PatternLooping)
    {
        Song.BreakRow   = hc->CmdVal;
        Song.ProcessRow = 0xFFFE;
    }

    InitNoCommand(hc);
}

static void InitCommandD(hostChn_t *hc)
{
    InitNoCommand(hc);

    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->DKL;

    hc->DKL = CmdVal;

    if (!(hc->Flags & HF_CHAN_ON))
        return;

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    InitCommandD7(hc, sc);
}

static void InitCommandE(hostChn_t *hc)
{
    InitNoCommand(hc);

    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->EFG;

    hc->EFG = CmdVal;

    if (!(hc->Flags & HF_CHAN_ON) || hc->EFG == 0)
        return;

    if ((hc->EFG & 0xF0) < 0xE0)
    {
        *(uint16_t *)&hc->MiscEfxData[0] = hc->EFG << 2;
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // call update only if necess.
        return;
    }

    if ((hc->EFG & 0x0F) == 0)
        return;

    uint16_t SlideVal = hc->EFG & 0x0F;
    if ((hc->EFG & 0xF0) != 0xE0)
        SlideVal <<= 2;

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    PitchSlideDown(hc, sc, SlideVal);
    sc->FrequencySet = sc->Frequency;
}

static void InitCommandF(hostChn_t *hc)
{
    InitNoCommand(hc);

    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->EFG;

    hc->EFG = CmdVal;

    if (!(hc->Flags & HF_CHAN_ON) || hc->EFG == 0)
        return;

    if ((hc->EFG & 0xF0) < 0xE0)
    {
        *(uint16_t *)&hc->MiscEfxData[0] = hc->EFG << 2;
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // call update only if necess.
        return;
    }

    if ((hc->EFG & 0x0F) == 0)
        return;

    uint16_t SlideVal = hc->EFG & 0x0F;
    if ((hc->EFG & 0xF0) != 0xE0)
        SlideVal <<= 2;

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    PitchSlideUp(hc, sc, SlideVal);
    sc->FrequencySet = sc->Frequency;
}

static bool Gxx_ChangeSample(hostChn_t *hc, slaveChn_t *sc, uint8_t sample)
{
    sc->Flags &= ~(SF_NOTE_STOP | SF_LOOP_CHANGED | SF_CHN_MUTED | SF_VOLENV_ON | SF_PANENV_ON | SF_PITCHENV_ON |
                   SF_PAN_CHANGED);

    sc->Flags |= SF_NEW_NOTE;

    // Now to update sample info.

    sample_t *s = sc->SmpPtr = &Song.Smp[sample];
    sc->Smp                  = sample;
    sc->AutoVibratoDepth     = 0;
    sc->LoopDirection        = 0;
    sc->Frac32               = 0; // 8bb: reset sampling position fraction
    sc->Frac64               = 0; // 8bb: also clear frac for my high-quality driver/mixer
    sc->SamplingPosition     = 0;
    sc->SmpVol               = s->GlobVol * 2;

    if (!(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
    {
        // 8bb: turn off channel
        sc->Flags = SF_NOTE_STOP;
        hc->Flags &= ~HF_CHAN_ON;
        return false;
    }

    sc->SmpBitDepth = s->Flags & SMPF_16BIT;
    GetLoopInformation(sc);

    return true;
}

static void InitCommandG11(hostChn_t *hc) // Jumped to from Lxx (8bb: and normal tone portamento)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

    if ((hc->NotePackMask & 0x22) && hc->Smp > 0)
    {
        // Checking for change of sample or instrument.

        bool ChangeInstrument = false;

        if (Song.Header.Flags & ITF_COMPAT_GXX)
        {
            hc->Smp    = sc->Smp + 1;
            sc->SmpVol = Song.Smp[sc->Smp].GlobVol * 2;

            ChangeInstrument = true;
        }
        else if (hc->Smp != 101) // Don't overwrite note if MIDI!
        {
            const uint8_t hcSmp       = hc->Smp - 1;
            const uint8_t oldSlaveIns = sc->Ins;

            sc->Note = hc->RawNote;
            sc->Ins  = hc->Ins;

            if (sc->Ins != oldSlaveIns) // Ins the same?
            {
                if (sc->Smp != hcSmp)   // Sample the same?
                {
                    if (!Gxx_ChangeSample(hc, sc, hcSmp))
                        return;         // 8bb: sample was not assciated with sample header
                }

                ChangeInstrument = true;
            }
            else if (sc->Smp != hcSmp)
            {
                if (!Gxx_ChangeSample(hc, sc, hcSmp))
                    return; // 8bb: sample was not assciated with sample header

                ChangeInstrument = true;
            }
        }

        if ((Song.Header.Flags & ITF_INSTR_MODE) && ChangeInstrument)
        {
            // Now for instruments

            instrument_t *ins = &Song.Ins[hc->Ins - 1];

            sc->FadeOut = 1024;

            uint16_t oldSCFlags = sc->Flags;
            InitPlayInstrument(hc, sc, ins);

            if (oldSCFlags & SF_CHAN_ON)
                sc->Flags &= ~SF_NEW_NOTE;

            sc->SmpVol = (ins->GlobVol * sc->SmpVol) >> 7;
        }
    }

    if ((Song.Header.Flags & ITF_INSTR_MODE) || (hc->NotePackMask & 0x11))
    {
        // OK. Time to calc freq.

        if (hc->TranslatedNote <= 119)
        {
            // Don't overwrite note if MIDI!
            if (hc->Smp != 101)
                sc->Note = hc->TranslatedNote;

            sample_t *s = sc->SmpPtr;

            hc->PortaFreq = ((uint64_t)s->C5Speed * (uint32_t)PitchTable[hc->TranslatedNote]) >> 16;
            hc->Flags |= HF_PITCH_SLIDE_ONGOING;
        }
        else if (hc->Flags & HF_CHAN_ON)
        {
            if (hc->TranslatedNote == 255)
            {
                sc->Flags |= SF_NOTE_OFF;
                GetLoopInformation(sc);
            }
            else if (hc->TranslatedNote == 254)
            {
                hc->Flags &= ~HF_CHAN_ON;
                sc->Flags = SF_NOTE_STOP;
            }
            else
            {
                sc->Flags |= SF_FADEOUT;
            }
        }
    }

    bool    volFromVolColumn = false;
    uint8_t vol              = 0; // 8bb: set to 0, just to make the compiler happy..

    if (hc->NotePackMask & 0x44)
    {
        if (hc->Vol <= 64)
        {
            vol              = hc->Vol;
            volFromVolColumn = true;
        }
        else
        {
            if ((hc->Vol & 0x7F) < 65)
                InitCommandX2(hc, hc->Vol - 128);
        }
    }

    if (volFromVolColumn || (hc->NotePackMask & 0x22))
    {
        if (!volFromVolColumn)
            vol = sc->SmpPtr->Vol;

        sc->Flags |= SF_RECALC_VOL;
        sc->Vol = sc->VolSet = hc->VolSet = vol;
    }

    if (hc->Flags & HF_PITCH_SLIDE_ONGOING) // Slide on???
    {
        // Work out magnitude + dirn

        uint16_t SlideSpeed;
        if (Song.Header.Flags & ITF_COMPAT_GXX) // Command G memory
            SlideSpeed = hc->GOE << 2;
        else
            SlideSpeed = hc->EFG << 2;

        if (SlideSpeed > 0)
        {
            *(uint16_t *)&hc->MiscEfxData[0] = SlideSpeed;

            if (sc->FrequencySet != hc->PortaFreq)
            {
                if (sc->FrequencySet > hc->PortaFreq)
                    hc->MiscEfxData[2] = 0;                // slide down
                else
                    hc->MiscEfxData[2] = 1;                // slide up

                if (!(hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
                    hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update effect if necess.
            }
        }
    }

    // Don't call volume effects if it has a Gxx!
    if (!(hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
        InitVolumeEffect(hc);
}

static void InitCommandG(hostChn_t *hc)
{
    if (hc->CmdVal != 0)
    {
        if (Song.Header.Flags & ITF_COMPAT_GXX) // Compatibility Gxx?
            hc->GOE = hc->CmdVal;
        else
            hc->EFG = hc->CmdVal;
    }

    if (!(hc->Flags & HF_CHAN_ON))
    {
        InitNoCommand(hc);
        return;
    }

    InitCommandG11(hc);
}

static void InitCommandH(hostChn_t *hc)
{
    if ((hc->NotePackMask & 0x11) && hc->RawNote <= 119)
        hc->VibratoPos = hc->LastVibratoData = 0;

    uint8_t speed = (hc->CmdVal >> 4) << 2;
    uint8_t depth = (hc->CmdVal & 0x0F) << 2;

    if (speed > 0)
        hc->VibratoSpeed = speed;

    if (depth > 0)
    {
        if (Song.Header.Flags & ITF_OLD_EFFECTS)
            depth <<= 1;

        hc->VibratoDepth = depth;
    }

    InitNoCommand(hc);

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
        InitVibrato(hc);
    }
}

static void InitCommandI(hostChn_t *hc)
{
    InitNoCommand(hc);

    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal > 0)
        hc->I00 = CmdVal;

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

        uint8_t OffTime = hc->I00 & 0x0F;
        uint8_t OnTime  = hc->I00 >> 4;

        if (Song.Header.Flags & ITF_OLD_EFFECTS)
        {
            OffTime++;
            OnTime++;
        }

        hc->MiscEfxData[0] = OffTime;
        hc->MiscEfxData[1] = OnTime;

        CommandI(hc);
    }
}

static void InitCommandJ(hostChn_t *hc)
{
    InitNoCommand(hc);

    *(uint16_t *)&hc->MiscEfxData[0] = 0; // 8bb: clear arp tick counter

    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->J00;

    hc->J00 = CmdVal;

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update when channel on

        /* 8bb: Original code stores 16-bit PitchTable memory addresses here,
        ** but we store notes instead because we work with bigger pointer sizes.
        ** The outcome is the same.
        */
        *(uint16_t *)&hc->MiscEfxData[2] = 60 + (hc->J00 >> 4);   // 8bb: Tick 1 note
        *(uint16_t *)&hc->MiscEfxData[4] = 60 + (hc->J00 & 0x0F); // 8bb: Tick 2 note
    }
}

static void InitCommandK(hostChn_t *hc)
{
    if (hc->CmdVal > 0)
        hc->DKL = hc->CmdVal;

    InitNoCommand(hc);

    if (hc->Flags & HF_CHAN_ON)
    {
        InitVibrato(hc);
        InitCommandD7(hc, (slaveChn_t *)hc->SlaveChnPtr);

        hc->Flags |= HF_ALWAYS_UPDATE_EFX; // Always update.
    }
}

static void InitCommandL(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal > 0)
        hc->DKL = CmdVal;

    if (hc->Flags & HF_CHAN_ON)
    {
        InitCommandG11(hc);
        InitCommandD7(hc, (slaveChn_t *)hc->SlaveChnPtr);
    }
}

static void InitCommandM2(hostChn_t *hc, uint8_t vol)
{
    if (hc->Flags & HF_CHAN_ON)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
        sc->ChnVol     = vol;
        sc->Flags |= SF_RECALC_VOL;
    }

    hc->ChnVol = vol;
}

static void InitCommandM(hostChn_t *hc)
{
    InitNoCommand(hc);

    if (hc->CmdVal <= 0x40)
        InitCommandM2(hc, hc->CmdVal);
}

static void InitCommandN(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal > 0)
        hc->N00 = CmdVal;

    InitNoCommand(hc);

    uint8_t hi = hc->N00 & 0xF0;
    uint8_t lo = hc->N00 & 0x0F;

    if (lo == 0)
    {
        hc->MiscEfxData[0] = hi >> 4;
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (hi == 0)
    {
        hc->MiscEfxData[0] = -lo;
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (lo == 0x0F)
    {
        uint8_t vol = hc->ChnVol + (hi >> 4);
        if (vol > 64)
            vol = 64;

        InitCommandM2(hc, vol);
    }
    else if (hi == 0xF0)
    {
        uint8_t vol = hc->ChnVol - lo;
        if ((int8_t)vol < 0)
            vol = 0;

        InitCommandM2(hc, vol);
    }
}

static void InitCommandO(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->O00;

    hc->O00 = CmdVal;

    InitNoCommand(hc);

    if ((hc->NotePackMask & 0x33) && hc->TranslatedNote < 120 && (hc->Flags & HF_CHAN_ON))
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

        int32_t offset = ((hc->HighSmpOffs << 8) | hc->O00) << 8;
        if (offset >= sc->LoopEnd)
        {
            if (!(Song.Header.Flags & ITF_OLD_EFFECTS))
                return;

            offset = sc->LoopEnd - 1;
        }

        sc->SamplingPosition = offset;
        sc->Frac32           = 0; // 8bb: clear fractional sampling position
        sc->Frac64           = 0; // 8bb: also clear frac for my high-quality driver/mixer
    }
}

static void InitCommandP(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal > 0)
        hc->P00 = CmdVal;

    InitNoCommand(hc);

    uint8_t pan = hc->ChnPan;
    if (hc->Flags & HF_CHAN_ON)
        pan = ((slaveChn_t *)hc->SlaveChnPtr)->PanSet;

    if (pan == PAN_SURROUND) // Surround??
        return;

    uint8_t hi = hc->P00 & 0xF0;
    uint8_t lo = hc->P00 & 0x0F;

    if (lo == 0)
    {
        hc->MiscEfxData[0] = -(hi >> 4);
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (hi == 0)
    {
        hc->MiscEfxData[0] = lo;
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (lo == 0x0F)
    {
        pan -= hi >> 4;
        if ((int8_t)pan < 0)
            pan = 0;

        InitCommandX2(hc, pan);
    }
    else if (hi == 0xF0)
    {
        pan += lo;
        if (pan > 64)
            pan = 64;

        InitCommandX2(hc, pan);
    }
}

static void InitCommandQ(hostChn_t *hc)
{
    InitNoCommand(hc);

    if (hc->CmdVal > 0)
        hc->Q00 = hc->CmdVal;

    if (!(hc->Flags & HF_CHAN_ON))
        return;

    hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

    if (hc->NotePackMask & 0x11)
        hc->RetrigCount = hc->Q00 & 0x0F;
    else
        CommandQ(hc);
}

static void InitTremelo(hostChn_t *hc)
{
    if (Song.Header.Flags & ITF_OLD_EFFECTS)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

        sc->Flags |= SF_RECALC_FINALVOL; // Volume change...
        CommandR2(hc, sc, hc->LastTremoloData);
    }
    else
    {
        CommandR(hc);
    }
}

static void InitCommandR(hostChn_t *hc)
{
    uint8_t speed = hc->CmdVal >> 4;
    uint8_t depth = hc->CmdVal & 0x0F;

    if (speed > 0)
        hc->TremoloSpeed = speed << 2;

    if (depth > 0)
        hc->TremoloDepth = depth << 1;

    InitNoCommand(hc);

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
        InitTremelo(hc);
    }
}

static void InitCommandS(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->S00;

    hc->S00 = CmdVal;

    uint8_t cmd = CmdVal & 0xF0;
    uint8_t val = CmdVal & 0x0F;

    hc->MiscEfxData[0] = cmd;
    hc->MiscEfxData[1] = val;

    switch (cmd)
    {
    default:
    case 0x00:
    case 0x10:
    case 0x20:
        InitNoCommand(hc);
        break;

    case 0x30: // set vibrato waveform
    {
        if (val <= 3)
            hc->VibratoWaveform = val;

        InitNoCommand(hc);
    }
    break;

    case 0x40: // set tremelo waveform
    {
        if (val <= 3)
            hc->TremoloWaveform = val;

        InitNoCommand(hc);
    }
    break;

    case 0x50: // set panbrello waveform
    {
        if (val <= 3)
        {
            hc->PanbrelloWaveform = val;
            hc->PanbrelloPos      = 0;
        }

        InitNoCommand(hc);
    }
    break;

    case 0x60: // extra delay of x frames
    {
        Song.CurrentTick += val;
        Song.ProcessTick += val;
        InitNoCommand(hc);
    }
    break;

    case 0x70: // instrument functions
    {
        switch (val)
        {
        default:
        case 0xD:
        case 0xE:
        case 0xF:
            InitNoCommand(hc);
            break;

        case 0x0: // Past note cut
        {
            InitNoCommand(hc);

            const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

            slaveChn_t *sc = sChn;
            for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
            {
                if (sc->HostChnNum == targetHostChnNum)
                {
                    if (Driver.Flags & DF_USES_VOLRAMP)
                        sc->Flags |= SF_NOTE_STOP;
                    else
                        sc->Flags = SF_NOTE_STOP;
                }
            }
        }
        break;

        case 0x1: // Past note off
        {
            InitNoCommand(hc);

            const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

            slaveChn_t *sc = sChn;
            for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
            {
                if (sc->HostChnNum == targetHostChnNum)
                    sc->Flags |= SF_NOTE_OFF;
            }
        }
        break;

        case 0x2: // Past note fade
        {
            InitNoCommand(hc);

            const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

            slaveChn_t *sc = sChn;
            for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
            {
                if (sc->HostChnNum == targetHostChnNum)
                    sc->Flags |= SF_FADEOUT;
            }
        }
        break;

        case 0x3: // Set NNA to cut
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->NNA = 0;
        }
        break;

        case 0x4: // Set NNA to continue
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->NNA = 1;
        }
        break;

        case 0x5: // Set NNA to off
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->NNA = 2;
        }
        break;

        case 0x6: // Set NNA to fade
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->NNA = 3;
        }
        break;

        case 0x7: // Set volume envelope off
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_VOLENV_ON;
        }
        break;

        case 0x8: // Set volume envelope on
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_VOLENV_ON;
        }
        break;

        case 0x9: // Set panning envelope off
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_PANENV_ON;
        }
        break;

        case 0xA: // Set panning envelope on
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_PANENV_ON;
        }
        break;

        case 0xB: // Set pitch envelope off
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_PITCHENV_ON;
        }
        break;

        case 0xC: // Set pitch envelope on
        {
            InitNoCommand(hc);
            if (hc->Flags & HF_CHAN_ON)
                ((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_PITCHENV_ON;
        }
        break;
        }
    }
    break;

    case 0x80: // set pan
    {
        uint8_t pan = (((val << 4) | val) + 2) >> 2;
        InitNoCommand(hc);
        InitCommandX2(hc, pan);
    }
    break;

    case 0x90: // set surround
    {
        InitNoCommand(hc);
        if (val == 1)
            InitCommandX2(hc, PAN_SURROUND);
    }
    break;

    case 0xA0: // Set high order offset
    {
        hc->HighSmpOffs = val;
        InitNoCommand(hc);
    }
    break;

    case 0xB0: // loop control (8bb: pattern loop)
    {
        InitNoCommand(hc);

        if (val == 0)
        {
            hc->PattLoopStartRow = (uint8_t)Song.CurrentRow;
        }
        else if (hc->PattLoopCount == 0)
        {
            hc->PattLoopCount   = val;
            Song.ProcessRow     = hc->PattLoopStartRow - 1;
            Song.PatternLooping = true;
        }
        else if (--hc->PattLoopCount != 0)
        {
            Song.ProcessRow     = hc->PattLoopStartRow - 1;
            Song.PatternLooping = true;
        }
        else
        {
            hc->PattLoopStartRow = (uint8_t)Song.CurrentRow + 1;
        }
    }
    break;

    case 0xC0: // note cut
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
        InitNoCommand(hc);
    }
    break;

    case 0xD0: // note delay
    {
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    break;

    case 0xE0: // pattern delay
    {
        if (!Song.RowDelayOn)
        {
            Song.RowDelay   = val + 1;
            Song.RowDelayOn = true;
        }

        InitNoCommand(hc);
    }
    break;

    case 0xF0: // MIDI Macro select
    {
        hc->SFx = val;
        InitNoCommand(hc);
    }
    break;
    }
}

static void InitCommandT(hostChn_t *hc)
{
    uint8_t CmdVal = hc->CmdVal;
    if (CmdVal == 0)
        CmdVal = hc->T00;

    hc->T00 = CmdVal;

    if (CmdVal >= 0x20)
    {
        Song.Tempo = CmdVal;
        Music_InitTempo();
        InitNoCommand(hc);
    }
    else
    {
        InitNoCommand(hc);
        hc->Flags |= HF_ALWAYS_UPDATE_EFX; // Update mode
    }
}

static void InitCommandU(hostChn_t *hc)
{
    if (hc->NotePackMask & 0x11)
        hc->VibratoPos = hc->LastVibratoData = 0;

    uint8_t speed = (hc->CmdVal >> 4) << 2;
    uint8_t depth = hc->CmdVal & 0x0F;

    if (speed > 0)
        hc->VibratoSpeed = speed;

    if (depth > 0)
    {
        if (Song.Header.Flags & ITF_OLD_EFFECTS)
            depth <<= 1;

        hc->VibratoDepth = depth;
    }

    InitNoCommand(hc);

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
        InitVibrato(hc);
    }
}

static void InitCommandV(hostChn_t *hc)
{
    if (hc->CmdVal <= 0x80)
    {
        Song.GlobalVolume = hc->CmdVal;
        RecalculateAllVolumes();
    }

    InitNoCommand(hc);
}

static void InitCommandW(hostChn_t *hc)
{
    InitNoCommand(hc);

    if (hc->CmdVal > 0)
        hc->W00 = hc->CmdVal;

    if (hc->W00 == 0)
        return;

    uint8_t hi = hc->W00 & 0xF0;
    uint8_t lo = hc->W00 & 0x0F;

    if (lo == 0)
    {
        hc->MiscEfxData[0] = hi >> 4;
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (hi == 0)
    {
        hc->MiscEfxData[0] = -lo;
        hc->Flags |= HF_ALWAYS_UPDATE_EFX;
    }
    else if (lo == 0x0F)
    {
        uint16_t vol = Song.GlobalVolume + (hi >> 4);
        if (vol > 128)
            vol = 128;

        Song.GlobalVolume = vol;
        RecalculateAllVolumes();
    }
    else if (hi == 0xF0)
    {
        uint16_t vol = Song.GlobalVolume - lo;
        if ((int16_t)vol < 0)
            vol = 0;

        Song.GlobalVolume = vol;
        RecalculateAllVolumes();
    }
}

static void InitCommandX2(hostChn_t *hc, uint8_t pan) // 8bb: pan = 0..63
{
    if (hc->Flags & HF_CHAN_ON)
    {
        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
        sc->Pan = sc->PanSet = pan;
        sc->Flags |= (SF_RECALC_PAN | SF_RECALC_FINALVOL);
    }

    hc->ChnPan = pan;
}

static void InitCommandX(hostChn_t *hc)
{
    InitNoCommand(hc);

    uint8_t pan = (hc->CmdVal + 2) >> 2; // 8bb: 0..255 -> 0..63 (rounded)
    InitCommandX2(hc, pan);
}

static void InitCommandY(hostChn_t *hc)
{
    uint8_t speed = hc->CmdVal >> 4;
    uint8_t depth = hc->CmdVal & 0x0F;

    if (speed > 0)
        hc->PanbrelloSpeed = speed;

    if (depth > 0)
        hc->PanbrelloDepth = depth << 1;

    InitNoCommand(hc);

    if (hc->Flags & HF_CHAN_ON)
    {
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
        CommandY(hc);
    }
}

static void InitCommandZ(hostChn_t *hc) // Macros start at 120h, 320h
{
    InitNoCommand(hc);

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

    if (hc->CmdVal >= 0x80) // Macros!
        MIDITranslate(hc, sc, 0x320 + ((hc->CmdVal & 0x7F) << 5));
    else
        MIDITranslate(hc, sc, 0x120 + ((hc->SFx & 0xF) << 5));
}

static void CommandD(hostChn_t *hc)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

    uint8_t vol = sc->VolSet + hc->VolSlideDelta;
    if ((int8_t)vol < 0)
    {
        hc->Flags &= ~HF_UPDATE_EFX_IF_CHAN_ON;
        vol = 0;
    }
    else if (vol > 64)
    {
        hc->Flags &= ~HF_UPDATE_EFX_IF_CHAN_ON;
        vol = 64;
    }

    CommandD2(hc, sc, vol);
}

static void CommandE(hostChn_t *hc)
{
    CommandEChain(hc, *(uint16_t *)&hc->MiscEfxData[0]);
}

static void CommandF(hostChn_t *hc)
{
    CommandFChain(hc, *(uint16_t *)&hc->MiscEfxData[0]);
}

static void CommandG(hostChn_t *hc)
{
    if (!(hc->Flags & HF_PITCH_SLIDE_ONGOING))
        return;

    uint16_t    SlideValue = *(uint16_t *)&hc->MiscEfxData[0];
    slaveChn_t *sc         = (slaveChn_t *)hc->SlaveChnPtr;

    if (hc->MiscEfxData[2] == 1) // 8bb: slide direction
    {
        // Slide up!

        PitchSlideUp(hc, sc, SlideValue);

        /* Check that:
        **  1) Channel is on
        **  2) Frequency (set) is below porta to frequency
        */

        if (!(sc->Flags & SF_NOTE_STOP) && sc->Frequency < hc->PortaFreq)
        {
            sc->FrequencySet = sc->Frequency;
        }
        else
        {
            sc->Flags &= ~SF_NOTE_STOP;
            hc->Flags |= HF_CHAN_ON; // Turn on.

            sc->Frequency = sc->FrequencySet = hc->PortaFreq;
            hc->Flags &=
                ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_PITCH_SLIDE_ONGOING); // Turn off calling
        }
    }
    else
    {
        // Slide down

        PitchSlideDown(hc, sc, SlideValue);

        // Check that frequency is above porta to frequency.
        if (sc->Frequency > hc->PortaFreq)
        {
            sc->FrequencySet = sc->Frequency;
        }
        else
        {
            sc->Frequency = sc->FrequencySet = hc->PortaFreq;
            hc->Flags &=
                ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_PITCH_SLIDE_ONGOING); // Turn off calling
        }
    }
}

static void CommandH5(hostChn_t *hc, slaveChn_t *sc, int8_t VibratoData)
{
    VibratoData = (((VibratoData * (int8_t)hc->VibratoDepth) << 2) + 128) >> 8;
    if (Song.Header.Flags & ITF_OLD_EFFECTS)
        VibratoData = -VibratoData;

    if (VibratoData < 0)
        PitchSlideDown(hc, sc, -VibratoData);
    else
        PitchSlideUp(hc, sc, VibratoData);
}

static void CommandH(hostChn_t *hc)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    sc->Flags |= SF_FREQ_CHANGE;

    hc->VibratoPos += hc->VibratoSpeed;

    int8_t VibratoData;
    if (hc->VibratoWaveform == 3)
        VibratoData = (Random() & 127) - 64;
    else
        VibratoData = FineSineData[(hc->VibratoWaveform << 8) + hc->VibratoPos];

    hc->LastVibratoData = VibratoData; // Save last vibrato.
    CommandH5(hc, sc, VibratoData);
}

static void CommandI(hostChn_t *hc)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    sc->Flags |= SF_RECALC_VOL;

    hc->TremorCount--;
    if ((int8_t)hc->TremorCount <= 0)
    {
        hc->TremorOnOff ^= 1;
        hc->TremorCount = hc->MiscEfxData[hc->TremorOnOff];
    }

    if (hc->TremorOnOff != 1)
        sc->Vol = 0;
}

static void CommandJ(hostChn_t *hc)
{
    slaveChn_t *sc   = (slaveChn_t *)hc->SlaveChnPtr;
    uint16_t    tick = *(uint16_t *)&hc->MiscEfxData[0];

    sc->Flags |= SF_FREQ_CHANGE;

    // 8bb: used as an index to a 16-bit LUT (hence increments of 2)
    tick += 2;
    if (tick >= 6)
    {
        *(uint16_t *)&hc->MiscEfxData[0] = 0;
        return;
    }

    *(uint16_t *)&hc->MiscEfxData[0] = tick;

    const uint16_t arpNote = *(uint16_t *)&hc->MiscEfxData[tick];

    uint64_t freq = (uint64_t)sc->Frequency * (uint32_t)PitchTable[arpNote];
    if (freq & 0xFFFF000000000000) // 8bb: arp freq overflow
        sc->Frequency = 0;
    else
        sc->Frequency = (uint32_t)(freq >> 16);
}

static void CommandK(hostChn_t *hc)
{
    CommandH(hc);
    CommandD(hc);
}

static void CommandL(hostChn_t *hc)
{
    if (hc->Flags & HF_PITCH_SLIDE_ONGOING)
    {
        CommandG(hc);
        hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
    }

    CommandD(hc);
}

static void CommandN(hostChn_t *hc)
{
    uint8_t vol = hc->ChnVol + (int8_t)hc->MiscEfxData[0];

    if ((int8_t)vol < 0)
        vol = 0;
    else if (vol > 64)
        vol = 64;

    InitCommandM2(hc, vol);
}

static void CommandP(hostChn_t *hc)
{
    uint8_t pan = hc->ChnPan;
    if (hc->Flags & HF_CHAN_ON)
        pan = ((slaveChn_t *)hc->SlaveChnPtr)->PanSet;

    pan += hc->MiscEfxData[0];

    if ((int8_t)pan < 0)
        pan = 0;
    else if (pan > 64)
        pan = 64;

    InitCommandX2(hc, pan);
}

static void CommandQ(hostChn_t *hc)
{
    hc->RetrigCount--;
    if ((int8_t)hc->RetrigCount > 0)
        return;

    // OK... reset counter.
    hc->RetrigCount = hc->Q00 & 0x0F;

    // retrig count done.

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    if (Driver.Flags & DF_USES_VOLRAMP)
    {
        if (Song.Header.Flags & ITF_INSTR_MODE)
        {
            slaveChn_t *scTmp = sChn;
            for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, scTmp++)
            {
                if (!(scTmp->Flags & SF_CHAN_ON))
                {
                    memcpy(scTmp, sc, sizeof(slaveChn_t));
                    sc->Flags |= SF_NOTE_STOP; // Cut
                    sc->HostChnNum |= CHN_DISOWNED;

                    sc              = scTmp;
                    hc->SlaveChnPtr = scTmp;
                    break;
                }
            }
        }
        else // 8bb: samples-only mode
        {
            slaveChn_t *scTmp = sc + MAX_HOST_CHANNELS;
            memcpy(scTmp, sc, sizeof(slaveChn_t));
            scTmp->Flags |= SF_NOTE_STOP; // Cut
            scTmp->HostChnNum |= CHN_DISOWNED;
        }
    }

    sc->Frac32           = 0; // 8bb: clear sampling position fraction
    sc->Frac64           = 0; // 8bb: also clear frac for my high-quality driver/mixer
    sc->SamplingPosition = 0;

    sc->Flags |= (SF_RECALC_FINALVOL | SF_NEW_NOTE | SF_LOOP_CHANGED);

    uint8_t vol = sc->VolSet;
    switch (hc->Q00 >> 4)
    {
    default:
    case 0x0:
        return;
    case 0x1:
        vol -= 1;
        break;
    case 0x2:
        vol -= 2;
        break;
    case 0x3:
        vol -= 4;
        break;
    case 0x4:
        vol -= 8;
        break;
    case 0x5:
        vol -= 16;
        break;
    case 0x6:
        vol = (vol << 1) / 3;
        break;
    case 0x7:
        vol >>= 1;
        break;
    case 0x8:
        return;
    case 0x9:
        vol += 1;
        break;
    case 0xA:
        vol += 2;
        break;
    case 0xB:
        vol += 4;
        break;
    case 0xC:
        vol += 8;
        break;
    case 0xD:
        vol += 16;
        break;
    case 0xE:
        vol = (vol * 3) >> 1;
        break;
    case 0xF:
        vol <<= 1;
        break;
    }

    if ((int8_t)vol < 0)
        vol = 0;
    else if (vol > 64)
        vol = 64;

    sc->VolSet = sc->Vol = hc->VolSet = vol;
    sc->Flags |= SF_RECALC_VOL;

    if (hc->Smp == 101) // MIDI sample
        MIDITranslate(hc, sc, MIDICOMMAND_STOPNOTE);
}

static void CommandR2(hostChn_t *hc, slaveChn_t *sc, int8_t TremoloData)
{
    TremoloData = (((TremoloData * (int8_t)hc->TremoloDepth) << 2) + 128) >> 8;

    int16_t vol = sc->Vol + TremoloData;
    if (vol < 0)
        vol = 0;
    else if (vol > 64)
        vol = 64;

    sc->Vol = (uint8_t)vol;
}

static void CommandR(hostChn_t *hc)
{
    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
    sc->Flags |= SF_RECALC_VOL;

    hc->TremoloPos += hc->TremoloSpeed;

    int8_t TremoloData;
    if (hc->TremoloWaveform == 3)
        TremoloData = (Random() & 127) - 64;
    else
        TremoloData = FineSineData[(hc->TremoloWaveform << 8) + hc->TremoloPos];

    hc->LastTremoloData = TremoloData; // Save last tremelo
    CommandR2(hc, sc, TremoloData);
}

static void CommandS(hostChn_t *hc)
{
    // Have to handle SDx, SCx

    const uint8_t SCmd = hc->MiscEfxData[0];
    if (SCmd == 0xD0) // 8bb: Note delay
    {
        hc->MiscEfxData[1]--;
        if ((int8_t)hc->MiscEfxData[1] > 0)
            return;

        hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX);
        InitNoCommand(hc);
        hc->Flags |= HF_ROW_UPDATED;

        bool ChannelMuted = !!(Song.Header.ChnlPan[hc->HostChnNum] & 128);
        if (ChannelMuted && !(hc->Flags & HF_FREEPLAY_NOTE) && (hc->Flags & HF_CHAN_ON))
            ((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_CHN_MUTED;
    }
    else if (SCmd == 0xC0) // Note cut.
    {
        if (!(hc->Flags & HF_CHAN_ON))
            return;

        hc->MiscEfxData[1]--;
        if ((int8_t)hc->MiscEfxData[1] > 0)
            return;

        slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

        hc->Flags &= ~HF_CHAN_ON;

        if (sc->Smp == 100 || (Driver.Flags & DF_USES_VOLRAMP))
            sc->Flags |= SF_NOTE_STOP;
        else
            sc->Flags = SF_NOTE_STOP;
    }
}

static void CommandT(hostChn_t *hc)
{
    int16_t Tempo = Song.Tempo;

    if (hc->T00 & 0xF0)
    {
        // Slide Up
        Tempo += hc->T00 - 16;
        if (Tempo > 255)
            Tempo = 255;
    }
    else
    {
        // Slide Down
        Tempo -= hc->T00;
        if (Tempo < 32)
            Tempo = 32;
    }

    Song.Tempo = Tempo;
    SB16_SetTempo((uint8_t)Tempo);
}

static void CommandW(hostChn_t *hc)
{
    uint16_t vol = Song.GlobalVolume + (int8_t)hc->MiscEfxData[0];

    if ((int16_t)vol < 0)
        vol = 0;
    else if (vol > 128)
        vol = 128;

    Song.GlobalVolume = vol;
    RecalculateAllVolumes();
}

static void CommandY(hostChn_t *hc)
{
    if (!(hc->Flags & HF_CHAN_ON))
        return;

    slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

    int8_t panData;
    if (hc->PanbrelloWaveform >= 3) // 8bb: panbrello waveform
    {
        // Random panning make speed the delay time.

        hc->PanbrelloPos--;
        if ((int8_t)hc->PanbrelloPos <= 0)
        {
            hc->PanbrelloPos      = hc->PanbrelloSpeed; // reset countdown.
            hc->LastPanbrelloData = panData = (Random() & 127) - 64;
        }
        else
        {
            panData = hc->LastPanbrelloData;
        }
    }
    else
    {
        hc->PanbrelloPos += hc->PanbrelloSpeed;
        panData = FineSineData[(hc->PanbrelloWaveform << 8) + hc->PanbrelloPos];
    }

    if (sc->PanSet != PAN_SURROUND)
    {
        panData = (((panData * (int8_t)hc->PanbrelloDepth) << 2) + 128) >> 8;
        panData += sc->PanSet;

        if (panData < 0)
            panData = 0;
        else if (panData > 64)
            panData = 64;

        sc->Flags |= SF_RECALC_PAN;
        sc->Pan = panData;
    }
}

static void RecalculateAllVolumes(void)
{
    slaveChn_t *sc = sChn;
    for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
        sc->Flags |= (SF_RECALC_PAN | SF_RECALC_VOL);
}

static void Music_SetDefaultMIDIDataArea(void) // 8bb: added this
{
    // fill default MIDI configuration values (important for filters)

    memset(MIDIDataArea, 0, (9 + 16 + 128) * 32); // data is padded with zeroes, not spaces!

    // MIDI commands
    memcpy(&MIDIDataArea[0 * 32], "FF", 2);
    memcpy(&MIDIDataArea[1 * 32], "FC", 2);
    memcpy(&MIDIDataArea[3 * 32], "9c n v", 6);
    memcpy(&MIDIDataArea[4 * 32], "9c n 0", 6);
    memcpy(&MIDIDataArea[8 * 32], "Cc p", 4);

    // macro setup (SF0)
    memcpy(&MIDIDataArea[9 * 32], "F0F000z", 7);

    // macro setup (Z80..Z8F)
    memcpy(&MIDIDataArea[25 * 32], "F0F00100", 8);
    memcpy(&MIDIDataArea[26 * 32], "F0F00108", 8);
    memcpy(&MIDIDataArea[27 * 32], "F0F00110", 8);
    memcpy(&MIDIDataArea[28 * 32], "F0F00118", 8);
    memcpy(&MIDIDataArea[29 * 32], "F0F00120", 8);
    memcpy(&MIDIDataArea[30 * 32], "F0F00128", 8);
    memcpy(&MIDIDataArea[31 * 32], "F0F00130", 8);
    memcpy(&MIDIDataArea[32 * 32], "F0F00138", 8);
    memcpy(&MIDIDataArea[33 * 32], "F0F00140", 8);
    memcpy(&MIDIDataArea[34 * 32], "F0F00148", 8);
    memcpy(&MIDIDataArea[35 * 32], "F0F00150", 8);
    memcpy(&MIDIDataArea[36 * 32], "F0F00158", 8);
    memcpy(&MIDIDataArea[37 * 32], "F0F00160", 8);
    memcpy(&MIDIDataArea[38 * 32], "F0F00168", 8);
    memcpy(&MIDIDataArea[39 * 32], "F0F00170", 8);
    memcpy(&MIDIDataArea[40 * 32], "F0F00178", 8);
}

static char *Music_GetMIDIDataArea(void)
{
    return (char *)MIDIDataArea;
}

static void MIDISendFilter(hostChn_t *hc, slaveChn_t *sc, uint8_t Data)
{
    if (!(Driver.Flags & DF_SUPPORTS_MIDI))
        return;

    if (Data >= 0x80 && Data < 0xF0)
    {
        if (Data == Song.LastMIDIByte)
            return;

        Song.LastMIDIByte = Data;
    }

    /* 8bb: We implement the SendUARTOut() code found in the
    ** SB16 MMX driver and WAV writer driver and use it here
    ** instead of doing real MIDI data handling.
    **
    ** It will only interpret filter commands (set and clear).
    */
    if (MIDIInterpretState < 2)
    {
        if (Data == 0xF0)
        {
            MIDIInterpretState++;
        }
        else
        {
            if (Data == 0xFA || Data == 0xFC || Data == 0xFF)
            {
                // 8bb: reset filters
                for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
                {
                    Driver.FilterParameters[i]      = 127; // 8bb: Cutoff
                    Driver.FilterParameters[64 + i] = 0;   // 8bb: Q
                }
            }

            MIDIInterpretState = 0;
        }
    }
    else if (MIDIInterpretState == 2)
    {
        if (Data < 2) // 8bb: must be 0..1 (Cutoff or Q)
        {
            MIDIInterpretType = Data;
            MIDIInterpretState++;
        }
        else
        {
            MIDIInterpretState = 0;
        }
    }
    else if (MIDIInterpretState == 3)
    {
        // Have InterpretType, now get parameter, then return to normal.

        if (Data <= 0x7F)
        {
            bool IsFilterQ = (MIDIInterpretType == 1);
            if (IsFilterQ)
                Driver.FilterParameters[(64 + hc->HostChnNum) & 127] = Data;
            else
                Driver.FilterParameters[hc->HostChnNum & 127] = Data;

            if (sc != NULL)
                sc->Flags |= SF_RECALC_FINALVOL;
        }

        MIDIInterpretState = 0;
    }
}

static void SetFilterCutoff(hostChn_t *hc, slaveChn_t *sc, uint8_t value) // Assumes that channel is non-disowned
{
    MIDISendFilter(hc, sc, 0xF0);
    MIDISendFilter(hc, sc, 0xF0);
    MIDISendFilter(hc, sc, 0x00);
    MIDISendFilter(hc, sc, value);
}

static void SetFilterResonance(hostChn_t *hc, slaveChn_t *sc, uint8_t value) // Assumes that channel is non-disowned
{
    MIDISendFilter(hc, sc, 0xF0);
    MIDISendFilter(hc, sc, 0xF0);
    MIDISendFilter(hc, sc, 0x01);
    MIDISendFilter(hc, sc, value);
}

static void MIDITranslate(hostChn_t *hc, slaveChn_t *sc, uint16_t Input)
{
    if (!(Driver.Flags & DF_SUPPORTS_MIDI))
        return;

    if (Input >= 0xF000)
        return;                     // 8bb: we don't support (nor need) MIDI commands

    if (Input / 32 >= 9 + 16 + 128) // 8bb: added protection, just in case
        return;

    uint8_t MIDIData    = 0;
    uint8_t CharsParsed = 0;

    while (true)
    {
        int16_t Byte = MIDIDataArea[Input++];

        if (Byte == 0)
        {
            if (CharsParsed > 0)
                MIDISendFilter(hc, sc, MIDIData);

            break; // 8bb: and we're done!
        }

        if (Byte == ' ')
        {
            if (CharsParsed > 0)
                MIDISendFilter(hc, sc, MIDIData);

            continue;
        }

        // Interpretation time.

        Byte -= '0';
        if (Byte < 0)
            continue;

        if (Byte <= 9)
        {
            MIDIData = (MIDIData << 4) | (uint8_t)Byte;
            CharsParsed++;

            if (CharsParsed >= 2)
            {
                MIDISendFilter(hc, sc, MIDIData);
                CharsParsed = 0;
                MIDIData    = 0;
            }

            continue;
        }

        Byte -= 'A' - '0';
        if (Byte < 0)
            continue;

        if (Byte <= 'F' - 'A')
        {
            MIDIData = (MIDIData << 4) | (uint8_t)(Byte + 10);
            CharsParsed++;

            if (CharsParsed >= 2)
            {
                MIDISendFilter(hc, sc, MIDIData);
                CharsParsed = 0;
                MIDIData    = 0;
            }

            continue;
        }

        Byte -= 'a' - 'A';
        if (Byte < 0)
            continue;

        if (Byte > 'z' - 'a')
            continue;

        if (Byte == 'c' - 'a')
        {
            if (sc == NULL)
                continue;

            MIDIData = (MIDIData << 4) | (sc->MIDIChn - 1);
            CharsParsed++;

            if (CharsParsed >= 2)
            {
                MIDISendFilter(hc, sc, MIDIData);
                CharsParsed = 0;
                MIDIData    = 0;
            }

            continue;
        }

        if (CharsParsed > 0)
        {
            MIDISendFilter(hc, sc, MIDIData);
            MIDIData = 0;
        }

        if (Byte == 'z' - 'a') // Zxx?
        {
            MIDISendFilter(hc, sc, hc->CmdVal);
        }
        else if (Byte == 'o' - 'a') // 8bb: sample offset?
        {
            MIDISendFilter(hc, sc, hc->O00);
        }
        else if (sc != NULL)
        {
            if (Byte == 'n' - 'a') // Note?
            {
                MIDISendFilter(hc, sc, sc->Note);
            }
            else if (Byte == 'm' - 'a') // 8bb: MIDI note (sample loop direction on sample channels)
            {
                MIDISendFilter(hc, sc, sc->LoopDirection);
            }
            else if (Byte == 'v' - 'a') // Velocity?
            {
                if (sc->Flags & SF_CHN_MUTED)
                {
                    MIDISendFilter(hc, sc, 0);
                }
                else
                {
                    uint16_t volume = (sc->VolSet * Song.GlobalVolume * sc->ChnVol) >> 4;
                    volume          = (volume * sc->SmpVol) >> 15;

                    if (volume == 0)
                        volume = 1;
                    else if (volume >= 128)
                        volume = 127;

                    MIDISendFilter(hc, sc, (uint8_t)volume);
                }
            }
            else if (Byte == 'u' - 'a') // Volume?
            {
                if (sc->Flags & SF_CHN_MUTED)
                {
                    MIDISendFilter(hc, sc, 0);
                }
                else
                {
                    uint16_t volume = sc->FinalVol7Bit;

                    if (volume == 0)
                        volume = 1;
                    else if (volume >= 128)
                        volume = 127;

                    MIDISendFilter(hc, sc, (uint8_t)volume);
                }
            }
            else if (Byte == 'h' - 'a') // HCN (8bb: host channel number)
            {
                MIDISendFilter(hc, sc, sc->HostChnNum & 0x7F);
            }
            else if (Byte == 'x' - 'a')       // Pan set
            {
                uint16_t value = sc->Pan * 2; // 8bb: yes sc->Pan, not sc->PS
                if (value >= 128)
                    value--;

                if (value >= 128)
                    value = 64;

                MIDISendFilter(hc, sc, (uint8_t)value);
            }
            else if (Byte == 'p' - 'a') // Program?
            {
                MIDISendFilter(hc, sc, sc->MIDIProg);
            }
            else if (Byte == 'b' - 'a') // 8bb: MIDI bank low
            {
                MIDISendFilter(hc, sc, sc->MIDIBank & 0xFF);
            }
            else if (Byte == 'a' - 'a') // 8bb: MIDI bank high
            {
                MIDISendFilter(hc, sc, sc->MIDIBank >> 8);
            }
        }

        MIDIData    = 0;
        CharsParsed = 0;
    }
}

static void InitPlayInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins)
{
    sc->InsPtr = ins;

    sc->NNA = ins->NNA;
    sc->DCT = ins->DCT;
    sc->DCA = ins->DCA;

    if (hc->MIDIChn != 0) // 8bb: MIDI?
    {
        sc->MIDIChn       = ins->MIDIChn;
        sc->MIDIProg      = ins->MIDIProg;
        sc->MIDIBank      = ins->MIDIBank;
        sc->LoopDirection = hc->RawNote; // 8bb: during MIDI, LpD = MIDI note
    }

    sc->ChnVol = hc->ChnVol;

    uint8_t pan = (ins->DefPan & 0x80) ? hc->ChnPan : ins->DefPan;
    if (hc->Smp != 0)
    {
        sample_t *s = &Song.Smp[hc->Smp - 1];
        if (s->DefPan & 0x80)
            pan = s->DefPan & 127;
    }

    if (pan != PAN_SURROUND)
    {
        int16_t newPan = pan + (((int8_t)(hc->RawNote - ins->PitchPanCenter) * (int8_t)ins->PitchPanSep) >> 3);

        if (newPan < 0)
            newPan = 0;
        else if (newPan > 64)
            newPan = 64;

        pan = (uint8_t)newPan;
    }

    sc->Pan = sc->PanSet = pan;

    // Envelope init
    sc->VolEnvState.Value = 64 << 16; // 8bb: clears fractional part
    sc->VolEnvState.Tick = sc->VolEnvState.NextTick = 0;
    sc->VolEnvState.CurNode                         = 0;

    sc->PanEnvState.Value = 0; // 8bb: clears fractional part
    sc->PanEnvState.Tick = sc->PanEnvState.NextTick = 0;
    sc->PanEnvState.CurNode                         = 0;

    sc->PitchEnvState.Value = 0; // 8bb: clears fractional part
    sc->PitchEnvState.Tick = sc->PitchEnvState.NextTick = 0;
    sc->PitchEnvState.CurNode                           = 0;

    sc->Flags = SF_CHAN_ON + SF_RECALC_PAN + SF_RECALC_VOL + SF_FREQ_CHANGE + SF_NEW_NOTE;

    if (ins->VolEnv.Flags & ENVF_ENABLED)
        sc->Flags |= SF_VOLENV_ON;
    if (ins->PanEnv.Flags & ENVF_ENABLED)
        sc->Flags |= SF_PANENV_ON;
    if (ins->PitchEnv.Flags & ENVF_ENABLED)
        sc->Flags |= SF_PITCHENV_ON;

    if (LastSlaveChannel != NULL)
    {
        slaveChn_t *lastSC = LastSlaveChannel;

        if ((ins->VolEnv.Flags & (ENVF_ENABLED | ENVF_CARRY)) == ENVF_ENABLED + ENVF_CARRY) // Transfer volume data
        {
            sc->VolEnvState.Value    = lastSC->VolEnvState.Value;
            sc->VolEnvState.Delta    = lastSC->VolEnvState.Delta;
            sc->VolEnvState.Tick     = lastSC->VolEnvState.Tick;
            sc->VolEnvState.CurNode  = lastSC->VolEnvState.CurNode;
            sc->VolEnvState.NextTick = lastSC->VolEnvState.NextTick;
        }

        if ((ins->PanEnv.Flags & (ENVF_ENABLED | ENVF_CARRY)) == ENVF_ENABLED + ENVF_CARRY) // Transfer pan data
        {
            sc->PanEnvState.Value    = lastSC->PanEnvState.Value;
            sc->PanEnvState.Delta    = lastSC->PanEnvState.Delta;
            sc->PanEnvState.Tick     = lastSC->PanEnvState.Tick;
            sc->PanEnvState.CurNode  = lastSC->PanEnvState.CurNode;
            sc->PanEnvState.NextTick = lastSC->PanEnvState.NextTick;
        }

        if ((ins->PitchEnv.Flags & (ENVF_ENABLED | ENVF_CARRY)) == ENVF_ENABLED + ENVF_CARRY) // Transfer pitch data
        {
            sc->PitchEnvState.Value    = lastSC->PitchEnvState.Value;
            sc->PitchEnvState.Delta    = lastSC->PitchEnvState.Delta;
            sc->PitchEnvState.Tick     = lastSC->PitchEnvState.Tick;
            sc->PitchEnvState.CurNode  = lastSC->PitchEnvState.CurNode;
            sc->PitchEnvState.NextTick = lastSC->PitchEnvState.NextTick;
        }
    }

    hc->Flags |= HF_APPLY_RANDOM_VOL; // Apply random volume/pan

    if (hc->MIDIChn == 0)
    {
        sc->MIDIBank = 0x00FF;        // 8bb: reset filter resonance (Q) & cutoff

        if (ins->FilterCutoff & 0x80) // If IFC bit 7 == 1, then set filter cutoff
        {
            uint8_t filterCutOff = ins->FilterCutoff & 0x7F;
            SetFilterCutoff(hc, sc, filterCutOff);
        }

        if (ins->FilterResonance & 0x80) // If IFR bit 7 == 1, then set filter resonance
        {
            const uint8_t filterQ = ins->FilterResonance & 0x7F;
            sc->MIDIBank          = (filterQ << 8) | (sc->MIDIBank & 0x00FF);
            SetFilterResonance(hc, sc, filterQ);
        }
    }
}

// 8bb: this function is used in AllocateChannel()
static slaveChn_t *AllocateChannelSample(hostChn_t *hc, uint8_t *hcFlags)
{
    // Sample handler

    slaveChn_t *sc = &sChn[hc->HostChnNum];
    if ((Driver.Flags & DF_USES_VOLRAMP) && (sc->Flags & SF_CHAN_ON))
    {
        // copy out channel
        sc->Flags |= SF_NOTE_STOP;
        sc->HostChnNum |= CHN_DISOWNED;
        memcpy(sc + MAX_HOST_CHANNELS, sc, sizeof(slaveChn_t));
    }

    hc->SlaveChnPtr = sc;
    sc->HostChnPtr  = hc;
    sc->HostChnNum  = hc->HostChnNum;

    sc->ChnVol = hc->ChnVol;
    sc->Pan = sc->PanSet  = hc->ChnPan;
    sc->FadeOut           = 1024;
    sc->VolEnvState.Value = (64 << 16) | (sc->VolEnvState.Value & 0xFFFF); // 8bb: keeps frac
    sc->MIDIBank          = 0x00FF;                                        // Filter cutoff
    sc->Note              = hc->RawNote;
    sc->Ins               = hc->Ins;

    sc->Flags = SF_CHAN_ON + SF_RECALC_PAN + SF_RECALC_VOL + SF_FREQ_CHANGE + SF_NEW_NOTE;

    if (hc->Smp > 0)
    {
        sc->Smp     = hc->Smp - 1;
        sample_t *s = sc->SmpPtr = &Song.Smp[sc->Smp];

        sc->SmpBitDepth      = 0;          // 8bb: 8-bit
        sc->AutoVibratoDepth = sc->AutoVibratoPos = 0;
        sc->PanEnvState.Value &= 0xFFFF;   // No pan deviation (8bb: keeps frac)
        sc->PitchEnvState.Value &= 0xFFFF; // No pitch deviation (8bb: keeps frac)
        sc->LoopDirection = DIR_FORWARDS;  // Reset loop dirn

        if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
        {
            sc->Flags = SF_NOTE_STOP;
            *hcFlags &= ~HF_CHAN_ON;
            return NULL;
        }

        sc->SmpBitDepth = s->Flags & SMPF_16BIT;
        sc->SmpVol      = s->GlobVol * 2;
        return sc;
    }
    else // No sample!
    {
        sc->Flags = SF_NOTE_STOP;
        *hcFlags &= ~HF_CHAN_ON;
        return NULL;
    }
}

// 8bb: this function is used in AllocateChannel()
static slaveChn_t *AllocateChannelInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins, uint8_t *hcFlags)
{
    assert(hc != NULL && sc != NULL && ins != NULL);

    hc->SlaveChnPtr = sc;
    sc->HostChnNum  = hc->HostChnNum;
    sc->HostChnPtr  = hc;

    sc->SmpBitDepth      = 0;                                 // 8bb: 8-bit
    sc->AutoVibratoDepth = sc->AutoVibratoPos = 0;
    sc->LoopDirection                         = DIR_FORWARDS; // Reset loop dirn

    InitPlayInstrument(hc, sc, ins);

    sc->SmpVol  = ins->GlobVol;
    sc->FadeOut = 1024;
    sc->Note    = (hc->Smp == 101) ? hc->TranslatedNote : hc->RawNote;
    sc->Ins     = hc->Ins;

    if (hc->Smp == 0)
    {
        // 8bb: shut down channel
        sc->Flags = SF_NOTE_STOP;
        *hcFlags &= ~HF_CHAN_ON;
        return NULL;
    }

    sc->Smp     = hc->Smp - 1;
    sample_t *s = sc->SmpPtr = &Song.Smp[sc->Smp];

    if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
    {
        // 8bb: shut down channel
        sc->Flags = SF_NOTE_STOP;
        *hcFlags &= ~HF_CHAN_ON;
        return NULL;
    }

    sc->SmpBitDepth = s->Flags & SMPF_16BIT;
    sc->SmpVol      = (s->GlobVol * sc->SmpVol) >> 6; // 0->128
    return sc;
}

// 8bb: this function is used in AllocateChannel()
static bool DuplicateCheck(slaveChn_t **scOut, hostChn_t *hc, uint8_t hostChnNum, instrument_t *ins, uint8_t DCT,
                           uint8_t DCVal)
{
    slaveChn_t *sc = AllocateSlaveOffset;
    for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
    {
        *scOut = sc; // 8bb: copy current slave channel pointer to scOut

        if (!(sc->Flags & SF_CHAN_ON) || (hc->Smp != 101 && sc->HostChnNum != hostChnNum) || sc->Ins != hc->Ins)
            continue;

        // 8bb: the actual duplicate test

        if (DCT == DCT_NOTE && sc->Note != DCVal)
            continue;

        if (DCT == DCT_SAMPLE && sc->Smp != DCVal)
            continue;

        if (DCT == DCT_INSTRUMENT && sc->Ins != DCVal)
            continue;

        if (hc->Smp == 101)                                  // New note is a MIDI?
        {
            if (sc->Smp == 100 && sc->MIDIChn == hostChnNum) // Is current channel a MIDI chan
            {
                sc->Flags |= SF_NOTE_STOP;
                if (!(sc->HostChnNum & CHN_DISOWNED))
                {
                    sc->HostChnNum |= CHN_DISOWNED;
                    ((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON;
                }
            }
        }
        else if (sc->DCA == ins->DCA)
        {
            return true; // 8bb: dupe found
        }
    }

    return false; // 8bb: dupe not found
}

// 8bb: are you sure you want to know? ;)
static slaveChn_t *AllocateChannel(hostChn_t *hc, uint8_t *hcFlags)
{
    LastSlaveChannel = NULL;

    if (!(Song.Header.Flags & ITF_INSTR_MODE) || hc->Ins == 255)
        return AllocateChannelSample(hc, hcFlags);

    // Instrument handler!

    if (hc->Ins == 0)
        return NULL;

    if (hc->Smp == 101 &&
        Driver.NumChannels < MAX_SLAVE_CHANNELS) // 8bb: MIDI and below 256 virtual channels in driver?
    {
        AllocateNumChannels = MAX_SLAVE_CHANNELS - Driver.NumChannels;
        AllocateSlaveOffset = &sChn[Driver.NumChannels];
    }
    else
    {
        AllocateNumChannels = Driver.NumChannels;
        AllocateSlaveOffset = sChn; // 8bb: points to first virtual channel
    }

    // 8bb: some of these are initialized only to prevent compiler warnings
    uint8_t     NNA = 0;
    slaveChn_t *sc  = NULL;
    uint8_t     hostChnNum, DCT, DCVal;

    instrument_t *ins = &Song.Ins[hc->Ins - 1];

    bool scInitialized = false;
    if ((*hcFlags) & HF_CHAN_ON) // 8bb: host channel on?
    {
        sc = (slaveChn_t *)hc->SlaveChnPtr;
        if (sc->InsPtr == ins)   // 8bb: slave channel has same inst. as host channel?
            LastSlaveChannel = sc;

        NNA = sc->NNA;
        if (NNA != NNA_NOTE_CUT)            // 8bb: not note-cut
            sc->HostChnNum |= CHN_DISOWNED; // Disown channel

        scInitialized = true;
    }

    while (true) // New note action handling...
    {
        bool skipMIDITest = false;
        if (scInitialized)
        {
            if (NNA != NNA_NOTE_CUT && sc->VolSet > 0 && sc->ChnVol > 0 && sc->SmpVol > 0)
            {
                if (NNA == NNA_NOTE_OFF)
                {
                    sc->Flags |= SF_NOTE_OFF;
                    GetLoopInformation(sc); // 8bb: update sample loop (sustain released)
                }
                else if (NNA >= NNA_NOTE_FADE)
                {
                    sc->Flags |= SF_FADEOUT;
                }
                // 8bb: else: NNA_CONTINUE
            }
            else
            {
                // 8bb: NNA=Note Cut (or volumes are zero)
                if (sc->Smp == 100)                 // MIDI?
                {
                    sc->Flags |= SF_NOTE_STOP;
                    sc->HostChnNum |= CHN_DISOWNED; // Disown channel

                    if (hc->Smp != 101)
                        break;                      // Sample.. (8bb: find available voice now)
                }
                else
                {
                    if (Driver.Flags & DF_USES_VOLRAMP)
                    {
                        sc->Flags |= SF_NOTE_STOP;
                        sc->HostChnNum |= CHN_DISOWNED; // Disown channel
                        break;                          // 8bb: find available voice now
                    }

                    sc->Flags = SF_NOTE_STOP;
                    if (ins->DCT == DCT_DISABLED)
                        return AllocateChannelInstrument(hc, sc, ins, hcFlags);

                    skipMIDITest = true;
                }
            }
        }

        hostChnNum = DCT = DCVal = 0; // 8bb: prevent stupid compiler warning...

        bool doDupeCheck = false;
        if (!skipMIDITest && hc->Smp == 101)
        {
            // 8bb: MIDI note, do a "duplicate note" check regardless of instrument's DCT setting
            hostChnNum = hc->MIDIChn;
            DCT        = DCT_NOTE;
            DCVal      = hc->TranslatedNote;

            doDupeCheck = true;
        }
        else if (ins->DCT != DCT_DISABLED)
        {
            hostChnNum = hc->HostChnNum | CHN_DISOWNED; // 8bb: only search disowned host channels
            DCT        = ins->DCT;

            if (ins->DCT == DCT_NOTE)
            {
                DCVal = hc->RawNote; // 8bb: not the translated note!
            }
            else if (ins->DCT == DCT_INSTRUMENT)
            {
                DCVal = hc->Ins;
            }
            else
            {
                /* 8bb:
                ** .ITs from OpenMPT can have DCT=4, which tests for duplicate instrument plugins.
                ** This will be handled as DCT_SAMPLE in Impulse Tracker. Oops...
                */
                DCVal = hc->Smp - 1;
                if ((int8_t)DCVal < 0)
                    break; // 8bb: illegal (or no) sample, ignore dupe test and find available voice now
            }

            doDupeCheck = true;
        }

        if (doDupeCheck) // 8bb: NNA Duplicate Check
        {
            sc = AllocateSlaveOffset;
            if (DuplicateCheck(&sc, hc, hostChnNum, ins, DCT, DCVal))
            {
                // 8bb: dupe found!

                scInitialized = true; // 8bb: we have an sc pointer now (we could come from a shutdown host channel)
                if (ins->DCA == DCA_NOTE_CUT)
                {
                    NNA = NNA_NOTE_CUT;
                }
                else
                {
                    sc->DCT = DCT_DISABLED; // 8bb: turn of dupe check so that we don't do infinite NNA tests :)
                    sc->DCA = DCA_NOTE_CUT;
                    NNA     = ins->DCA + 1;
                }

                continue; // 8bb: do another NNA test with the new NNA type
            }
        }

        break; // NNA handling done, find available voice now
    }

    // 8bb: search for inactive channels

    sc = AllocateSlaveOffset;
    if (hc->Smp != 101)
    {
        // 8bb: no MIDI
        for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
        {
            if (!(sc->Flags & SF_CHAN_ON))
                return AllocateChannelInstrument(hc, sc, ins, hcFlags);
        }
    }
    else
    {
        // MIDI 'slave channels' have to be maintained if still referenced
        for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
        {
            if (!(sc->Flags & SF_CHAN_ON))
            {
                // Have a channel.. check that it's host's slave isn't SI (8bb: SI = sc)
                hostChn_t *hcTmp = (hostChn_t *)sc->HostChnPtr;

                if (hcTmp == NULL || hcTmp->SlaveChnPtr != sc)
                    return AllocateChannelInstrument(hc, sc, ins, hcFlags);
            }
        }
    }

    // Common sample search

    memset(ChannelCountTable, 0, sizeof(ChannelCountTable));
    memset(ChannelVolumeTable, 255, sizeof(ChannelVolumeTable));
    memset(ChannelLocationTable, 0, sizeof(ChannelLocationTable));

    sc = AllocateSlaveOffset;
    for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
    {
        if (sc->Smp > 99) // Just for safety
            continue;

        ChannelCountTable[sc->Smp]++;
        if ((sc->HostChnNum & CHN_DISOWNED) && sc->FinalVol7Bit < ChannelVolumeTable[sc->Smp])
        {
            ChannelLocationTable[sc->Smp] = sc;
            ChannelVolumeTable[sc->Smp]   = sc->FinalVol7Bit;
        }
    }

    // OK.. now search table for maximum occurrence of sample...

    sc            = NULL;
    uint8_t count = 2; // Find maximum count, has to be greater than 2 channels
    for (int32_t i = 0; i < 100; i++)
    {
        if (count < ChannelCountTable[i])
        {
            count = ChannelCountTable[i];
            sc    = ChannelLocationTable[i];
        }
    }

    if (sc != NULL)
        return AllocateChannelInstrument(hc, sc, ins, hcFlags);

    /*
    ** Find out which host channel has the most (disowned) slave channels.
    ** Then find the softest non-single sample in that channel.
    */

    memset(ChannelCountTable, 0, MAX_HOST_CHANNELS);

    sc = AllocateSlaveOffset;
    for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
        ChannelCountTable[sc->HostChnNum & 63]++;

    // OK.. search through and find the most heavily used channel
    uint8_t lowestVol;
    while (true)
    {
        hostChnNum = 0;
        count      = 1;
        for (uint8_t i = 0; i < MAX_HOST_CHANNELS; i++)
        {
            if (count < ChannelCountTable[i])
            {
                count      = ChannelCountTable[i];
                hostChnNum = i;
            }
        }

        if (count <= 1)
        {
            // Now search for softest disowned sample (not non-single)

            sc                = NULL;
            slaveChn_t *scTmp = AllocateSlaveOffset;

            lowestVol = 255;
            for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
            {
                if ((scTmp->HostChnNum & CHN_DISOWNED) && scTmp->FinalVol7Bit <= lowestVol)
                {
                    sc        = scTmp;
                    lowestVol = scTmp->FinalVol7Bit;
                }
            }

            if (sc == NULL)
            {
                *hcFlags &= ~HF_CHAN_ON;
                return NULL;
            }

            return AllocateChannelInstrument(hc, sc, ins, hcFlags);
        }

        hostChnNum |= CHN_DISOWNED; // Search for disowned only
        sc = NULL;                  // Offset

        lowestVol         = 255;
        uint8_t targetSmp = hc->Smp - 1;

        slaveChn_t *scTmp = AllocateSlaveOffset;
        for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
        {
            if (scTmp->HostChnNum != hostChnNum || scTmp->FinalVol7Bit >= lowestVol)
                continue;

            // Now check if any other channel contains this sample

            if (scTmp->Smp == targetSmp)
            {
                sc        = scTmp;
                lowestVol = scTmp->FinalVol7Bit;
                continue;
            }

            slaveChn_t *scTmp2 = AllocateSlaveOffset;

            uint8_t scSmp = scTmp->Smp;
            scTmp->Smp    = 255;
            for (uint32_t j = 0; j < AllocateNumChannels; j++, scTmp2++)
            {
                if (scTmp2->Smp == targetSmp || scTmp2->Smp == scSmp)
                {
                    // OK found a second sample.
                    sc        = scTmp;
                    lowestVol = scTmp->FinalVol7Bit;
                    break;
                }
            }
            scTmp->Smp = scSmp;
        }

        if (sc != NULL)
            break;                              // 8bb: done

        ChannelCountTable[hostChnNum & 63] = 0; // Next cycle...
    }

    // 8bb: we have a slave channel in sc at this point

    lowestVol = 255;

    slaveChn_t *scTmp = AllocateSlaveOffset;
    for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
    {
        if (scTmp->Smp == sc->Smp && (scTmp->HostChnNum & CHN_DISOWNED) && scTmp->FinalVol7Bit < lowestVol)
        {
            sc        = scTmp;
            lowestVol = scTmp->FinalVol7Bit;
        }
    }

    return AllocateChannelInstrument(hc, sc, ins, hcFlags);
}

static uint8_t Random(void) // 8bb: verified to be ported correctly
{
    uint16_t r1, r2, r3, r4;

    r1 = RandSeed1;
    r2 = r3 = r4 = RandSeed2;

    r1 += r2;
    r1 = (r1 << (r3 & 15)) | (r1 >> ((16 - r3) & 15));
    r1 ^= r4;
    r3 = (r3 >> 8) | (r3 << 8);
    r2 += r3;
    r4 += r2;
    r3 += r1;
    r1 -= r4 + (r2 & 1);
    r2 = (r2 << 15) | (r2 >> 1);

    RandSeed2 = r4;
    RandSeed1 = r1;

    return (uint8_t)r1;
}

static void GetLoopInformation(slaveChn_t *sc)
{
    uint8_t LoopMode;
    int32_t LoopBegin, LoopEnd;

    assert(sc->SmpPtr != NULL);
    sample_t *s = sc->SmpPtr;

    bool LoopEnabled = !!(s->Flags & (SMPF_USE_LOOP | SMPF_USE_SUSTAINLOOP));
    bool SustainLoopOnlyAndNoteOff =
        (s->Flags & SMPF_USE_SUSTAINLOOP) && (sc->Flags & SF_NOTE_OFF) && !(s->Flags & SMPF_USE_LOOP);

    if (!LoopEnabled || SustainLoopOnlyAndNoteOff)
    {
        LoopBegin = 0;
        LoopEnd   = s->Length;
        LoopMode  = 0;
    }
    else
    {
        LoopBegin = s->LoopBegin;
        LoopEnd   = s->LoopEnd;
        LoopMode  = s->Flags;

        if (s->Flags & SMPF_USE_SUSTAINLOOP)
        {
            if (!(sc->Flags & SF_NOTE_OFF)) // 8bb: sustain on (note not released)?
            {
                LoopBegin = s->SustainLoopBegin;
                LoopEnd   = s->SustainLoopEnd;
                LoopMode >>= 1; // 8bb: loop mode = sustain loop mode
            }
        }

        LoopMode = (LoopMode & SMPF_LOOP_PINGPONG) ? LOOP_PINGPONG
                                                   : LOOP_FORWARDS; // 8bb: set loop type (Ping-Pong or Forwards)
    }

    // 8bb: if any parameter changed, update all
    if (sc->LoopMode != LoopMode || sc->LoopBegin != LoopBegin || sc->LoopEnd != LoopEnd)
    {
        sc->LoopMode  = LoopMode;
        sc->LoopBegin = LoopBegin;
        sc->LoopEnd   = LoopEnd;
        sc->Flags |= SF_LOOP_CHANGED;
    }
}

static void ApplyRandomValues(hostChn_t *hc)
{
    slaveChn_t   *sc  = (slaveChn_t *)hc->SlaveChnPtr;
    instrument_t *ins = sc->InsPtr;

    hc->Flags &= ~HF_APPLY_RANDOM_VOL;

    int8_t value = Random(); // -128->+127
    if (ins->RandVol != 0)   // Random volume, 0->100
    {
        int16_t vol = (((int8_t)ins->RandVol * value) >> 6) + 1;
        vol         = sc->SmpVol + ((vol * (int16_t)sc->SmpVol) / 199);

        if (vol < 0)
            vol = 0;
        else if (vol > 128)
            vol = 128;

        sc->SmpVol = (uint8_t)vol;
    }

    value = Random();                                 // -128->+127
    if (ins->RandPan != 0 && sc->Pan != PAN_SURROUND) // Random pan, 0->64
    {
        int16_t pan = sc->Pan + (((int8_t)ins->RandPan * value) >> 7);

        if (pan < 0)
            pan = 0;
        else if (pan > 64)
            pan = 64;

        sc->Pan = sc->PanSet = (uint8_t)pan;
    }
}

static void PitchSlideUp(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
    assert(sc != NULL);
    assert(hc != NULL);

    if (Song.Header.Flags & ITF_LINEAR_FRQ)
    {
        // 8bb: linear frequencies
        PitchSlideUpLinear(hc, sc, SlideValue);
    }
    else
    {
        // 8bb: Amiga frequencies
        sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

        const uint32_t PeriodBase = 1712 * 8363;

        if (SlideValue < 0)
        {
            SlideValue = -SlideValue;

            // 8bb: slide down

            uint64_t FreqSlide64 = (uint64_t)sc->Frequency * (uint32_t)SlideValue;
            if (FreqSlide64 > UINT32_MAX)
            {
                sc->Flags |= SF_NOTE_STOP; // Turn off channel
                hc->Flags &= ~HF_CHAN_ON;
                return;
            }

            FreqSlide64 += PeriodBase;

            uint32_t ShitValue = 0;
            while (FreqSlide64 > UINT32_MAX)
            {
                FreqSlide64 >>= 1;
                ShitValue++;
            }

            uint32_t Temp32 = (uint32_t)FreqSlide64;
            uint64_t Temp64 = (uint64_t)sc->Frequency * (uint32_t)PeriodBase;

            if (ShitValue > 0)
                Temp64 >>= ShitValue;

            if (Temp32 <= Temp64 >> 32)
            {
                sc->Flags |= SF_NOTE_STOP;
                hc->Flags &= ~HF_CHAN_ON;
                return;
            }

            sc->Frequency = (uint32_t)(Temp64 / Temp32);
        }
        else
        {
            // 8bb: slide up

            uint64_t FreqSlide64 = (uint64_t)sc->Frequency * (uint32_t)SlideValue;
            if (FreqSlide64 > UINT32_MAX)
            {
                sc->Flags |= SF_NOTE_STOP; // Turn off channel
                hc->Flags &= ~HF_CHAN_ON;
                return;
            }

            uint32_t FreqSlide32 = (uint32_t)FreqSlide64;

            uint32_t Temp32 = PeriodBase - FreqSlide32;
            if ((int32_t)Temp32 <= 0)
            {
                sc->Flags |= SF_NOTE_STOP;
                hc->Flags &= ~HF_CHAN_ON;
                return;
            }

            uint64_t Temp64 = (uint64_t)sc->Frequency * (uint32_t)PeriodBase;
            if (Temp32 <= Temp64 >> 32)
            {
                sc->Flags |= SF_NOTE_STOP;
                hc->Flags &= ~HF_CHAN_ON;
                return;
            }

            sc->Frequency = (uint32_t)(Temp64 / Temp32);
        }
    }
}

static void PitchSlideUpLinear(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
    assert(sc != NULL);
    assert(hc != NULL);
    assert(SlideValue >= -1024 && SlideValue <= 1024);

    sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

    if (SlideValue < 0)
    {
        // 8bb: slide down

        SlideValue = -SlideValue;

        const uint16_t *SlideTable;
        if (SlideValue <= 15)
        {
            SlideTable = FineLinearSlideDownTable;
        }
        else
        {
            SlideTable = LinearSlideDownTable;
            SlideValue >>= 2;
        }

        sc->Frequency = ((uint64_t)sc->Frequency * SlideTable[SlideValue]) >> 16;
    }
    else
    {
        // 8bb: slide up

        const uint32_t *SlideTable;
        if (SlideValue <= 15)
        {
            SlideTable = FineLinearSlideUpTable;
        }
        else
        {
            SlideTable = LinearSlideUpTable;
            SlideValue >>= 2;
        }

        uint64_t Frequency = ((uint64_t)sc->Frequency * SlideTable[SlideValue]) >> 16;
        if (Frequency & 0xFFFF000000000000)
        {
            sc->Flags |= SF_NOTE_STOP; // Turn off channel
            hc->Flags &= ~HF_CHAN_ON;
        }
        else
        {
            sc->Frequency = (uint32_t)Frequency;
        }
    }
}

static void PitchSlideDown(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
    PitchSlideUp(hc, sc, -SlideValue);
}

static uint8_t *Music_GetPattern(uint32_t pattern, uint16_t *numRows)
{
    assert(pattern < MAX_PATTERNS);
    pattern_t *p = &Song.Patt[pattern];

    if (p->PackedData == NULL)
    {
        *numRows = 64;
        return EmptyPattern;
    }

    *numRows = p->Rows;
    return p->PackedData;
}

static void PreInitCommand(hostChn_t *hc)
{
    if (hc->NotePackMask & 0x33)
    {
        if (!(Song.Header.Flags & ITF_INSTR_MODE) || hc->RawNote >= 120 || hc->Ins == 0)
        {
            hc->TranslatedNote = hc->RawNote;
            hc->Smp            = hc->Ins;
        }
        else
        {
            instrument_t *ins = &Song.Ins[hc->Ins - 1];

            hc->TranslatedNote = ins->SmpNoteTable[hc->RawNote] & 0xFF;

            /* 8bb:
            ** Added >128 check to prevent instruments with ModPlug/OpenMPT plugins
            ** from being handled as MIDI (would result in silence, and crash IT2).
            */
            if (ins->MIDIChn == 0 || ins->MIDIChn > 128)
            {
                hc->Smp = ins->SmpNoteTable[hc->RawNote] >> 8;
            }
            else // 8bb: MIDI
            {
                hc->MIDIChn  = (ins->MIDIChn == 17) ? (hc->HostChnNum & 0x0F) + 1 : ins->MIDIChn;
                hc->MIDIProg = ins->MIDIProg;
                hc->Smp      = 101;
            }

            if (hc->Smp == 0) // No sample?
                return;
        }
    }

    InitCommandTable[hc->Cmd & 31](hc); // Init note

    hc->Flags |= HF_ROW_UPDATED;

    bool ChannelMuted = !!(Song.Header.ChnlPan[hc->HostChnNum] & 128);
    if (ChannelMuted && !(hc->Flags & HF_FREEPLAY_NOTE) && (hc->Flags & HF_CHAN_ON))
        ((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_CHN_MUTED;
}

static void UpdateGOTONote(void) // Get offset
{
    Song.DecodeExpectedPattern = Song.CurrentPattern;

    uint8_t *p = Music_GetPattern(Song.DecodeExpectedPattern, &Song.NumberOfRows);
    if (Song.ProcessRow >= Song.NumberOfRows)
        Song.ProcessRow = 0;

    Song.DecodeExpectedRow = Song.CurrentRow = Song.ProcessRow;

    uint16_t rowsTodo = Song.ProcessRow;
    if (rowsTodo > 0)
    {
        while (true)
        {
            uint8_t chnNum = *p++;
            if (chnNum == 0)
            {
                rowsTodo--;
                if (rowsTodo == 0)
                    break;

                continue;
            }

            hostChn_t *hc = &hChn[(chnNum & 0x7F) - 1];
            if (chnNum & 0x80)
                hc->NotePackMask = *p++;

            if (hc->NotePackMask & 1)
                hc->RawNote = *p++;

            if (hc->NotePackMask & 2)
                hc->Ins = *p++;

            if (hc->NotePackMask & 4)
                hc->Vol = *p++;

            if (hc->NotePackMask & 8)
            {
                hc->OldCmd    = *p++;
                hc->OldCmdVal = *p++;
            }
        }
    }

    Song.PatternOffset = p;
}

static void UpdateNoteData(void)
{
    hostChn_t *hc;

    Song.PatternLooping = false;
    if (Song.CurrentPattern != Song.DecodeExpectedPattern || ++Song.DecodeExpectedRow != Song.CurrentRow)
        UpdateGOTONote();

    // First clear all old command&value.
    hc = hChn;
    for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
        hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_ROW_UPDATED | HF_UPDATE_VOLEFX_IF_CHAN_ON);

    uint8_t *p = Song.PatternOffset;
    while (true)
    {
        uint8_t chnNum = *p++;
        if (chnNum == 0) // No more! else... go through decoding
            break;

        hc = &hChn[(chnNum & 0x7F) - 1];
        if (chnNum & 0x80)
            hc->NotePackMask = *p++;

        if (hc->NotePackMask & 1)
            hc->RawNote = *p++;

        if (hc->NotePackMask & 2)
            hc->Ins = *p++;

        if (hc->NotePackMask & 4)
            hc->Vol = *p++;

        if (hc->NotePackMask & 8)
        {
            hc->Cmd = hc->OldCmd = *p++;
            hc->CmdVal = hc->OldCmdVal = *p++;
        }
        else if (hc->NotePackMask & 128)
        {
            hc->Cmd    = hc->OldCmd;
            hc->CmdVal = hc->OldCmdVal;
        }
        else
        {
            hc->Cmd    = 0;
            hc->CmdVal = 0;
        }

        PreInitCommand(hc);
    }

    Song.PatternOffset = p;
}

static void UpdateData(void)
{
    // 8bb: I only added the logic for "Play Song" (2) mode

    Song.ProcessTick--;
    Song.CurrentTick--;

    if (Song.CurrentTick == 0)
    {
        Song.ProcessTick = Song.CurrentTick = Song.CurrentSpeed;

        Song.RowDelay--;
        if (Song.RowDelay == 0)
        {
            Song.RowDelay   = 1;
            Song.RowDelayOn = false;

            uint16_t NewRow = Song.ProcessRow + 1;
            if (NewRow >= Song.NumberOfRows)
            {
                uint16_t NewOrder = Song.ProcessOrder + 1;
                while (true)
                {
                    if (NewOrder >= 256)
                    {
                        NewOrder = 0;
                        continue;
                    }

                    uint8_t NewPattern = Song.Orders[NewOrder]; // next pattern
                    if (NewPattern >= 200)
                    {
                        if (NewPattern == 0xFE)                 // 8bb: skip pattern separator
                        {
                            NewOrder++;
                        }
                        else
                        {
                            NewOrder      = 0;
                            Song.StopSong = true; // 8bb: for WAV rendering
                        }
                    }
                    else
                    {
                        Song.CurrentPattern = NewPattern;
                        break;
                    }
                }

                Song.CurrentOrder = Song.ProcessOrder = NewOrder;
                NewRow                                = Song.BreakRow;
                Song.BreakRow                         = 0;
            }

            Song.CurrentRow = Song.ProcessRow = NewRow;
            UpdateNoteData();
        }
        else
        {
            hostChn_t *hc = hChn;
            for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
            {
                if (!(hc->Flags & HF_ROW_UPDATED) || !(hc->NotePackMask & 0x88))
                    continue;

                uint8_t OldNotePackMask = hc->NotePackMask;
                hc->NotePackMask &= 0x88;
                InitCommandTable[hc->Cmd & 31](hc);
                hc->NotePackMask = OldNotePackMask;
            }
        }
    }
    else
    {
        // OK. call update command.

        hostChn_t *hc = hChn;
        for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
        {
            if ((hc->Flags & HF_CHAN_ON) && (hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
                VolumeEffectTable[hc->VolCmd & 7](hc);

            if ((hc->Flags & (HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX)) &&
                ((hc->Flags & HF_ALWAYS_UPDATE_EFX) || (hc->Flags & HF_CHAN_ON)))
            {
                CommandTable[hc->Cmd & 31](hc);
            }
        }
    }
}

static void UpdateAutoVibrato(slaveChn_t *sc) // 8bb: renamed from UpdateVibrato() to UpdateAutoVibrato() for clarity
{
    assert(sc->SmpPtr != NULL);
    sample_t *smp = sc->SmpPtr;

    if (smp->AutoVibratoDepth == 0)
        return;

    sc->AutoVibratoDepth += smp->AutoVibratoRate;
    if (sc->AutoVibratoDepth >> 8 > smp->AutoVibratoDepth)
        sc->AutoVibratoDepth = (smp->AutoVibratoDepth << 8) | (sc->AutoVibratoDepth & 0xFF);

    if (smp->AutoVibratoSpeed == 0)
        return;

    int16_t VibratoData;
    if (smp->AutoVibratoWaveform == 3)
    {
        VibratoData = (Random() & 127) - 64;
    }
    else
    {
        sc->AutoVibratoPos += smp->AutoVibratoSpeed; // Update pointer.

        assert(smp->AutoVibratoWaveform < 3);
        VibratoData = FineSineData[(smp->AutoVibratoWaveform << 8) + sc->AutoVibratoPos];
    }

    VibratoData = (VibratoData * (int16_t)(sc->AutoVibratoDepth >> 8)) >> 6;
    if (VibratoData != 0)
        PitchSlideUpLinear(sc->HostChnPtr, sc, VibratoData);
}

static bool UpdateEnvelope(env_t *env, envState_t *envState, bool SustainReleased)
{
    if (envState->Tick < envState->NextTick)
    {
        envState->Tick++;
        envState->Value += envState->Delta;
        return false; // 8bb: last node not reached
    }

    envNode_t *Nodes = env->NodePoints;
    envState->Value  = Nodes[envState->CurNode & 0x00FF].Magnitude << 16;
    int16_t NextNode = (envState->CurNode & 0x00FF) + 1;

    if (env->Flags & 6) // 8bb: any loop at all?
    {
        uint8_t LoopBegin = env->LoopBegin;
        uint8_t LoopEnd   = env->LoopEnd;

        bool HasLoop        = !!(env->Flags & ENVF_LOOP);
        bool HasSustainLoop = !!(env->Flags & ENVF_SUSTAINLOOP);

        bool Looping = true;
        if (HasSustainLoop)
        {
            if (!SustainReleased)
            {
                LoopBegin = env->SustainLoopBegin;
                LoopEnd   = env->SustainLoopEnd;
            }
            else if (!HasLoop)
            {
                Looping = false;
            }
        }

        if (Looping && NextNode > LoopEnd)
        {
            envState->CurNode = (envState->CurNode & 0xFF00) | LoopBegin;
            envState->Tick = envState->NextTick = Nodes[envState->CurNode & 0x00FF].Tick;
            return false; // 8bb: last node not reached
        }
    }

    if (NextNode >= env->Num)
        return true; // 8bb: last node reached

    // 8bb: new node

    envState->NextTick = Nodes[NextNode].Tick;
    envState->Tick     = Nodes[envState->CurNode & 0x00FF].Tick + 1;

    int16_t TickDelta = envState->NextTick - Nodes[envState->CurNode & 0x00FF].Tick;
    if (TickDelta == 0)
        TickDelta = 1;

    int16_t Delta     = Nodes[NextNode].Magnitude - Nodes[envState->CurNode & 0x00FF].Magnitude;
    envState->Delta   = (Delta << 16) / TickDelta;
    envState->CurNode = (envState->CurNode & 0xFF00) | (uint8_t)NextNode;

    return false; // 8bb: last node not reached
}

static void UpdateInstruments(void)
{
    slaveChn_t *sc = sChn;
    for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
    {
        if (!(sc->Flags & SF_CHAN_ON))
            continue;

        if (sc->Ins != 0xFF) // 8bb: got an instrument?
        {
            int16_t       EnvVal;
            instrument_t *ins             = sc->InsPtr;
            bool          SustainReleased = !!(sc->Flags & SF_NOTE_OFF);

            // 8bb: handle pitch/filter envelope

            if (sc->Flags & SF_PITCHENV_ON)
            {
                if (UpdateEnvelope(&ins->PitchEnv, &sc->PitchEnvState, SustainReleased)) // 8bb: last node reached?
                    sc->Flags &= ~SF_PITCHENV_ON;
            }

            if (!(ins->PitchEnv.Flags & ENVF_TYPE_FILTER)) // 8bb: pitch envelope
            {
                EnvVal = (int16_t)((uint32_t)sc->PitchEnvState.Value >> 8);
                EnvVal >>= 3;                              // 8bb: arithmetic shift

                if (EnvVal != 0)
                {
                    PitchSlideUpLinear(sc->HostChnPtr, sc, EnvVal);
                    sc->Flags |= SF_FREQ_CHANGE;
                }
            }
            else if (sc->Smp != 100) // 8bb: filter envelope
            {
                EnvVal = (int16_t)((uint32_t)sc->PitchEnvState.Value >> 8);
                EnvVal >>= 6;        // 8bb: arithmetic shift, -128..128 (though -512..511 is in theory possible)

                /*
                ** 8bb: Some annoying logic.
                **
                ** Original asm code:
                **  add bx,128
                **  cmp bh,1
                **  adc bl,-1
                **
                ** The code below is confirmed to be correct
                ** for the whole -512..511 range.
                **
                ** However, EnvVal should only be -128..128
                ** (0..256 after +128 add) unless something
                ** nasty is going on.
                */
                EnvVal += 128;
                if (EnvVal & 0xFF00)
                    EnvVal--;

                sc->MIDIBank = (sc->MIDIBank & 0xFF00) | (uint8_t)EnvVal; // 8bb: don't mess with upper byte!
                sc->Flags |= SF_RECALC_FINALVOL;
            }

            if (sc->Flags & SF_PANENV_ON)
            {
                sc->Flags |= SF_RECALC_PAN;
                if (UpdateEnvelope(&ins->PanEnv, &sc->PanEnvState, SustainReleased)) // 8bb: last node reached?
                    sc->Flags &= ~SF_PANENV_ON;
            }

            bool HandleNoteFade = false;
            bool TurnOffCh      = false;

            if (sc->Flags & SF_VOLENV_ON) // Volume envelope on?
            {
                sc->Flags |= SF_RECALC_VOL;

                if (UpdateEnvelope(&ins->VolEnv, &sc->VolEnvState, SustainReleased)) // 8bb: last node reached?
                {
                    // Envelope turned off...

                    sc->Flags &= ~SF_VOLENV_ON;

                    if ((sc->VolEnvState.Value & 0x00FF0000) ==
                        0) // Turn off if end of loop is reached (8bb: last env. point is zero?)
                    {
                        TurnOffCh = true;
                    }
                    else
                    {
                        sc->Flags |= SF_FADEOUT;
                        HandleNoteFade = true;
                    }
                }
                else
                {
                    if (!(sc->Flags & SF_FADEOUT)) // Note fade on?
                    {
                        // Now, check if loop + sustain off
                        if (SustainReleased && (ins->VolEnv.Flags & ENVF_LOOP)) // Normal vol env loop?
                        {
                            sc->Flags |= SF_FADEOUT;
                            HandleNoteFade = true;
                        }
                    }
                    else
                    {
                        HandleNoteFade = true;
                    }
                }
            }
            else if (sc->Flags & SF_FADEOUT) // Note fade??
            {
                HandleNoteFade = true;
            }
            else if (sc->Flags & SF_NOTE_OFF) // Note off issued?
            {
                sc->Flags |= SF_FADEOUT;
                HandleNoteFade = true;
            }

            if (HandleNoteFade)
            {
                sc->FadeOut -= ins->FadeOut;
                if ((int16_t)sc->FadeOut <= 0)
                {
                    sc->FadeOut = 0;
                    TurnOffCh   = true;
                }

                sc->Flags |= SF_RECALC_VOL;
            }

            if (TurnOffCh)
            {
                if (!(sc->HostChnNum & CHN_DISOWNED))
                {
                    sc->HostChnNum |= CHN_DISOWNED; // Host channel exists
                    ((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON;
                }

                sc->Flags |= (SF_RECALC_VOL | SF_NOTE_STOP);
            }
        }

        if (sc->Flags & SF_RECALC_VOL) // Calculate volume
        {
            sc->Flags &= ~SF_RECALC_VOL;
            sc->Flags |= SF_RECALC_FINALVOL;

            uint16_t volume = (sc->Vol * sc->ChnVol * sc->FadeOut) >> 7;
            volume          = (volume * sc->SmpVol) >> 7;
            volume          = (volume * (uint16_t)((uint32_t)sc->VolEnvState.Value >> 8)) >> 14;
            volume          = (volume * Song.GlobalVolume) >> 7;
            assert(volume <= 32768);

            sc->FinalVol15Bit = volume;      // 8bb: 0..32768
            sc->FinalVol7Bit  = volume >> 8; // 8bb: 0..128
        }

        if (sc->Flags & SF_RECALC_PAN)       // Change in panning?
        {
            sc->Flags &= ~SF_RECALC_PAN;
            sc->Flags |= SF_PAN_CHANGED;

            if (sc->Pan == PAN_SURROUND)
            {
                sc->FinalPan = sc->Pan;
            }
            else
            {
                int8_t PanVal = 32 - sc->Pan;
                if (PanVal < 0)
                {
                    PanVal ^= 255;
                    PanVal -= 255;
                }
                PanVal = -PanVal;
                PanVal += 32;

                const int8_t PanEnvVal = (int8_t)(sc->PanEnvState.Value >> 16);
                PanVal                 = sc->Pan + ((PanVal * PanEnvVal) >> 5);
                PanVal -= 32;

                sc->FinalPan = (int8_t)(((PanVal * (int8_t)(Song.Header.PanSep >> 1)) >> 6) + 32); // 8bb: 0..64
                assert(sc->FinalPan <= 64);
            }
        }

        UpdateAutoVibrato(sc);
    }
}

static void UpdateSamples(void) // 8bb: for songs without instruments
{
    slaveChn_t *sc = sChn;
    for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
    {
        if (!(sc->Flags & SF_CHAN_ON))
            continue;

        if (sc->Flags & SF_RECALC_VOL) // 8bb: recalculate volume
        {
            sc->Flags &= ~SF_RECALC_VOL;
            sc->Flags |= SF_RECALC_FINALVOL;

            uint16_t volume = (((sc->Vol * sc->ChnVol * sc->SmpVol) >> 4) * Song.GlobalVolume) >> 7;
            assert(volume <= 32768);

            sc->FinalVol15Bit = volume;      // 8bb: 0..32768
            sc->FinalVol7Bit  = volume >> 8; // 8bb: 0..128
        }

        if (sc->Flags & SF_RECALC_PAN)       // 8bb: recalculate panning
        {
            sc->Flags &= ~SF_RECALC_PAN;
            sc->Flags |= SF_PAN_CHANGED;

            if (sc->Pan == PAN_SURROUND)
            {
                sc->FinalPan = sc->Pan;
            }
            else
            {
                sc->FinalPan = ((((int8_t)sc->Pan - 32) * (int8_t)(Song.Header.PanSep >> 1)) >> 6) + 32; // 8bb: 0..64
                assert(sc->FinalPan <= 64);
            }
        }

        UpdateAutoVibrato(sc);
    }
}

static void Update(void)
{
    slaveChn_t *sc = sChn;
    for (uint32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
    {
        if (!(sc->Flags & SF_CHAN_ON))
            continue;

        if (sc->Vol != sc->VolSet)
        {
            sc->Vol = sc->VolSet;
            sc->Flags |= SF_RECALC_VOL;
        }

        if (sc->Frequency != sc->FrequencySet)
        {
            sc->Frequency = sc->FrequencySet;
            sc->Flags |= SF_FREQ_CHANGE;
        }
    }

    UpdateData();

    if (Song.Header.Flags & ITF_INSTR_MODE)
        UpdateInstruments();
    else
        UpdateSamples();
}

void Music_FillAudioBuffer(int16_t *buffer, int32_t numSamples)
{
    if (!Song.Playing)
    {
        memset(buffer, 0, numSamples * 2 * sizeof(int16_t));
        return;
    }

    SB16_Mix(numSamples, buffer);
}

void Music_FillAudioBufferFloat(float *buffer, int32_t numSamples)
{
    if (!Song.Playing)
    {
        memset(buffer, 0, numSamples * 2 * sizeof(float));
        return;
    }

    SB16_Mix_Float(numSamples, buffer);
}

bool Music_Init(int32_t mixingFrequency)
{
    if (FirstTimeInit)
    {
        memset(&Driver, 0, sizeof(Driver));
        FirstTimeInit = false;
    }
    else
    {
        Music_Close();
    }

    if (!SB16_InitDriver(mixingFrequency))
        return false;

    return true;
}

void Music_Close(void) // 8bb: added this
{
    SB16_CloseDriver();
}

static void Music_InitTempo(void)
{
    SB16_SetTempo((uint8_t)Song.Tempo);
}

void Music_Stop(void)
{
    Song.Playing = false;

    MIDITranslate(NULL, sChn, MIDICOMMAND_STOP);

    Song.DecodeExpectedPattern = 0xFFFE;
    Song.DecodeExpectedRow     = 0xFFFE;
    Song.RowDelay              = 1;
    Song.RowDelayOn            = false;
    Song.CurrentRow            = 0;
    Song.CurrentOrder          = 0;
    Song.CurrentTick           = 1;
    Song.BreakRow              = 0;

    // 8bb: clear host/slave channels
    memset(hChn, 0, sizeof(hChn));
    memset(sChn, 0, sizeof(sChn));

    hostChn_t *hc = hChn;
    for (uint8_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
    {
        hc->HostChnNum = i;

        // 8bb: set initial channel pan and channel vol
        hc->ChnPan = Song.Header.ChnlPan[i] & 0x7F;
        hc->ChnVol = Song.Header.ChnlVol[i];
    }

    slaveChn_t *sc = sChn;
    for (uint32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
        sc->Flags = SF_NOTE_STOP;

    if (Song.Loaded)
    {
        Song.GlobalVolume = Song.Header.GlobalVol;
        Song.ProcessTick = Song.CurrentSpeed = Song.Header.InitialSpeed;
        Song.Tempo                           = Song.Header.InitialTempo;

        Music_InitTempo();
    }
}

void Music_PlaySong(uint16_t order)
{
    if (!Song.Loaded)
        return;

    Music_Stop();

    MIDITranslate(NULL, sChn, MIDICOMMAND_START); // 8bb: this will reset channel filters

    Song.CurrentOrder = order;
    Song.ProcessOrder = order - 1;
    Song.ProcessRow   = 0xFFFE;

    // 8bb: reset seed (IT2 only does this at tracker startup, but let's do it here)
    RandSeed1 = 0x1234;
    RandSeed2 = 0x5678;

    MIDIInterpretState = MIDIInterpretType = 0; // 8bb: clear MIDI filter interpretor state

    SB16_ResetMixer();

    Song.Playing = true;
}

static void Music_ReleaseSample(uint32_t sample)
{
    assert(sample < MAX_SAMPLES);
    sample_t *smp = &Song.Smp[sample];

    if (smp->OrigData != NULL)
        M4P_FREE(smp->OrigData);
    if (smp->OrigDataR != NULL)
        M4P_FREE(smp->OrigDataR);

    smp->Data = smp->OrigData = NULL;
    smp->DataR = smp->OrigDataR = NULL;
}

static bool Music_AllocatePattern(uint32_t pattern, uint32_t length)
{
    assert(pattern < MAX_PATTERNS);
    pattern_t *p = &Song.Patt[pattern];

    if (p->PackedData != NULL)
        return true;

    p->PackedData = (uint8_t *)M4P_MALLOC(length);
    if (p->PackedData == NULL)
        return false;

    return true;
}

static bool Music_AllocateSample(uint32_t sample, uint32_t length)
{
    assert(sample < MAX_SAMPLES);
    sample_t *s = &Song.Smp[sample];

    // 8bb: done a little differently than IT2

    s->OrigData = (int8_t *)M4P_MALLOC(length + SAMPLE_PAD_LENGTH); // 8bb: extra bytes for interpolation taps, filled later
    if (s->OrigData == NULL)
        return false;

    memset((int8_t *)s->OrigData, 0, SMP_DAT_OFFSET);
    memset((int8_t *)s->OrigData + length, 0, 32);

    // 8bb: offset sample so that we can fix negative interpolation taps
    s->Data = (int8_t *)s->OrigData + SMP_DAT_OFFSET;

    s->Length = length;
    s->Flags |= SMPF_ASSOCIATED_WITH_HEADER;

    return true;
}

static bool Music_AllocateRightSample(uint32_t sample, uint32_t length) // 8bb: added this
{
    assert(sample < MAX_SAMPLES);
    sample_t *s = &Song.Smp[sample];

    s->OrigDataR =
        (int8_t *)M4P_MALLOC(length + SAMPLE_PAD_LENGTH); // 8bb: extra bytes for interpolation taps, filled later
    if (s->OrigDataR == NULL)
        return false;

    memset((int8_t *)s->OrigDataR, 0, SMP_DAT_OFFSET);
    memset((int8_t *)s->OrigDataR + length, 0, 32);

    // 8bb: offset sample so that we can fix negative interpolation taps
    s->DataR = (int8_t *)s->OrigDataR + SMP_DAT_OFFSET;

    return true;
}

static void Music_ReleasePattern(uint32_t pattern)
{
    assert(pattern < MAX_PATTERNS);
    pattern_t *p = &Song.Patt[pattern];

    if (p->PackedData != NULL)
        M4P_FREE(p->PackedData);

    p->Rows       = 0;
    p->PackedData = NULL;
}

static void Music_ReleaseAllPatterns(void)
{
    for (int32_t i = 0; i < MAX_PATTERNS; i++)
        Music_ReleasePattern(i);
}

static void Music_ReleaseAllSamples(void)
{
    for (int32_t i = 0; i < MAX_SAMPLES; i++)
        Music_ReleaseSample(i);
}

void Music_FreeSong(void) // 8bb: added this
{
    Music_Stop();

    Music_ReleaseAllPatterns();
    Music_ReleaseAllSamples();

    memset(&Song, 0, sizeof(Song));
    memset(Song.Orders, 255, MAX_ORDERS);

    Song.Loaded = false;
}

static bool LoadIT(MEMFILE *m)
{
    /*
    ** ===================================
    ** =========== LOAD HEADER ===========
    ** ===================================
    */

    mseek(m, 4, SEEK_CUR);
    if (!mreadexact(m, Song.Header.SongName, 25))
        return false;
    mseek(m, 1 + 2, SEEK_CUR);
    if (!mreadexact(m, &Song.Header.OrdNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.InsNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.SmpNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.PatNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.Cwtv, 2))
        return false;
    if (!mreadexact(m, &Song.Header.Cmwt, 2))
        return false;
    if (!mreadexact(m, &Song.Header.Flags, 2))
        return false;
    if (!mreadexact(m, &Song.Header.Special, 2))
        return false;
    if (!mreadexact(m, &Song.Header.GlobalVol, 1))
        return false;
    if (!mreadexact(m, &Song.Header.MixVolume, 1))
        return false;
    if (!mreadexact(m, &Song.Header.InitialSpeed, 1))
        return false;
    if (!mreadexact(m, &Song.Header.InitialTempo, 1))
        return false;
    if (!mreadexact(m, &Song.Header.PanSep, 1))
        return false;
    mseek(m, 1, SEEK_CUR);
    if (!mreadexact(m, &Song.Header.MessageLength, 2))
        return false;
    if (!mreadexact(m, &Song.Header.MessageOffset, 4))
        return false;
    mseek(m, 4, SEEK_CUR); // skip unwanted stuff
    if (!mreadexact(m, Song.Header.ChnlPan, MAX_HOST_CHANNELS))
        return false;
    if (!mreadexact(m, Song.Header.ChnlVol, MAX_HOST_CHANNELS))
        return false;

    // IT2 doesn't do this test, but I do it for safety.
    if (Song.Header.OrdNum > MAX_ORDERS + 1 || Song.Header.InsNum > MAX_INSTRUMENTS ||
        Song.Header.SmpNum > MAX_SAMPLES || Song.Header.PatNum > MAX_PATTERNS)
    {
        return false;
    }

    // IT2 doesn't do this, but let's do it for safety
    if (Song.Header.MessageLength > MAX_SONGMSG_LENGTH)
        Song.Header.MessageLength = MAX_SONGMSG_LENGTH;

    Song.Header.SongName[25] = '\0'; // just in case...

    /* *absolute* lowest possible initial tempo is 31, we need to clamp
    ** it for safety reasons (yes, IT2 can do 31 as initial tempo!).
    */
    if (Song.Header.InitialTempo < LOWEST_BPM_POSSIBLE)
        Song.Header.InitialTempo = LOWEST_BPM_POSSIBLE;

    int32_t PtrListOffset = 192 + Song.Header.OrdNum;

    int16_t OrdersToLoad = Song.Header.OrdNum - 1; // IT2 does this (removes the count for the last 255 terminator)
    if (OrdersToLoad > 0)
    {
        if (!mreadexact(m, Song.Orders, OrdersToLoad))
            return false;

        // fill rest of order list with 255
        if (OrdersToLoad < MAX_ORDERS)
            memset(&Song.Orders[OrdersToLoad], 255, MAX_ORDERS - OrdersToLoad);
    }
    else
    {
        memset(Song.Orders, 255, MAX_ORDERS);
    }

    mseek(m, 192 + Song.Header.OrdNum + ((Song.Header.InsNum + Song.Header.SmpNum + Song.Header.PatNum) * 4), SEEK_SET);

    // skip time data, if present
    if (Song.Header.Special & 2)
    {
        uint16_t NumTimerData;
        mreadexact(m, &NumTimerData, 2);
        mseek(m, NumTimerData * 8, SEEK_CUR);
    }

    // read embedded MIDI configuration, if preset (needed for Zxx macros)
    char *MIDIDataArea = Music_GetMIDIDataArea();
    if (Song.Header.Special & 8)
        mreadexact(m, MIDIDataArea, (9 + 16 + 128) * 32);

    // load song message, if present
    if ((Song.Header.Special & 1) && Song.Header.MessageLength > 0 && Song.Header.MessageOffset > 0)
    {
        mseek(m, Song.Header.MessageOffset, SEEK_SET);
        mread(Song.Message, 1, Song.Header.MessageLength, m);
        Song.Message[MAX_SONGMSG_LENGTH] = '\0'; // just in case
    }

    /*
    ** ===================================
    ** ======== LOAD INSTRUMENTS =========
    ** ===================================
    */

    mseek(m, PtrListOffset, SEEK_SET);
    size_t InsPtrOffset = mtell(m);

    instrument_t *ins = Song.Ins;
    for (uint32_t i = 0; i < Song.Header.InsNum; i++, ins++)
    {
        mseek(m, InsPtrOffset + (i * 4), SEEK_SET);
        if (meof(m))
            return false;

        uint32_t InsOffset;
        if (!mreadexact(m, &InsOffset, 4))
            return false;

        if (InsOffset == 0)
            continue;

        mseek(m, InsOffset, SEEK_SET);
        if (meof(m))
            return false;

        if (Song.Header.Cmwt >= 0x200)
        {
            mseek(m, 4, SEEK_CUR); // skip unwanted stuff
            if (!mreadexact(m, ins->DOSFilename, 13))
                return false;
            if (!mreadexact(m, &ins->NNA, 1))
                return false;
            if (!mreadexact(m, &ins->DCT, 1))
                return false;
            if (!mreadexact(m, &ins->DCA, 1))
                return false;
            if (!mreadexact(m, &ins->FadeOut, 2))
                return false;
            if (!mreadexact(m, &ins->PitchPanSep, 1))
                return false;
            if (!mreadexact(m, &ins->PitchPanCenter, 1))
                return false;
            if (!mreadexact(m, &ins->GlobVol, 1))
                return false;
            if (!mreadexact(m, &ins->DefPan, 1))
                return false;
            if (!mreadexact(m, &ins->RandVol, 1))
                return false;
            if (!mreadexact(m, &ins->RandPan, 1))
                return false;
            mseek(m, 4, SEEK_CUR); // skip unwanted stuff
            if (!mreadexact(m, ins->InstrumentName, 26))
                return false;
            if (!mreadexact(m, &ins->FilterCutoff, 1))
                return false;
            if (!mreadexact(m, &ins->FilterResonance, 1))
                return false;
            if (!mreadexact(m, &ins->MIDIChn, 1))
                return false;
            if (!mreadexact(m, &ins->MIDIProg, 1))
                return false;
            if (!mreadexact(m, &ins->MIDIBank, 2))
                return false;
            if (!mreadexact(m, &ins->SmpNoteTable, 2 * 120))
                return false;

            // just in case
            ins->DOSFilename[12]    = '\0';
            ins->InstrumentName[25] = '\0';

            // read envelopes
            for (uint32_t j = 0; j < 3; j++)
            {
                env_t *env;

                if (j == 0)
                    env = &ins->VolEnv;
                else if (j == 1)
                    env = &ins->PanEnv;
                else
                    env = &ins->PitchEnv;

                if (!mreadexact(m, &env->Flags, 1))
                    return false;
                if (!mreadexact(m, &env->Num, 1))
                    return false;
                if (!mreadexact(m, &env->LoopBegin, 1))
                    return false;
                if (!mreadexact(m, &env->LoopEnd, 1))
                    return false;
                if (!mreadexact(m, &env->SustainLoopBegin, 1))
                    return false;
                if (!mreadexact(m, &env->SustainLoopEnd, 1))
                    return false;

                envNode_t *node = env->NodePoints;
                for (uint32_t k = 0; k < 25; k++, node++)
                {
                    if (!mreadexact(m, &node->Magnitude, 1))
                        return false;
                    if (!mreadexact(m, &node->Tick, 2))
                        return false;
                }

                mseek(m, 1, SEEK_CUR); // skip unwanted stuff
            }
        }
        else                           // old instruments (v1.xx)
        {
            mseek(m, 4, SEEK_CUR);     // skip unwanted stuff
            if (!mreadexact(m, ins->DOSFilename, 13))
                return false;
            if (!mreadexact(m, &ins->VolEnv.Flags, 1))
                return false;
            if (!mreadexact(m, &ins->VolEnv.LoopBegin, 1))
                return false;
            if (!mreadexact(m, &ins->VolEnv.LoopEnd, 1))
                return false;
            if (!mreadexact(m, &ins->VolEnv.SustainLoopBegin, 1))
                return false;
            if (!mreadexact(m, &ins->VolEnv.SustainLoopEnd, 1))
                return false;
            mseek(m, 2, SEEK_CUR); // skip unwanted stuff
            if (!mreadexact(m, &ins->FadeOut, 2))
                return false;
            if (!mreadexact(m, &ins->NNA, 1))
                return false;
            if (!mreadexact(m, &ins->DCT, 1))
                return false;
            mseek(m, 4, SEEK_CUR); // skip unwanted stuff
            if (!mreadexact(m, ins->InstrumentName, 26))
                return false;
            mseek(m, 6, SEEK_CUR); // skip unwanted stuff
            if (!mreadexact(m, &ins->SmpNoteTable, 2 * 120))
                return false;

            ins->FadeOut *= 2;

            // just in case
            ins->DOSFilename[12]    = '\0';
            ins->InstrumentName[25] = '\0';

            // set default values not present in old instrument
            ins->PitchPanCenter = 60;
            ins->GlobVol        = 128;
            ins->DefPan         = 32 + 128; // center + pan disabled

            mseek(m, 200, SEEK_CUR);

            // read volume envelope
            uint8_t j;
            for (j = 0; j < 25; j++)
            {
                uint16_t   word;
                envNode_t *node = &ins->VolEnv.NodePoints[j];

                if (!mreadexact(m, &word, 2))
                    return false;
                if (word == 0xFFFF)
                    break; // end of envelope

                node->Tick      = word & 0xFF;
                node->Magnitude = word >> 8;
            }

            ins->VolEnv.Num = j;

            ins->PanEnv.Num                = 2;
            ins->PanEnv.NodePoints[1].Tick = 99;

            ins->PitchEnv.Num                = 2;
            ins->PitchEnv.NodePoints[1].Tick = 99;
        }
    }

    /*
    ** ===================================
    ** ======= LOAD SAMPLE HEADERS =======
    ** ===================================
    */

    mseek(m, PtrListOffset + (Song.Header.InsNum * 4), SEEK_SET);
    size_t SmpPtrOffset = mtell(m);

    sample_t *s = Song.Smp;
    for (uint32_t i = 0; i < Song.Header.SmpNum; i++, s++)
    {
        mseek(m, SmpPtrOffset + (i * 4), SEEK_SET);
        if (meof(m))
            return false;

        uint32_t SmpOffset;
        if (!mreadexact(m, &SmpOffset, 4))
            return false;

        if (SmpOffset == 0)
            continue;

        mseek(m, SmpOffset, SEEK_SET);
        if (meof(m))
            return false;

        mseek(m, 4, SEEK_CUR); // skip unwanted stuff
        if (!mreadexact(m, s->DOSFilename, 13))
            return false;
        if (!mreadexact(m, &s->GlobVol, 1))
            return false;
        if (!mreadexact(m, &s->Flags, 1))
            return false;
        if (!mreadexact(m, &s->Vol, 1))
            return false;
        if (!mreadexact(m, s->SampleName, 26))
            return false;
        if (!mreadexact(m, &s->Cvt, 1))
            return false;
        if (!mreadexact(m, &s->DefPan, 1))
            return false;
        if (!mreadexact(m, &s->Length, 4))
            return false;
        if (!mreadexact(m, &s->LoopBegin, 4))
            return false;
        if (!mreadexact(m, &s->LoopEnd, 4))
            return false;
        if (!mreadexact(m, &s->C5Speed, 4))
            return false;
        if (!mreadexact(m, &s->SustainLoopBegin, 4))
            return false;
        if (!mreadexact(m, &s->SustainLoopEnd, 4))
            return false;
        if (!mreadexact(m, &s->OffsetInFile, 4))
            return false;
        if (!mreadexact(m, &s->AutoVibratoSpeed, 1))
            return false;
        if (!mreadexact(m, &s->AutoVibratoDepth, 1))
            return false;
        if (!mreadexact(m, &s->AutoVibratoRate, 1))
            return false;
        if (!mreadexact(m, &s->AutoVibratoWaveform, 1))
            return false;

        // just in case
        s->DOSFilename[12] = '\0';
        s->SampleName[25]  = '\0';
    }

    /* ===================================
    ** ======== LOAD SAMPLE DATA =========
    ** ===================================
    */

    s = Song.Smp;
    for (uint32_t i = 0; i < Song.Header.SmpNum; i++, s++)
    {
        if (s->OffsetInFile == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
            continue;

        mseek(m, s->OffsetInFile, SEEK_SET);
        if (meof(m))
            continue;                                    // This WAS a return false...will I regret this? - Dasho

        bool Stereo        = !!(s->Flags & SMPF_STEREO); // added stereo support
        bool Compressed    = !!(s->Flags & SMPF_COMPRESSED);
        bool Sample16Bit   = !!(s->Flags & SMPF_16BIT);
        bool SignedSamples = !!(s->Cvt & 1);
        bool DeltaEncoded  = !!(s->Cvt & 4);

        if (DeltaEncoded && !Compressed)
            continue;

        if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
            continue; // safely skip this sample

        if (s->Cvt & 0b11111010)
            continue; // not supported

        if (!Music_AllocateSample(i, s->Length << Sample16Bit))
            return false;

        // added stereo support
        if (Stereo)
        {
            if (!Music_AllocateRightSample(i, s->Length << Sample16Bit))
                return false;
        }

        if (Compressed)
        {
            if (Sample16Bit)
            {
                if (!LoadCompressed16BitSample(m, s, Stereo, DeltaEncoded))
                    return false;
            }
            else
            {
                if (!LoadCompressed8BitSample(m, s, Stereo, DeltaEncoded))
                    return false;
            }
        }
        else
        {
            mread(s->Data, 1, s->Length, m);

            // added stereo support for custom HQ driver
            if (Stereo)
                mread(s->DataR, 1, s->Length, m);
        }

        // convert unsigned sample to signed
        if (!SignedSamples)
        {
            if (Sample16Bit)
            {
                int16_t *Ptr16 = (int16_t *)s->Data;
                for (uint32_t j = 0; j < s->Length; j++)
                    Ptr16[j] ^= 0x8000;
            }
            else
            {
                int8_t *Ptr8 = (int8_t *)s->Data;
                for (uint32_t j = 0; j < s->Length; j++)
                    Ptr8[j] ^= 0x80;
            }
        }

        if (Sample16Bit) // Music_AllocateSample() also set s->Length, divide by two if 16-bit
            s->Length >>= 1;
    }

    /*
    ** ===================================
    ** ========== LOAD PATTERNS ==========
    ** ===================================
    */

    mseek(m, PtrListOffset + (Song.Header.InsNum * 4) + (Song.Header.SmpNum * 4), SEEK_SET);
    size_t PatPtrOffset = mtell(m);

    pattern_t *p = Song.Patt;
    for (uint32_t i = 0; i < Song.Header.PatNum; i++, p++)
    {
        mseek(m, PatPtrOffset + (i * 4), SEEK_SET);
        if (meof(m))
            return false;

        uint32_t PatOffset;
        if (!mreadexact(m, &PatOffset, 4))
            return false;

        if (PatOffset == 0)
            continue;

        mseek(m, PatOffset, SEEK_SET);
        if (meof(m))
            return false;

        uint16_t PatLength;
        if (!mreadexact(m, &PatLength, 2))
            return false;
        if (!mreadexact(m, &p->Rows, 2))
            return false;

        if (PatLength == 0 || p->Rows == 0)
            continue;

        mseek(m, 4, SEEK_CUR);

        if (!Music_AllocatePattern(i, PatLength))
            return false;
        if (!mreadexact(m, p->PackedData, PatLength))
            return false;
    }

    return true;
}

static void Decompress16BitData(int16_t *Dst, const uint8_t *Src, uint32_t BlockLength)
{
    uint8_t  Byte8, BitDepth, BitDepthInv, BitsRead;
    uint16_t Bytes16, LastVal;
    uint32_t Bytes32;

    LastVal     = 0;
    BitDepth    = 17;
    BitDepthInv = BitsRead = 0;

    BlockLength >>= 1;
    while (BlockLength != 0)
    {
        Bytes32 = (*(uint32_t *)Src) >> BitsRead;

        BitsRead += BitDepth;
        Src += BitsRead >> 3;
        BitsRead &= 7;

        if (BitDepth <= 6)
        {
            Bytes32 <<= BitDepthInv & 0x1F;

            Bytes16 = (uint16_t)Bytes32;
            if (Bytes16 != 0x8000)
            {
                LastVal += (int16_t)Bytes16 >> (BitDepthInv & 0x1F); // arithmetic shift
                *Dst++ = LastVal;
                BlockLength--;
            }
            else
            {
                Byte8 = ((Bytes32 >> 16) & 0xF) + 1;
                if (Byte8 >= BitDepth)
                    Byte8++;
                BitDepth = Byte8;

                BitDepthInv = 16;
                if (BitDepthInv < BitDepth)
                    BitDepthInv++;
                BitDepthInv -= BitDepth;

                BitsRead += 4;
            }

            continue;
        }

        Bytes16 = (uint16_t)Bytes32;

        if (BitDepth <= 16)
        {
            uint16_t DX = 0xFFFF >> (BitDepthInv & 0x1F);
            Bytes16 &= DX;
            DX = (DX >> 1) - 8;

            if (Bytes16 > DX + 16 || Bytes16 <= DX)
            {
                Bytes16 <<= BitDepthInv & 0x1F;
                Bytes16 = (int16_t)Bytes16 >> (BitDepthInv & 0x1F); // arithmetic shift
                LastVal += Bytes16;
                *Dst++ = LastVal;
                BlockLength--;
                continue;
            }

            Byte8 = (uint8_t)(Bytes16 - DX);
            if (Byte8 >= BitDepth)
                Byte8++;
            BitDepth = Byte8;

            BitDepthInv = 16;
            if (BitDepthInv < BitDepth)
                BitDepthInv++;
            BitDepthInv -= BitDepth;
            continue;
        }

        if (Bytes32 & 0x10000)
        {
            BitDepth    = (uint8_t)(Bytes16 + 1);
            BitDepthInv = 16 - BitDepth;
        }
        else
        {
            LastVal += Bytes16;
            *Dst++ = LastVal;
            BlockLength--;
        }
    }
}

static void Decompress8BitData(int8_t *Dst, const uint8_t *Src, uint32_t BlockLength)
{
    uint8_t  LastVal, Byte8, BitDepth, BitDepthInv, BitsRead;
    uint16_t Bytes16;

    LastVal     = 0;
    BitDepth    = 9;
    BitDepthInv = BitsRead = 0;

    while (BlockLength != 0)
    {
        Bytes16 = (*(uint16_t *)Src) >> BitsRead;

        BitsRead += BitDepth;
        Src += (BitsRead >> 3);
        BitsRead &= 7;

        Byte8 = Bytes16 & 0xFF;

        if (BitDepth <= 6)
        {
            Bytes16 <<= (BitDepthInv & 0x1F);
            Byte8 = Bytes16 & 0xFF;

            if (Byte8 != 0x80)
            {
                LastVal += (int8_t)Byte8 >> (BitDepthInv & 0x1F); // arithmetic shift
                *Dst++ = LastVal;
                BlockLength--;
                continue;
            }

            Byte8 = (Bytes16 >> 8) & 7;
            BitsRead += 3;
            Src += (BitsRead >> 3);
            BitsRead &= 7;
        }
        else
        {
            if (BitDepth == 8)
            {
                if (Byte8 < 0x7C || Byte8 > 0x83)
                {
                    LastVal += Byte8;
                    *Dst++ = LastVal;
                    BlockLength--;
                    continue;
                }
                Byte8 -= 0x7C;
            }
            else if (BitDepth < 8)
            {
                Byte8 <<= 1;
                if (Byte8 < 0x78 || Byte8 > 0x86)
                {
                    LastVal += (int8_t)Byte8 >> (BitDepthInv & 0x1F); // arithmetic shift
                    *Dst++ = LastVal;
                    BlockLength--;
                    continue;
                }
                Byte8 = (Byte8 >> 1) - 0x3C;
            }
            else
            {
                Bytes16 &= 0x1FF;
                if ((Bytes16 & 0x100) == 0)
                {
                    LastVal += Byte8;
                    *Dst++ = LastVal;
                    BlockLength--;
                    continue;
                }
            }
        }

        Byte8++;
        if (Byte8 >= BitDepth)
            Byte8++;
        BitDepth = Byte8;

        BitDepthInv = 8;
        if (BitDepthInv < BitDepth)
            BitDepthInv++;
        BitDepthInv -= BitDepth;
    }
}

static bool LoadCompressed16BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded)
{
    int8_t *DstPtr = (int8_t *)s->Data;

    uint8_t *DecompBuffer = (uint8_t *)M4P_MALLOC(65536);
    if (DecompBuffer == NULL)
        return false;

    uint32_t i = s->Length;
    while (i > 0)
    {
        uint32_t BytesToUnpack = 32768;
        if (BytesToUnpack > i)
            BytesToUnpack = i;

        uint16_t PackedLen;
        mread(&PackedLen, sizeof(uint16_t), 1, m);
        mread(DecompBuffer, 1, PackedLen, m);

        Decompress16BitData((int16_t *)DstPtr, DecompBuffer, BytesToUnpack);

        if (DeltaEncoded)           // convert from delta values to PCM
        {
            int16_t *Ptr16     = (int16_t *)DstPtr;
            int16_t  LastSmp16 = 0; // yes, reset this every block!

            const uint32_t Length = BytesToUnpack >> 1;
            for (uint32_t j = 0; j < Length; j++)
            {
                LastSmp16 += Ptr16[j];
                Ptr16[j] = LastSmp16;
            }
        }

        DstPtr += BytesToUnpack;
        i -= BytesToUnpack;
    }

    if (Stereo) // added stereo support for custom HQ driver
    {
        DstPtr = (int8_t *)s->DataR;

        i = s->Length;
        while (i > 0)
        {
            uint32_t BytesToUnpack = 32768;
            if (BytesToUnpack > i)
                BytesToUnpack = i;

            uint16_t PackedLen;
            mread(&PackedLen, sizeof(uint16_t), 1, m);
            mread(DecompBuffer, 1, PackedLen, m);

            Decompress16BitData((int16_t *)DstPtr, DecompBuffer, BytesToUnpack);

            if (DeltaEncoded)           // convert from delta values to PCM
            {
                int16_t *Ptr16     = (int16_t *)DstPtr;
                int16_t  LastSmp16 = 0; // yes, reset this every block!

                const uint32_t Length = BytesToUnpack >> 1;
                for (uint32_t j = 0; j < Length; j++)
                {
                    LastSmp16 += Ptr16[j];
                    Ptr16[j] = LastSmp16;
                }
            }

            DstPtr += BytesToUnpack;
            i -= BytesToUnpack;
        }
    }

    M4P_FREE(DecompBuffer);
    return true;
}

static bool LoadCompressed8BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded)
{
    int8_t *DstPtr = (int8_t *)s->Data;

    uint8_t *DecompBuffer = (uint8_t *)M4P_MALLOC(65536);
    if (DecompBuffer == NULL)
        return false;

    uint32_t i = s->Length;
    while (i > 0)
    {
        uint32_t BytesToUnpack = 32768;
        if (BytesToUnpack > i)
            BytesToUnpack = i;

        uint16_t PackedLen;
        mread(&PackedLen, sizeof(uint16_t), 1, m);
        mread(DecompBuffer, 1, PackedLen, m);

        Decompress8BitData(DstPtr, DecompBuffer, BytesToUnpack);

        if (DeltaEncoded)        // convert from delta values to PCM
        {
            int8_t LastSmp8 = 0; // yes, reset this every block!
            for (uint32_t j = 0; j < BytesToUnpack; j++)
            {
                LastSmp8 += DstPtr[j];
                DstPtr[j] = LastSmp8;
            }
        }

        DstPtr += BytesToUnpack;
        i -= BytesToUnpack;
    }

    if (Stereo) // added stereo support for custom HQ driver
    {
        DstPtr = (int8_t *)s->DataR;

        i = s->Length;
        while (i > 0)
        {
            uint32_t BytesToUnpack = 32768;
            if (BytesToUnpack > i)
                BytesToUnpack = i;

            uint16_t PackedLen;
            mread(&PackedLen, sizeof(uint16_t), 1, m);
            mread(DecompBuffer, 1, PackedLen, m);

            Decompress8BitData(DstPtr, DecompBuffer, BytesToUnpack);

            if (DeltaEncoded)        // convert from delta values to PCM
            {
                int8_t LastSmp8 = 0; // yes, reset this every block!
                for (uint32_t j = 0; j < BytesToUnpack; j++)
                {
                    LastSmp8 += DstPtr[j];
                    DstPtr[j] = LastSmp8;
                }
            }

            DstPtr += BytesToUnpack;
            i -= BytesToUnpack;
        }
    }

    M4P_FREE(DecompBuffer);
    return true;
}

static bool LoadS3M(MEMFILE *m)
{
    uint8_t  DefPan;
    uint16_t Flags, SmpPtrs[100], PatPtrs[100];

    if (!mreadexact(m, Song.Header.SongName, 25))
        return false;
    mseek(m, 0x20, SEEK_SET);
    if (!mreadexact(m, &Song.Header.OrdNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.SmpNum, 2))
        return false;
    if (!mreadexact(m, &Song.Header.PatNum, 2))
        return false;
    if (!mreadexact(m, &Flags, 2))
        return false;

    mseek(m, 0x30, SEEK_SET);
    if (!mreadexact(m, &Song.Header.GlobalVol, 1))
        return false;
    if (!mreadexact(m, &Song.Header.InitialSpeed, 1))
        return false;
    if (!mreadexact(m, &Song.Header.InitialTempo, 1))
        return false;
    if (!mreadexact(m, &Song.Header.MixVolume, 1))
        return false;
    mseek(m, 1, SEEK_CUR);
    if (!mreadexact(m, &DefPan, 1))
        return false;

    if (Song.Header.SmpNum > 100)
        Song.Header.SmpNum = 100;

    if (Song.Header.PatNum > 100)
        Song.Header.PatNum = 100;

    Song.Header.Flags = ITF_OLD_EFFECTS;
    if (Flags & 8)
        Song.Header.Flags = ITF_VOL0_OPTIMIZATION;

    Song.Header.PanSep = 128;
    Song.Header.GlobalVol *= 2;

    if (Song.Header.MixVolume & 128)
    {
        Song.Header.Flags |= ITF_STEREO;
        Song.Header.MixVolume &= 127;
    }

    // OK, panning now...
    mseek(m, 64, SEEK_SET);
    for (int32_t i = 0; i < 32; i++)
    {
        uint8_t Pan;
        if (!mreadexact(m, &Pan, 1))
            return false;

        if (Pan >= 128)
        {
            Song.Header.ChnlPan[i] = 32 | 128; // 8bb: center + channel off
        }
        else
        {
            Pan &= 127;
            if (Pan <= 7)
                Song.Header.ChnlPan[i] = 0;
            else if (Pan <= 15)
                Song.Header.ChnlPan[i] = 64;
            else
                Song.Header.ChnlPan[i] = 32;
        }
    }

    // 8bb: set rest of channels to "off"
    for (int32_t i = 32; i < MAX_HOST_CHANNELS; i++)
        Song.Header.ChnlPan[i] = 32 | 128;

    for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
        Song.Header.ChnlVol[i] = 64;

    memset(Song.Orders, 255, MAX_ORDERS);
    if (!mreadexact(m, Song.Orders, Song.Header.OrdNum))
        return false; // Order list loaded.

    if (!mreadexact(m, SmpPtrs, Song.Header.SmpNum * 2))
        return false;
    if (!mreadexact(m, PatPtrs, Song.Header.PatNum * 2))
        return false;

    if (DefPan == 252) // 8bb: load custom channel pans, if present
    {
        for (int32_t i = 0; i < 32; i++)
        {
            uint8_t Pan;
            if (!mreadexact(m, &Pan, 1))
                return false;

            if (Pan & 32)
            {
                uint8_t ChannelOffFlag = Song.Header.ChnlPan[i] & 128;
                Song.Header.ChnlPan[i] = (((Pan & 15) << 2) + 2) | ChannelOffFlag;
            }
        }
    }

    // Load instruments (8bb: and data)
    sample_t *s = Song.Smp;
    for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
    {
        const uint32_t HeaderOffset = SmpPtrs[i] << 4;
        if (HeaderOffset == 0) // 8bb: added this check
            continue;

        mseek(m, HeaderOffset, SEEK_SET);

        uint8_t Type;
        mreadexact(m, &Type, 1);

        mreadexact(m, &s->DOSFilename, 12);

        uint8_t MemSegH;
        mreadexact(m, &MemSegH, 1);
        uint16_t MemSegL;
        mreadexact(m, &MemSegL, 2);

        mreadexact(m, &s->Length, 4);
        mreadexact(m, &s->LoopBegin, 4);
        mreadexact(m, &s->LoopEnd, 4);
        mreadexact(m, &s->Vol, 1);

        mseek(m, 2, SEEK_CUR);

        uint8_t SmpFlags;
        mreadexact(m, &SmpFlags, 1);

        mreadexact(m, &s->C5Speed, 4);

        mseek(m, 12, SEEK_CUR);
        mreadexact(m, &s->SampleName, 25);

        if (Type == 1)
        {
            if (SmpFlags & 2)
                s->Flags |= SMPF_STEREO;

            if ((s->Length & 0xFFFF) > 0)
                s->Flags |= SMPF_ASSOCIATED_WITH_HEADER;

            s->OffsetInFile = ((MemSegH << 16) | MemSegL) << 4;
        }

        if (SmpFlags & 1)
            s->Flags |= SMPF_USE_LOOP;

        if (SmpFlags & 4)
            s->Flags |= SMPF_16BIT;

        s->GlobVol = 64;
        s->DefPan  = 32;

        if (s->Flags & SMPF_ASSOCIATED_WITH_HEADER)
        {
            if (s->OffsetInFile != 0)                          // 8bb: added this check
            {
                bool Stereo      = !!(s->Flags & SMPF_STEREO); // 8bb: added stereo support
                bool Sample16Bit = !!(s->Flags & SMPF_16BIT);

                uint32_t SampleBytes = s->Length << Sample16Bit;

                if (!Music_AllocateSample(i, SampleBytes))
                    return false;

                if (Stereo)
                {
                    if (!Music_AllocateRightSample(i, SampleBytes))
                        return false;
                }

                mseek(m, s->OffsetInFile, SEEK_SET);
                if (!mreadexact(m, s->Data, SampleBytes))
                    return false;

                if (Stereo)
                {
                    if (!mreadexact(m, s->DataR, SampleBytes))
                        return false;
                }

                if (!Sample16Bit)
                {
                    // 8bb: convert from unsigned to signed
                    int8_t *Ptr8 = (int8_t *)s->Data;
                    for (uint32_t j = 0; j < s->Length; j++)
                        Ptr8[j] ^= 0x80;

                    if (Stereo)
                    {
                        Ptr8 = (int8_t *)s->DataR;
                        for (uint32_t j = 0; j < s->Length; j++)
                            Ptr8[j] ^= 0x80;
                    }
                }
                else
                {
                    // 8bb: Music_AllocateSample() also set s->Length, divide by two if 16-bit
                    s->Length >>= 1;

                    // 8bb: convert from unsigned to signed
                    int16_t *Ptr16 = (int16_t *)s->Data;
                    for (uint32_t j = 0; j < s->Length; j++)
                        Ptr16[j] ^= 0x8000;

                    if (Stereo)
                    {
                        Ptr16 = (int16_t *)s->DataR;
                        for (uint32_t j = 0; j < s->Length; j++)
                            Ptr16[j] ^= 0x8000;
                    }
                }
            }
        }
    }

    // Load patterns....
    pattern_t *p = Song.Patt;
    for (int32_t i = 0; i < Song.Header.PatNum; i++, p++)
    {
        const uint32_t PatternOffset = PatPtrs[i] << 4;
        if (PatternOffset == 0)
            continue;

        mseek(m, PatternOffset, SEEK_SET);

        uint16_t PackedPatLength;
        if (!mreadexact(m, &PackedPatLength, 2))
            return false;

        uint8_t *PackedData = (uint8_t *)M4P_MALLOC(PackedPatLength);
        if (PackedData == NULL)
            return false;

        if (!mreadexact(m, PackedData, PackedPatLength))
            return false;

        if (!TranslateS3MPattern(PackedData, i))
        {
            M4P_FREE(PackedData);
            return false;
        }

        M4P_FREE(PackedData);
    }

    return true;
}

static bool TranslateS3MPattern(uint8_t *Src, int32_t Pattern)
{
    PatternDataArea = (uint8_t *)M4P_MALLOC(MAX_HOST_CHANNELS * MAX_ROWS * 5);
    if (PatternDataArea == NULL)
        return false;

    // 8bb: clear destination pattern
    uint8_t *Ptr8 = PatternDataArea;
    for (int32_t i = 0; i < 200; i++)
    {
        for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Ptr8 += 5)
        {
            Ptr8[0] = 253; // note
            Ptr8[1] = 0;   // ins
            Ptr8[2] = 255; // vol
            Ptr8[3] = 0;   // cmd
            Ptr8[4] = 0;   // value
        }
    }

    uint8_t *OrigDst = PatternDataArea;
    for (int32_t i = 0; i < S3M_ROWS; i++)
    {
        while (true)
        {
            uint8_t Byte, Mask = *Src++;
            if (Mask == 0)
            {
                OrigDst += MAX_HOST_CHANNELS * 5; // 8bb: end of channels, go to next row
                break;
            }

            uint8_t *Dst = OrigDst + ((Mask & 31) * 5); // 8bb: aligned to current channel to write into

            // 8bb: Note and sample
            if (Mask & 32)
            {
                Byte = *Src++;
                if (Byte == 254)
                    Dst[0] = 254;
                else if (Byte <= 127)
                    Dst[0] = 12 + (((Byte >> 4) * 12) + (Byte & 0x0F)); // C5 is now central octave

                // Instrument
                Byte = *Src++;
                if (Byte <= 99)
                    Dst[1] = Byte;
                else
                    Dst[1] = 0;
            }

            // Volume
            if (Mask & 64)
            {
                Byte = *Src++;
                if (Byte != 255)
                {
                    if (Byte <= 64)
                        Dst[2] = Byte;
                    else
                        Dst[2] = 64;
                }
            }

            // 8bb: Effect + parameter
            if (Mask & 128)
            {
                Dst[3] = *Src++;
                Dst[4] = *Src++;

                if (Dst[3] == 'C' - '@')
                {
                    // 8bb: IT2's broken (?) way of converting between decimal/hex
                    Dst[4] = (Dst[4] & 0x0F) + ((Dst[4] & 0xF0) >> 1) + ((Dst[4] & 0xF0) >> 3);
                }
                else if (Dst[3] == 'V' - '@')
                {
                    if (Dst[4] < 128)
                        Dst[4] <<= 1;
                    else
                        Dst[4] = 255;
                }
                else if (Dst[3] == 'X' - '@')
                {
                    if (Dst[4] == 0xA4) // 8bb: surround
                    {
                        Dst[3] = 'S' - '@';
                        Dst[4] = 0x91;
                    }
                    else
                    {
                        if (Dst[4] < 128)
                            Dst[4] <<= 1;
                        else
                            Dst[4] = 255;
                    }
                }
                else if (Dst[3] == 'D' - '@')
                {
                    uint8_t lo = Dst[4] & 0x0F;
                    uint8_t hi = Dst[4] & 0xF0;

                    if (lo != 0 && hi != 0)
                    {
                        if (lo != 0x0F && hi != 0xF0)
                            Dst[4] &= 0x0F;
                    }
                }
            }
        }
    }

    bool result = StorePattern(S3M_ROWS, Pattern);

    M4P_FREE(PatternDataArea);
    return result;
}

static void ClearEncodingInfo(void)
{
    uint8_t *Enc = EncodingInfo;
    for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, Enc += 6)
    {
        Enc[0] = 0;   // mask
        Enc[1] = 253; // note
        Enc[2] = 0;   // ins
        Enc[3] = 255; // vol
        Enc[4] = 0;   // cmd
        Enc[5] = 0;   // value
    }
}

static bool GetPatternLength(uint16_t Rows, uint16_t *LengthOut)
{
    ClearEncodingInfo();

    uint8_t *Src   = PatternDataArea;
    uint32_t Bytes = Rows; // End of row bytes added.

    for (int32_t i = 0; i < Rows; i++)
    {
        uint8_t *Enc = EncodingInfo;
        for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Src += 5, Enc += 6)
        {
            if (Src[0] == 253 && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
                continue;

            Bytes++; // 1 byte for channel indication

            uint8_t Mask = 0;

            uint8_t Note = Src[0];
            if (Note != 253)
            {
                if (Enc[1] != Note)
                {
                    Enc[1] = Note;
                    Bytes++;
                    Mask |= 1;
                }
                else
                {
                    Mask |= 16;
                }
            }

            uint8_t Instr = Src[1];
            if (Instr != 0)
            {
                if (Enc[2] != Instr)
                {
                    Enc[2] = Instr;
                    Bytes++;
                    Mask |= 2;
                }
                else
                {
                    Mask |= 32;
                }
            }

            uint8_t Vol = Src[2];
            if (Vol != 255)
            {
                if (Enc[3] != Vol)
                {
                    Enc[3] = Vol;
                    Bytes++;
                    Mask |= 4;
                }
                else
                {
                    Mask |= 64;
                }
            }

            uint16_t EfxAndParam = *(uint16_t *)&Src[3];
            if (EfxAndParam != 0)
            {
                if (*(uint16_t *)&Enc[4] != EfxAndParam)
                {
                    *(uint16_t *)&Enc[4] = EfxAndParam;
                    Bytes += 2;
                    Mask |= 8;
                }
                else
                {
                    Mask |= 128;
                }
            }

            if (Mask != Enc[0])
            {
                Enc[0] = Mask;
                Bytes++;
            }
        }
    }

    if (Bytes > 65535)
        return false;

    *LengthOut = (uint16_t)Bytes;
    return true;
}

static void EncodePattern(pattern_t *p, uint8_t Rows)
{
    ClearEncodingInfo();

    p->Rows = Rows;

    uint8_t *Src = PatternDataArea;
    uint8_t *Dst = p->PackedData;

    for (int32_t i = 0; i < Rows; i++)
    {
        uint8_t *Enc = EncodingInfo;
        for (uint8_t ch = 0; ch < MAX_HOST_CHANNELS; ch++, Src += 5, Enc += 6)
        {
            if (Src[0] == 253 && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
                continue;

            uint8_t Mask = 0;

            uint8_t Note = Src[0];
            if (Note != 253)
            {
                if (Enc[1] != Note)
                {
                    Enc[1] = Note;
                    Mask |= 1;
                }
                else
                {
                    Mask |= 16;
                }
            }

            uint8_t Ins = Src[1];
            if (Src[1] != 0)
            {
                if (Enc[2] != Ins)
                {
                    Enc[2] = Ins;
                    Mask |= 2;
                }
                else
                {
                    Mask |= 32;
                }
            }

            uint8_t Vol = Src[2];
            if (Vol != 255)
            {
                if (Enc[3] != Vol)
                {
                    Enc[3] = Vol;
                    Mask |= 4;
                }
                else
                {
                    Mask |= 64;
                }
            }

            uint16_t EfxAndParam = *(uint16_t *)&Src[3];
            if (EfxAndParam != 0)
            {
                if (EfxAndParam != *(uint16_t *)&Enc[4])
                {
                    *(uint16_t *)&Enc[4] = EfxAndParam;
                    Mask |= 8;
                }
                else
                {
                    Mask |= 128;
                }
            }

            if (Enc[0] != Mask)
            {
                Enc[0] = Mask;

                *Dst++ = (ch + 1) | 128; // read another mask...
                *Dst++ = Mask;
            }
            else
            {
                *Dst++ = ch + 1;
            }

            if (Mask & 1)
                *Dst++ = Note;

            if (Mask & 2)
                *Dst++ = Ins;

            if (Mask & 4)
                *Dst++ = Vol;

            if (Mask & 8)
            {
                *(uint16_t *)Dst = EfxAndParam;
                Dst += 2;
            }
        }

        *Dst++ = 0;
    }
}

static bool StorePattern(uint8_t NumRows, int32_t Pattern)
{
    uint16_t PackedLength;
    if (!GetPatternLength(NumRows, &PackedLength))
        return false;

    if (!Music_AllocatePattern(Pattern, PackedLength))
        return false;

    EncodePattern(&Song.Patt[Pattern], NumRows);
    return true;
}

static int8_t GetModuleType(MEMFILE *m) // 8bb: added this
{
    static uint8_t Header[44 + 4];

    size_t OldOffset = mtell(m);

    mseek(m, 0, SEEK_END);
    size_t DataLen = mtell(m);
    mseek(m, 0, SEEK_SET);

    mread(Header, 1, sizeof(Header), m);

    int8_t Format = FORMAT_UNKNOWN;
    if (DataLen >= 4 && !memcmp(&Header[0], "IMPM", 4))
        Format = FORMAT_IT;
    else if (DataLen >= 44 + 4 && !memcmp(&Header[44], "SCRM", 4))
        Format = FORMAT_S3M;

    mseek(m, OldOffset, SEEK_SET);
    return Format;
}

bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen)
{
    bool WasCompressed = false;
    if (DataLen >= 4 + 4) // find out if module is MMCMP compressed
    {
        uint32_t Sig1 = *(uint32_t *)&Data[0];
        uint32_t Sig2 = *(uint32_t *)&Data[4];
        if (Sig1 == 0x4352697A && Sig2 == 0x61694E4F) // Sig1 = "ziRCONia"
        {
            if (unpackMMCMP(&Data, &DataLen))
                WasCompressed = true;
            else
                return false;
        }
    }

    MEMFILE *m = mopen(Data, DataLen);
    if (m == NULL)
        return false;

    if (FirstTimeLoading)
    {
        memset(&Song, 0, sizeof(Song));
        FirstTimeLoading = false;
    }
    else
    {
        Music_FreeSong();
    }

    bool WasLoaded = false;

    uint8_t Format = GetModuleType(m);
    if (Format != FORMAT_UNKNOWN)
    {
        Music_SetDefaultMIDIDataArea();
        switch (Format)
        {
        default:
            break;
        case FORMAT_IT:
            WasLoaded = LoadIT(m);
            break;
        case FORMAT_S3M:
            WasLoaded = LoadS3M(m);
            break;
        }
    }

    mclose(&m);
    if (WasCompressed)
        M4P_FREE(Data);

    if (WasLoaded)
    {
        SB16_SetMixVolume(Song.Header.MixVolume);
        SB16_FixSamples();

        Song.Loaded = true;
        return true;
    }
    else
    {
        Music_FreeSong();

        Song.Loaded = false;
        return false;
    }
}

//-----------------------------------------------------------------------------------
// 								Tables - Impulse Tracker 2
//-----------------------------------------------------------------------------------

/* Bit-accurate IT2 tables. Copied from IT2 source code.
** All of the comments in this file are written by me (8bitbubsy)
*/

/* Formula:
** for (i = 0; i < 10*12; i++)
**     LUT[i] = round(pow(2.0, (i - 5*12) / 12.0) * 65536.0);
*/
static const uint32_t PitchTable[120] = {
    2048,    2170,    2299,    2435,    2580,    2734,    2896,    3069,    3251,    3444,    3649,    3866,
    4096,    4340,    4598,    4871,    5161,    5468,    5793,    6137,    6502,    6889,    7298,    7732,
    8192,    8679,    9195,    9742,    10321,   10935,   11585,   12274,   13004,   13777,   14596,   15464,
    16384,   17358,   18390,   19484,   20643,   21870,   23170,   24548,   26008,   27554,   29193,   30929,
    32768,   34716,   36781,   38968,   41285,   43740,   46341,   49097,   52016,   55109,   58386,   61858,
    65536,   69433,   73562,   77936,   82570,   87480,   92682,   98193,   104032,  110218,  116772,  123715,
    131072,  138866,  147123,  155872,  165140,  174960,  185364,  196386,  208064,  220436,  233544,  247431,
    262144,  277732,  294247,  311744,  330281,  349920,  370728,  392772,  416128,  440872,  467088,  494862,
    524288,  555464,  588493,  623487,  660561,  699841,  741455,  785544,  832255,  881744,  934175,  989724,
    1048576, 1110928, 1176987, 1246974, 1321123, 1399681, 1482910, 1571089, 1664511, 1763488, 1868350, 1979448};

/* Formula:
** for (i = 0; i < 256; i++)
** {
**      LUT[(256*0)+i] = round(sin(i * (2.0 * M_PI / 256.0)) * 64.0); // sine
**      LUT[(256*1)+i] = (128 - i) >> 1;                              // ramp
**      LUT[(256*2)+i] = ((i ^ 255) & 128) >> 1;                      // square
** }
*/
static const int8_t FineSineData[3 * 256] = // sine/ramp/square
    {0,   2,   3,   5,   6,   8,   9,   11,  12,  14,  16,  17,  19,  20,  22,  23,  24,  26,  27,  29,  30,  32,
     33,  34,  36,  37,  38,  39,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
     56,  57,  58,  59,  59,  60,  60,  61,  61,  62,  62,  62,  63,  63,  63,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  63,  63,  63,  62,  62,  62,  61,  61,  60,  60,  59,  59,  58,  57,  56,  56,  55,  54,
     53,  52,  51,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,  39,  38,  37,  36,  34,  33,  32,  30,  29,
     27,  26,  24,  23,  22,  20,  19,  17,  16,  14,  12,  11,  9,   8,   6,   5,   3,   2,   0,   -2,  -3,  -5,
     -6,  -8,  -9,  -11, -12, -14, -16, -17, -19, -20, -22, -23, -24, -26, -27, -29, -30, -32, -33, -34, -36, -37,
     -38, -39, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -55, -56, -56, -57, -58, -59,
     -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64, -64,
     -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59, -59, -58, -57, -56, -56, -55, -54, -53, -52, -51, -50,
     -49, -48, -47, -46, -45, -44, -43, -42, -41, -39, -38, -37, -36, -34, -33, -32, -30, -29, -27, -26, -24, -23,
     -22, -20, -19, -17, -16, -14, -12, -11, -9,  -8,  -6,  -5,  -3,  -2,

     64,  63,  63,  62,  62,  61,  61,  60,  60,  59,  59,  58,  58,  57,  57,  56,  56,  55,  55,  54,  54,  53,
     53,  52,  52,  51,  51,  50,  50,  49,  49,  48,  48,  47,  47,  46,  46,  45,  45,  44,  44,  43,  43,  42,
     42,  41,  41,  40,  40,  39,  39,  38,  38,  37,  37,  36,  36,  35,  35,  34,  34,  33,  33,  32,  32,  31,
     31,  30,  30,  29,  29,  28,  28,  27,  27,  26,  26,  25,  25,  24,  24,  23,  23,  22,  22,  21,  21,  20,
     20,  19,  19,  18,  18,  17,  17,  16,  16,  15,  15,  14,  14,  13,  13,  12,  12,  11,  11,  10,  10,  9,
     9,   8,   8,   7,   7,   6,   6,   5,   5,   4,   4,   3,   3,   2,   2,   1,   1,   0,   0,   -1,  -1,  -2,
     -2,  -3,  -3,  -4,  -4,  -5,  -5,  -6,  -6,  -7,  -7,  -8,  -8,  -9,  -9,  -10, -10, -11, -11, -12, -12, -13,
     -13, -14, -14, -15, -15, -16, -16, -17, -17, -18, -18, -19, -19, -20, -20, -21, -21, -22, -22, -23, -23, -24,
     -24, -25, -25, -26, -26, -27, -27, -28, -28, -29, -29, -30, -30, -31, -31, -32, -32, -33, -33, -34, -34, -35,
     -35, -36, -36, -37, -37, -38, -38, -39, -39, -40, -40, -41, -41, -42, -42, -43, -43, -44, -44, -45, -45, -46,
     -46, -47, -47, -48, -48, -49, -49, -50, -50, -51, -51, -52, -52, -53, -53, -54, -54, -55, -55, -56, -56, -57,
     -57, -58, -58, -59, -59, -60, -60, -61, -61, -62, -62, -63, -63, -64,

     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

/* Formula:
** for (i = 0; i < 16; i++)
**     LUT[i] = round(pow(2.0, i / 768.0) * 65536.0);
*/
static const uint32_t FineLinearSlideUpTable[16] = {65536, 65595, 65654, 65714, 65773, 65832, 65892, 65951,
                                                    66011, 66071, 66130, 66190, 66250, 66309, 66369, 66429};

/* Formula (can't be used to recreate LUT):
** for (i = 0; i < 257; i++)
**     LUT[i] = round(pow(2.0, i / 192.0) * 65536.0);
**
** Note:
**  The following entries have unexplainable errors: 21,35,108,152,201,243
*/
static const uint32_t LinearSlideUpTable[257] = {
    65536,  65773,  66011,  66250,  66489,  66730,  66971,  67213,  67456,  67700,  67945,  68191,  68438,  68685,
    68933,  69183,  69433,  69684,  69936,  70189,  70443,  70693,  70953,  71210,  71468,  71726,  71985,  72246,
    72507,  72769,  73032,  73297,  73562,  73828,  74095,  73563,  74632,  74902,  75172,  75444,  75717,  75991,
    76266,  76542,  76819,  77096,  77375,  77655,  77936,  78218,  78501,  78785,  79069,  79355,  79642,  79930,
    80220,  80510,  80801,  81093,  81386,  81681,  81976,  82273,  82570,  82869,  83169,  83469,  83771,  84074,
    84378,  84683,  84990,  85297,  85606,  85915,  86226,  86538,  86851,  87165,  87480,  87796,  88114,  88433,
    88752,  89073,  89396,  89719,  90043,  90369,  90696,  91024,  91353,  91684,  92015,  92348,  92682,  93017,
    93354,  93691,  94030,  94370,  94711,  95054,  95398,  95743,  96089,  96436,  96784,  97135,  97487,  97839,
    98193,  98548,  98905,  99262,  99621,  99982,  100343, 100706, 101070, 101436, 101803, 102171, 102540, 102911,
    103283, 103657, 104032, 104408, 104786, 105165, 105545, 105927, 106310, 106694, 107080, 107468, 107856, 108246,
    108638, 109031, 109425, 109821, 110218, 110617, 111017, 111418, 111821, 112226, 112631, 113039, 113453, 113858,
    114270, 114683, 115098, 115514, 115932, 116351, 116772, 117194, 117618, 118043, 118470, 118899, 119329, 119760,
    120194, 120628, 121065, 121502, 121942, 122383, 122825, 123270, 123715, 124163, 124612, 125063, 125515, 125969,
    126425, 126882, 127341, 127801, 128263, 128727, 129193, 129660, 130129, 130600, 131072, 131546, 132022, 132499,
    132978, 133459, 133942, 134427, 134913, 135399, 135890, 136382, 136875, 137370, 137867, 138366, 138866, 139368,
    139872, 140378, 140886, 141395, 141907, 142420, 142935, 143452, 143971, 144491, 145014, 145539, 146065, 146593,
    147123, 147655, 148189, 148725, 149263, 149803, 150345, 150889, 151434, 151982, 152532, 153083, 153637, 154193,
    154750, 155310, 155872, 156435, 157001, 156569, 158139, 158711, 159285, 159861, 160439, 161019, 161602, 162186,
    162773, 163361, 163952, 164545, 165140};

/* Formula (can't be used to recreate LUT):
** for (i = 0; i < 16; i++)
** {
**      x = round(pow(2.0, i / -768.0) * 65536.0);
**      if (x > 65535)
**          x = 65535;
**
**      LUT[i] = x;
** }
**
** Note:
**  The following entries have unexplainable errors: 7,11,15
*/
static const uint16_t FineLinearSlideDownTable[16] = {65535, 65477, 65418, 65359, 65300, 65241, 65182, 65359,
                                                      65065, 65006, 64947, 64888, 64830, 64772, 64713, 64645};

/* Formula (can't be used to recreate LUT):
** for (i = 0; i < 257; i++)
** {
**      x = round(pow(2.0, i / -192.0) * 65536.0);
**      if (x > 65535)
**          x = 65535;
**
**      LUT[i] = x;
** }
**
** Note:
**  The following entries have unexplainable errors: 85,132,133,135,214
*/
static const uint16_t LinearSlideDownTable[257] = {
    65535, 65300, 65065, 64830, 64596, 64364, 64132, 63901, 63670, 63441, 63212, 62984, 62757, 62531, 62306, 62081,
    61858, 61635, 61413, 61191, 60971, 60751, 60532, 60314, 60097, 59880, 59664, 59449, 59235, 59022, 58809, 58597,
    58386, 58176, 57966, 57757, 57549, 57341, 57135, 56929, 56724, 56519, 56316, 56113, 55911, 55709, 55508, 55308,
    55109, 54910, 54713, 54515, 54319, 54123, 53928, 53734, 53540, 53347, 53155, 52963, 52773, 52582, 52393, 52204,
    52016, 51829, 51642, 51456, 51270, 51085, 50901, 50718, 50535, 50353, 50172, 49991, 49811, 49631, 49452, 49274,
    49097, 48920, 48743, 48568, 48393, 48128, 48044, 47871, 47699, 47527, 47356, 47185, 47015, 46846, 46677, 46509,
    46341, 46174, 46008, 45842, 45677, 45512, 45348, 45185, 45022, 44859, 44698, 44537, 44376, 44216, 44057, 43898,
    43740, 43582, 43425, 43269, 43113, 42958, 42803, 42649, 42495, 42342, 42189, 42037, 41886, 41735, 41584, 41434,
    41285, 41136, 40988, 40840, 40639, 40566, 40400, 40253, 40110, 39965, 39821, 39678, 39535, 39392, 39250, 39109,
    38968, 38828, 38688, 38548, 38409, 38271, 38133, 37996, 37859, 37722, 37586, 37451, 37316, 37181, 37047, 36914,
    36781, 36648, 36516, 36385, 36254, 36123, 35993, 35863, 35734, 35605, 35477, 35349, 35221, 35095, 34968, 34842,
    34716, 34591, 34467, 34343, 34219, 34095, 33973, 33850, 33728, 33607, 33486, 33365, 33245, 33125, 33005, 32887,
    32768, 32650, 32532, 32415, 32298, 32182, 32066, 31950, 31835, 31720, 31606, 31492, 31379, 31266, 31153, 31041,
    30929, 30817, 30706, 30596, 30485, 30376, 30226, 30157, 30048, 29940, 29832, 29725, 29618, 29511, 29405, 29299,
    29193, 29088, 28983, 28879, 28774, 28671, 28567, 28464, 28362, 28260, 28158, 28056, 27955, 27855, 27754, 27654,
    27554, 27455, 27356, 27258, 27159, 27062, 26964, 26867, 26770, 26674, 26577, 26482, 26386, 26291, 26196, 26102,
    26008};

//-----------------------------------------------------------------------------------
// 							Enumerations - Mod4Play
//-----------------------------------------------------------------------------------

enum
{
    M4P_FORMAT_UNKNOWN = 0,
    M4P_FORMAT_IT_S3M  = 1,
    M4P_FORMAT_XM_MOD  = 2
};

//-----------------------------------------------------------------------------------
// 							Declarations - Mod4Play
//-----------------------------------------------------------------------------------

static int current_format = M4P_FORMAT_UNKNOWN;

//-----------------------------------------------------------------------------------
// 							Implementation - Mod4Play
//-----------------------------------------------------------------------------------

int m4p_TestFromData(uint8_t *Data, uint32_t DataLen)
{
    if ((DataLen >= 4 && (Data[0] == 'I' && Data[1] == 'M' && Data[2] == 'P' && Data[3] == 'M')) ||
        (DataLen >= 48 && (Data[44] == 'S' && Data[45] == 'C' && Data[46] == 'R' && Data[47] == 'M')))
    {
        return M4P_FORMAT_IT_S3M;
    }
    if (DataLen >= 17)
    {
        bool        is_xm_mod = true;
        const char *hdrtxt    = "Extended Module:";
        for (int i = 0; i < 16; i++)
        {
            if (Data[i] != *hdrtxt++)
            {
                is_xm_mod = false;
                break;
            }
        }
        if (is_xm_mod)
            return M4P_FORMAT_XM_MOD;
    }
    if (DataLen >= 1084)
    {
        for (uint8_t i = 0; i < 16; i++)
        {
            if (Data[1080] == MODSig[i][0] && Data[1081] == MODSig[i][1] && Data[1082] == MODSig[i][2] &&
                Data[1083] == MODSig[i][3])
                return M4P_FORMAT_XM_MOD;
        }
    }

    return M4P_FORMAT_UNKNOWN;
}

bool m4p_LoadFromData(uint8_t *Data, uint32_t DataLen, int32_t mixingFrequency, int32_t mixingBufferSize)
{
    current_format = m4p_TestFromData(Data, DataLen);

    if (current_format == M4P_FORMAT_IT_S3M)
    {
        if (Music_Init(mixingFrequency))
            return Music_LoadFromData(Data, DataLen);
        else
            return false;
    }
    else if (current_format == M4P_FORMAT_XM_MOD)
    {
        if (initMusic(mixingFrequency, mixingBufferSize, true, true))
            return loadMusicFromData(Data, DataLen);
        else
            return false;
    }

    return false;
}

void m4p_PlaySong(void)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_PlaySong(0);
    else
        startPlaying();
}

void m4p_GenerateSamples(int16_t *buffer, int32_t numSamples)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_FillAudioBuffer(buffer, numSamples);
    else
        mix_UpdateBuffer(buffer, numSamples);
}

void m4p_GenerateFloatSamples(float *buffer, int32_t numSamples)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_FillAudioBufferFloat(buffer, numSamples);
    else
        mix_UpdateBufferFloat(buffer, numSamples);
}

void m4p_Stop(void)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_Stop();
    else
        stopPlaying();
}

void m4p_Close(void)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_Close();
    else
        stopMusic();

    current_format = M4P_FORMAT_UNKNOWN;
}

void m4p_FreeSong(void)
{
    if (current_format == M4P_FORMAT_IT_S3M)
        Music_FreeSong();
    else
        freeMusic();
}

#ifdef __cplusplus
}
#endif

#endif