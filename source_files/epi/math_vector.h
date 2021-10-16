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

#ifndef __EPI_MATH_VECTOR__
#define __EPI_MATH_VECTOR__

#include "macros.h"
#include "math_angle.h"

namespace epi
{

class ivec_c
{
	/* sealed class, value semantics */

public:
	int x, y;

	ivec_c() : x(0), y(0) { }
	ivec_c(int nx, int ny) : x(nx), y(ny) { }
	ivec_c(const ivec_c& rhs) : x(rhs.x), y(rhs.y) { }

	/* ---- read-only operations ---- */

	float Length() const;
	int ApproxLength() const;

	bool operator==(const ivec_c& rhs) const;
	bool operator!=(const ivec_c& rhs) const;

	ivec_c operator- ();

	ivec_c operator+ (const ivec_c& rhs) const;
	ivec_c operator- (const ivec_c& rhs) const;
	ivec_c operator* (int scale) const;
	ivec_c operator/ (int scale) const;

	int operator* (const ivec_c& rhs) const;  // dot product

	/* ---- modifying operations ---- */

	ivec_c& Rotate90();   //
	ivec_c& Rotate180();  // anti-clockwise
	ivec_c& Rotate270();  //

	ivec_c& operator= (const ivec_c& rhs);

	ivec_c& operator+= (const ivec_c& rhs);
	ivec_c& operator-= (const ivec_c& rhs);
    ivec_c& operator*= (int scale);
    ivec_c& operator/= (int scale);
};

class vec2_c
{
	/* sealed class, value semantics */

public:
	float x, y;

	vec2_c() : x(0), y(0) { }
	vec2_c(float nx, float ny) : x(nx), y(ny) { }
	vec2_c(const vec2_c& rhs)  : x(rhs.x), y(rhs.y) { }
	vec2_c(const ivec_c& rhs)  : x(rhs.x), y(rhs.y) { }
	vec2_c(const angle_c& ang) : x(ang.getX()), y(ang.getY()) { }
	vec2_c(const angle_c& ang, float len)
		: x(ang.getX() * len), y(ang.getY() * len) { }

	/* ---- read-only operations ---- */

	float Length() const;
	float ApproxLength() const;

	bool onRight(const vec2_c& point) const;
	// returns true if point is on the RIGHT side of this vector,
	// false if on the LEFT side.  Undefined if directly on the line.

	float PerpDist(const vec2_c& point) const;
	// return perpendicular distance from this vector (extended to
	// infinity) to the given point.  Result is positive on the
	// right side, negative on the left.

	float AlongDist(const vec2_c& point) const;
	// return parallel distance from (0,0) to the point on the vector
	// (extended to infinity) which is closest to the given point.

	bool Match(const vec2_c& rhs, float precision = 0.001f);
	// no equality operators since we're using floating point.

	vec2_c operator- ();

	vec2_c operator+ (const vec2_c& rhs) const;
	vec2_c operator- (const vec2_c& rhs) const;
	vec2_c operator* (float scale) const;
	vec2_c operator/ (float scale) const;

	float operator* (const vec2_c& rhs) const;  // dot product

	/* ---- modifying operations ---- */

	vec2_c& MakeUnit();  // make unit length

	vec2_c& Rotate(const angle_c& ang); // anti-clockwise

	vec2_c& Rotate90();   //
	vec2_c& Rotate180();  // anti-clockwise
	vec2_c& Rotate270();  //

	vec2_c& operator= (const vec2_c& rhs);

	vec2_c& operator+= (const vec2_c& rhs);
	vec2_c& operator-= (const vec2_c& rhs);
	vec2_c& operator*= (float scale);
	vec2_c& operator/= (float scale);
};

class vec3_c
{
	/* sealed class, value semantics */

public:
	float x, y, z;

	vec3_c() : x(0), y(0), z(0) { }
	vec3_c(float nx, float ny, float nz) : x(nx), y(ny), z(nz) { }
	vec3_c(const vec3_c& rhs) : x(rhs.x), y(rhs.y), z(rhs.z) { }
	vec3_c(const vec2_c& horiz, float nz) : x(horiz.x), y(horiz.y), z(nz) { }

	/* ---- read-only operations ---- */

	float Length() const;
	float Slope() const;

	float ApproxLength() const;
	float ApproxSlope() const;

	float AlongDist(const vec3_c& point) const;
	// return parallel distance from (0,0,0) to the point on the vector
	// (extended to infinity) which is closest to the given point.

	bool Match(const vec3_c& rhs, float precision = 0.001f);
	// no equality operators since we're using floating point.

	vec2_c Get2D() const;
	// return the 2D vector (dropping the z coord).

	vec3_c operator- ();

	vec3_c operator+ (const vec3_c& rhs) const;
	vec3_c operator- (const vec3_c& rhs) const;
	vec3_c operator* (float scale) const;
	vec3_c operator/ (float scale) const;

	float operator* (const vec3_c& rhs) const;  // dot product

	vec3_c Cross(const vec3_c& rhs) const;  // cross product

	/* ---- modifying operations ---- */

	vec3_c& MakeUnit();  // make unit length

	vec3_c& operator= (const vec3_c& rhs);

	vec3_c& operator+= (const vec3_c& rhs);
	vec3_c& operator-= (const vec3_c& rhs);
	vec3_c& operator*= (float scale);
	vec3_c& operator/= (float scale);
};

//------------------------------------------------------------------------
//  IMPLEMENTATION
//------------------------------------------------------------------------

inline int ivec_c::ApproxLength() const
{
	int ax = ABS(x);
	int ay = ABS(y);

	return (ax > ay) ? (ax + ay / 2) : (ay + ax / 2);
}

inline bool ivec_c::operator==(const ivec_c& rhs) const
{
	return (rhs.x == x) && (rhs.y == y);
}

inline bool ivec_c::operator!=(const ivec_c& rhs) const
{
	return (rhs.x != x) || (rhs.y != y);
}

inline ivec_c ivec_c::operator- ()
{
	return ivec_c(-x, -y);
}

inline ivec_c ivec_c::operator+ (const ivec_c& rhs) const
{
	return ivec_c(x + rhs.x, y + rhs.y);
}

inline ivec_c ivec_c::operator- (const ivec_c& rhs) const
{
	return ivec_c(x - rhs.x, y - rhs.y);
}

inline ivec_c ivec_c::operator* (int scale) const
{
	return ivec_c(x * scale, y * scale);
}

inline ivec_c ivec_c::operator/ (int scale) const
{
	return ivec_c(x / scale, y / scale);
}

inline int ivec_c::operator* (const ivec_c& rhs) const
{
	return x * rhs.x + y * rhs.y;
}

inline ivec_c& ivec_c::operator= (const ivec_c& rhs)
{
	x = rhs.x;  y = rhs.y;
	return *this;
}

inline ivec_c& ivec_c::operator+= (const ivec_c& rhs)
{
	x += rhs.x;  y += rhs.y;
	return *this;
}

inline ivec_c& ivec_c::operator-= (const ivec_c& rhs)
{
	x -= rhs.x;  y -= rhs.y;
	return *this;
}

inline ivec_c& ivec_c::operator*= (int scale)
{
	x *= scale;  y *= scale;
	return *this;
}

inline ivec_c& ivec_c::operator/= (int scale)
{
	x /= scale;  y /= scale;
	return *this;
}

inline ivec_c& ivec_c::Rotate90()
{
	int tmp = y; y = x; x = -tmp;
	return *this;
}

inline ivec_c& ivec_c::Rotate180()
{
	x = -x; y = -y;
	return *this;
}

inline ivec_c& ivec_c::Rotate270()
{
	int tmp = x; x = y; y = -tmp;
	return *this;
}


//------------------------------------------------------------------------

inline float vec2_c::ApproxLength() const
{
	float ax = ABS(x);
	float ay = ABS(y);

	return (ax > ay) ? (ax + ay / 2) : (ay + ax / 2);
}

inline bool vec2_c::onRight(const vec2_c& point) const
{
	return (point.x * y >= point.y * x);
}

inline bool vec2_c::Match(const vec2_c& rhs, float precision)
{
	return fabs(x - rhs.x) < precision && fabs(y - rhs.y) < precision;
}

inline vec2_c vec2_c::operator- ()
{
	return vec2_c(-x, -y);
}

inline vec2_c vec2_c::operator+ (const vec2_c& rhs) const
{
	return vec2_c(x + rhs.x, y + rhs.y);
}

inline vec2_c vec2_c::operator- (const vec2_c& rhs) const
{
	return vec2_c(x - rhs.x, y - rhs.y);
}

inline vec2_c vec2_c::operator* (float scale) const
{
	return vec2_c(x * scale, y * scale);
}

inline vec2_c vec2_c::operator/ (float scale) const
{
	return vec2_c(x / scale, y / scale);
}

inline float vec2_c::operator* (const vec2_c& rhs) const
{
	return x * rhs.x + y * rhs.y;
}

inline vec2_c& vec2_c::MakeUnit()
{
	*this /= Length();
	return *this;
}

inline vec2_c& vec2_c::operator= (const vec2_c& rhs)
{
	x = rhs.x;  y = rhs.y;
	return *this;
}

inline vec2_c& vec2_c::operator+= (const vec2_c& rhs)
{
	x += rhs.x;  y += rhs.y;
	return *this;
}

inline vec2_c& vec2_c::operator-= (const vec2_c& rhs)
{
	x -= rhs.x;  y -= rhs.y;
	return *this;
}

inline vec2_c& vec2_c::operator*= (float scale)
{
	x *= scale;  y *= scale;
	return *this;
}

inline vec2_c& vec2_c::operator/= (float scale)
{
	x /= scale;  y /= scale;
	return *this;
}

inline vec2_c& vec2_c::Rotate(const angle_c& ang)
{
	float s = ang.Sin();
	float c = ang.Cos();

	float ox = x;
	float oy = y;

	x = ox * c - oy * s;
	y = oy * c + ox * s;

	return *this;
}

inline vec2_c& vec2_c::Rotate90()
{
	float tmp = y; y = x; x = -tmp;
	return *this;
}

inline vec2_c& vec2_c::Rotate180()
{
	x = -x; y = -y;
	return *this;
}

inline vec2_c& vec2_c::Rotate270()
{
	float tmp = x; x = y; y = -tmp;
	return *this;
}


//------------------------------------------------------------------------

inline float vec3_c::ApproxLength() const
{
	float ax = ABS(x);
	float ay = ABS(y);
	float az = ABS(z);

	float axy = (ax > ay) ? (ax + ay / 2) : (ay + ax / 2);

	return (axy > az) ? (axy + az / 2) : (az + axy / 2);
}

inline bool vec3_c::Match(const vec3_c& rhs, float precision)
{
	return fabs(x - rhs.x) < precision &&
		   fabs(y - rhs.y) < precision &&
		   fabs(z - rhs.z) < precision;
}

inline vec2_c vec3_c::Get2D() const
{
	return vec2_c(x, y);
}

inline vec3_c vec3_c::operator- ()
{
	return vec3_c(-x, -y, -z);
}

inline vec3_c vec3_c::operator+ (const vec3_c& rhs) const
{
	return vec3_c(x + rhs.x, y + rhs.y, z + rhs.z);
}

inline vec3_c vec3_c::operator- (const vec3_c& rhs) const
{
	return vec3_c(x - rhs.x, y - rhs.y, z - rhs.z);
}

inline vec3_c vec3_c::operator* (float scale) const
{
	return vec3_c(x * scale, y * scale, z * scale);
}

inline vec3_c vec3_c::operator/ (float scale) const
{
	return vec3_c(x / scale, y / scale, z / scale);
}

inline float vec3_c::operator* (const vec3_c& rhs) const
{
	return x * rhs.x + y * rhs.y + z * rhs.z;
}

inline vec3_c& vec3_c::MakeUnit()
{
	*this /= Length();
	return *this;
}

inline vec3_c& vec3_c::operator= (const vec3_c& rhs)
{
	x = rhs.x;  y = rhs.y;  z = rhs.z;
	return *this;
}

inline vec3_c& vec3_c::operator+= (const vec3_c& rhs)
{
	x += rhs.x;  y += rhs.y;  z += rhs.z;
	return *this;
}

inline vec3_c& vec3_c::operator-= (const vec3_c& rhs)
{
	x -= rhs.x;  y -= rhs.y;  z -= rhs.z;
	return *this;
}

inline vec3_c& vec3_c::operator*= (float scale)
{
	x *= scale;  y *= scale;  z *= scale;
	return *this;
}

inline vec3_c& vec3_c::operator/= (float scale)
{
	x /= scale;  y /= scale;  z /= scale;
	return *this;
}

}  // namespace epi

#endif  /* __EPI_MATH_VECTOR__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
