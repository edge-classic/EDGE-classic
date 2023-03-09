//------------------------------------------------------------------------
//  EPI Colour types (RGBA and HSV)
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

#ifndef __EPI_MATH_COLOR_H__
#define __EPI_MATH_COLOR_H__

namespace epi
{

class color_c
{
public:
	// sealed class with value semantics (i.e. pass it around whole,
	// rather than use pointers or references).
	//
	// r is red, g is green, b is blue, a is alpha. 
	// 
	// Color components range from 0 to 255.  Alpha is like OpenGL: 0 is
	// totally transparent, and 255 is totally solid/opaque.

	byte r, g, b, a;

	color_c() : r(0), g(0), b(0), a(255) { }

	color_c(byte nr, byte ng, byte nb, byte na = 255) : 
		r(nr), g(ng), b(nb), a(na) { }
	
	color_c(const color_c& rhs) :
		r(rhs.r), g(rhs.g), b(rhs.b), a(rhs.a) { }

	color_c(int packed) :  // Note: no alpha field
		r((packed >> 16) & 0xFF), g((packed >> 8) & 0xFF), b(packed & 0xFF),
		a(255) { }

	int GetPacked() const;  // Note: no alpha field

	color_c& operator= (const color_c& rhs);

	bool operator== (const color_c& rhs) const;
	bool operator!= (const color_c& rhs) const;

	color_c& ClampSet(int r, int g, int b, int a = 255);
	// set the color value, clamping the given components.

	int Dist(const color_c& other) const;
	int DistAlpha(const color_c& other) const;
	// compute distance between the two colors.  Returns 0 for an exact
	// match, and higher values for bigger differences.  First version
	// ignores alpha, second version includes it.

	color_c  Mix(const color_c& other, int qty = 128) const;
	color_c& MixThis(const color_c& other, int qty = 128);
	// mixes this color with the other color.  Qty is how much of the
	// other color to mix in (from 0 to 255, where 0 is none of it, and
	// 255 is all of it).  Alpha is also mixed (same as the others).

	color_c  Blend(const color_c& other) const;
	color_c& BlendThis(const color_c& other);
	// blend this color with the other color, using the other color's
	// alpha (where 0 means no change, and 255 will make the result the
	// same as the other color).  The alpha remains unchanged.

	color_c  Solidify(const color_c& other) const;
	color_c& SolidifyThis(const color_c& other);
	// solidify this color by mixing in the other color using the
	// alpha here.  The other color's alpha is ignored.  The resulting
	// alpha will be 255.

	color_c Hue() const;
	color_c& HueThis();
	// compute the color's "hue", which for pure black (0,0,0) is just
	// black, and for everything else is the color made as bright as
	// possible (e.g. dark grey becomes white, and dark blue becomes
	// bright blue).  The alpha will be solid (255).

	float Intensity() const;
	// compute the intensity, from 0.0 (darkest) to 1.0 (lightest).
	// Uses sqrt(), so can be slow.  Ignores alpha.

	int RoughIntensity() const;
	// a faster but crappier intensity function.

	/* some useful constants */
	static inline color_c Black()     { return color_c(0, 0, 0); }
	static inline color_c Blue()      { return color_c(0, 0, 255); }
	static inline color_c Cyan()      { return color_c(0, 255, 255); }
	static inline color_c Green()     { return color_c(0, 255, 0); }
	static inline color_c Red()       { return color_c(255, 0, 0); }
	static inline color_c Purple()    { return color_c(255, 0, 255); }
	static inline color_c Yellow()    { return color_c(255, 255, 0); }
	static inline color_c White()     { return color_c(255, 255, 255); }

	static inline color_c Grey()      { return color_c(128, 128, 128); }
	static inline color_c DarkGrey()  { return color_c(64, 64, 64); }
	static inline color_c LightGrey() { return color_c(192, 192, 192); }
	static inline color_c Orange()    { return color_c(255, 128, 0); }
};


class hsv_col_c
{
public:
   // sealed, value semantics.
   //
   // h is hue (angle from 0 to 359: 0 = RED, 120 = GREEN, 240 = BLUE).
   // s is saturation (0 to 255: 0 = White, 255 = Pure color).
   // v is value (0 to 255: 0 = Darkest, 255 = Brightest).

	short h; byte s, v;

	hsv_col_c() : h(0), s(0), v(0) { }
	hsv_col_c(short nh, short ns, short nv) : h(nh), s(ns), v(nv) { }
	hsv_col_c(const hsv_col_c& rhs) : h(rhs.h), s(rhs.s), v(rhs.v) { }
	hsv_col_c(const color_c& col);  // conversion from RGBA

	color_c GetRGBA() const;  // conversion to RGBA

	hsv_col_c& ClampSet(int nh, int ns, int nv);
	hsv_col_c& Rotate(int delta);  // usable range: -1800 to +1800

	hsv_col_c& operator= (const hsv_col_c& rhs);

	bool operator== (const hsv_col_c& rhs) const;
	bool operator!= (const hsv_col_c& rhs) const;

	int Dist(const hsv_col_c& other) const;
	// compute distance between the two colors.  Returns 0 for an exact
	// match, and higher values for bigger differences.  This method may
	// produce different results than if you use color_c::Dist().

	/* some useful constants */
	static inline hsv_col_c Black()     { return hsv_col_c(0, 0, 0); }
	static inline hsv_col_c DarkGrey()  { return hsv_col_c(0, 0, 64); }
	static inline hsv_col_c Grey()      { return hsv_col_c(0, 0, 128); }
	static inline hsv_col_c LightGrey() { return hsv_col_c(0, 0, 192); }
	static inline hsv_col_c White()     { return hsv_col_c(0, 0, 255); }

	static inline hsv_col_c Red()       { return hsv_col_c(0,   255, 255); }
	static inline hsv_col_c Purple()    { return hsv_col_c(300, 255, 255); }
	static inline hsv_col_c Blue()      { return hsv_col_c(240, 255, 255); }
	static inline hsv_col_c Cyan()      { return hsv_col_c(180, 255, 255); }
	static inline hsv_col_c Green()     { return hsv_col_c(120, 255, 255); }
	static inline hsv_col_c Yellow()    { return hsv_col_c(60,  255, 255); }
	static inline hsv_col_c Orange()    { return hsv_col_c(30,  255, 255); }
};

//------------------------------------------------------------------------
//  IMPLEMENTATION
//------------------------------------------------------------------------

inline int color_c::GetPacked() const
{
	return (int(r) << 16) | (int(g) << 8) | int(b);
}

inline color_c& color_c::ClampSet(int _r, int _g, int _b, int _a)
{
	r = byte(MAX(0, MIN(255, _r)));
	g = byte(MAX(0, MIN(255, _g)));
	b = byte(MAX(0, MIN(255, _b)));
	a = byte(MAX(0, MIN(255, _a)));

	return *this;
}

inline color_c& color_c::operator= (const color_c& rhs)
{
	// self assignment won't matter here
	r = rhs.r; g = rhs.g; b = rhs.b; a = rhs.a;
	return *this;
}

inline bool color_c::operator== (const color_c& rhs) const
{
	return (r == rhs.r) && (g == rhs.g) && (b == rhs.b) && (a == rhs.a);
}

inline bool color_c::operator!= (const color_c& rhs) const
{
	return (r != rhs.r) || (g != rhs.g) || (b != rhs.b) || (a != rhs.a);
}

inline int color_c::Dist(const color_c& other) const
{
	int dr = int(r) - int(other.r);
	int dg = int(g) - int(other.g);
	int db = int(b) - int(other.b);

	return (dr * dr) + (dg * dg) + (db * db);
}

inline int color_c::DistAlpha(const color_c& other) const
{
	int dr = int(r) - int(other.r);
	int dg = int(g) - int(other.g);
	int db = int(b) - int(other.b);
	int da = int(a) - int(other.a);

	return (dr * dr) + (dg * dg) + (db * db) + (da * da);
}

inline color_c color_c::Mix(const color_c& other, int qty) const
{
	int nr = int(r) * (255 - qty) + int(other.r) * qty;
	int ng = int(g) * (255 - qty) + int(other.g) * qty;
	int nb = int(b) * (255 - qty) + int(other.b) * qty;
	int na = int(a) * (255 - qty) + int(other.a) * qty;

	return color_c(byte(nr / 255), byte(ng / 255),
					byte(nb / 255), byte(na / 255));
}

inline color_c& color_c::MixThis(const color_c& other, int qty)
{
	int nr = int(r) * (255 - qty) + int(other.r) * qty;
	int ng = int(g) * (255 - qty) + int(other.g) * qty;
	int nb = int(b) * (255 - qty) + int(other.b) * qty;
	int na = int(a) * (255 - qty) + int(other.a) * qty;

	r = byte(nr / 255); g = byte(ng / 255);
	b = byte(nb / 255); a = byte(na / 255);

	return *this;
}

inline color_c color_c::Blend(const color_c& other) const
{
	int qty = other.a;

	int nr = int(r) * (255 - qty) + int(other.r) * qty;
	int ng = int(g) * (255 - qty) + int(other.g) * qty;
	int nb = int(b) * (255 - qty) + int(other.b) * qty;

	return color_c(byte(nr / 255), byte(ng / 255), byte(nb / 255), a);
}

inline color_c& color_c::BlendThis(const color_c& other)
{
	int qty = other.a;

	int nr = int(r) * (255 - qty) + int(other.r) * qty;
	int ng = int(g) * (255 - qty) + int(other.g) * qty;
	int nb = int(b) * (255 - qty) + int(other.b) * qty;

	r = byte(nr / 255); g = byte(ng / 255); b = byte(nb / 255);

	return *this;
}

inline color_c color_c::Solidify(const color_c& other) const
{
	int qty = a;

	int nr = int(r) * qty + int(other.r) * (255 - qty);
	int ng = int(g) * qty + int(other.g) * (255 - qty);
	int nb = int(b) * qty + int(other.b) * (255 - qty);

	return color_c(byte(nr / 255), byte(ng / 255), byte(nb / 255), 255);
}

inline color_c& color_c::SolidifyThis(const color_c& other)
{
	int qty = a;

	int nr = int(r) * qty + int(other.r) * (255 - qty);
	int ng = int(g) * qty + int(other.g) * (255 - qty);
	int nb = int(b) * qty + int(other.b) * (255 - qty);

	r = byte(nr / 255); g = byte(ng / 255); b = byte(nb / 255); a = 255;

	return *this;
}

inline color_c color_c::Hue() const
{
	int maxval = MAX(r, MAX(g, b));

	// prevent division by zero (black --> black)
	if (maxval == 0)
		return color_c(0, 0, 0);

	return color_c(int(r) * 255 / maxval, int(g) * 255 / maxval,
					int(b) * 255 / maxval);
}

inline color_c& color_c::HueThis()
{
	int maxval = MAX(r, MAX(g, b));

	// prevent division by zero (black --> black)
	if (maxval == 0)
	{
		r = g = b = 0;
	}
	else
	{
		r = int(r) * 255 / maxval;
		g = int(g) * 255 / maxval;
		b = int(b) * 255 / maxval;
	}

	a = 255;

	return *this;
}

inline float color_c::Intensity() const
{
	float r2 = r / 255.0;
	float g2 = g / 255.0;
	float b2 = b / 255.0;

	return sqrt(r2 * r2 * 0.30 + g2 * g2 * 0.50 + b2 * b2 * 0.20);
}

inline int color_c::RoughIntensity() const
{
	// result is between 0 and 255
	return (int(r) * 3 + int(g) * 5 + int(b) * 2) / 10;
}

//------------------------------------------------------------------------

inline hsv_col_c& hsv_col_c::ClampSet(int nh, int ns, int nv)
{
	h = byte(MAX(0, MIN(359, nh)));
	s = byte(MAX(0, MIN(255, ns)));
	v = byte(MAX(0, MIN(255, nv)));

	return *this;
}

inline hsv_col_c& hsv_col_c::Rotate(int delta)
{
	int bam = int(h + delta) * 372827;
	 
	h = short((bam & 0x7FFFFFF) / 372827);

	return *this;
}

inline hsv_col_c& hsv_col_c::operator= (const hsv_col_c& rhs)
{
	// (self assignment should not be any problem)
	h = rhs.h; s = rhs.s; v = rhs.v;
	return *this;
}

inline bool hsv_col_c::operator== (const hsv_col_c& rhs) const
{
	return (h == rhs.h) && (s == rhs.s) && (v == rhs.v);
}

inline bool hsv_col_c::operator!= (const hsv_col_c& rhs) const
{
	return (h != rhs.h) || (s != rhs.s) || (v != rhs.v);
}

inline int hsv_col_c::Dist(const hsv_col_c& other) const
{
	int dh = int(h) - int(other.h);
	int ds = int(s) - int(other.s);
	int dv = int(v) - int(other.v);

	if (dh > 180)
		dh -= 360;
	else if (dh < -180)
		dh += 360;

	// weightings in favour of hue first, value second
	return dh * dh * 5 + dv * dv * 3 + ds * ds * 2;
}

}  // namespace epi

#endif  /* __EPI_MATH_COLOR_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
