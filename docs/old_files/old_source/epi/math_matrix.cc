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

#include "epi.h"

#include "math_matrix.h"
#include "str_format.h"


namespace epi
{

mat3_c::mat3_c()
{
	m[0]=1;  m[3]=0;  m[6]=0;
	m[1]=0;  m[4]=1;  m[7]=0;
	m[2]=0;  m[5]=0;  m[8]=1;
}

mat3_c::mat3_c(const mat3_c& rhs)
{
	memcpy(m, rhs.m, sizeof(m));
}

std::string mat3_c::ToStr() const
{
	return STR_Format(
		"\n(%1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f)",
		m[0], m[3], m[6],
		m[1], m[4], m[7],
		m[2], m[5], m[8]);
}

mat3_c& mat3_c::Negate()
{
	for (int i = 0; i < 9; i++)
		m[i] = -m[i];

	return *this;
}

mat3_c& mat3_c::Transpose()
{
	swap(1, 3); swap(2, 6); swap(5, 7);

	return *this;
}

mat3_c& mat3_c::operator= (const mat3_c& rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	memcpy(m, rhs.m, sizeof(m));

	return *this;
}

mat3_c& mat3_c::operator+= (const mat3_c& rhs)
{
	for (int i = 0; i < 9; i++)
		m[i] += rhs.m[i];

	return *this;
}

mat3_c& mat3_c::operator-= (const mat3_c& rhs)
{
	for (int i = 0; i < 9; i++)
		m[i] -= rhs.m[i];

	return *this;
}

mat3_c& mat3_c::operator*= (const mat3_c& rhs)
{
	float tmp[9];

	for (int x = 0; x < 3; x++)
	for (int y = 0; y < 3; y++)
	{
		tmp[x*3+y] = m[    y] * rhs.m[x*3  ] +
		             m[1*3+y] * rhs.m[x*3+1] +
		             m[2*3+y] * rhs.m[x*3+2];
	}

	memcpy(m, tmp, sizeof(m));

	return *this;
}

mat3_c& mat3_c::operator*= (float scale)
{
	for (int i = 0; i < 9; i++)
		m[i] *= scale;

	return *this;
}

mat3_c& mat3_c::operator/= (float scale)
{
	for (int i = 0; i < 9; i++)
		m[i] /= scale;

	return *this;
}

//------------------------------------------------------------------------

mat4_c::mat4_c()
{
	m[0]=1;  m[4]=0;  m[8] =0;  m[12]=0;
	m[1]=0;  m[5]=1;  m[9] =0;  m[13]=0;
	m[2]=0;  m[6]=0;  m[10]=1;  m[14]=0;
	m[3]=0;  m[7]=0;  m[11]=0;  m[15]=1;
}

mat4_c::mat4_c(const mat4_c& rhs)
{
	memcpy(m, rhs.m, sizeof(m));
}

mat4_c::mat4_c(const mat3_c& rhs, float w)
{
	for (int x = 0; x < 4; x++)
	for (int y = 0; y < 4; y++)
	{
		m[x*4+y] = (x < 3 && y < 3) ? rhs.m[x*3+y] : 0.0;
	}

	m[15] = w;
}

std::string mat4_c::ToStr() const
{
	return STR_Format(
		"\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)"
	    "\n(%1.4f  %1.4f  %1.4f  %1.4f)",
		m[0], m[4], m[8],  m[12],
		m[1], m[5], m[9],  m[13],
		m[2], m[6], m[10], m[14],
		m[3], m[7], m[11], m[15]);
}

mat4_c& mat4_c::Negate()
{
	for (int i = 0; i < 16; i++)
		m[i] = -m[i];

	return *this;
}

mat4_c& mat4_c::Transpose()
{
	swap(1, 4); swap(2, 8);  swap(3,  12);
	swap(6, 9); swap(7, 13); swap(11, 14);

	return *this;
}

mat4_c& mat4_c::operator= (const mat4_c& rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	memcpy(m, rhs.m, sizeof(m));

	return *this;
}

mat4_c& mat4_c::operator+= (const mat4_c& rhs)
{
	for (int i = 0; i < 16; i++)
		m[i] += rhs.m[i];

	return *this;
}

mat4_c& mat4_c::operator-= (const mat4_c& rhs)
{
	for (int i = 0; i < 16; i++)
		m[i] -= rhs.m[i];

	return *this;
}

mat4_c& mat4_c::operator*= (const mat4_c& rhs)
{
	float tmp[16];

	for (int x = 0; x < 4; x++)
	for (int y = 0; y < 4; y++)
	{
		tmp[x*4+y] = m[    y] * rhs.m[x*4  ] +
		             m[1*4+y] * rhs.m[x*4+1] +
		             m[2*4+y] * rhs.m[x*4+2] +
					 m[3*4+y] * rhs.m[x*4+3];
	}

	memcpy(m, tmp, sizeof(m));

	return *this;
}

mat4_c& mat4_c::operator*= (float scale)
{
	for (int i = 0; i < 16; i++)
		m[i] *= scale;

	return *this;
}

mat4_c& mat4_c::operator/= (float scale)
{
	for (int i = 0; i < 16; i++)
		m[i] /= scale;

	return *this;
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
