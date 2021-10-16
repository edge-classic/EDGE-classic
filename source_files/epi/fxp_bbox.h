//------------------------------------------------------------------------
//  EPI Fixed-point Bounding Boxes
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

#ifndef __EPI_FXP_BBOX_H__
#define __EPI_FXP_BBOX_H__

#include "fxp_fixed.h"
#include "fxp_vector.h"

namespace epi
{

class xbox2_c
{
	/* sealed, value semantics */

public:
	xvec2_c lo;
	xvec2_c hi;

public:
	xbox2_c() : lo(), hi() { }
	xbox2_c(const xbox2_c& rhs) : lo(rhs.lo), hi(rhs.hi) { }

	explicit xbox2_c(const xvec2_c& point);
	explicit xbox2_c(const xvec2_c& p1, const xvec2_c& p2);
	explicit xbox2_c(const xvec2_c& p1, const xvec2_c& p2, const xvec2_c& p3);

	/* ---- read-only operations ---- */

	inline bool Contains(const xvec2_c& point) const;
	// checks if the point lies inside the bounding box.

	inline bool Contains(const xbox2_c& other) const;

	inline bool Touches(const xbox2_c& other) const;
	// checks if two bounding boxes intersect.  Returns false when only
	// touching, i.e. the intersected volume must be > 0.
	
	std::string ToStr(int precision = 1) const;

	/* ---- modifying operations ---- */

	xbox2_c& operator= (const xbox2_c& rhs);

	xbox2_c& operator+= (const xvec2_c& offset);
	xbox2_c& operator-= (const xvec2_c& offset);
	// move the whole bbox by the given offset.

	inline xbox2_c& Insert(const xvec2_c& point);
	// enlarge the bbox (if needed) to include the given point.

	inline xbox2_c& Merge(const xbox2_c& other);
	// merge the other bbox with this one.

	inline xbox2_c& Enlarge(const fix_c& radius);
	// enlarge this bbox by moving each edge by radius.
};

class xbox3_c
{
	/* sealed, value semantics */

public:
	xvec3_c lo;
	xvec3_c hi;

public:
	xbox3_c() : lo(), hi() { }
	xbox3_c(const xbox3_c& rhs) : lo(rhs.lo), hi(rhs.hi) { }
	xbox3_c(const xbox2_c& rhs, const fix_c& z1, const fix_c& z2)
		: lo(rhs.lo, z1), hi(rhs.hi, z2) { }

	explicit xbox3_c(const xvec3_c& point);
	explicit xbox3_c(const xvec3_c& p1, const xvec3_c& p2);
	explicit xbox3_c(const xvec3_c& p1, const xvec3_c& p2, const xvec3_c& p3);

	/* ---- read-only operations ---- */

	// checks if the point lies inside the bounding box.
	inline bool Contains(const xvec3_c& point) const;
	inline bool Contains(const xbox3_c& other) const;

	// checks if two bounding boxes intersect.  Returns false when only
	// touching, i.e. the intersected volume must be > 0.
	inline bool Touches(const xbox3_c& other) const;

	std::string ToStr(int precision = 1) const;

	/* ---- modifying operations ---- */

	xbox3_c& operator= (const xbox3_c& rhs);

	xbox3_c& operator+= (const xvec3_c& offset);
	xbox3_c& operator-= (const xvec3_c& offset);
	// move the whole bbox by the given offset.

	inline xbox3_c& Insert(const xvec3_c& point);
	// enlarge the bbox (if needed) to include the given point.

	inline xbox3_c& Merge(const xbox3_c& other);
	// merge the other bbox with this one.

	inline xbox3_c& Enlarge(const fix_c& radius);
	inline xbox3_c& Enlarge(const fix_c& horiz_R, const fix_c& vert_R);
	// enlarge this bbox by moving each edge by radius.
};


//------------------------------------------------------------------------
//    IMPLEMENTATION
//------------------------------------------------------------------------

inline xbox2_c::xbox2_c(const xvec2_c& point) : lo(point), hi(point)
{ }

inline xbox2_c::xbox2_c(const xvec2_c& p1, const xvec2_c& p2) : lo(p1), hi(p1)
{
	Insert(p2);
}

inline xbox2_c::xbox2_c(const xvec2_c& p1, const xvec2_c& p2, const xvec2_c& p3)
	: lo(p1), hi(p1)
{
	Insert(p2);
	Insert(p3);
}

inline bool xbox2_c::Contains(const xvec2_c& point) const
{
	return (lo.x <= point.x && point.x <= hi.x &&
	        lo.y <= point.y && point.y <= hi.y);
}

inline bool xbox2_c::Contains(const xbox2_c& other) const
{
	return (lo.x <= other.lo.x && other.hi.x <= hi.x &&
	        lo.y <= other.lo.y && other.hi.y <= hi.y);
}

inline bool xbox2_c::Touches(const xbox2_c& other) const
{
	return (other.hi.x > lo.x && other.lo.x < hi.x &&
			other.hi.y > lo.y && other.lo.y < hi.y);
}

inline xbox2_c& xbox2_c::operator= (const xbox2_c& rhs)
{
	// no need to check for self assignment
	lo = rhs.lo;
	hi = rhs.hi;

	return *this;
}

inline xbox2_c& xbox2_c::operator+= (const xvec2_c& offset)
{
	lo += offset;
	hi += offset;

	return *this;
}

inline xbox2_c& xbox2_c::operator-= (const xvec2_c& offset)
{
	lo -= offset;
	hi -= offset;

	return *this;
}

inline xbox2_c& xbox2_c::Insert(const xvec2_c& point)
{
	if (point.x < lo.x) lo.x = point.x;
	if (point.y < lo.y) lo.y = point.y;
	if (point.x > hi.x) hi.x = point.x;
	if (point.y > hi.y) hi.y = point.y;

	return *this;
}

inline xbox2_c& xbox2_c::Merge(const xbox2_c& other)
{
	Insert(other.lo);
	Insert(other.hi);

	return *this;
}

inline xbox2_c& xbox2_c::Enlarge(const fix_c& radius)
{
	lo.x -= radius; lo.y -= radius;
	hi.x += radius; hi.y += radius;

	return *this;
}

//------------------------------------------------------------------------

inline xbox3_c::xbox3_c(const xvec3_c& point) : lo(point), hi(point)
{ }

inline xbox3_c::xbox3_c(const xvec3_c& p1, const xvec3_c& p2) : lo(p1), hi(p1)
{
	Insert(p2);
}

inline xbox3_c::xbox3_c(const xvec3_c& p1, const xvec3_c& p2, const xvec3_c& p3)
	: lo(p1), hi(p1)
{
	Insert(p2);
	Insert(p3);
}

inline bool xbox3_c::Contains(const xvec3_c& point) const
{
	return (lo.x <= point.x && point.x <= hi.x &&
	        lo.y <= point.y && point.y <= hi.y &&
	        lo.z <= point.z && point.z <= hi.z);
}

inline bool xbox3_c::Contains(const xbox3_c& other) const
{
	return (lo.x <= other.lo.x && other.hi.x <= hi.x &&
	        lo.y <= other.lo.y && other.hi.y <= hi.y &&
	        lo.z <= other.lo.z && other.hi.z <= hi.z);
}

inline bool xbox3_c::Touches(const xbox3_c& other) const
{
	return (other.hi.x > lo.x && other.lo.x < hi.x &&
			other.hi.y > lo.y && other.lo.y < hi.y &&
			other.hi.z > lo.z && other.lo.z < hi.z);
}

inline xbox3_c& xbox3_c::operator= (const xbox3_c& rhs)
{
	// no need to check for self assignment
	lo = rhs.lo;
	hi = rhs.hi;

	return *this;
}

inline xbox3_c& xbox3_c::operator+= (const xvec3_c& offset)
{
	lo += offset;
	hi += offset;

	return *this;
}

inline xbox3_c& xbox3_c::operator-= (const xvec3_c& offset)
{
	lo -= offset;
	hi -= offset;

	return *this;
}

inline xbox3_c& xbox3_c::Insert(const xvec3_c& point)
{
	if (point.x < lo.x) lo.x = point.x;
	if (point.y < lo.y) lo.y = point.y;
	if (point.z < lo.z) lo.z = point.z;

	if (point.x > hi.x) hi.x = point.x;
	if (point.y > hi.y) hi.y = point.y;
	if (point.z > hi.z) hi.z = point.z;

	return *this;
}

inline xbox3_c& xbox3_c::Merge(const xbox3_c& other)
{
	Insert(other.lo);
	Insert(other.hi);

	return *this;
}

inline xbox3_c& xbox3_c::Enlarge(const fix_c& radius)
{
	lo.x -= radius; lo.y -= radius; lo.z -= radius;
	hi.x += radius; hi.y += radius; hi.z += radius;

	return *this;
}

inline xbox3_c& xbox3_c::Enlarge(const fix_c& horiz_R, const fix_c& vert_R)
{
	lo.x -= horiz_R; lo.y -= horiz_R; lo.z -= vert_R;
	hi.x += horiz_R; hi.y += horiz_R; hi.z += vert_R;

	return *this;
}

}  // namespace epi

#endif /* __EPI_FXP_BBOX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
