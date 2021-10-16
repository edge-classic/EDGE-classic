//------------------------------------------------------------------------
//  EPI Matrix types
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

#ifndef __EPI_MATH_MATRIX_H__
#define __EPI_MATH_MATRIX_H__

#include "math_vector.h"

namespace epi
{

class mat3_c
{
	/* sealed class, by-reference semantics */

public:
	// order of coefficients is column major, like in OpenGL:
	// ( m0  m3  m6 )
	// ( m1  m4  m7 )
	// ( m2  m5  m8 )

	float m[9];

public:
	mat3_c();  // default value is the Identity matrix
	mat3_c(const mat3_c& rhs);

	/* ---- read-only operations ---- */

	vec2_c operator* (const vec2_c& rhs) const;

	std::string ToStr() const;

	/* ---- modifying operations ---- */

	mat3_c& Negate();
	mat3_c& Transpose();

	mat3_c& operator= (const mat3_c& rhs);

	mat3_c& operator+= (const mat3_c& rhs);
	mat3_c& operator-= (const mat3_c& rhs);
	mat3_c& operator*= (const mat3_c& rhs);

	mat3_c& operator*= (float scale);
	mat3_c& operator/= (float scale);

private:
	inline void swap(int i, int j)
	{
		float tmp = m[i]; m[i] = m[j]; m[j] = tmp;
	}
};

class mat4_c
{
	/* sealed class, by-reference semantics */

public:
	// order of coefficients is column major, like in OpenGL:
	// ( m0  m4  m8   m12 )
	// ( m1  m5  m9   m13 )
	// ( m2  m6  m10  m14 )
	// ( m3  m7  m11  m15 )

	float m[16];

public:
	mat4_c();  // default value is the Identity matrix
	mat4_c(const mat4_c& rhs);
	mat4_c(const mat3_c& rhs, float w = 1.0);

	/* ---- read-only operations ---- */

	vec3_c operator* (const vec3_c& rhs) const;

	std::string ToStr() const;

	/* ---- modifying operations ---- */

	mat4_c& Negate();
	mat4_c& Transpose();

	mat4_c& operator= (const mat4_c& rhs);

	mat4_c& operator+= (const mat4_c& rhs);
	mat4_c& operator-= (const mat4_c& rhs);
	mat4_c& operator*= (const mat4_c& rhs);

	mat4_c& operator*= (float scale);
	mat4_c& operator/= (float scale);

private:
	inline void swap(int i, int j)
	{
		float tmp = m[i]; m[i] = m[j]; m[j] = tmp;
	}
};

//------------------------------------------------------------------------
//  IMPLEMENTATION
//------------------------------------------------------------------------

inline vec2_c mat3_c::operator* (const vec2_c& rhs) const
{
	return vec2_c(m[0] * rhs.x + m[3] * rhs.y + m[6],
	              m[1] * rhs.x + m[4] * rhs.y + m[7]);
}

inline vec3_c mat4_c::operator* (const vec3_c& rhs) const
{
	return vec3_c(m[0] * rhs.x + m[4] * rhs.y + m[8]  * rhs.z + m[12],
	              m[1] * rhs.x + m[5] * rhs.y + m[9]  * rhs.z + m[13],
	              m[2] * rhs.x + m[6] * rhs.y + m[10] * rhs.z + m[14]);
}

}  // namespace epi

#endif /* __EPI_MATH_MATRIX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
