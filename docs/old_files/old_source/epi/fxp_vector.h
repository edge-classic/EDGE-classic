//------------------------------------------------------------------------
//  EPI Fixed-point Vector types
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

#ifndef __EPI_FXP_VECTOR_H__
#define __EPI_FXP_VECTOR_H__

#include "fxp_fixed.h"
#include "math_angle.h"

namespace epi
{

class xvec2_c
{
	/* sealed class, value semantics */

public:
	fix_c x, y;

	xvec2_c() : x(0), y(0) { }
	xvec2_c(const fix_c& nx, const fix_c& ny) : x(nx), y(ny) { }
	xvec2_c(const xvec2_c& rhs)  : x(rhs.x), y(rhs.y) { }

	xvec2_c(const angle_c& ang) : x(fxcos(ang)), y(fxsin(ang)) { }
	xvec2_c(const angle_c& ang, const fix_c& len) :
		x(fxcos(ang) * len), y(fxsin(ang) * len) { }

	/* ---- read-only operations ---- */

	fix_c Length() const;
	fix_c ApproxLength() const;

	bool onRight(const xvec2_c& point) const;
	// returns true if point is on the RIGHT side of this vector,
	// false if on the LEFT side.  Undefined if directly on the line.

	fix_c PerpDist(const xvec2_c& point) const;
	// return perpendicular distance from this vector (extended to
	// infinity) to the given point.  Result is positive on the
	// right side, negative on the left.

	fix_c AlongDist(const xvec2_c& point) const;
	// return parallel distance from (0,0) to the point on the vector
	// (extended to infinity) which is closest to the given point.

	bool Match(const xvec2_c& rhs, const fix_c& precision);

	bool operator== (const xvec2_c& rhs) const;
	bool operator!= (const xvec2_c& rhs) const;

	xvec2_c operator- ();

	xvec2_c operator+ (const xvec2_c& rhs) const;
	xvec2_c operator- (const xvec2_c& rhs) const;
	xvec2_c operator* (const fix_c& scale) const;
	xvec2_c operator/ (const fix_c& scale) const;

	fix_c operator* (const xvec2_c& rhs) const;
	// dot product

	angle_c ToAngle() const;
	// result undefined when vector is zero length.

	std::string ToStr(int precision = 2) const;

	/* ---- modifying operations ---- */

	xvec2_c& MakeUnit();  // make unit length

	xvec2_c& Rotate(const angle_c& ang); // anti-clockwise

	xvec2_c& Rotate90();   //
	xvec2_c& Rotate180();  // anti-clockwise
	xvec2_c& Rotate270();  //

	xvec2_c& operator= (const xvec2_c& rhs);

	xvec2_c& operator+= (const xvec2_c& rhs);
	xvec2_c& operator-= (const xvec2_c& rhs);
	xvec2_c& operator*= (const fix_c& scale);
	xvec2_c& operator/= (const fix_c& scale);
};

class xvec3_c
{
	/* sealed class, value semantics */

public:
	fix_c x, y, z;

	xvec3_c() : x(0), y(0), z(0) { }
	xvec3_c(const fix_c& nx, const fix_c& ny, const fix_c& nz) :
		x(nx), y(ny), z(nz) { }
	xvec3_c(const xvec3_c& rhs) : x(rhs.x), y(rhs.y), z(rhs.z) { }
	xvec3_c(const xvec2_c& horiz, const fix_c& nz) :
		x(horiz.x), y(horiz.y), z(nz) { }

	/* ---- read-only operations ---- */

	fix_c Length() const;
	fix_c Slope() const;

	fix_c ApproxLength() const;
	fix_c ApproxSlope() const;

	fix_c AlongDist(const xvec3_c& point) const;
	// return parallel distance from (0,0,0) to the point on the vector
	// (extended to infinity) which is closest to the given point.

	xvec2_c Get2D() const;
	// return the 2D vector (dropping the z coord).

	bool Match(const xvec3_c& rhs, const fix_c& precision);

	bool operator== (const xvec3_c& rhs) const;
	bool operator!= (const xvec3_c& rhs) const;

	xvec3_c operator- ();

	xvec3_c operator+ (const xvec3_c& rhs) const;
	xvec3_c operator- (const xvec3_c& rhs) const;
	xvec3_c operator* (const fix_c& scale) const;
	xvec3_c operator/ (const fix_c& scale) const;

	fix_c operator* (const xvec3_c& rhs) const;
	// dot product

	xvec3_c Cross(const xvec3_c& rhs) const;
	// cross product

	std::string ToStr(int precision = 2) const;

	/* ---- modifying operations ---- */

	xvec3_c& MakeUnit();  // make unit length

	xvec3_c& operator= (const xvec3_c& rhs);

	xvec3_c& operator+= (const xvec3_c& rhs);
	xvec3_c& operator-= (const xvec3_c& rhs);
	xvec3_c& operator*= (const fix_c& scale);
	xvec3_c& operator/= (const fix_c& scale);
};

//------------------------------------------------------------------------
//  IMPLEMENTATION
//------------------------------------------------------------------------

inline fix_c xvec2_c::ApproxLength() const
{
	fix_c ax (fxabs(x));
	fix_c ay (fxabs(y));

	return (ax > ay) ? ax + (ay >> 1) : ay + (ax >> 1);
}

inline bool xvec2_c::onRight(const xvec2_c& point) const
{
	return (point.x * y >= point.y * x);
}

inline bool xvec2_c::Match(const xvec2_c& rhs, const fix_c& precision)
{
	return fxabs(x - rhs.x) < precision &&
		   fxabs(y - rhs.y) < precision;
}

inline bool xvec2_c::operator== (const xvec2_c& rhs) const
{
	return (x == rhs.x) && (y == rhs.y);
}

inline bool xvec2_c::operator!= (const xvec2_c& rhs) const
{
	return (x != rhs.x) || (y != rhs.y);
}

inline xvec2_c xvec2_c::operator- ()
{
	return xvec2_c(-x, -y);
}

inline xvec2_c xvec2_c::operator+ (const xvec2_c& rhs) const
{
	return xvec2_c(x + rhs.x, y + rhs.y);
}

inline xvec2_c xvec2_c::operator- (const xvec2_c& rhs) const
{
	return xvec2_c(x - rhs.x, y - rhs.y);
}

inline xvec2_c xvec2_c::operator* (const fix_c& scale) const
{
	return xvec2_c(x * scale, y * scale);
}

inline xvec2_c xvec2_c::operator/ (const fix_c& scale) const
{
	return xvec2_c(x / scale, y / scale);
}

inline fix_c xvec2_c::operator* (const xvec2_c& rhs) const
{
	return x * rhs.x + y * rhs.y;
}

inline angle_c xvec2_c::ToAngle() const
{
	return fxatan2(x, y);
}

inline xvec2_c& xvec2_c::MakeUnit()
{
	*this /= Length();
	return *this;
}

inline xvec2_c& xvec2_c::operator= (const xvec2_c& rhs)
{
	x = rhs.x;  y = rhs.y;
	return *this;
}

inline xvec2_c& xvec2_c::operator+= (const xvec2_c& rhs)
{
	x += rhs.x;  y += rhs.y;
	return *this;
}

inline xvec2_c& xvec2_c::operator-= (const xvec2_c& rhs)
{
	x -= rhs.x;  y -= rhs.y;
	return *this;
}

inline xvec2_c& xvec2_c::operator*= (const fix_c& scale)
{
	x *= scale;  y *= scale;
	return *this;
}

inline xvec2_c& xvec2_c::operator/= (const fix_c& scale)
{
	x /= scale;  y /= scale;
	return *this;
}

inline xvec2_c& xvec2_c::Rotate90()
{
	fix_c tmp(y); y = x; x = -tmp;
	return *this;
}

inline xvec2_c& xvec2_c::Rotate180()
{
	x = -x; y = -y;
	return *this;
}

inline xvec2_c& xvec2_c::Rotate270()
{
	fix_c tmp(x); x = y; y = -tmp;
	return *this;
}

inline xvec2_c& xvec2_c::Rotate(const angle_c& ang)
{
	fix_c s (fxsin(ang));
	fix_c c (fxcos(ang));

	fix_c ox (x);
	fix_c oy (y);

	x = ox * c - oy * s;
	y = oy * c + ox * s;

	return *this;
}


//------------------------------------------------------------------------

inline fix_c xvec3_c::ApproxLength() const
{
	fix_c ax (fxabs(x));
	fix_c ay (fxabs(y));
	fix_c az (fxabs(z));

	fix_c axy ((ax > ay) ? (ax + ay >> 1) : (ay + ax >> 1));

	return (axy > az) ? (axy + az >> 1) : (az + axy >> 1);
}

inline xvec2_c xvec3_c::Get2D() const
{
	return xvec2_c(x, y);
}

inline bool xvec3_c::Match(const xvec3_c& rhs, const fix_c& precision)
{
	return fxabs(x - rhs.x) < precision &&
		   fxabs(y - rhs.y) < precision &&
		   fxabs(z - rhs.z) < precision;
}

inline bool xvec3_c::operator== (const xvec3_c& rhs) const
{
	return (x == rhs.x) && (y == rhs.y) && (z == rhs.z);
}

inline bool xvec3_c::operator!= (const xvec3_c& rhs) const
{
	return (x != rhs.x) || (y != rhs.y) || (z != rhs.z);
}

inline xvec3_c xvec3_c::operator- ()
{
	return xvec3_c(-x, -y, -z);
}

inline xvec3_c xvec3_c::operator+ (const xvec3_c& rhs) const
{
	return xvec3_c(x + rhs.x, y + rhs.y, z + rhs.z);
}

inline xvec3_c xvec3_c::operator- (const xvec3_c& rhs) const
{
	return xvec3_c(x - rhs.x, y - rhs.y, z - rhs.z);
}

inline xvec3_c xvec3_c::operator* (const fix_c& scale) const
{
	return xvec3_c(x * scale, y * scale, z * scale);
}

inline xvec3_c xvec3_c::operator/ (const fix_c& scale) const
{
	return xvec3_c(x / scale, y / scale, z / scale);
}

inline fix_c xvec3_c::operator* (const xvec3_c& rhs) const
{
	return x * rhs.x + y * rhs.y + z * rhs.z;
}

inline xvec3_c& xvec3_c::MakeUnit()
{
	*this /= Length();
	return *this;
}

inline xvec3_c& xvec3_c::operator= (const xvec3_c& rhs)
{
	x = rhs.x;  y = rhs.y;  z = rhs.z;
	return *this;
}

inline xvec3_c& xvec3_c::operator+= (const xvec3_c& rhs)
{
	x += rhs.x;  y += rhs.y;  z += rhs.z;
	return *this;
}

inline xvec3_c& xvec3_c::operator-= (const xvec3_c& rhs)
{
	x -= rhs.x;  y -= rhs.y;  z -= rhs.z;
	return *this;
}

inline xvec3_c& xvec3_c::operator*= (const fix_c& scale)
{
	x *= scale;  y *= scale;  z *= scale;
	return *this;
}

inline xvec3_c& xvec3_c::operator/= (const fix_c& scale)
{
	x /= scale;  y /= scale;  z /= scale;
	return *this;
}

}  // namespace epi

#endif /* __EPI_FXP_VECTOR_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
