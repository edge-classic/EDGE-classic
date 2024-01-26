//------------------------------------------------------------------------
//  EDGE Endian handling
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
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

#ifndef __EPI_ENDIAN_H__
#define __EPI_ENDIAN_H__

// the two types of endianness
#define EPI_LIL_ENDIAN 1234
#define EPI_BIG_ENDIAN 4321

#if defined(__LITTLE_ENDIAN__) || defined(__i386__) || defined(__ia64__) || defined(WIN32) || defined(__alpha__) ||    \
    defined(__alpha) || defined(__arm__) || (defined(__mips__) && defined(__MIPSEL__)) || defined(__SYMBIAN32__) ||    \
    defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__)
#define EPI_BYTEORDER EPI_LIL_ENDIAN
#else
#define EPI_BYTEORDER EPI_BIG_ENDIAN
#endif

// The macros used to swap values.  Try to use superfast macros on systems
// that support them, otherwise use C++ inline functions.
#if defined(__GNUC__) || defined(__clang__)
#define __Swap16 __builtin_bswap16
#define __Swap32 __builtin_bswap32
#define __Swap64 __builtin_bswap64

#elif defined(_MSC_VER)
#define __Swap16 _byteswap_ushort
#define __Swap32 _byteswap_ulong
#define __Swap64 _byteswap_uint64

#else
static inline uint16_t __Swap16(uint16_t n)
{
    uint16_t a;
    a = (n & 0xFF) << 8;
    a |= (n >> 8) & 0xFF;
    return a;
}
static inline uint32_t __Swap32(uint32_t n)
{
    uint32_t a;
    a = (n & 0xFFU) << 24;
    a |= (n & 0xFF00U) << 8;
    a |= (n >> 8) & 0xFF00U;
    a |= (n >> 24) & 0xFFU;
    return a;
}
static inline uint64_t __Swap64(uint64_t n)
{
    uint64_t a;
    a = (n & 0xFFULL) << 56;
    a |= (n & 0xFF00ULL) << 40;
    a |= (n & 0xFF0000ULL) << 24;
    a |= (n & 0xFF000000ULL) << 8;
    a |= (n >> 8) & 0xFF000000ULL;
    a |= (n >> 24) & 0xFF0000ULL;
    a |= (n >> 40) & 0xFF00ULL;
    a |= (n >> 56) & 0xFFULL;
    return a;
}
#endif

// Byteswap item between the specified endianness to the native endianness
#if EPI_BYTEORDER == EPI_LIL_ENDIAN
#define EPI_LE_U16(x) ((uint16_t)(x))
#define EPI_LE_U32(x) ((uint32_t)(x))
#define EPI_LE_U64(x) ((u64_t)(x))
#define EPI_BE_U16(x) __Swap16(x)
#define EPI_BE_U32(x) __Swap32(x)
#define EPI_BE_U64(x) __Swap64(x)
#else
#define EPI_LE_U16(x) __Swap16(x)
#define EPI_LE_U32(x) __Swap32(x)
#define EPI_LE_U64(x) __Swap64(x)
#define EPI_BE_U16(x) ((uint16_t)(x))
#define EPI_BE_U32(x) ((uint32_t)(x))
#define EPI_BE_U64(x) ((u64_t)(x))
#endif

namespace epi
{
class endian_swapper_c
{
  public:
    static uint16_t Swap16(uint16_t x);
    static uint32_t Swap32(uint32_t x);
};

// Swap 16bit, that is, MSB and LSB byte.
inline uint16_t endian_swapper_c::Swap16(uint16_t x)
{
    // No masking with 0xFF should be necessary.
    return (x >> 8) | (x << 8);
}

// Swapping 32bit.
inline uint32_t endian_swapper_c::Swap32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

// Get LE/BE values from pointer regardless of buffer alignment
inline uint16_t GetU16LE(const uint8_t *p)
{
    return EPI_LE_U16(p[1] << 8 | p[0]);
}

inline int16_t GetS16LE(const uint8_t *p)
{
    return (int16_t)GetU16LE(p);
}

inline uint32_t GetU32LE(const uint8_t *p)
{
    return EPI_LE_U32(p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0]);
}

inline int32_t GetS32LE(const uint8_t *p)
{
    return (int32_t)GetU32LE(p);
}

inline uint16_t GetU16BE(const uint8_t *p)
{
    return EPI_BE_U16(p[0] << 8 | p[1]);
}

inline int16_t GetS16BE(const uint8_t *p)
{
    return (int16_t)GetU16BE(p);
}

inline uint32_t GetU32BE(const uint8_t *p)
{
    return EPI_BE_U32(p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
}

inline int32_t GetS32BE(const uint8_t *p)
{
    return (int32_t)GetU32BE(p);
}

} // namespace epi


// signed versions of the above
#define EPI_LE_S16(x) ((int16_t)EPI_LE_U16((uint16_t)(x)))
#define EPI_LE_S32(x) ((int32_t)EPI_LE_U32((uint32_t)(x)))
#define EPI_LE_S64(x) ((s64_t)EPI_LE_U64((u64_t)(x)))

#define EPI_BE_S16(x) ((int16_t)EPI_BE_U16((uint16_t)(x)))
#define EPI_BE_S32(x) ((int32_t)EPI_BE_U32((uint32_t)(x)))
#define EPI_BE_S64(x) ((s64_t)EPI_BE_U64((u64_t)(x)))

#endif /* __EPI_ENDIAN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
