//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Cameras)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
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
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "r_camera.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "con_main.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_misc.h"
#include "r_state.h"
#include "r_gldefs.h"
#include "r_draw.h"
#include "z_zone.h"

/* #define TEST_CAMERA_CLASS 1 */

static const float ASPECT_RATIO = 200.0f / 320.0f;

camera_c::camera_c() : FOV(epi::angle_c::Ang90()), pos(), turn(), mlook()
{
	Recalculate();
}

camera_c::camera_c(const camera_c& other) : FOV(other.FOV), pos(other.pos),
	turn(other.turn), mlook(other.mlook)
{
	// Note: a tad inefficient, but keeps it simple
	Recalculate();
}

camera_c::~camera_c()
{
	// nothing to free here, move along...
}

void camera_c::FindAngles(epi::vec3_c _dir)
{
	float len_2d = sqrt(_dir.x * _dir.x + _dir.y * _dir.y);

#if 0
	if (len_2d < 0.001f)
	{
		// vector is directly upwards or downwards, do the best we can.
		// (don't change TURN angle, hopefully current value is better
		// than an abitrary choice).

		mlook = (_dir.z < 0) ? epi::angle_c(-89.9f) : epi::angle_c(89.9f);
		return;
	}
#endif

	turn  = epi::angle_c::FromVector(_dir.x, _dir.y);
	mlook = epi::angle_c::FromVector(len_2d, _dir.z);

#ifdef TEST_CAMERA_CLASS
	L_WriteDebug("  FindAngles: (%1.4f,%1.4f,%1.4f) --> turn %1.2f | mlook %1.2f\n",
		_dir.x, _dir.y, _dir.z, turn.Degrees(), mlook.Degrees());
#endif
}

void camera_c::Recalculate()
{
#ifdef TEST_CAMERA_CLASS
	L_WriteDebug("  Recalculate: turn %1.1f mlook %1.1f pos (%1.3f,%1.3f,%1.3f)\n",
		turn.Degrees(), mlook.Degrees(), pos.x, pos.y, pos.z);
#endif

	// compute slopes
	epi::angle_c SideAng = FOV / 2;

	horiz_slope = SideAng.Tan();

	float aspect = ASPECT_RATIO *
		(float(viewwindow_h) / SCREENHEIGHT) /
		(float(viewwindow_w) / SCREENWIDTH);

	vert_slope = horiz_slope * aspect;

	// compute face direction (this code ensures length == 1)
	dir.z = mlook.Sin();

	float len_2d = mlook.Cos();

	dir.x = len_2d * turn.Cos();
	dir.y = len_2d * turn.Sin();

#ifdef TEST_CAMERA_CLASS
	L_WriteDebug("    aspect %1.3f horiz %1.3f vert %1.3f dir (%1.3f,%1.3f,%1.3f)\n",
		aspect, horiz_slope, vert_slope, dir.x, dir.y, dir.z);
#endif

	// cone information
	float diagonal = sqrtf(horiz_slope * horiz_slope + vert_slope * vert_slope);

	cone_cos = Z_NEAR / sqrtf(Z_NEAR * Z_NEAR + diagonal * diagonal);
	cone_tan = diagonal / Z_NEAR;

	// plane vectors... (must face inwards)
	left_v  = epi::vec3_c(Z_NEAR, -horiz_slope, 0);
	right_v = epi::vec3_c(Z_NEAR,  horiz_slope, 0);

	top_v    = epi::vec3_c(vert_slope, 0, -Z_NEAR);
	bottom_v = epi::vec3_c(vert_slope, 0,  Z_NEAR);

#ifdef TEST_CAMERA_CLASS
	L_WriteDebug("    cone_diagonal %1.3f cone_cos %1.3f cone_tan %1.3f\n",
		diagonal, cone_cos, cone_tan);

	L_WriteDebug("  Raw vectors: L=(%1.3f,%1.3f,%1.3f) R=(%1.3f,%1.3f,%1.3f)\n",
		left_v.x, left_v.y, left_v.z, right_v.x, right_v.y, right_v.z);
	L_WriteDebug("               T=(%1.3f,%1.3f,%1.3f) B=(%1.3f,%1.3f,%1.3f)\n",
		top_v.x, top_v.y, top_v.z, bottom_v.x, bottom_v.y, bottom_v.z);
#endif

	AdjustPlane(&left_v);
	AdjustPlane(&right_v);
	AdjustPlane(&top_v);
	AdjustPlane(&bottom_v);

#ifdef TEST_CAMERA_CLASS
	L_WriteDebug("  Final vectors: L=(%1.3f,%1.3f,%1.3f) R=(%1.3f,%1.3f,%1.3f)\n",
		left_v.x, left_v.y, left_v.z, right_v.x, right_v.y, right_v.z);
	L_WriteDebug("                 T=(%1.3f,%1.3f,%1.3f) B=(%1.3f,%1.3f,%1.3f)\n",
		top_v.x, top_v.y, top_v.z, bottom_v.x, bottom_v.y, bottom_v.z);
	
	L_WriteDebug("\n");
#endif
}

void camera_c::AdjustPlane(epi::vec3_c *v)
{
	// initial vector will be relative to the camera looking
	// due east (no rotation or mlook).  First we apply mlook,
	// then the camera's turn angle, and lastly scale the
	// vector to be unit length.

	epi::vec2_c temp(v->x, v->z);
	temp.Rotate(mlook);

	v->z = temp.y;

	temp.y = v->y;
	temp.Rotate(turn);

	v->x = temp.x;
	v->y = temp.y;

	v->MakeUnit();
}

void camera_c::LoadGLMatrices(bool translate) const
{
	glMatrixMode(GL_PROJECTION);

	glLoadIdentity();
	glFrustum(-horiz_slope, horiz_slope, -vert_slope, vert_slope, Z_NEAR, Z_FAR);

	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();
	glRotatef(270.0f - mlook.Degrees(), 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f  - turn.Degrees(), 0.0f, 0.0f, 1.0f);

	if (translate)
		glTranslatef(-pos.x, -pos.y, -pos.z);
}

int camera_c::TestSphere(epi::vec3_c mid, float R) const
{
	// first step: check for sphere surrounding camera
	epi::vec3_c relat(mid - pos);
	
	if (relat * relat <= R * R)
		return HIT_SURROUND;

	// second step: check if completely behind the near plane
	float along_axis = relat * dir;

	if (along_axis - Z_NEAR <= -R)
		return HIT_OUTSIDE;
	
	// third step: check sphere against the cone
	float perp_sphr2 = relat * relat - along_axis * along_axis;
	float perp_cone  = along_axis * cone_tan + R / cone_cos;

	// NOTE: not useful to return HIT_INSIDE (when dist <= -R) since
	//       the cone is larger than the view frustum, hence leads to
	//       false positives.

	if (perp_sphr2 >= perp_cone * perp_cone)
		return HIT_OUTSIDE;

	return HIT_PARTIAL;
}

int camera_c::TestBBox(const epi::bbox3_c *bb) const
{
	// first step: check for bbox surrounding camera
	if (bb->Contains(pos + dir))
		return HIT_SURROUND;

	// second step: check bbox against each frustum plane
	bool partial = false;

	if (TryBBoxPlane(bb, pos + dir, dir, &partial))
		return HIT_OUTSIDE;

	if (TryBBoxPlane(bb, pos, left_v, &partial))
		return HIT_OUTSIDE;

	if (TryBBoxPlane(bb, pos, right_v, &partial))
		return HIT_OUTSIDE;

	if (TryBBoxPlane(bb, pos, top_v, &partial))
		return HIT_OUTSIDE;

	if (TryBBoxPlane(bb, pos, bottom_v, &partial))
		return HIT_OUTSIDE;

	return partial ? HIT_PARTIAL : HIT_INSIDE;
}

//------------------------------------------------------------------------
//  TEST ROUTINES
//------------------------------------------------------------------------

#ifdef TEST_CAMERA_CLASS

static void Test_Camera_Recalc(void)
{
	// generate bounding spheres and bboxes
	float BR = 10;
	float SR = BR * sqrtf(3.0f);

	epi::vec3_c Sph1(-150, 50, 20);
	epi::vec3_c Sph2(0,    50, 20);
	epi::vec3_c Sph3(50,   50, 20);

	epi::vec3_c Adj(BR, BR, BR);

	epi::bbox3_c Box1(Sph1 - Adj, Sph1 + Adj);
	epi::bbox3_c Box2(Sph2 - Adj, Sph2 + Adj);
	epi::bbox3_c Box3(Sph3 - Adj, Sph3 + Adj);

//	L_WriteDebug("Box1 (%1.0f,%1.0f,%1.0f) .. (%1.0f,%1.0f,%1.0f)\n",
//		Box1.lo.x, Box1.lo.y, Box1.lo.z,
//		Box1.hi.x, Box1.hi.y, Box1.hi.z);

	L_WriteDebug("\n---CAMERA-RECALC-TEST-------------------------------\n\n");

	L_WriteDebug("Standard constructor...\n");

	camera_c cam;

	for (int mlook_idx = 0; mlook_idx < 3; mlook_idx++)
	{
		epi::angle_c m((mlook_idx == 0) ? 0 : (mlook_idx == 1) ? 45 : -45);

		for (int turn_idx = 0; turn_idx < 8; turn_idx++)
		{
			epi::angle_c t(turn_idx * 45);

			L_WriteDebug("Setting angles to: turn %1.3f | mlook %1.3f\n",
				t.Degrees(), m.Degrees());

			cam.SetDir(t, m);

			int hs = cam.TestSphere(Sph1, SR);
			int hb = cam.TestBBox(&Box1);

			L_WriteDebug("  Sphere1: %c  Box1: %c\n",
				(hs == camera_c::HIT_OUTSIDE) ? 'O' : 'P',
				(hb == camera_c::HIT_OUTSIDE) ? 'O' :
				(hb == camera_c::HIT_INSIDE)  ? 'I' : 'P');

			hs = cam.TestSphere(Sph2, SR);
			hb = cam.TestBBox(&Box2);

			L_WriteDebug("  Sphere2: %c  Box2: %c\n",
				(hs == camera_c::HIT_OUTSIDE) ? 'O' : 'P',
				(hb == camera_c::HIT_OUTSIDE) ? 'O' :
				(hb == camera_c::HIT_INSIDE)  ? 'I' : 'P');

			hs = cam.TestSphere(Sph3, SR);
			hb = cam.TestBBox(&Box3);

			L_WriteDebug("  Sphere3: %c  Box3: %c\n",
				(hs == camera_c::HIT_OUTSIDE) ? 'O' : 'P',
				(hb == camera_c::HIT_OUTSIDE) ? 'O' :
				(hb == camera_c::HIT_INSIDE)  ? 'I' : 'P');

			L_WriteDebug("\n");
		}
	}

	L_WriteDebug("-------------------------------------------------\n\n");
}

static void Test_Camera_FromDir(void)
{
	L_WriteDebug("\n---CAMERA-FROMDIR-TEST------------------------------\n\n");

	L_WriteDebug("Standard constructor...\n");

	camera_c cam;

	epi::vec3_c new_dir;
	
	new_dir = epi::vec3_c(123, 234, 0);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(-123, 56, 234);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(45, -68, -89);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(-45, 0, -89);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(0, 2, 1);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(0, 0, 3);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	new_dir = epi::vec3_c(0, 0, -5);
	L_WriteDebug("Setting dir to (%1.1f,%1.1f,%1.1f)\n", new_dir.x, new_dir.y, new_dir.z);
	cam.SetDir(new_dir);

	L_WriteDebug("-------------------------------------------------\n\n");
}

void Test_Camera(void)
{
	Test_Camera_Recalc();
	Test_Camera_FromDir();
}

#endif // TEST_CAMERA_CLASS


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
