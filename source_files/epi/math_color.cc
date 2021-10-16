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

#include "epi.h"
#include "math_color.h"

#include "asserts.h"

namespace epi
{

hsv_col_c::hsv_col_c(const color_c& col)
{
	int m = MIN(col.r, MIN(col.b, col.g));

	v = MAX(col.r, MAX(col.b, col.g));

	s = (v == 0) ? 0 : (v - m) * 255 / v;

	if (v <= m)
	{
		h = 0;
		return;
	}

	int r1 = (v - col.r) * 59 / (v - m);
	int g1 = (v - col.g) * 59 / (v - m);
	int b1 = (v - col.b) * 59 / (v - m);

	if (v == col.r && m == col.g)
		h = 300 + b1;
	else if (v == col.r)
		h = 60 - g1;

	else if (v == col.g && m == col.b)
		h = 60 + r1;
	else if (v == col.g)
		h = 180 - b1;

	else if (m == col.r)
		h = 180 + g1;
	else
		h = 300 - r1;

	SYS_ASSERT(0 <= h && h <= 360);
}
 
color_c hsv_col_c::GetRGBA() const
{
	SYS_ASSERT(0 <= h && h <= 360);

	int sextant = (h % 360) / 60;
	int frac = h % 60;

	int p1 = 255 - s;
	int p2 = 255 - (s * frac) / 59;
	int p3 = 255 - (s * (59 - frac)) / 59;

	p1 = p1 * v / 255;
	p2 = p2 * v / 255;
	p3 = p3 * v / 255;

	int r, g, b;

	SYS_ASSERT(0 <= sextant && sextant <= 5);

	switch (sextant)
	{
		case 0: r = v,  g = p3, b = p1; break;
		case 1: r = p2, g = v,  b = p1; break;
		case 2: r = p1, g = v,  b = p3; break;
		case 3: r = p1, g = p2, b = v;  break;
		case 4: r = p3, g = p1, b = v;  break;
		default: r = v,  g = p1, b = p2; break;
	}

	SYS_ASSERT(0 <= r && r <= 255);
	SYS_ASSERT(0 <= g && g <= 255);
	SYS_ASSERT(0 <= b && b <= 255);

	return color_c(r, g, b);
}


//------------------------------------------------------------------------
//
//  TEST ROUTINES
//

#ifdef TEST_HSV_CLASS

#include <iostream>

void Test_HSV_Col(const hsv_col_c hsv, const char *name)
{
	cout << name << " : ";

	cout << "[" << hsv.h << ", " << hsv.s << ", " << hsv.v << "]";

	color_c rgb = hsv.ToRGB();

	cout << "   RGB : (" << int(rgb.r) << ", " << int(rgb.g) 
		<< ", " << int(rgb.b) << ")\n";

	hsv_col_c fin = hsv_col_c(rgb);

	cout << "   FIN : [" << fin.h << ", " << fin.s << ", " << fin.v << "]\n";
}

void Test_HSV()
{
	cout << "\n---HSV-TEST-COLORS---------------------\n";

	Test_HSV_Col(hsv_col_c::Black(),     "BLACK");
	Test_HSV_Col(hsv_col_c::DarkGrey(),  "D_GREY");
	Test_HSV_Col(hsv_col_c::Grey(),      "M_GREY");
	Test_HSV_Col(hsv_col_c::LightGrey(), "L_GREY");
	Test_HSV_Col(hsv_col_c::White(),     "WHITE");

	Test_HSV_Col(hsv_col_c::Red(),      "RED");
	Test_HSV_Col(hsv_col_c::Purple(),   "PURPLE");
	Test_HSV_Col(hsv_col_c::Blue(),     "BLUE");
	Test_HSV_Col(hsv_col_c::Cyan(),     "CYAN");
	Test_HSV_Col(hsv_col_c::Green(),    "GREEN");
	Test_HSV_Col(hsv_col_c::Yellow(),   "YELLOW");
	Test_HSV_Col(hsv_col_c::Orange(),   "ORANGE");

	cout << "\n";

	int i;
	int seed = 123456789;

	for (i=0; i < 30; i++)
	{
		int H = abs(seed >> 8) % 360;
		int S = ((seed >> 4) & 0xF) * 17;
		int V = ((seed     ) & 0xF) * 17;

		TestHSV(hsv_col_c(H, S, V), "Random");

		/* simple K&R random */
		seed = seed * 1103515245 + 12345;
	}

	cout << "---------------------------------------\n\n";
}

#endif  /* TEST_HSV_CLASS */

}  // namespace epi


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
