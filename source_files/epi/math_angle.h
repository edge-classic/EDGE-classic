//------------------------------------------------------------------------
//  EPI Angle type
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

#ifndef __EPI_ANGLE_CLASS__
#define __EPI_ANGLE_CLASS__

namespace epi
{

class angle_c
{
	/* sealed class, value semantics */

private:
	// binary angle measurement:
	// 0 = EAST, 0.25 = NORTH, 0.5 = WEST, 0.75 = SOUTH.

	u32_t bam;

	angle_c(u32_t _bam, bool xxx) : bam(_bam) { }
	// xxx is a dummy argument, without it this method's signature
	// becomes too similiar to the integer constructor below.

public:
	angle_c() : bam(0) { };
	angle_c(const angle_c& rhs) : bam(rhs.bam) { }

	angle_c(int deg);    // usable range is -360 .. +360
	angle_c(float deg);  //

	static angle_c FromRadians(double rad);  // range is -2pi .. +2pi
	static angle_c FromVector(float x, float y);

	float Degrees()  const;
	double Radians() const;

	std::string ToStr(int precision = 1) const;

	/* ---- read-only operations ---- */

	inline float Sin() const { return sin(Radians()); }
	inline float Cos() const { return cos(Radians()); }
	inline float Tan() const { return tan(Radians()); }

	static angle_c ATan(float slope);

	inline float getX() const { return Cos(); }
	inline float getY() const { return Sin(); }

	angle_c Abs() const;
	angle_c Dist(const angle_c& other) const;

	inline bool Less180() const { return (bam & 0x80000000) ? false : true; }
	inline bool More180() const { return (bam & 0x80000000) ? true : false; }
	inline bool Less90()  const { return (bam & 0xC0000000) ? false : true; }
	inline bool More90()  const { return (bam & 0xC0000000) ? true : false; }

	inline bool operator<  (const angle_c& rhs) const { return bam <  rhs.bam; }
	inline bool operator>  (const angle_c& rhs) const { return bam >  rhs.bam; }
	inline bool operator>= (const angle_c& rhs) const { return bam >= rhs.bam; }
	inline bool operator<= (const angle_c& rhs) const { return bam <= rhs.bam; }
	inline bool operator== (const angle_c& rhs) const { return bam == rhs.bam; }
	inline bool operator!= (const angle_c& rhs) const { return bam != rhs.bam; }

	angle_c operator- () const;

	angle_c operator+ (const angle_c& rhs) const;
	angle_c operator- (const angle_c& rhs) const;
	angle_c operator* (int factor) const;
	angle_c operator/ (int factor) const;

	/* ---- modifying operations ---- */

	angle_c& operator= (const angle_c& rhs);

	inline angle_c& Add180() { bam ^= 0x80000000; return *this; }
	inline angle_c& Add90()  { bam += 0x40000000; return *this; }
	inline angle_c& Sub90()  { bam -= 0x40000000; return *this; }

	angle_c& operator+= (const angle_c& rhs);
	angle_c& operator-= (const angle_c& rhs);
	angle_c& operator*= (int factor);
	angle_c& operator/= (int factor);

	/* ---- useful constants ---- */

	static inline angle_c Ang0()   { return angle_c(0x00000000, false); }
	static inline angle_c Ang15()  { return angle_c(0x0AAAAAAA, false); }
	static inline angle_c Ang30()  { return angle_c(0x15555555, false); }
	static inline angle_c Ang45()  { return angle_c(0x20000000, false); }
	static inline angle_c Ang60()  { return angle_c(0x2AAAAAAA, false); }
	static inline angle_c Ang90()  { return angle_c(0x40000000, false); }
	static inline angle_c Ang145() { return angle_c(0x60000000, false); }
	static inline angle_c Ang180() { return angle_c(0x80000000, false); }
	static inline angle_c Ang225() { return angle_c(0xA0000000, false); }
	static inline angle_c Ang270() { return angle_c(0xC0000000, false); }
	static inline angle_c Ang315() { return angle_c(0xE0000000, false); }
	static inline angle_c Ang360() { return angle_c(0xFFFFFFFF, false); }
};

//------------------------------------------------------------------------
//    IMPLEMENTATION
//------------------------------------------------------------------------

inline angle_c::angle_c(int deg) : bam(deg * 11930464 + deg * 7 / 10)
{
	/* nothing needed */
}

inline angle_c::angle_c(float deg) :
	bam((u32_t) ((deg < 0 ? (deg + 360.0) : double(deg)) * 11930464.7084))
{
	/* nothing needed */
}

inline angle_c angle_c::FromRadians(double rad)
{
	if (rad < 0)
		rad += M_PI * 2.0;

	return angle_c((u32_t)(rad * 683565275.42), false);
}

inline angle_c angle_c::FromVector(float x, float y)
{
	if (x == 0)
		return (y >= 0) ? Ang90() : Ang270();

	double rad = atan2((double) y, (double) x);

	return FromRadians(rad);
}

inline float angle_c::Degrees() const
{
	return double(bam) / 11930467.0;
}

inline double angle_c::Radians() const
{
	return double(bam) / 683565275.42;
}

inline angle_c angle_c::ATan(float slope)
{
	return FromRadians(atan(slope));
}

inline angle_c angle_c::operator- () const
{
	return angle_c(bam ^ 0xFFFFFFFF, false);
}

inline angle_c angle_c::operator+ (const angle_c& rhs) const
{
	return angle_c(bam + rhs.bam, false);
}

inline angle_c angle_c::operator- (const angle_c& rhs) const
{
	return angle_c(bam - rhs.bam, false);
}

inline angle_c angle_c::operator* (int factor) const
{
	return angle_c(bam * factor, false);
}

inline angle_c angle_c::operator/ (int factor) const
{
	return angle_c(bam / factor, false);
}

inline angle_c angle_c::Abs() const
{
	return angle_c(bam ^ (Less180() ? 0 : 0xFFFFFFFF), false);
}

inline angle_c angle_c::Dist(const angle_c &other) const
{
	return (bam >= other.bam) ? angle_c(bam - other.bam, false) :
	                            angle_c(other.bam - bam, false);
}

inline angle_c& angle_c::operator+= (const angle_c& rhs)
{
	bam += rhs.bam;
	return *this;
}

inline angle_c& angle_c::operator-= (const angle_c& rhs)
{
	bam -= rhs.bam;
	return *this;
}

inline angle_c& angle_c::operator*= (int factor)
{
	bam *= factor;
	return *this;
}

inline angle_c& angle_c::operator/= (int factor)
{
	bam /= factor;
	return *this;
}

inline angle_c& angle_c::operator= (const angle_c& rhs)
{
	// no need to check for self assignment
	bam = rhs.bam;
	return *this;
}

}  // namespace epi

#endif  /* __EPI_ANGLE_CLASS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
