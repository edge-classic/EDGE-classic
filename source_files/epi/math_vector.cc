//------------------------------------------------------------------------
//  EPI Vector (point) types
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

#include "epi.h"
#include "math_vector.h"

#define SMALL_DIST  0.01f

namespace epi
{

float ivec_c::Length() const
{
	return sqrtf(x * x + y * y);
}

//------------------------------------------------------------------------

float vec2_c::Length() const
{
	return sqrtf(x * x + y * y);
}

float vec2_c::PerpDist(const vec2_c& point) const
{
	return (point.x * y - point.y * x) / sqrtf(x * x + y * y);
}

float vec2_c::AlongDist(const vec2_c& point) const
{
	// same as the dot-product with length adjustment
    return (point.x * x + point.y * y) / sqrtf(x * x + y * y);
}

//------------------------------------------------------------------------

float vec3_c::Length() const
{
	return sqrtf(x * x + y * y + z * z);
}

float vec3_c::Slope() const
{
	float dist = sqrtf(x * x + y * y);

	// kludge to prevent division by zero
	if (dist < SMALL_DIST)
		dist = SMALL_DIST;

	return z / dist;
}

float vec3_c::ApproxSlope() const
{
	float ax = ABS(x);
	float ay = ABS(y);

	// approximate distance
	float dist = (ax > ay) ? (ax + ay / 2) : (ay + ax / 2);

	// kludge to prevent division by zero
	if (dist < SMALL_DIST)
		dist = SMALL_DIST;

	return z / dist;
}

float vec3_c::AlongDist(const vec3_c& point) const
{
	// same as the dot-product with length adjustment
	return (*this * point) / Length();
}

vec3_c vec3_c::Cross(const vec3_c& rhs) const
{
	return vec3_c(y * rhs.z - z * rhs.y,
	              z * rhs.x - x * rhs.z,
	              x * rhs.y - y * rhs.x);
}

}  // namespace epi


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
