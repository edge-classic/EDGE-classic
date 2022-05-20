//------------------------------------------------------------------------
//  EPI Vector (point) types
//------------------------------------------------------------------------
//
//  Copyright (c) 2004-2005  The EDGE Team.
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

#include "fxp_vector.h"
#include "str_format.h"


namespace epi
{

fix_c xvec2_c::Length() const
{
	return fxdist2(x, y);
}

fix_c xvec2_c::PerpDist(const xvec2_c& point) const
{
	return (point.x * y - point.y * x) / fxdist2(x, y);
}

fix_c xvec2_c::AlongDist(const xvec2_c& point) const
{
	// same as the dot-product with length adjustment
    return (*this * point) / fxdist2(x, y);
}

std::string xvec2_c::ToStr(int precision) const
{
	return STR_Format("(%1.*f,%1.*f,%1.*f)",
				 precision, x.ToFloat(),
				 precision, y.ToFloat());
}

//------------------------------------------------------------------------

fix_c xvec3_c::Length() const
{
	return fxdist3(x, y, z);
}

fix_c xvec3_c::Slope() const
{
	fix_c dist (fxdist2(x, y));
	fix_c SM_Z (fxabs(z) >> 14);

	// prevent overflow or division by zero
	if (dist < SM_Z)
		dist = SM_Z;

	return z / dist;
}

fix_c xvec3_c::ApproxSlope() const
{
	fix_c ax (fxabs(x));
	fix_c ay (fxabs(y));

	// approximate distance
	fix_c dist ((ax > ay) ? (ax + ay >> 1) : (ay + ax >> 1));
	fix_c SM_Z (fxabs(z) >> 14);

	// prevent overflow or division by zero
	if (dist < SM_Z)
		dist = SM_Z;

	return z / dist;
}

fix_c xvec3_c::AlongDist(const xvec3_c& point) const
{
	// same as the dot-product with length adjustment
	return (*this * point) / Length();
}

xvec3_c xvec3_c::Cross(const xvec3_c& rhs) const
{
	return xvec3_c(y * rhs.z - z * rhs.y,
	              z * rhs.x - x * rhs.z,
	              x * rhs.y - y * rhs.x);
}

std::string xvec3_c::ToStr(int precision) const
{
	return STR_Format("(%1.*f,%1.*f,%1.*f)",
				 precision, x.ToFloat(),
				 precision, y.ToFloat(),
				 precision, z.ToFloat());
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
