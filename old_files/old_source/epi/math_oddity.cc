//------------------------------------------------------------------------
//  Oddball stuff
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

#include "epi.h"

#include "math_oddity.h"

namespace epi
{

int int_sqrt(int value)
{
    /* Integer sqrt routine ("divide-and-average") */

	if (value < 0)
		I_Error("epi::int_sqrt : Negative value!\n");

	if (value < 2)
		return value;

    int est = value >> 1;

    /* Unrolled loop (needs 18 iterations) */
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;

	if (est * est > value) est--;

    return est;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
