//------------------------------------------------------------------------
//  EDGE Endian handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public License
//  (LGPL) as published by the Free Software Foundation.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------
//
//  Based on SDL_byteorder.h and SDL_endian.h.
//
//------------------------------------------------------------------------

#ifndef __EPI_ENDIAN_H__
#define __EPI_ENDIAN_H__

// the two types of endianness
#define EPI_LIL_ENDIAN  1234
#define EPI_BIG_ENDIAN  4321

#if defined(__LITTLE_ENDIAN__) ||  \
    defined(__i386__)  || defined(__ia64__) || defined(WIN32)   ||  \
	defined(__alpha__) || defined(__alpha)  || defined(__arm__) ||  \
	(defined(__mips__) && defined(__MIPSEL__)) ||  \
	defined(__SYMBIAN32__) || defined(__x86_64__)
#define EPI_BYTEORDER   EPI_LIL_ENDIAN
#else
#define EPI_BYTEORDER   EPI_BIG_ENDIAN
#endif

// The macros used to swap values.  Try to use superfast macros on systems
// that support them, otherwise use C++ inline functions.
#ifdef LINUX
#include <endian.h>
#ifdef __arch__swab16
#define EPI_Swap16  __arch__swab16
#endif
#ifdef __arch__swab32
#define EPI_Swap32  __arch__swab32
#endif
#endif /* LINUX */

#ifndef EPI_Swap16
#define EPI_Swap16  epi::endian_swapper_c::Swap16
#endif

#ifndef EPI_Swap32
#define EPI_Swap32  epi::endian_swapper_c::Swap32
#endif


// Byteswap item between the specified endianness to the native endianness
#if EPI_BYTEORDER == EPI_LIL_ENDIAN
#define EPI_LE_U16(X)  ((u16_t)(X))
#define EPI_LE_U32(X)  ((u32_t)(X))
#define EPI_BE_U16(X)  EPI_Swap16(X)
#define EPI_BE_U32(X)  EPI_Swap32(X)
#else
#define EPI_LE_U16(X)  EPI_Swap16(X)
#define EPI_LE_U32(X)  EPI_Swap32(X)
#define EPI_BE_U16(X)  ((u16_t)(X))
#define EPI_BE_U32(X)  ((u32_t)(X))
#endif

#define EPI_LE_S16(X)  ((s16_t) EPI_LE_U16((u16_t) (X)))
#define EPI_LE_S32(X)  ((s32_t) EPI_LE_U32((u32_t) (X)))
#define EPI_BE_S16(X)  ((s16_t) EPI_BE_U16((u16_t) (X)))
#define EPI_BE_S32(X)  ((s32_t) EPI_BE_U32((u32_t) (X)))

namespace epi
{
	class endian_swapper_c
	{
	public:
		static u16_t Swap16(u16_t x);
		static u32_t Swap32(u32_t x);
	};
	
	// Swap 16bit, that is, MSB and LSB byte.
	inline u16_t endian_swapper_c::Swap16(u16_t x)
	{
		// No masking with 0xFF should be necessary.
		return (x >> 8) | (x << 8);
	}
	
	// Swapping 32bit.
	inline u32_t endian_swapper_c::Swap32(u32_t x)
	{
		return
			(x >> 24)
			| ((x >> 8) & 0xff00)
			| ((x << 8) & 0xff0000)
			| (x << 24);
	}

} // namespace epi

#endif  /* __EPI_ENDIAN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
