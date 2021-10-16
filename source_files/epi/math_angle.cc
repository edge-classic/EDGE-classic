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

#include "epi.h"

#include "math_angle.h"
#include "str_format.h"

namespace epi
{

std::string angle_c::ToStr(int precision) const
{
	return STR_Format("%1.*f", precision, Degrees());
}


//------------------------------------------------------------------------
//  TEST ROUTINES
//------------------------------------------------------------------------

#ifdef TEST_ANGLE_CLASS

void Test_Angle_Conv()
{
	printf("Conversions....\n");

	for (int i = -355; i < 360; i += 37)
	{
		float f = float(i);
		float rad = i * M_PI / 180.0f;

		angle_c D(i);
		angle_c F(f);
		angle_c R = angle_c::FromRadians(rad);

		printf("%+4d -> D=% 8.3f F=% 8.3f R=% 8.3f  |  % 9.6f -> rad=% 9.6f\n",
			i, D.Degrees(), F.Degrees(), R.Degrees(), rad, F.Radians());
	}

	printf("\n");

	for (int i = -360; i <= 360; i += 360)
	{
		float f = float(i);
		float rad = i * M_PI / 180.0f;

		angle_c D(i);
		angle_c F(f);
		angle_c R = angle_c::FromRadians(rad);

		printf("%+4d -> D=% 8.3f F=% 8.3f R=% 8.3f  |  % 9.6f -> rad=% 9.6f\n",
			i, D.Degrees(), F.Degrees(), R.Degrees(), rad, F.Radians());
	}

	printf("\n");
}

void Test_Angle_Vector()
{
	printf("Vectors....\n");

	for (int x = -21; x <= +21; x += 7)
	for (int y = -33; y <= +33; y += 11)
	{
		float slope = (y + x / 5.0f) / 10.0f;

		angle_c V = angle_c::FromVector(x, y);
		angle_c S = angle_c::ATan(slope);

		float x2 = V.getX();
		float y2 = V.getY();

		printf("(%+3d,%+3d) -> A=%-3.0f (%+7.4f,%+7.4f)  |  "
				"slope % 7.4f -> % 7.4f (A=%-3.0f)\n",
				x, y, V.Degrees(), x2, y2, slope, S.Tan(), S.Degrees());
	}

	printf("\n");
}

void Test_Angle_Dist()
{
	printf("Distances....\n");

	for (int m = 0; m <= 360; m += 60)
	for (int n = 0; n <= 360; n += 37)
	{
		angle_c A(m);
		angle_c B(n);

		angle_c P = A + B;
		angle_c M = A - B;
		angle_c D = A.Dist(B);

		printf("|%5.1f .. %5.1f|  Dist = %5.1f  Add = %5.1f  Sub = %5.1f\n",
			   A.Degrees(), B.Degrees(), D.Degrees(),
			   P.Degrees(), M.Degrees());
	}

	printf("\n");
}

void Test_Angle_Const()
{
	printf("Constants....\n");

	printf("%3d = %10.6f  rad = %12.10f\n", 15,
			angle_c::Ang15().Degrees(), angle_c::Ang15().Radians());
	
	printf("%3d = %10.6f  rad = %12.10f\n", 30,
			angle_c::Ang30().Degrees(), angle_c::Ang30().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 45,
			angle_c::Ang45().Degrees(), angle_c::Ang45().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 60,
			angle_c::Ang60().Degrees(), angle_c::Ang60().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 90,
			angle_c::Ang90().Degrees(), angle_c::Ang90().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 145,
			angle_c::Ang145().Degrees(), angle_c::Ang145().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 180,
			angle_c::Ang180().Degrees(), angle_c::Ang180().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 225,
			angle_c::Ang225().Degrees(), angle_c::Ang225().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 270,
			angle_c::Ang270().Degrees(), angle_c::Ang270().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 315,
			angle_c::Ang315().Degrees(), angle_c::Ang315().Radians());

	printf("%3d = %10.6f  rad = %12.10f\n", 360,
			angle_c::Ang360().Degrees(), angle_c::Ang360().Radians());

	printf("\n");
}

void Test_Angle_Misc()
{
	printf("Miscellaneous....\n");

	for (int m = 0; m <= 360; m += 20)
	{
		angle_c X(m);

		angle_c Neg = - X;
		angle_c Abs = X.Abs();

		angle_c Doub = X * 2;
		angle_c Half = X / 2;

		angle_c J(X);  J.Add90();
		angle_c K(X);  K.Add180();

		printf("%3.0f -> Neg=%3.0f  Abs=%3.0f  Doub=%3.0f  Half=%3.0f  +90=%3.0f  +180=%3.0f\n",
			   X.Degrees(), Neg.Degrees(), Abs.Degrees(),
			   Doub.Degrees(), Half.Degrees(), J.Degrees(), K.Degrees());
	}

	printf("\n");
}

void Test_Angle_Bool()
{
	printf("Booleans....\n");

	for (int m =  9; m <= 360; m += 41)
	for (int n = 12; n <= 360; n += 60)
	{
		angle_c A(m);
		angle_c B(n);

		printf("%5.1f __ %5.1f:  <%c  <=%c  >%c  >=%c  ==%c  !=%c  |  <90%c  >180%c\n",
			   A.Degrees(), B.Degrees(),

			   (A <  B) ? 'Y' : 'n', (A <= B) ? 'Y' : 'n',
			   (A >  B) ? 'Y' : 'n', (A >= B) ? 'Y' : 'n',
			   (A == B) ? 'Y' : 'n', (A != B) ? 'Y' : 'n',

			   (A.Less90())  ? 'Y' : 'n',
			   (A.More180()) ? 'Y' : 'n');
	}

	printf("\n");
}

void Test_Angles()
{
	printf("\n---ANGLE-TEST--------------------------------------\n\n");

	Test_Angle_Conv();
	Test_Angle_Vector();
	Test_Angle_Dist();
	Test_Angle_Const();
	Test_Angle_Misc();
	Test_Angle_Bool();

	printf("-------------------------------------------------\n\n");
}

#endif  /* TEST_ANGLE_CLASS */

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
