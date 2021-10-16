//------------------------------------------------------------------------
//  EDGE CRC : Cyclic Rendundancy Check
//------------------------------------------------------------------------
// 
//  Copyright (c) 2003-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------
//
//  Based on the Adler-32 algorithm as described in RFC-1950.
//
//------------------------------------------------------------------------

#ifndef __EPI_CRC_H__
#define __EPI_CRC_H__

namespace epi
{
	class crc32_c
	{
		/* sealed */
	
	public:
		u32_t crc;
	
		crc32_c()  { Reset(); }
		crc32_c(const crc32_c &rhs) { crc = rhs.crc; }
		~crc32_c() { }
	
		crc32_c& operator= (const crc32_c &rhs) { crc = rhs.crc; return *this; }

		crc32_c& operator+= (byte value);
		crc32_c& operator+= (s32_t value);
		crc32_c& operator+= (u32_t value);				
		crc32_c& operator+= (float value);

//		bool operator== (const crc32_c &rhs) const { return crc == rhs.crc; }
//		bool operator!= (const crc32_c &rhs) const { return crc != rhs.crc; }
	
		crc32_c& AddBlock(const byte *data, int len);
		crc32_c& AddCStr(const char *str);
		
		void Reset(void) { crc = 1; }
	};
};
#endif  /* __EPI_CRC_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
