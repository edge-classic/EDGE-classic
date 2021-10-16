//------------------------------------------------------------------------
//  EPI Bounding Boxes
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

#ifndef __EPI_MATH_BBOX_H__
#define __EPI_MATH_BBOX_H__

#include "math_vector.h"

namespace epi
{

class bbox2_c
{
	/* sealed, value semantics */

friend class bbox3_c;

private:
	vec2_c lo;
	vec2_c hi;

public:
	bbox2_c() : lo(), hi() { }
	bbox2_c(const bbox2_c& rhs) : lo(rhs.lo), hi(rhs.hi) { }

	explicit bbox2_c(const vec2_c& point);
	explicit bbox2_c(const vec2_c& p1, const vec2_c& p2);
	explicit bbox2_c(const vec2_c& p1, const vec2_c& p2, const vec2_c& p3);

	/* ---- read-only operations ---- */

	inline bool Contains(vec2_c point) const;
	// checks if the point lies inside the bounding box.

	inline bool DoesIntersect(const bbox2_c& other) const;
	// checks if two bounding boxes intersect.  Returns false when only
	// touching, i.e. the intersected volume must be > 0.
	
	/* ---- modifying operations ---- */

	bbox2_c& operator= (const bbox2_c& rhs);

	bbox2_c& operator+= (const vec2_c& offset);
	bbox2_c& operator-= (const vec2_c& offset);
	// move the whole bbox by the given offset.

	inline bbox2_c& Insert(const vec2_c& point);
	// enlarge the bbox (if needed) to include the given point.

	inline bbox2_c& Merge(const bbox2_c& other);
	// merge the other bbox with this one.

	inline bbox2_c& Enlarge(float radius);
	// enlarge this bbox by moving each edge by radius.
};

class bbox3_c
{
	/* sealed, value semantics */

private:
	vec3_c lo;
	vec3_c hi;

public:
	bbox3_c() : lo(), hi() { }
	bbox3_c(const bbox3_c& rhs) : lo(rhs.lo), hi(rhs.hi) { }
	bbox3_c(const bbox2_c& rhs, float z1, float z2)
		: lo(rhs.lo, z1), hi(rhs.hi, z2) { }

	explicit bbox3_c(const vec3_c& point);
	explicit bbox3_c(const vec3_c& p1, const vec3_c& p2);
	explicit bbox3_c(const vec3_c& p1, const vec3_c& p2, const vec3_c& p3);

	/* ---- read-only operations ---- */

	inline bool Contains(vec3_c point) const;
	// checks if the point lies inside the bounding box.

	inline bool DoesIntersect(const bbox3_c& other) const;
	// checks if two bounding boxes intersect.  Returns false when only
	// touching, i.e. the intersected volume must be > 0.

	enum
	{
		HIT_OUTSIDE = 0,  // fully outside the plane (back side)
		HIT_PARTIAL,      // intersects the plane
		HIT_INSIDE,       // fully inside the plane (front side)
	};

	int IntersectPlane(vec3_c loc, vec3_c face) const; 
	// check if this bounding box intersects the given plane,
	// returning one of the HIT_XXX values above.
	// NOTE: face vector must be unit length.
	
	/* ---- modifying operations ---- */

	bbox3_c& operator= (const bbox3_c& rhs);

	bbox3_c& operator+= (const vec3_c& offset);
	bbox3_c& operator-= (const vec3_c& offset);
	// move the whole bbox by the given offset.

	inline bbox3_c& Insert(const vec3_c& point);
	// enlarge the bbox (if needed) to include the given point.

	inline bbox3_c& Merge(const bbox3_c& other);
	// merge the other bbox with this one.

	inline bbox3_c& Enlarge(float radius);
	inline bbox3_c& Enlarge(float horiz_R, float vert_R);
	// enlarge this bbox by moving each edge by radius.
};


//------------------------------------------------------------------------
//    IMPLEMENTATION
//------------------------------------------------------------------------

inline bbox2_c::bbox2_c(const vec2_c& point) : lo(point), hi(point)
{ }

inline bbox2_c::bbox2_c(const vec2_c& p1, const vec2_c& p2) : lo(p1), hi(p1)
{
	Insert(p2);
}

inline bbox2_c::bbox2_c(const vec2_c& p1, const vec2_c& p2, const vec2_c& p3)
	: lo(p1), hi(p1)
{
	Insert(p2);
	Insert(p3);
}

inline bool bbox2_c::Contains(vec2_c point) const
{
	return (lo.x <= point.x && point.x <= hi.x &&
	        lo.y <= point.y && point.y <= hi.y);
}

inline bool bbox2_c::DoesIntersect(const bbox2_c& other) const
{
	return (other.hi.x > lo.x && other.lo.x < hi.x &&
			other.hi.y > lo.y && other.lo.y < hi.y);
}

inline bbox2_c& bbox2_c::operator= (const bbox2_c& rhs)
{
	// no need to check for self assignment
	lo = rhs.lo;
	hi = rhs.hi;

	return *this;
}

inline bbox2_c& bbox2_c::operator+= (const vec2_c& offset)
{
	lo += offset;
	hi += offset;

	return *this;
}

inline bbox2_c& bbox2_c::operator-= (const vec2_c& offset)
{
	lo -= offset;
	hi -= offset;

	return *this;
}

inline bbox2_c& bbox2_c::Insert(const vec2_c& point)
{
	if (point.x < lo.x) lo.x = point.x;
	if (point.y < lo.y) lo.y = point.y;
	if (point.x > hi.x) hi.x = point.x;
	if (point.y > hi.y) hi.y = point.y;

	return *this;
}

inline bbox2_c& bbox2_c::Merge(const bbox2_c& other)
{
	Insert(other.lo);
	Insert(other.hi);

	return *this;
}

inline bbox2_c& bbox2_c::Enlarge(float radius)
{
	lo.x -= radius; lo.y -= radius;
	hi.x += radius; hi.y += radius;

	return *this;
}

//------------------------------------------------------------------------

inline bbox3_c::bbox3_c(const vec3_c& point) : lo(point), hi(point)
{ }

inline bbox3_c::bbox3_c(const vec3_c& p1, const vec3_c& p2) : lo(p1), hi(p1)
{
	Insert(p2);
}

inline bbox3_c::bbox3_c(const vec3_c& p1, const vec3_c& p2, const vec3_c& p3)
	: lo(p1), hi(p1)
{
	Insert(p2);
	Insert(p3);
}

inline bool bbox3_c::Contains(vec3_c point) const
{
	return (lo.x <= point.x && point.x <= hi.x &&
	        lo.y <= point.y && point.y <= hi.y &&
	        lo.z <= point.z && point.z <= hi.z);
}

inline bool bbox3_c::DoesIntersect(const bbox3_c& other) const
{
	return (other.hi.x > lo.x && other.lo.x < hi.x &&
			other.hi.y > lo.y && other.lo.y < hi.y &&
			other.hi.z > lo.z && other.lo.z < hi.z);
}

inline bbox3_c& bbox3_c::operator= (const bbox3_c& rhs)
{
	// no need to check for self assignment
	lo = rhs.lo;
	hi = rhs.hi;

	return *this;
}

inline bbox3_c& bbox3_c::operator+= (const vec3_c& offset)
{
	lo += offset;
	hi += offset;

	return *this;
}

inline bbox3_c& bbox3_c::operator-= (const vec3_c& offset)
{
	lo -= offset;
	hi -= offset;

	return *this;
}

inline bbox3_c& bbox3_c::Insert(const vec3_c& point)
{
	if (point.x < lo.x) lo.x = point.x;
	if (point.y < lo.y) lo.y = point.y;
	if (point.z < lo.z) lo.z = point.z;

	if (point.x > hi.x) hi.x = point.x;
	if (point.y > hi.y) hi.y = point.y;
	if (point.z > hi.z) hi.z = point.z;

	return *this;
}

inline bbox3_c& bbox3_c::Merge(const bbox3_c& other)
{
	Insert(other.lo);
	Insert(other.hi);

	return *this;
}

inline bbox3_c& bbox3_c::Enlarge(float radius)
{
	lo.x -= radius; lo.y -= radius; lo.z -= radius;
	hi.x += radius; hi.y += radius; hi.z += radius;

	return *this;
}

inline bbox3_c& bbox3_c::Enlarge(float horiz_R, float vert_R)
{
	lo.x -= horiz_R; lo.y -= horiz_R;
	hi.x += horiz_R; hi.y += horiz_R;

	lo.z -= vert_R;
	hi.z += vert_R;

	return *this;
}

}  // namespace epi

#endif  /* __EPI_MATH_BBOX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
