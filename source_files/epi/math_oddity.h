//------------------------------------------------------------------------
//  Oddball stuff
//------------------------------------------------------------------------
// 
//  Copyright (c) 2005-2008  The EDGE Team.
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

#ifndef __MATH_ODDITY_H__
#define __MATH_ODDITY_H__

namespace epi
{

int int_sqrt(int value);

/* Thomas Wang's 32-bit Mix function */
inline u32_t int_hash(u32_t key)
{
	key += ~(key << 15);
	key ^=  (key >> 10);
	key +=  (key << 3);
	key ^=  (key >> 6);
	key += ~(key << 11);
	key ^=  (key >> 16);

	return key;
}

inline u32_t str_hash(const char *str)
{
	u32_t hash = 0;

	if (str) while (*str) hash = (hash << 5) - hash + *str++;

	return hash;
}

} // namespace epi

#endif /* __MATH_ODDITY_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
