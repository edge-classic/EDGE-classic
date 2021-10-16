//------------------------------------------------------------------------
//  EPI Table-based Random numbers
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2008  The EDGE Team.
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

#ifndef __EPI_MATH_RANDOM_H__
#define __EPI_MATH_RANDOM_H__

#include "math_angle.h"

namespace epi
{

#define RAND_TABLE_SIZE  2048
  
class table_rand_c
{
private:
	u32_t raw;

	static const byte random_data[RAND_TABLE_SIZE];

public:
	 table_rand_c() : raw(0) { }
	~table_rand_c() { }

	// random value from 0 to 255
	inline byte Byte()
	{
		raw++;

		unsigned int idx = ((raw >> 10) | 1) * raw;

		return random_data[idx & (RAND_TABLE_SIZE-1)];
	}

	inline short Short()
	{
		short A = Byte();
		short B = Byte();

		return A | (short)(B << 8);
	}

	// random value from -255 to +255 (skewed: values near 0 more likely)
	inline short NegPos()
	{
		short A = Byte();
		short B = Byte();

		return A - B;
	}

	inline angle_c Angle()
	{
		return angle_c(Byte() * 358 / 255);
	}

	void Seed(unsigned int seed)
	{
		raw = seed;
	}
};

}  // namespace ut

#endif /* __EPI_MATH_RANDOM_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
